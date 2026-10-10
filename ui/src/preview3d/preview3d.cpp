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
#include <QAction>
#include <QIcon>
#include <QSize>

#include "preview3d.h"
#include "preview3dview.h"
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
    , m_toolbar(NULL)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(18, 18));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    layout->addWidget(m_toolbar);

    m_fitAction = m_toolbar->addAction(QIcon(":/fittoroom.png"), tr("Fit to room"));
    m_fitAction->setToolTip(tr("Frame the whole room"));
    connect(m_fitAction, &QAction::triggered, this, &Preview3D::slotFit);

    m_resetAction = m_toolbar->addAction(QIcon(":/refresh.png"), tr("Reset view"));
    m_resetAction->setToolTip(tr("Restore the default camera"));
    connect(m_resetAction, &QAction::triggered, this, &Preview3D::slotReset);

    m_gridAction = m_toolbar->addAction(QIcon(":/grid.png"), tr("Grid"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setToolTip(tr("Show or hide the floor grid"));
    connect(m_gridAction, &QAction::toggled, this, &Preview3D::slotGridToggled);

    m_toolbar->addSeparator();

    m_trussAction = m_toolbar->addAction(QIcon(":/truss.png"), tr("Truss"));
    m_trussAction->setCheckable(true);
    m_trussAction->setToolTip(tr("Show or hide the truss configuration panel"));
    connect(m_trussAction, &QAction::toggled, this, &Preview3D::slotTrussToggled);

    m_fixtureAction = m_toolbar->addAction(QIcon(":/fixture.png"), tr("Fixtures"));
    m_fixtureAction->setCheckable(true);
    m_fixtureAction->setToolTip(tr("Show or hide the fixture placement panel"));
    connect(m_fixtureAction, &QAction::toggled, this, &Preview3D::slotFixtureToggled);

    QWidget *spacer = new QWidget(m_toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_infoLabel = new QLabel(m_toolbar);
    m_infoLabel->setContentsMargins(0, 0, 8, 0);
    m_toolbar->addWidget(m_infoLabel);

    m_splitter = new QSplitter(Qt::Horizontal, this);
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

    refresh();
    s_instance = this;
}

Preview3D::~Preview3D()
{
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
    if (m_trussAction != NULL)
        m_trussAction->setChecked(false);
    if (m_fixtureAction != NULL)
        m_fixtureAction->setChecked(false);
    if (m_view != NULL)
        m_view->resetView();
    m_loading = false;
}

void Preview3D::slotTrussSceneChanged()
{
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

void Preview3D::slotTrussToggled(bool on)
{
    if (m_trussPanel != NULL)
        m_trussPanel->setVisible(on);
    if (m_view != NULL)
        m_view->setTrussInteractive(on);

    // Truss and fixture modes are mutually exclusive.
    if (on && m_fixtureAction != NULL && m_fixtureAction->isChecked())
        m_fixtureAction->setChecked(false);
}

void Preview3D::slotFixtureToggled(bool on)
{
    if (m_fixturePanel != NULL)
        m_fixturePanel->setVisible(on);
    if (m_view != NULL)
        m_view->setFixtureInteractive(on);

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
    }
}
