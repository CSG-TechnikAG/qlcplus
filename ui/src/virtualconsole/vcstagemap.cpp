/*
  Q Light Controller Plus
  vcstagemap.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialog>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPaintEvent>
#include <QSlider>
#include <QLineEdit>
#include <QGroupBox>
#include <QDebug>
#include <QLabel>

#include "stagegeometryview.h"
#include "stagegeometry.h"
#include "inputselectionwidget.h"
#include "pathfunction.h"
#include "flowlayout.h"
#include "vcstagemap.h"
#include "function.h"
#include "doc.h"
#include <QTimer>

const quint8 VCStageMap::xInputSourceId = 0;
const quint8 VCStageMap::yInputSourceId = 1;
const quint8 VCStageMap::zInputSourceId = 2;

VCStageMap::VCStageMap(QWidget *parent, Doc *doc)
    : VCWidget(parent, doc)
    , m_view(NULL)
    , m_captionLabel(NULL)
    , m_height(NULL)
    , m_heightLabel(NULL)
    , m_clearButton(NULL)
    , m_lampsHost(NULL)
    , m_lampsLayout(NULL)
    , m_stretch(false)
    , m_hasTarget(false)
    , m_target(0, 0)
    , m_targetZ(0)
    , m_xInput(127)
    , m_yInput(127)
    , m_zInput(0)
    , m_invertX(false)
    , m_invertY(false)
    , m_invertZ(false)
    , m_previewTimer(NULL)
{
    setObjectName(VCStageMap::staticMetaObject.className());
    setType(VCWidget::StageMapWidget);
    resize(QSize(340, 400));
    setAllowResize(true);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(4, 2, 4, 2);
    main->setSpacing(2);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setAlignment(Qt::AlignCenter);
    main->addWidget(m_captionLabel);

    m_view = new StageGeometryView(this);
    m_view->setResizable(false);
    m_view->setMinimumSize(0, 0);
    m_view->setFitPadding(2);
    m_view->setFitMargin(0.02);
    m_view->setStretchToFit(m_stretch);
    connect(m_view, &StageGeometryView::clicked, this, &VCStageMap::slotViewClicked);
    main->addWidget(m_view, 1);

    QHBoxLayout *hrow = new QHBoxLayout();
    m_heightLabel = new QLabel(this);
    hrow->addWidget(m_heightLabel);
    m_height = new QSlider(Qt::Horizontal, this);
    m_height->setRange(0, 400);
    connect(m_height, &QSlider::valueChanged, this, &VCStageMap::slotHeightChanged);
    hrow->addWidget(m_height, 1);

    m_clearButton = new QToolButton(this);
    m_clearButton->setText(QString::fromUtf8("\u2715"));
    m_clearButton->setToolTip(tr("Clear the manual position (return control to the running function)"));
    m_clearButton->setStyleSheet("QToolButton { color: #d33; font-weight: bold; }");
    connect(m_clearButton, &QToolButton::clicked, this, &VCStageMap::slotClearAim);
    m_clearButton->setVisible(false);
    hrow->addWidget(m_clearButton);

    main->addLayout(hrow);

    m_lampsHost = new QWidget(this);
    m_lampsLayout = new FlowLayout(m_lampsHost);
    main->addWidget(m_lampsHost);

    StageGeometry *sg = StageGeometry::instance();
    if (sg != NULL)
    {
        connect(sg, &StageGeometry::stageChanged, this, &VCStageMap::slotStageChanged);
        connect(sg, &StageGeometry::fixturesChanged, this, &VCStageMap::slotFixturesChanged);
        connect(sg, &StageGeometry::fixturePositionChanged, this,
                [this](quint32, double, double, double) { updateView(); });
    }

    setCaption(tr("Stage Map"));

    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout, this, &VCStageMap::slotPreview);
    m_previewTimer->start(40);

    rebuildLamps();
    updateView();
    slotHeightChanged(m_height->value());
}

VCStageMap::~VCStageMap()
{
}

/*****************************************************************************
 * VCWidget interface
 *****************************************************************************/

void VCStageMap::enableWidgetUI(bool enable)
{
    if (m_view != NULL)
    {
        m_view->setAttribute(Qt::WA_TransparentForMouseEvents, !enable);
        m_view->setCursor(enable ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    if (m_height != NULL)
        m_height->setAttribute(Qt::WA_TransparentForMouseEvents, !enable);
    if (m_clearButton != NULL)
        m_clearButton->setAttribute(Qt::WA_TransparentForMouseEvents, !enable);
    for (QToolButton *b : m_lamps)
        b->setAttribute(Qt::WA_TransparentForMouseEvents, !enable);
}

VCWidget *VCStageMap::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != NULL);
    VCStageMap *copy = new VCStageMap(parent, m_doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    return copy;
}

bool VCStageMap::copyFrom(const VCWidget *widget)
{
    const VCStageMap *other = qobject_cast<const VCStageMap *>(widget);
    if (other == NULL)
        return false;
    if (VCWidget::copyFrom(widget) == false)
        return false;

    m_selected = other->m_selected;
    m_targetZ = other->m_targetZ;
    m_stretch = other->m_stretch;
    m_invertX = other->m_invertX;
    m_invertY = other->m_invertY;
    m_invertZ = other->m_invertZ;
    setInputSource(other->inputSource(xInputSourceId), xInputSourceId);
    setInputSource(other->inputSource(yInputSourceId), yInputSourceId);
    setInputSource(other->inputSource(zInputSourceId), zInputSourceId);
    if (m_view != NULL)
        m_view->setStretchToFit(m_stretch);
    rebuildLamps();
    return true;
}

void VCStageMap::editProperties()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Stage Map Properties"));
    dlg.setMinimumWidth(420);

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);

    QFormLayout *form = new QFormLayout();
    QLineEdit *captionEdit = new QLineEdit(caption(), &dlg);
    form->addRow(tr("Caption:"), captionEdit);
    QCheckBox *stretchBox = new QCheckBox(tr("Stretch to fit"), &dlg);
    stretchBox->setChecked(m_stretch);
    stretchBox->setToolTip(tr("Stretch the map to fill the widget instead of "
                              "keeping the room's aspect ratio."));
    form->addRow(QString(), stretchBox);
    vbox->addLayout(form);

    QGroupBox *inputBox = new QGroupBox(tr("Movement inputs (X / Y / Z)"), &dlg);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputBox);

    InputSelectionWidget *xWidget = new InputSelectionWidget(m_doc, inputBox);
    xWidget->setTitle(tr("X (right)"));
    xWidget->setKeyInputVisibility(false);
    xWidget->setInputSource(inputSource(xInputSourceId));
    xWidget->setWidgetPage(page());
    xWidget->emitOddValues(true);
    inputLayout->addWidget(xWidget);
    QCheckBox *invX = new QCheckBox(tr("Invert X"), inputBox);
    invX->setChecked(m_invertX);
    inputLayout->addWidget(invX);

    InputSelectionWidget *yWidget = new InputSelectionWidget(m_doc, inputBox);
    yWidget->setTitle(tr("Y (forward)"));
    yWidget->setKeyInputVisibility(false);
    yWidget->setInputSource(inputSource(yInputSourceId));
    yWidget->setWidgetPage(page());
    yWidget->emitOddValues(true);
    inputLayout->addWidget(yWidget);
    QCheckBox *invY = new QCheckBox(tr("Invert Y"), inputBox);
    invY->setChecked(m_invertY);
    inputLayout->addWidget(invY);

    InputSelectionWidget *zWidget = new InputSelectionWidget(m_doc, inputBox);
    zWidget->setTitle(tr("Z (height)"));
    zWidget->setKeyInputVisibility(false);
    zWidget->setInputSource(inputSource(zInputSourceId));
    zWidget->setWidgetPage(page());
    zWidget->emitOddValues(true);
    inputLayout->addWidget(zWidget);
    QCheckBox *invZ = new QCheckBox(tr("Invert Z"), inputBox);
    invZ->setChecked(m_invertZ);
    inputLayout->addWidget(invZ);

    vbox->addWidget(inputBox);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted)
    {
        setCaption(captionEdit->text());
        m_stretch = stretchBox->isChecked();
        if (m_view != NULL)
            m_view->setStretchToFit(m_stretch);

        setInputSource(xWidget->inputSource(), xInputSourceId);
        setInputSource(yWidget->inputSource(), yInputSourceId);
        setInputSource(zWidget->inputSource(), zInputSourceId);

        m_invertX = invX->isChecked();
        m_invertY = invY->isChecked();
        m_invertZ = invZ->isChecked();
    }
}

void VCStageMap::setCaption(const QString &text)
{
    VCWidget::setCaption(text);
    if (m_captionLabel != NULL)
        m_captionLabel->setText(text);
}

void VCStageMap::updateFeedback()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL)
        return;
    StageGeom::Room r = sg->room();

    auto toValue = [](double v, double lo, double hi) {
        if (hi <= lo)
            return 0;
        return int(255.0 * (v - lo) / (hi - lo));
    };

    int xv = toValue(m_target.x(), r.minX(), r.maxX());
    int yv = toValue(m_target.y(), r.minY(), r.maxY());
    int zv = toValue(m_targetZ, r.minZ(), r.maxZ());
    if (m_invertX) xv = 255 - xv;
    if (m_invertY) yv = 255 - yv;
    if (m_invertZ) zv = 255 - zv;

    sendFeedback(xv, xInputSourceId);
    sendFeedback(yv, yInputSourceId);
    sendFeedback(zv, zInputSourceId);
}

void VCStageMap::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    if (acceptsInput() == false)
        return;

    quint32 pagedCh = (page() << 16) | channel;

    if (checkInputSource(universe, pagedCh, value, sender(), xInputSourceId))
    {
        m_xInput = value;
        updatePosition();
    }
    else if (checkInputSource(universe, pagedCh, value, sender(), yInputSourceId))
    {
        m_yInput = value;
        updatePosition();
    }
    else if (checkInputSource(universe, pagedCh, value, sender(), zInputSourceId))
    {
        m_zInput = value;
        updatePosition();
    }
}

void VCStageMap::updatePosition()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL)
        return;
    StageGeom::Room r = sg->room();

    auto frac = [](uchar v) { return double(v) / 255.0; };

    double fx = frac(m_xInput);
    if (m_invertX) fx = 1.0 - fx;
    double x = r.minX() + fx * (r.maxX() - r.minX());

    double fy = frac(m_yInput);
    if (m_invertY) fy = 1.0 - fy;
    double y = r.minY() + fy * (r.maxY() - r.minY());

    double fz = frac(m_zInput);
    if (m_invertZ) fz = 1.0 - fz;
    double z = r.minZ() + fz * (r.maxZ() - r.minZ());

    m_target = QPointF(x, y);
    m_targetZ = z;
    m_hasTarget = true;

    applyTarget();

    m_height->blockSignals(true);
    m_height->setValue(int(z * 100));
    m_height->blockSignals(false);

    updateView();
}

void VCStageMap::slotModeChanged(Doc::Mode mode)
{
    enableWidgetUI(mode == Doc::Operate && isDisabled() == false);
    VCWidget::slotModeChanged(mode);
}

/*****************************************************************************
 * Lamp selector
 *****************************************************************************/

QList<quint32> VCStageMap::selectedLampIds() const
{
    return m_selected;
}

void VCStageMap::rebuildLamps()
{
    if (m_lampsLayout == NULL)
        return;

    while (m_lampsLayout->count() > 0)
    {
        QLayoutItem *item = m_lampsLayout->takeAt(0);
        if (item->widget() != NULL)
            delete item->widget();
        delete item;
    }
    m_lamps.clear();

    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL)
        return;

    for (quint32 id : sg->calibratedFixtureIds())
    {
        QToolButton *b = new QToolButton(m_lampsHost);
        b->setText(sg->fixtureName(id));
        b->setCheckable(true);
        b->setChecked(m_selected.contains(id));
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setAttribute(Qt::WA_TransparentForMouseEvents, mode() != Doc::Operate);
        connect(b, &QToolButton::toggled, this, &VCStageMap::slotLampToggled);
        m_lampsLayout->addWidget(b);
        m_lamps[id] = b;
    }

    // Drop selections whose lamp disappeared.
    for (int i = m_selected.size() - 1; i >= 0; i--)
        if (!m_lamps.contains(m_selected.at(i)))
            m_selected.removeAt(i);
}

void VCStageMap::slotLampToggled(bool checked)
{
    QToolButton *b = qobject_cast<QToolButton *>(sender());
    if (b == NULL)
        return;
    quint32 id = m_lamps.key(b, 0xFFFFFFFF);
    if (id == 0xFFFFFFFF)
        return;

    if (checked)
    {
        if (!m_selected.contains(id))
            m_selected.append(id);
    }
    else
        m_selected.removeAll(id);

    updateView();
    if (mode() == Doc::Operate)
        applyTarget();
}

/*****************************************************************************
 * 2D map
 *****************************************************************************/

void VCStageMap::updateView()
{
    if (m_view == NULL)
        return;

    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL)
        return;

    StageGeom::Stage st = sg->stage();
    StageGeom::Room r = sg->room();
    m_view->setStage(st.sx(), st.sy());
    m_view->setRoom(r.minX(), r.maxX(), r.minY(), r.maxY());

    QList<StageGeometryView::Marker> markers;
    for (quint32 id : sg->calibratedFixtureIds())
    {
        double x = 0, y = 0, z = 0;
        if (!sg->fixturePosition(id, x, y, z))
            continue;
        StageGeometryView::Marker mk;
        mk.pos = QPointF(x, y);
        mk.name = sg->fixtureName(id);
        mk.selected = m_selected.contains(id);
        mk.color = mk.selected ? QColor(255, 220, 0) : QColor(140, 140, 140);
        mk.calibrated = true;
        markers << mk;
    }
    m_view->setMarkers(markers);
    m_view->setTarget(m_target, m_hasTarget);

    int minZ = int(r.minZ() * 100);
    int maxZ = int(r.maxZ() * 100);
    if (maxZ <= minZ)
        maxZ = minZ + 1;
    m_height->blockSignals(true);
    m_height->setRange(minZ, maxZ);
    if (m_height->value() > maxZ)
        m_height->setValue(maxZ);
    if (m_height->value() < minZ)
        m_height->setValue(minZ);
    m_height->blockSignals(false);
}

void VCStageMap::slotStageChanged()
{
    updateView();
}

void VCStageMap::slotFixturesChanged()
{
    rebuildLamps();
    updateView();
}

void VCStageMap::slotViewClicked(QPointF world)
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL)
        return;

    StageGeom::Stage st = sg->stage();
    StageGeom::Room r = sg->room();
    if (!st.contains(world) && !r.containsXY(world.x(), world.y()))
        return;

    m_target = world;
    m_hasTarget = true;
    applyTarget();
    updateView();
}

void VCStageMap::slotHeightChanged(int value)
{
    m_targetZ = value / 100.0;
    if (m_heightLabel != NULL)
        m_heightLabel->setText(tr("z %1 m").arg(m_targetZ, 0, 'f', 2));
    if (m_hasTarget)
        applyTarget();
}

void VCStageMap::slotPreview()
{
    QList<QPointF> spots;
    QList<QPolygonF> paths;

    if (m_selected.isEmpty() == false && m_doc != NULL)
    {
        for (Function* f : m_doc->functions())
        {
            if (f->type() != Function::PathType || f->isRunning() == false)
                continue;
            PathFunction* pf = qobject_cast<PathFunction*>(f);
            if (pf == NULL)
                continue;
            bool affects = false;
            for (const PathFunction::PathFixture& pfx : pf->fixtures())
            {
                if (m_selected.contains(pfx.id))
                {
                    affects = true;
                    break;
                }
            }
            if (affects)
            {
                // One spot per selected lamp, honouring per-lamp invert/offset.
                for (const PathFunction::PathFixture& pfx : pf->fixtures())
                    if (m_selected.contains(pfx.id))
                        spots << pf->fixtureTarget(pfx.id);
                paths << pf->path();
            }
        }
    }

    if (m_view != NULL)
    {
        m_view->setSpots(spots);
        m_view->setPaths(paths);
    }

    // If the Simple Desk overrides were cleared elsewhere (e.g. Simple Desk
    // tab), drop the manual target so the running function takes over.
    if (m_hasTarget && !m_selected.isEmpty())
    {
        StageGeometry *sg = StageGeometry::instance();
        if (sg != NULL && !sg->isAimOverridden(m_selected))
        {
            m_hasTarget = false;
            updateView();
        }
    }

    // The clear button is only useful while at least one lamp is selected.
    if (m_clearButton != NULL)
        m_clearButton->setVisible(!m_selected.isEmpty());
}

void VCStageMap::slotClearAim()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg != NULL)
        sg->clearAim(m_selected);

    m_hasTarget = false;
    updateView();
}

void VCStageMap::applyTarget()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL || !m_hasTarget)
        return;
    sg->aimAt(selectedLampIds(), m_target.x(), m_target.y(), m_targetZ);
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool VCStageMap::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCStageMap)
    {
        qWarning() << Q_FUNC_INFO << "StageMap node not found";
        return false;
    }

    m_selected.clear();

    /* Widget commons */
    loadXMLCommon(root);

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            int x = 0, y = 0, w = 0, h = 0;
            bool visible = false;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCStageMapLamps)
        {
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCVCStageMapLamp)
                {
                    quint32 id = root.readElementText().toUInt();
                    if (!m_selected.contains(id))
                        m_selected.append(id);
                }
                else
                    root.skipCurrentElement();
            }
        }
        else if (root.name() == KXMLQLCVCStageMapHeight)
        {
            m_targetZ = root.readElementText().toDouble();
        }
        else if (root.name() == KXMLQLCVCStageMapStretch)
        {
            m_stretch = (root.readElementText() == QLatin1String("true"));
            if (m_view != NULL)
                m_view->setStretchToFit(m_stretch);
        }
        else if (root.name() == KXMLQLCVCStageMapXInput)
        {
            loadXMLSources(root, xInputSourceId);
        }
        else if (root.name() == KXMLQLCVCStageMapYInput)
        {
            loadXMLSources(root, yInputSourceId);
        }
        else if (root.name() == KXMLQLCVCStageMapZInput)
        {
            loadXMLSources(root, zInputSourceId);
        }
        else if (root.name() == KXMLQLCVCStageMapInvertX)
        {
            m_invertX = (root.readElementText() == QLatin1String("true"));
        }
        else if (root.name() == KXMLQLCVCStageMapInvertY)
        {
            m_invertY = (root.readElementText() == QLatin1String("true"));
        }
        else if (root.name() == KXMLQLCVCStageMapInvertZ)
        {
            m_invertZ = (root.readElementText() == QLatin1String("true"));
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown StageMap tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    rebuildLamps();
    updateView();
    return true;
}

bool VCStageMap::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCVCStageMap);

    saveXMLCommon(doc);
    saveXMLWindowState(doc);
    saveXMLAppearance(doc);

    doc->writeStartElement(KXMLQLCVCStageMapLamps);
    for (quint32 id : m_selected)
        doc->writeTextElement(KXMLQLCVCStageMapLamp, QString::number(id));
    doc->writeEndElement();

    doc->writeTextElement(KXMLQLCVCStageMapHeight, QString::number(m_targetZ));
    doc->writeTextElement(KXMLQLCVCStageMapStretch, m_stretch ? "true" : "false");

    doc->writeStartElement(KXMLQLCVCStageMapXInput);
    saveXMLInput(doc, inputSource(xInputSourceId));
    doc->writeEndElement();
    doc->writeStartElement(KXMLQLCVCStageMapYInput);
    saveXMLInput(doc, inputSource(yInputSourceId));
    doc->writeEndElement();
    doc->writeStartElement(KXMLQLCVCStageMapZInput);
    saveXMLInput(doc, inputSource(zInputSourceId));
    doc->writeEndElement();

    doc->writeTextElement(KXMLQLCVCStageMapInvertX, m_invertX ? "true" : "false");
    doc->writeTextElement(KXMLQLCVCStageMapInvertY, m_invertY ? "true" : "false");
    doc->writeTextElement(KXMLQLCVCStageMapInvertZ, m_invertZ ? "true" : "false");

    doc->writeEndElement();

    return true;
}
