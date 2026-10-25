/*
  Q Light Controller Plus
  preview3d.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QLabel>
#include <QComboBox>
#include <QAction>
#include <QIcon>
#include <QSize>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSysInfo>
#include <QTextStream>

#include "preview3d.h"
#include "qlcconfig.h"
#include "appicons.h"
#include "preview3dview.h"
#include "preview3drenderbackend.h"
#include "preview3dsettingsdialog.h"
#include "trusspanel.h"
#include "trussscene.h"
#include "fixturepanel.h"
#include "fixturescene.h"
#include "stagegeometry.h"
#include "doc.h"

Preview3D *Preview3D::s_instance = NULL;

Preview3D::Preview3D(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_loading(false)
    , m_view(NULL)
    , m_trussPanel(NULL)
    , m_trussScene(NULL)
    , m_fixturePanel(NULL)
    , m_fixtureScene(NULL)
    , m_splitter(NULL)
    , m_infoLabel(NULL)
    , m_renderStatusLabel(NULL)
    , m_toolbar(NULL)
    , m_settingsAction(NULL)
    , m_rendererCombo(NULL)
    , m_renderBackend(NULL)
    , m_activeRenderer(Preview3DRenderer::Software)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 10);
    layout->setSpacing(10);

    QFrame *actionBar = new QFrame(this);
    actionBar->setObjectName("pageActionBar");
    actionBar->setFixedHeight(48);
    QHBoxLayout *barLayout = new QHBoxLayout(actionBar);
    barLayout->setContentsMargins(6, 4, 6, 4);
    barLayout->setSpacing(6);

    m_toolbar = new QToolBar(actionBar);
    m_toolbar->setObjectName("pageToolBar");
    m_toolbar->setIconSize(QSize(24, 24));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    barLayout->addWidget(m_toolbar);
    layout->addWidget(actionBar);

    m_renderStatusLabel = new QLabel(this);
    m_renderStatusLabel->setObjectName("preview3dRenderStatus");
    m_renderStatusLabel->setTextFormat(Qt::PlainText);
    m_renderStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_renderStatusLabel->setContentsMargins(6, 0, 6, 0);
    m_renderStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QPalette statusPalette = m_renderStatusLabel->palette();
    statusPalette.setColor(QPalette::WindowText, statusPalette.color(QPalette::Link));
    m_renderStatusLabel->setPalette(statusPalette);
    layout->addWidget(m_renderStatusLabel);

    m_fitAction = m_toolbar->addAction(AppIcons::icon("maximize", 24), tr("Fit to room"));
    m_fitAction->setToolTip(tr("Frame the whole room"));
    connect(m_fitAction, &QAction::triggered, this, &Preview3D::slotFit);

    m_resetAction = m_toolbar->addAction(AppIcons::icon("refresh-cw", 24), tr("Reset view"));
    m_resetAction->setToolTip(tr("Restore the default camera"));
    connect(m_resetAction, &QAction::triggered, this, &Preview3D::slotReset);

    m_gridAction = m_toolbar->addAction(AppIcons::icon("grid-3x3", 24), tr("Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setToolTip(tr("Show or hide the floor grid"));
    connect(m_gridAction, &QAction::toggled, this, &Preview3D::slotGridToggled);

    m_beamPreviewAction = m_toolbar->addAction(AppIcons::icon("fixtures", 24), tr("Beam preview"));
    m_beamPreviewAction->setCheckable(true);
    m_beamPreviewAction->setChecked(false);
    m_beamPreviewAction->setToolTip(tr("Show static beams in design mode"));
    connect(m_beamPreviewAction, &QAction::toggled, this, &Preview3D::slotBeamPreviewToggled);

    m_toolbar->addSeparator();

    m_trussAction = m_toolbar->addAction(AppIcons::icon("frame", 24), tr("Truss"));
    m_trussAction->setCheckable(true);
    m_trussAction->setToolTip(tr("Show or hide the truss configuration panel"));
    connect(m_trussAction, &QAction::toggled, this, &Preview3D::slotTrussToggled);

    m_fixtureAction = m_toolbar->addAction(AppIcons::icon("fixtures", 24), tr("Fixtures"));
    m_fixtureAction->setCheckable(true);
    m_fixtureAction->setToolTip(tr("Show or hide the fixture placement panel"));
    connect(m_fixtureAction, &QAction::toggled, this, &Preview3D::slotFixtureToggled);

    m_rendererCombo = new QComboBox(m_toolbar);
    m_rendererCombo->setObjectName("preview3dRendererCombo");
    m_rendererCombo->addItem(tr("Renderer: Auto"), int(Preview3DRenderer::Auto));
    if (Preview3DRenderBackendFactory::gpuBackendAvailable())
        m_rendererCombo->addItem(tr("Renderer: OpenGL"), int(Preview3DRenderer::Gpu));
    m_rendererCombo->addItem(tr("Renderer: Software"), int(Preview3DRenderer::Software));
    m_rendererCombo->setToolTip(tr("Select the 3D Preview renderer. The software renderer is always available."));
    m_toolbar->addWidget(m_rendererCombo);
    connect(m_rendererCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Preview3D::slotRendererChanged);

    m_settingsAction = m_toolbar->addAction(AppIcons::icon("settings-2", 24), QString());
    m_settingsAction->setToolTip(tr("3D Preview settings and diagnostics"));
    connect(m_settingsAction, &QAction::triggered, this, &Preview3D::slotSettings);

    QWidget *spacer = new QWidget(m_toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_infoLabel = new QLabel(m_toolbar);
    m_infoLabel->setContentsMargins(0, 0, 8, 0);
    m_toolbar->addWidget(m_infoLabel);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("pagePanel");
    layout->addWidget(m_splitter);
    m_view = new Preview3DView(m_splitter);
    m_splitter->addWidget(m_view);

    m_trussScene = new TrussScene(this);
    m_view->setTrussScene(m_trussScene);
    connect(m_trussScene, &TrussScene::changed, this, &Preview3D::slotTrussSceneChanged);

    m_trussPanel = new TrussPanel(m_trussScene, m_splitter);
    m_trussPanel->setVisible(false);
    m_splitter->addWidget(m_trussPanel);

    m_fixtureScene = new FixtureScene(m_doc, this);
    m_view->setFixtureScene(m_fixtureScene);
    connect(m_fixtureScene, &FixtureScene::changed, this, &Preview3D::slotFixtureSceneChanged);

    m_fixturePanel = new FixturePanel(m_fixtureScene, m_splitter);
    m_fixturePanel->setVisible(false);
    m_splitter->addWidget(m_fixturePanel);

    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);
    layout->addWidget(m_splitter, 1);

    connect(m_trussPanel, &TrussPanel::selectionChanged, m_view, &Preview3DView::setSelectedTruss);
    connect(m_view, &Preview3DView::trussSelected, m_trussPanel, &TrussPanel::setSelectedId);
    connect(m_fixturePanel, &FixturePanel::selectionChanged, m_view, &Preview3DView::setSelectedFixture);
    connect(m_view, &Preview3DView::fixtureSelected, m_fixturePanel, &FixturePanel::setSelectedId);

    StageGeometry *sg = StageGeometry::instance();
    if (sg != NULL)
        connect(sg, &StageGeometry::stageChanged, this, &Preview3D::slotGeometryChanged);

    m_renderSettings = Preview3DRenderBackendFactory::loadSettings();
    rebuildRenderBackend();

    refresh();
    s_instance = this;
}

Preview3D::~Preview3D()
{
    if (m_view != NULL)
        m_view->setRenderBackend(NULL);

    if (m_renderBackend != NULL)
    {
        m_renderBackend->shutdown();
        delete m_renderBackend;
        m_renderBackend = NULL;
    }

    if (s_instance == this)
        s_instance = NULL;
}

Preview3D *Preview3D::instance()
{
    return s_instance;
}

void Preview3D::clearContents()
{
    m_loading = true;
    if (m_trussScene != NULL)
        m_trussScene->clear();
    if (m_fixtureScene != NULL)
        m_fixtureScene->clear();
    if (m_gridAction != NULL)
        m_gridAction->setChecked(true);
    if (m_beamPreviewAction != NULL)
        m_beamPreviewAction->setChecked(false);
    if (m_trussAction != NULL)
        m_trussAction->setChecked(false);
    if (m_fixtureAction != NULL)
        m_fixtureAction->setChecked(false);
    if (m_view != NULL)
        m_view->resetView();
    m_loading = false;
}

void Preview3D::slotRendererChanged(int index)
{
    if (m_rendererCombo == NULL || index < 0)
        return;

    const Preview3DRenderer requested =
            Preview3DRenderer(m_rendererCombo->itemData(index).toInt());
    if (requested == m_renderSettings.renderer)
    {
        updateRenderStatus();
        return;
    }

    m_renderSettings.renderer = requested;
    Preview3DRenderBackendFactory::saveSettings(m_renderSettings);
    rebuildRenderBackend();
}

void Preview3D::slotSettings()
{
    Preview3DSettingsDialog dialog(m_renderSettings,
                                   Preview3DRenderBackendFactory::gpuBackendAvailable(),
                                   diagnosticsText(), this);
    connect(&dialog, &Preview3DSettingsDialog::capturePreviewRequested,
            this, &Preview3D::slotCapturePreview);
    if (dialog.exec() != QDialog::Accepted)
        return;

    applyRenderSettings(dialog.settings());
}

void Preview3D::slotCapturePreview()
{
    const QString fileName = QFileDialog::getSaveFileName(
                this, tr("Capture 3D Preview"),
                QDir::homePath() + QStringLiteral("/qlcplus-preview3d.png"),
                tr("PNG image (*.png);;All files (*)"));
    if (fileName.isEmpty())
        return;

    if (m_view == NULL || m_view->grab().save(fileName, "PNG") == false)
        QMessageBox::warning(this, tr("Capture 3D Preview"),
                             tr("Could not save the preview image to %1").arg(fileName));
}

void Preview3D::applyRenderSettings(const Preview3DRenderSettings &settings)
{
    const bool rendererChanged = (settings.renderer != m_renderSettings.renderer);
    m_renderSettings = settings;
    Preview3DRenderBackendFactory::saveSettings(m_renderSettings);

    if (rendererChanged || m_renderBackend == NULL)
    {
        rebuildRenderBackend();
        return;
    }

    if (m_view != NULL)
    {
        m_view->setRenderSettings(m_renderSettings);
        m_view->update();
    }
    updateRenderStatus();
}

void Preview3D::rebuildRenderBackend()
{
    if (m_renderBackend != NULL)
    {
        m_renderBackend->shutdown();
        delete m_renderBackend;
        m_renderBackend = NULL;
    }

    m_renderDiagnostic.clear();
    m_renderBackend = Preview3DRenderBackendFactory::createBackend(
                m_renderSettings, &m_activeRenderer, &m_renderDiagnostic);

    if (m_renderBackend != NULL)
    {
        QString error;
        if (m_renderBackend->initialize(&error) == false)
        {
            if (error.isEmpty() == false)
                m_renderDiagnostic = error;

            /* A requested GPU backend must never leave the Preview blank. */
            if (m_activeRenderer == Preview3DRenderer::Gpu)
            {
                const QString gpuFailure = m_renderDiagnostic;
                delete m_renderBackend;
                Preview3DRenderSettings fallbackSettings = m_renderSettings;
                fallbackSettings.renderer = Preview3DRenderer::Software;
                m_renderBackend = Preview3DRenderBackendFactory::createBackend(
                            fallbackSettings, &m_activeRenderer, &m_renderDiagnostic);
                if (m_renderBackend != NULL)
                    m_renderBackend->initialize(&error);
                m_renderDiagnostic = gpuFailure;
            }
        }
    }

    if (m_view != NULL)
    {
        m_view->setRenderBackend(m_renderBackend);
        m_view->setRenderSettings(m_renderSettings);
        m_view->update();
    }

    if (m_rendererCombo != NULL)
    {
        /* Auto stays Auto even though it currently resolves to software. */
        const Preview3DRenderer visibleRenderer =
                (m_renderSettings.renderer == Preview3DRenderer::Auto)
                ? Preview3DRenderer::Auto : m_activeRenderer;
        int index = m_rendererCombo->findData(int(visibleRenderer));
        if (index < 0)
            index = m_rendererCombo->findData(int(Preview3DRenderer::Software));
        if (index >= 0)
        {
            QSignalBlocker blocker(m_rendererCombo);
            m_rendererCombo->setCurrentIndex(index);
        }
    }

    updateRenderStatus();
}

void Preview3D::updateRenderStatus()
{
    if (m_renderStatusLabel == NULL)
        return;

    m_renderStatusLabel->setVisible(m_renderSettings.showRenderStatus);

    QString quality = Preview3DRenderBackendFactory::qualityToString(m_renderSettings.quality);
    if (quality.isEmpty() == false)
        quality[0] = quality[0].toUpper();
    QString text;
    if (m_renderBackend == NULL)
    {
        text = tr("Renderer unavailable - software fallback is used");
    }
    else
    {
        text = tr("%1  \u00b7  %2  \u00b7  %3%")
                .arg(m_renderBackend->displayName(), quality)
                .arg(qRound(m_renderSettings.resolutionScale * 100.0));
        if (m_renderSettings.renderer == Preview3DRenderer::Auto)
            text += tr("  \u00b7  Auto");
        else if (m_renderSettings.renderer == Preview3DRenderer::Gpu &&
                 m_activeRenderer != Preview3DRenderer::Gpu)
            text += tr("  \u00b7  GPU fallback");
        if (m_renderDiagnostic.isEmpty() == false)
            text += tr("  \u00b7  %1").arg(m_renderDiagnostic);
    }
    m_renderStatusLabel->setText(text);
    m_renderStatusLabel->setToolTip(diagnosticsText());
}

QString Preview3D::diagnosticsText() const
{
    QStringList lines;
    lines << QStringLiteral("%1 %2").arg(QStringLiteral(APPNAME), QStringLiteral(APPVERSION));
    lines << QStringLiteral("Qt %1 on %2").arg(QString::fromLatin1(qVersion()),
                                                 QSysInfo::prettyProductName());
    lines << tr("Requested renderer: %1")
             .arg(Preview3DRenderBackendFactory::rendererToString(m_renderSettings.renderer));
    lines << tr("Active renderer: %1")
             .arg(m_renderBackend != NULL ? m_renderBackend->displayName()
                                          : tr("unavailable"));
    lines << tr("Fallback reason: %1")
             .arg(m_renderDiagnostic.isEmpty() ? tr("none") : m_renderDiagnostic);
    lines << tr("OpenGL available: %1")
             .arg(Preview3DRenderBackendFactory::gpuBackendAvailable() ? tr("yes") : tr("no"));
    lines << tr("Quality: %1, resolution %2%, shadows %3, haze %4")
             .arg(Preview3DRenderBackendFactory::qualityToString(m_renderSettings.quality))
             .arg(qRound(m_renderSettings.resolutionScale * 100.0))
             .arg(m_renderSettings.shadows ? tr("on") : tr("off"))
             .arg(m_renderSettings.haze ? tr("on") : tr("off"));

    if (m_renderBackend != NULL)
    {
        const Preview3DRenderCapabilities caps = m_renderBackend->capabilities();
        lines << tr("Capabilities: accelerated=%1 shadows=%2 haze=%3 volumetricBeams=%4 msaa=%5")
                 .arg(caps.accelerated ? tr("yes") : tr("no"))
                 .arg(caps.shadows ? tr("yes") : tr("no"))
                 .arg(caps.haze ? tr("yes") : tr("no"))
                 .arg(caps.volumetricBeams ? tr("yes") : tr("no"))
                 .arg(caps.maxSupersample);
        lines << tr("GPU frame time: %1 ms").arg(m_renderBackend->lastGpuFrameMs(), 0, 'f', 2);
        lines << tr("Static uploads: %1, shadow-map renders: %2")
                 .arg(m_renderBackend->staticUploadCount())
                 .arg(m_renderBackend->shadowRenderCount());
        if (m_renderBackend->diagnostics().isEmpty() == false)
            lines << m_renderBackend->diagnostics();
    }

    if (m_view != NULL)
    {
        const Preview3DRenderStats stats = m_view->lastRenderStats();
        lines << tr("Viewport: %1x%2; frame %3 ms; GPU=%4; triangles %5, lines %6, sprites %7")
                 .arg(stats.viewport.width())
                 .arg(stats.viewport.height())
                 .arg(stats.frameMs, 0, 'f', 2)
                 .arg(stats.gpu ? tr("yes") : tr("no"))
                 .arg(stats.triangles)
                 .arg(stats.lines)
                 .arg(stats.sprites);
        lines << tr("Fixtures: %1; truss segments: %2")
                 .arg(stats.fixtures).arg(stats.trussSegments);
    }

    return lines.join(QStringLiteral("\n"));
}

void Preview3D::slotTrussSceneChanged()
{
    if (m_view != NULL)
        m_view->invalidateOccluders();

    // Only real user edits mark the project dirty; loading/clearing does not.
    if (m_loading)
        return;
    if (m_doc != NULL)
        m_doc->setModified();
}

void Preview3D::slotFixtureSceneChanged()
{
    // Height and pitch live only in the 3D Preview, so a change here must dirty
    // the project just like a truss edit does. Loading/clearing does not.
    if (m_loading)
        return;
    if (m_doc != NULL)
        m_doc->setModified();
}

bool Preview3D::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLQLCPreview3D);

    double yaw = 0.0, pitch = 0.0, dist = 0.0;
    QVector3D target;
    if (m_view != NULL)
        m_view->camera(yaw, pitch, dist, target);

    doc->writeAttribute("Yaw", QString::number(yaw, 'g', 8));
    doc->writeAttribute("Pitch", QString::number(pitch, 'g', 8));
    doc->writeAttribute("Dist", QString::number(dist, 'g', 8));
    doc->writeAttribute("TargetX", QString::number(target.x(), 'g', 8));
    doc->writeAttribute("TargetY", QString::number(target.y(), 'g', 8));
    doc->writeAttribute("TargetZ", QString::number(target.z(), 'g', 8));
    doc->writeAttribute("Grid", (m_gridAction != NULL && m_gridAction->isChecked()) ? "true" : "false");
    doc->writeAttribute("BeamPreview", (m_beamPreviewAction != NULL && m_beamPreviewAction->isChecked()) ? "true" : "false");
    doc->writeAttribute("Panel", (m_trussAction != NULL && m_trussAction->isChecked()) ? "true" : "false");

    if (m_trussScene != NULL)
        m_trussScene->saveXML(doc);

    if (m_fixtureScene != NULL)
        m_fixtureScene->saveXML(doc);

    doc->writeEndElement(); // Preview3D
    return true;
}

bool Preview3D::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCPreview3D)
    {
        root.skipCurrentElement();
        return false;
    }

    const QXmlStreamAttributes a = root.attributes();
    const bool grid = (a.value("Grid") != "false");
    const bool beamPreview = (a.value("BeamPreview") == "true");
    const bool panel = (a.value("Panel") == "true");

    m_loading = true;

    // Reads (and consumes) the <Trusses>/<Links>/<Fixtures> children.
    if (m_trussScene != NULL)
        m_trussScene->beginLoad();
    if (m_fixtureScene != NULL)
        m_fixtureScene->beginLoad();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCPreview3DTrusses || root.name() == KXMLQLCPreview3DLinks)
        {
            if (m_trussScene != NULL)
                m_trussScene->loadXML(root);
            else
                root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCPreview3DFixtures)
        {
            if (m_fixtureScene != NULL)
                m_fixtureScene->loadXML(root);
            else
                root.skipCurrentElement();
        }
        else
        {
            root.skipCurrentElement();
        }
    }

    if (m_trussScene != NULL)
        m_trussScene->endLoad();

    if (m_gridAction != NULL)
        m_gridAction->setChecked(grid);
    if (m_beamPreviewAction != NULL)
        m_beamPreviewAction->setChecked(beamPreview);
    if (m_trussAction != NULL)
        m_trussAction->setChecked(panel);

    if (m_view != NULL && a.hasAttribute("Dist"))
    {
        m_view->setCamera(a.value("Yaw").toDouble(),
                          a.value("Pitch").toDouble(),
                          a.value("Dist").toDouble(),
                          QVector3D(a.value("TargetX").toDouble(),
                                    a.value("TargetY").toDouble(),
                                    a.value("TargetZ").toDouble()));
    }
    else if (m_view != NULL)
    {
        m_view->resetView();
    }

    m_loading = false;
    return true;
}

void Preview3D::slotGeometryChanged()
{
    refresh();
}

void Preview3D::slotFit()
{
    if (m_view != NULL)
        m_view->fitToRoom();
}

void Preview3D::slotReset()
{
    if (m_view != NULL)
        m_view->resetView();
}

void Preview3D::slotGridToggled(bool on)
{
    if (m_view != NULL)
        m_view->setGridVisible(on);
}

void Preview3D::slotBeamPreviewToggled(bool on)
{
    if (m_view != NULL)
        m_view->setBeamPreview(on);
}

void Preview3D::slotTrussToggled(bool on)
{
    if (m_trussPanel != NULL)
        m_trussPanel->setVisible(on);
    if (m_view != NULL)
    {
        m_view->setTrussInteractive(on);
        if (!on)
            m_view->setSelectedTruss(-1);
    }
    if (!on && m_trussPanel != NULL)
        m_trussPanel->setSelectedId(-1);

    // Truss and fixture modes are mutually exclusive.
    if (on && m_fixtureAction != NULL && m_fixtureAction->isChecked())
        m_fixtureAction->setChecked(false);
}

void Preview3D::slotFixtureToggled(bool on)
{
    if (m_fixturePanel != NULL)
        m_fixturePanel->setVisible(on);
    if (m_view != NULL)
    {
        m_view->setFixtureInteractive(on);
        if (!on)
            m_view->setSelectedFixture(-1);
    }
    if (!on && m_fixturePanel != NULL)
        m_fixturePanel->setSelectedId(-1);

    if (on && m_trussAction != NULL && m_trussAction->isChecked())
        m_trussAction->setChecked(false);
}

void Preview3D::refresh()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL || m_view == NULL)
        return;

    const StageGeom::Room r = sg->room();
    const StageGeom::Stage st = sg->stage();

    m_view->setScene(r.minX(), r.maxX(), r.minY(), r.maxY(),
                     r.minZ(), r.maxZ(), st.sx(), st.sy());

    if (m_trussPanel != NULL)
        m_trussPanel->setFloorZ(r.minZ());
    if (m_fixturePanel != NULL)
        m_fixturePanel->setFloorZ(r.minZ());

    if (m_infoLabel != NULL)
    {
        m_infoLabel->setText(tr("Room %1 \u00d7 %2 m   \u00b7   Stage %3 \u00d7 %4 m   \u00b7   Stage height %5 m")
            .arg(r.maxX() - r.minX(), 0, 'f', 2)
            .arg(r.maxY() - r.minY(), 0, 'f', 2)
            .arg(st.sx(), 0, 'f', 2)
            .arg(st.sy(), 0, 'f', 2)
            .arg(sg->stageHeight(), 0, 'f', 2));

        if (m_renderBackend != NULL)
        {
            QString tip = tr("Renderer: %1\nQuality: %2")
                    .arg(m_renderBackend->displayName())
                    .arg(Preview3DRenderBackendFactory::qualityToString(m_renderSettings.quality));
            if (m_renderDiagnostic.isEmpty() == false)
                tip += QString("\n") + m_renderDiagnostic;
            m_infoLabel->setToolTip(tip);
        }
    }
}
