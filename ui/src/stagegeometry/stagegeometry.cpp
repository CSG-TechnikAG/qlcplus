/*
  Q Light Controller Plus
  stagegeometry.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "stagegeometry.h"
#include "stagegeometryview.h"
#include "consolechannel.h"

#include "simpledesk.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSlider>
#include <QTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>
#include <QTimer>
#include <QMessageBox>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <stdexcept>
#include <algorithm>

StageGeometry *StageGeometry::s_instance = NULL;

static const char *FIXTURE_COLORS[] = {
    "#ffcc44", "#5fd0d0", "#c58cff", "#ff9a5b", "#7fd06a", "#f56fa0"
};
static const int FIXTURE_COLOR_COUNT = int(sizeof(FIXTURE_COLORS) / sizeof(FIXTURE_COLORS[0]));

// UserRole tags for the usage list
enum { ItemLamp = 0, ItemGroup = 1 };

StageGeometry::StageGeometry(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_engine(new StageGeometryEngine(doc))
    , m_activeIndex(0)
    , m_stepIndex(0)
    , m_updating(false)
    , m_tabs(NULL)
    , m_infoLabel(NULL)
    , m_fixtureCombo(NULL)
    , m_statusLabel(NULL)
    , m_stepList(NULL)
    , m_calibView(NULL)
    , m_sideView(NULL)
    , m_stepTitle(NULL)
    , m_stepInstr(NULL)
    , m_frontHint(NULL)
    , m_sliderHost(NULL)
    , m_sliderLayout(NULL)
    , m_panSlider(NULL)
    , m_tiltSlider(NULL)
    , m_captureButton(NULL)
    , m_addRefButton(NULL)
    , m_clearRefButton(NULL)
    , m_refValues(NULL)
    , m_confirmCheck(NULL)
    , m_calibrateButton(NULL)
    , m_calibResult(NULL)
    , m_usageView(NULL)
    , m_usageList(NULL)
    , m_heightSlider(NULL)
    , m_heightLabel(NULL)
    , m_usageInfo(NULL)
    , m_applyCheck(NULL)
    , m_hasTarget(false)
    , m_target2d(0, 0)
    , m_targetZ(0)
    , m_liveUniverse(NULL)
    , m_liveXCh(NULL)
    , m_liveYCh(NULL)
    , m_liveZCh(NULL)
    , m_liveAll(NULL)
    , m_liveButton(NULL)
    , m_liveStatus(NULL)
    , m_liveLog(NULL)
    , m_timer(NULL)
{
    s_instance = this;

    initUi();

    connect(m_engine, &StageGeometryEngine::fixturesChanged, this, &StageGeometry::onFixturesChanged);
    connect(m_engine, &StageGeometryEngine::geometryChanged, this, &StageGeometry::onGeometryChanged);
    connect(m_doc->inputOutputMap(), &InputOutputMap::universeWritten,
            this, &StageGeometry::onUniverseWritten);

    refreshFixtureCombo();
    rebuildSliders();
    onGeometryChanged();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &StageGeometry::liveTick);
    m_timer->start(50);
}

StageGeometry::~StageGeometry()
{
    if (s_instance == this)
        s_instance = NULL;
    delete m_engine;
}

StageGeometry *StageGeometry::instance()
{
    return s_instance;
}

double StageGeometry::stageWidth() const
{
    return m_engine->stage().sx();
}

double StageGeometry::stageDepth() const
{
    return m_engine->stage().sy();
}

double StageGeometry::stageOffsetX() const { return m_engine->stageOffsetX(); }
double StageGeometry::stageOffsetY() const { return m_engine->stageOffsetY(); }
double StageGeometry::stageHeight() const { return m_engine->stageHeight(); }
double StageGeometry::roomHeight() const { return m_engine->roomHeight(); }

void StageGeometry::setStageSize(double sx, double sy) { m_engine->setStage(sx, sy); }
void StageGeometry::setStageOffset(double ox, double oy) { m_engine->setStageOffset(ox, oy); }
void StageGeometry::setStageHeight(double h) { m_engine->setStageHeight(h); }
void StageGeometry::setRoomHeight(double h) { m_engine->setRoomHeight(h); }

StageGeom::Room StageGeometry::room() const
{
    return m_engine->room();
}

StageGeom::Stage StageGeometry::stage() const
{
    return m_engine->stage();
}

QString StageGeometry::fixtureName(quint32 id) const
{
    StageGeometryFixture *f = m_engine->fixtureById(id);
    return f != NULL ? f->name : QString();
}

void StageGeometry::aimAt(const QList<quint32> &ids, double x, double y, double z)
{
    for (quint32 id : ids)
    {
        StageGeometryFixture *f = m_engine->fixtureById(id);
        if (f == NULL || !f->calibrated)
            continue;
        QPair<int, int> pair = m_engine->choosePair(f, x, y, z);
        if (pair.first < 0)
            continue;
        setDeskValue(f->panAddress(), pair.first);
        setDeskValue(f->tiltAddress(), pair.second);
        if (f->dimmerAddress() >= 0)
            setDeskValue(f->dimmerAddress(), 255);
    }
    syncSliderValues();
}

void StageGeometry::clearAim(const QList<quint32> &ids)
{
    SimpleDesk *desk = SimpleDesk::instance();
    if (desk == NULL)
        return;
    for (quint32 id : ids)
    {
        StageGeometryFixture *f = m_engine->fixtureById(id);
        if (f == NULL)
            continue;
        desk->resetChannel(uint(f->panAddress()));
        desk->resetChannel(uint(f->tiltAddress()));
        if (f->dimmerAddress() >= 0)
            desk->resetChannel(uint(f->dimmerAddress()));
    }
    syncSliderValues();
}

bool StageGeometry::isAimOverridden(const QList<quint32> &ids) const
{
    SimpleDesk *desk = SimpleDesk::instance();
    if (desk == NULL)
        return false;
    for (quint32 id : ids)
    {
        StageGeometryFixture *f = m_engine->fixtureById(id);
        if (f == NULL)
            continue;
        if (desk->isChannelOverridden(uint(f->panAddress())) ||
            desk->isChannelOverridden(uint(f->tiltAddress())))
            return true;
    }
    return false;
}

bool StageGeometry::fixtureAimData(quint32 id, double &x, double &y, double &z,
                                   double &panK, double &panT, double &tiltK, double &tiltT,
                                   int &panCh, int &tiltCh, int &dimCh) const
{
    StageGeometryFixture *f = m_engine->fixtureById(id);
    if (f == NULL || !f->calibrated)
        return false;

    x = f->fixture.x();
    y = f->fixture.y();
    z = f->fixture.z();
    panK = f->fixture.pan().k();
    panT = f->fixture.pan().t();
    tiltK = f->fixture.tilt().k();
    tiltT = f->fixture.tilt().t();
    panCh = f->panChannel;
    tiltCh = f->tiltChannel;
    dimCh = f->dimmerChannel;
    return true;
}

bool StageGeometry::fixturePosition(quint32 id, double &x, double &y, double &z) const
{
    StageGeometryFixture *f = m_engine->fixtureById(id);
    if (f == NULL || !f->calibrated)
        return false;
    x = f->fixture.x();
    y = f->fixture.y();
    z = f->fixture.z();
    return true;
}

QList<quint32> StageGeometry::calibratedFixtureIds() const
{
    QList<quint32> ids;
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        if (f->calibrated)
            ids << f->id;
    }
    return ids;
}

void StageGeometry::syncFixtureToMonitor(StageGeometryFixture *f)
{
    if (f == NULL || !f->calibrated)
        return;

    // Stage Geometry world (y up) -> Fixture Monitor room (y down), both in mm.
    StageGeom::Room r = m_engine->room();
    double mx = (f->fixture.x() - r.minX()) * 1000.0;
    double my = (r.maxY() - f->fixture.y()) * 1000.0;
    m_doc->monitorProperties()->setFixturePosition(f->id, 0, 0, QVector3D(mx, my, 0));

    emit fixturePositionChanged(f->id, f->fixture.x(), f->fixture.y(), f->fixture.z());
}

void StageGeometry::setFixturePosition(quint32 id, double x, double y)
{
    StageGeometryFixture *f = m_engine->fixtureById(id);
    if (f == NULL || !f->calibrated)
        return;

    f->fixture.setPosition(x, y, f->fixture.z());
    f->lastPan = -1;
    f->lastTilt = -1;
    syncFixtureToMonitor(f);
    drawUsage();
}

void StageGeometry::clearContents()
{
    m_engine->setLive(false);
    m_engine->setStage(4.0, 3.0);
    m_engine->setStageOffset(0.5, 1.5);
    m_engine->setStageHeight(0.5);
    m_engine->setRoomHeight(4.0);
    m_activeIndex = 0;
    m_stepIndex = 0;
    m_hasTarget = false;
    // calibration data is owned per QLC+ fixture; a new workspace removes the
    // fixtures themselves, which the engine follows.
    onFixturesChanged();
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void StageGeometry::initUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    initTopBar();

    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    initCalibrationTab();
    initUsageTab();
    initLiveTab();
    mainLayout->addWidget(m_tabs, 1);
}

void StageGeometry::initTopBar()
{
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout *bar = new QHBoxLayout(frame);
    bar->setContentsMargins(4, 2, 4, 2);

    bar->addWidget(new QLabel(tr("Lamp:"), frame));
    m_fixtureCombo = new QComboBox(frame);
    m_fixtureCombo->setMinimumWidth(200);
    connect(m_fixtureCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StageGeometry::onFixtureSelected);
    bar->addWidget(m_fixtureCombo);

    bar->addSpacing(16);
    m_infoLabel = new QLabel(frame);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bar->addWidget(m_infoLabel);

    bar->addStretch();
    m_statusLabel = new QLabel(frame);
    m_statusLabel->setStyleSheet("color:#0a7");
    bar->addWidget(m_statusLabel);

    if (layout() != NULL)
        layout()->addWidget(frame);
}

void StageGeometry::initCalibrationTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(2, 2, 2, 2);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, tab);
    layout->addWidget(splitter);

    // left: steps
    QGroupBox *stepsBox = new QGroupBox(tr("Calibration steps"), splitter);
    QVBoxLayout *stepsLayout = new QVBoxLayout(stepsBox);
    m_stepList = new QListWidget(stepsBox);
    connect(m_stepList, &QListWidget::currentRowChanged, this, &StageGeometry::selectStep);
    stepsLayout->addWidget(m_stepList);
    splitter->addWidget(stepsBox);

    // center: view
    QGroupBox *viewBox = new QGroupBox(tr("Stage & room (room x/y from the Fixture Monitor)"), splitter);
    QVBoxLayout *viewLayout = new QVBoxLayout(viewBox);
    m_calibView = new StageGeometryView(viewBox);
    m_calibView->setResizable(false);
    viewLayout->addWidget(m_calibView);
    // Small side-view inset used only by the front / tilt-reference steps.
    m_sideView = new StageGeometryView(viewBox);
    m_sideView->setMode(StageGeometryView::SideView);
    m_sideView->setResizable(false);
    m_sideView->setFixedHeight(150);
    m_sideView->hide();
    viewLayout->addWidget(m_sideView);
    splitter->addWidget(viewBox);

    // right: fixture setup + step
    QWidget *right = new QWidget(splitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_stepTitle = new QLabel(right);
    QFont f = m_stepTitle->font();
    f.setBold(true);
    m_stepTitle->setFont(f);
    m_stepTitle->setWordWrap(true);
    rightLayout->addWidget(m_stepTitle);
    m_stepInstr = new QLabel(right);
    m_stepInstr->setWordWrap(true);
    rightLayout->addWidget(m_stepInstr);
    m_frontHint = new QLabel(right);
    m_frontHint->setWordWrap(true);
    m_frontHint->setStyleSheet("color:#b8860b");
    rightLayout->addWidget(m_frontHint);

    // Adaptive Simple Desk sliders for the active fixture
    QGroupBox *sliderBox = new QGroupBox(tr("Simple Desk — fixture channels"), right);
    m_sliderLayout = new QHBoxLayout(sliderBox);
    m_sliderHost = sliderBox;
    rightLayout->addWidget(sliderBox);

    QHBoxLayout *captureRow = new QHBoxLayout();
    m_captureButton = new QPushButton(tr("Capture"), right);
    connect(m_captureButton, &QPushButton::clicked, this, &StageGeometry::captureStep);
    captureRow->addWidget(m_captureButton);
    m_addRefButton = new QPushButton(tr("Add current DMX"), right);
    connect(m_addRefButton, &QPushButton::clicked, this, &StageGeometry::addReferenceValue);
    captureRow->addWidget(m_addRefButton);
    m_clearRefButton = new QPushButton(tr("Clear"), right);
    connect(m_clearRefButton, &QPushButton::clicked, this, &StageGeometry::clearReference);
    captureRow->addWidget(m_clearRefButton);
    captureRow->addStretch();
    rightLayout->addLayout(captureRow);

    m_refValues = new QLabel(right);
    m_refValues->setWordWrap(true);
    rightLayout->addWidget(m_refValues);
    m_confirmCheck = new QCheckBox(tr("Other axis parked correctly"), right);
    rightLayout->addWidget(m_confirmCheck);

    QHBoxLayout *nav = new QHBoxLayout();
    QPushButton *prev = new QPushButton(tr("◀ Back"), right);
    connect(prev, &QPushButton::clicked, this, &StageGeometry::prevStep);
    QPushButton *next = new QPushButton(tr("Next ▶"), right);
    connect(next, &QPushButton::clicked, this, &StageGeometry::nextStep);
    nav->addWidget(prev);
    nav->addWidget(next);
    nav->addStretch();
    rightLayout->addLayout(nav);

    m_calibrateButton = new QPushButton(tr("Calibrate"), right);
    connect(m_calibrateButton, &QPushButton::clicked, this, &StageGeometry::calibrate);
    rightLayout->addWidget(m_calibrateButton);
    m_calibResult = new QLabel(right);
    m_calibResult->setWordWrap(true);
    rightLayout->addWidget(m_calibResult);
    rightLayout->addStretch();

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 0);

    m_tabs->addTab(tab, tr("Calibration"));
}

void StageGeometry::initUsageTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(2, 2, 2, 2);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, tab);
    layout->addWidget(splitter);

    QGroupBox *viewBox = new QGroupBox(tr("Target — click in the plan"), splitter);
    QVBoxLayout *viewLayout = new QVBoxLayout(viewBox);
    m_usageView = new StageGeometryView(viewBox);
    m_usageView->setResizable(false);
    connect(m_usageView, &StageGeometryView::clicked, this, &StageGeometry::onUsageClicked);
    viewLayout->addWidget(m_usageView);
    splitter->addWidget(viewBox);

    QWidget *right = new QWidget(splitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    rightLayout->addWidget(new QLabel(tr("Controlled lamps / groups"), right));
    m_usageList = new QListWidget(right);
    m_usageList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_usageList->setMinimumHeight(120);
    connect(m_usageList, &QListWidget::itemSelectionChanged, this, &StageGeometry::updateTarget);
    rightLayout->addWidget(m_usageList);

    QGroupBox *target = new QGroupBox(tr("Target height"), right);
    QVBoxLayout *tl = new QVBoxLayout(target);
    m_heightSlider = new QSlider(Qt::Horizontal, target);
    m_heightSlider->setRange(0, 400);
    connect(m_heightSlider, &QSlider::valueChanged, this, &StageGeometry::updateTarget);
    tl->addWidget(m_heightSlider);
    m_heightLabel = new QLabel(tr("z = 0.00 m"), target);
    tl->addWidget(m_heightLabel);
    rightLayout->addWidget(target);

    m_usageInfo = new QTextEdit(right);
    m_usageInfo->setReadOnly(true);
    rightLayout->addWidget(m_usageInfo, 1);

    m_applyCheck = new QCheckBox(tr("Apply to Simple Desk on click"), right);
    m_applyCheck->setChecked(true);
    rightLayout->addWidget(m_applyCheck);

    QPushButton *apply = new QPushButton(tr("Apply to Simple Desk"), right);
    connect(apply, &QPushButton::clicked, this, [this]() { applyTarget(false); });
    rightLayout->addWidget(apply);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 0);

    m_tabs->addTab(tab, tr("Usage"));
}

void StageGeometry::initLiveTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QLabel *title = new QLabel(tr("Live coordinate server"), tab);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);
    QLabel *desc = new QLabel(tr("A console sends a target point as three DMX channels (x, y, z) "
                                 "on an input universe. They are translated into pan/tilt and "
                                 "written to the Simple Desk for every driven lamp."), tab);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    QGroupBox *cfg = new QGroupBox(tr("Input (from console)"), tab);
    QHBoxLayout *cl = new QHBoxLayout(cfg);
    cl->addWidget(new QLabel(tr("Universe:"), cfg));
    m_liveUniverse = new QSpinBox(cfg);
    m_liveUniverse->setRange(0, 511);
    cl->addWidget(m_liveUniverse);
    cl->addWidget(new QLabel(tr("Channels x/y/z:"), cfg));
    m_liveXCh = new QSpinBox(cfg);
    m_liveXCh->setRange(1, 512);
    m_liveXCh->setValue(1);
    m_liveYCh = new QSpinBox(cfg);
    m_liveYCh->setRange(1, 512);
    m_liveYCh->setValue(2);
    m_liveZCh = new QSpinBox(cfg);
    m_liveZCh->setRange(1, 512);
    m_liveZCh->setValue(3);
    cl->addWidget(m_liveXCh);
    cl->addWidget(m_liveYCh);
    cl->addWidget(m_liveZCh);
    cl->addStretch();
    layout->addWidget(cfg);

    m_liveAll = new QCheckBox(tr("Drive all calibrated lamps (otherwise the Usage selection)"), tab);
    m_liveAll->setChecked(true);
    layout->addWidget(m_liveAll);

    QHBoxLayout *btns = new QHBoxLayout();
    m_liveButton = new QPushButton(tr("Start"), tab);
    connect(m_liveButton, &QPushButton::clicked, this, &StageGeometry::toggleLive);
    btns->addWidget(m_liveButton);
    m_liveStatus = new QLabel(tr("stopped"), tab);
    m_liveStatus->setStyleSheet("color:#7fb0e0");
    btns->addWidget(m_liveStatus);
    btns->addStretch();
    layout->addLayout(btns);

    m_liveLog = new QTextEdit(tab);
    m_liveLog->setReadOnly(true);
    layout->addWidget(m_liveLog, 1);

    m_tabs->addTab(tab, tr("Live"));
}

// ---------------------------------------------------------------------------
// State updates
// ---------------------------------------------------------------------------

void StageGeometry::refreshFixtureCombo()
{
    m_updating = true;
    m_fixtureCombo->clear();
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        m_fixtureCombo->addItem(QString("%1. %2%3").arg(i + 1).arg(f->name,
                              f->calibrated ? "  ✓" : ""));
    }
    if (m_activeIndex >= m_engine->fixtureCount())
        m_activeIndex = qMax(0, m_engine->fixtureCount() - 1);
    if (m_fixtureCombo->count() > 0)
        m_fixtureCombo->setCurrentIndex(m_activeIndex);
    m_updating = false;
}

void StageGeometry::rebuildSliders()
{
    // drop previous sliders
    if (m_panSlider != NULL) { delete m_panSlider; m_panSlider = NULL; }
    if (m_tiltSlider != NULL) { delete m_tiltSlider; m_tiltSlider = NULL; }

    StageGeometryFixture *f = activeFixture();
    if (f == NULL || !f->isMovingHead())
        return;

    m_panSlider = new ConsoleChannel(m_sliderHost, m_doc, f->id, f->panChannel, false);
    m_panSlider->setTitle(tr("Pan"));
    if (m_doc->fixture(f->id) != NULL && m_doc->fixture(f->id)->channel(f->panChannel) != NULL)
        m_panSlider->setLabel(m_doc->fixture(f->id)->channel(f->panChannel)->name());
    m_panSlider->setFixedWidth(72);
    m_panSlider->setMinimumHeight(200);
    // A non-empty channel stylesheet is what makes ConsoleChannel reveal its
    // slider on first show (otherwise only the spin box is shown).
    m_panSlider->setChannelStyleSheet("QGroupBox {}");
    connect(m_panSlider, &ConsoleChannel::valueChanged, this, &StageGeometry::slotSliderValueChanged);
    m_sliderLayout->addWidget(m_panSlider);

    m_tiltSlider = new ConsoleChannel(m_sliderHost, m_doc, f->id, f->tiltChannel, false);
    m_tiltSlider->setTitle(tr("Tilt"));
    if (m_doc->fixture(f->id) != NULL && m_doc->fixture(f->id)->channel(f->tiltChannel) != NULL)
        m_tiltSlider->setLabel(m_doc->fixture(f->id)->channel(f->tiltChannel)->name());
    m_tiltSlider->setFixedWidth(72);
    m_tiltSlider->setMinimumHeight(200);
    m_tiltSlider->setChannelStyleSheet("QGroupBox {}");
    connect(m_tiltSlider, &ConsoleChannel::valueChanged, this, &StageGeometry::slotSliderValueChanged);
    m_sliderLayout->addWidget(m_tiltSlider);
    m_sliderLayout->addStretch();

    syncSliderValues();
}

void StageGeometry::onFixtureSelected(int index)
{
    if (m_updating || index < 0)
        return;
    m_activeIndex = index;
    m_stepIndex = 0;
    rebuildSliders();
    buildSteps();
    showStep();
    drawUsage();
}

void StageGeometry::onFixturesChanged()
{
    refreshFixtureCombo();
    rebuildSliders();
    buildSteps();
    showStep();
    refreshUsageList();
    drawUsage();
    emit fixturesChanged();
}

void StageGeometry::updateRoomLabel()
{
    StageGeom::Stage st = m_engine->stage();
    StageGeom::Room room = m_engine->room();
    if (m_infoLabel == NULL)
        return;
    m_infoLabel->setText(tr("Stage %1×%2 m @ (%3, %4), height %5 m   Room %6×%7 m, height %8 m   (set up in the Fixture Monitor)")
                         .arg(st.sx(), 0, 'f', 2).arg(st.sy(), 0, 'f', 2)
                         .arg(m_engine->stageOffsetX(), 0, 'f', 2).arg(m_engine->stageOffsetY(), 0, 'f', 2)
                         .arg(m_engine->stageHeight(), 0, 'f', 2)
                         .arg(room.maxX() - room.minX(), 0, 'f', 2)
                         .arg(room.maxY() - room.minY(), 0, 'f', 2)
                         .arg(room.maxZ(), 0, 'f', 2));
}

void StageGeometry::onGeometryChanged()
{
    if (m_updating)
        return;
    m_updating = true;

    StageGeom::Stage st = m_engine->stage();
    StageGeom::Room room = m_engine->room();
    m_cachedRoom = room;

    m_calibView->setStage(st.sx(), st.sy());
    m_calibView->setRoom(room.minX(), room.maxX(), room.minY(), room.maxY());
    m_usageView->setStage(st.sx(), st.sy());
    m_usageView->setRoom(room.minX(), room.maxX(), room.minY(), room.maxY());

    updateRoomLabel();

    // drop stage points that no longer exist after a stage resize
    QList<QPointF> valid = st.points();
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        for (auto it = f->pointDmx.begin(); it != f->pointDmx.end(); )
        {
            if (!valid.contains(it.key()))
                it = f->pointDmx.erase(it);
            else
                ++it;
        }
    }

    m_heightSlider->setRange(int(room.minZ() * 100), int(room.maxZ() * 100));
    m_updating = false;

    buildSteps();
    showStep();
    drawUsage();
    emit stageChanged();
}

void StageGeometry::notifyRoomChanged()
{
    // The room footprint is derived from the Fixture Monitor grid, which has no
    // signal of its own. Re-emit the geometry change so consumers (the 3D
    // Preview, the Monitor overlay) pick up the new room size.
    emit stageChanged();
}

void StageGeometry::onUniverseWritten(quint32 idx, const QByteArray &ua)
{
    Q_UNUSED(ua)
    StageGeometryFixture *f = activeFixture();
    if (f == NULL || quint32(f->universe) != idx)
        return;
    syncSliderValues();
}

void StageGeometry::syncSliderValues()
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
        return;

    if (m_panSlider != NULL)
    {
        m_panSlider->blockSignals(true);
        m_panSlider->setValue(uchar(deskValue(f->panAddress())), false);
        m_panSlider->blockSignals(false);
    }
    if (m_tiltSlider != NULL)
    {
        m_tiltSlider->blockSignals(true);
        m_tiltSlider->setValue(uchar(deskValue(f->tiltAddress())), false);
        m_tiltSlider->blockSignals(false);
    }
}

StageGeometryFixture *StageGeometry::activeFixture() const
{
    return m_engine->fixtureAt(m_activeIndex);
}

// ---------------------------------------------------------------------------
// Simple Desk bridge
// ---------------------------------------------------------------------------

void StageGeometry::setDeskValue(int address, int value)
{
    if (address < 0)
        return;
    SimpleDesk *desk = SimpleDesk::instance();
    if (desk != NULL)
        desk->setAbsoluteChannelValue(uint(address), uchar(qBound(0, value, 255)));
}

int StageGeometry::deskValue(int address) const
{
    if (address < 0)
        return 0;
    SimpleDesk *desk = SimpleDesk::instance();
    return desk != NULL ? int(desk->getAbsoluteChannelValue(uint(address))) : 0;
}

void StageGeometry::slotSliderValueChanged(quint32 fid, quint32 chan, uchar value)
{
    Fixture *fx = m_doc->fixture(fid);
    if (fx == NULL)
        return;
    uint address = (uint(fx->universe()) << 9) + fx->universeAddress() + chan;
    setDeskValue(int(address), int(value));
}

// ---------------------------------------------------------------------------
// Calibration wizard
// ---------------------------------------------------------------------------

void StageGeometry::buildSteps()
{
    m_steps.clear();
    Step front;
    front.kind = Step::Front;
    front.name = tr("front");
    front.angle = M_PI / 2.0;
    m_steps.append(front);

    for (const QString &name : StageGeometryEngine::panDirectionNames())
    {
        Step s;
        s.kind = Step::PanRef;
        s.name = name;
        s.angle = StageGeometryEngine::panDirectionAngle(name);
        m_steps.append(s);
    }
    for (const QString &name : StageGeometryEngine::tiltDirectionNames())
    {
        Step s;
        s.kind = Step::TiltRef;
        s.name = name;
        s.angle = StageGeometryEngine::tiltDirectionAngle(name);
        m_steps.append(s);
    }
    StageGeom::Stage stage = m_engine->stage();
    for (const QPointF &p : stage.points())
    {
        Step s;
        s.kind = Step::Point;
        s.point = p;
        m_steps.append(s);
    }
    m_stepIndex = m_steps.isEmpty() ? 0 : qBound(0, m_stepIndex, m_steps.size() - 1);
}

QString StageGeometry::pointLabel(const QPointF &point) const
{
    StageGeom::Stage stage = m_engine->stage();
    QString hx = (point.x() < stage.sx() / 2.0 - 1e-9) ? tr("left")
               : (point.x() > stage.sx() / 2.0 + 1e-9) ? tr("right") : "";
    QString vy = (point.y() < stage.sy() / 2.0 - 1e-9) ? tr("back")
               : (point.y() > stage.sy() / 2.0 + 1e-9) ? tr("front") : "";
    QStringList parts;
    if (!vy.isEmpty()) parts << vy;
    if (!hx.isEmpty()) parts << hx;
    return parts.isEmpty() ? tr("center") : parts.join("-");
}

bool StageGeometry::stepComplete(const Step &step) const
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
        return false;
    switch (step.kind)
    {
    case Step::Front:   return f->hasFront;
    case Step::Point:   return f->pointDmx.contains(step.point);
    case Step::PanRef:  return !f->panRefs.value(step.name).isEmpty();
    case Step::TiltRef: return !f->tiltRefs.value(step.name).isEmpty();
    }
    return false;
}

QString StageGeometry::stepShort(const Step &step) const
{
    switch (step.kind)
    {
    case Step::Front:   return tr("Alignment front");
    case Step::Point:   return tr("Point %1").arg(pointLabel(step.point));
    case Step::PanRef:  return tr("Pan → %1").arg(step.name);
    case Step::TiltRef: return tr("Tilt → %1").arg(step.name);
    }
    return QString();
}

QString StageGeometry::frontHint(const Step &step) const
{
    if (step.kind != Step::PanRef && step.kind != Step::TiltRef)
        return QString();
    StageGeometryFixture *f = activeFixture();
    if (f == NULL || !f->hasFront)
        return tr("⚠ Step 1 (alignment front) is still missing.");
    QString other = (step.kind == Step::PanRef) ? "Tilt" : "Pan";
    int value = (step.kind == Step::PanRef) ? f->frontTilt : f->frontPan;
    return tr("From step 1:  Pan %1  ·  Tilt %2\n→ keep %3 on DMX %4.")
            .arg(f->frontPan).arg(f->frontTilt).arg(other).arg(value);
}

void StageGeometry::refreshStepList()
{
    m_stepList->blockSignals(true);
    m_stepList->clear();
    for (int i = 0; i < m_steps.size(); i++)
    {
        QString mark = stepComplete(m_steps.at(i)) ? "✓" : "○";
        m_stepList->addItem(QString(" %1  %2. %3").arg(mark).arg(i + 1).arg(stepShort(m_steps.at(i))));
    }
    m_stepList->setCurrentRow(m_stepIndex);
    m_stepList->blockSignals(false);
}

void StageGeometry::showStep()
{
    if (m_steps.isEmpty())
        return;
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
    {
        m_stepTitle->setText(tr("No moving head fixtures"));
        m_stepInstr->setText(tr("Patch a moving head (a fixture with Pan and Tilt channels) in the "
                                "Fixtures tab. It will appear here automatically."));
        m_frontHint->clear();
        m_refValues->clear();
        m_captureButton->setVisible(false);
        m_addRefButton->setVisible(false);
        m_clearRefButton->setVisible(false);
        m_confirmCheck->setVisible(false);
        m_refValues->setVisible(false);
        m_stepList->clear();
        m_calibView->setMode(StageGeometryView::TopDown);
        StageGeom::Stage st = m_engine->stage();
        StageGeom::Room rm = m_engine->room();
        m_calibView->setStage(st.sx(), st.sy());
        m_calibView->setRoom(rm.minX(), rm.maxX(), rm.minY(), rm.maxY());
        m_calibView->setGridPoints(QList<QPointF>(), QList<QPointF>());
        m_calibView->setHighlight(QPointF(), false);
        m_calibView->setPanArrow(0, QString(), false);
        m_calibView->setSideArrow(0, QString(), false);
        m_calibView->setMessage(tr("Patch a moving head fixture to begin"));
        m_sideView->hide();
        return;
    }

    const Step &step = m_steps.at(m_stepIndex);
    int i = m_stepIndex + 1, n = m_steps.size();
    bool isRef = (step.kind == Step::PanRef || step.kind == Step::TiltRef);
    bool isFront = (step.kind == Step::Front);
    bool isPoint = (step.kind == Step::Point);

    m_captureButton->setVisible(isFront || isPoint);
    m_addRefButton->setVisible(isRef);
    m_clearRefButton->setVisible(isRef);
    m_confirmCheck->setVisible(isRef);
    m_refValues->setVisible(isRef);

    if (isFront)
    {
        m_stepTitle->setText(tr("[%1] Step %2/%3 — align forward").arg(f->name).arg(i).arg(n));
        m_stepInstr->setText(tr("Move the Pan/Tilt Simple Desk sliders until the beam points "
                                "horizontally FORWARD (+Y), then click Capture."));
        m_captureButton->setText(tr("Capture forward"));
    }
    else if (isPoint)
    {
        m_stepTitle->setText(tr("[%1] Step %2/%3 — point “%4” (%5, %6)")
                             .arg(f->name).arg(i).arg(n).arg(pointLabel(step.point))
                             .arg(step.point.x()).arg(step.point.y()));
        m_stepInstr->setText(tr("Aim the beam at the marked point with the sliders, then click Capture."));
        m_captureButton->setText(tr("Capture point"));
    }
    else
    {
        bool isPan = (step.kind == Step::PanRef);
        QString axis = isPan ? "Pan" : "Tilt";
        QString other = isPan ? "TILT" : "PAN";
        int parked = isPan ? f->frontTilt : f->frontPan;
        m_stepTitle->setText(tr("[%1] Step %2/%3 — %4 reference “%5”")
                             .arg(f->name).arg(i).arg(n).arg(axis).arg(step.name));
        QString cross = f->hasFront ? tr("Keep %1 on DMX %2 (step 1).").arg(other).arg(parked)
                                    : tr("First set %1 as in step 1.").arg(other);
        m_stepInstr->setText(tr("%1\nMove the %2 slider across its range and click “Add current DMX” "
                                "for every position where the beam shows the reference direction.")
                             .arg(cross).arg(axis));
        m_confirmCheck->setChecked(f->confirmed.contains(
            QString("%1:%2").arg(isPan ? "pan" : "tilt").arg(step.name)));
        const QList<int> values = isPan ? f->panRefs.value(step.name) : f->tiltRefs.value(step.name);
        QStringList sv;
        for (int v : values)
            sv << QString::number(v);
        m_refValues->setText(tr("Captured: %1").arg(sv.isEmpty() ? tr("(none)") : sv.join(", ")));
    }

    m_frontHint->setText(frontHint(step));

    // The plan view is ALWAYS top-down, so the room can be edited on any step.
    m_calibView->setMode(StageGeometryView::TopDown);
    StageGeom::Stage st = m_engine->stage();
    StageGeom::Room rm = m_engine->room();
    m_calibView->setStage(st.sx(), st.sy());
    m_calibView->setRoom(rm.minX(), rm.maxX(), rm.minY(), rm.maxY());
    QList<QPointF> done;
    for (auto it = f->pointDmx.constBegin(); it != f->pointDmx.constEnd(); ++it)
        done << it.key();
    m_calibView->setGridPoints(st.points(), done);
    m_calibView->setHighlight(isPoint ? step.point : QPointF(), isPoint);

    bool showPan = (step.kind == Step::PanRef) || isFront;
    m_calibView->setPanArrow(showPan ? step.angle : 0.0, step.name, showPan);
    if (isFront)
        m_calibView->setMessage(tr("Beam horizontal forward (+Y) — see the side view"));
    else if (isPoint)
        m_calibView->setMessage(tr("Aim the beam at the highlighted point"));
    else if (step.kind == Step::PanRef)
        m_calibView->setMessage(tr("Keep tilt horizontal – pan in the arrow direction"));
    else
        m_calibView->setMessage(tr("Move the tilt slider – see the side view"));

    // Side-view inset for the front / tilt-reference steps.
    if (isFront || step.kind == Step::TiltRef)
    {
        double angle = isFront ? M_PI / 2.0 : step.angle;
        m_sideView->setMode(StageGeometryView::SideView);
        m_sideView->setSideArrow(angle, isFront ? tr("forward") : step.name, true);
        m_sideView->setMessage(isFront
            ? tr("Beam horizontal forward (+Y)")
            : tr("Keep pan forward – tilt in the arrow direction"));
        m_sideView->show();
    }
    else
    {
        m_sideView->hide();
    }

    refreshStepList();
}

void StageGeometry::captureStep()
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL || m_panSlider == NULL || m_tiltSlider == NULL)
        return;
    int pan = m_panSlider->value();
    int tilt = m_tiltSlider->value();
    const Step &step = m_steps.at(m_stepIndex);

    if (step.kind == Step::Front)
    {
        bool hadOld = f->hasFront;
        int oldPan = f->frontPan, oldTilt = f->frontTilt;
        f->frontPan = pan;
        f->frontTilt = tilt;
        f->hasFront = true;
        for (int axis = 0; axis < 2; axis++)
        {
            QMap<QString, QList<int> > &store = (axis == 0) ? f->panRefs : f->tiltRefs;
            int value = (axis == 0) ? pan : tilt;
            int oldValue = (axis == 0) ? oldPan : oldTilt;
            QList<int> values = store.value("vorne");
            if (hadOld && values.contains(oldValue) && oldValue != value)
                values.removeAll(oldValue);
            if (!values.contains(value))
                values.prepend(value);
            store["vorne"] = values;
            f->confirmed.insert(QString("%1:vorne").arg(axis == 0 ? "pan" : "tilt"));
        }
        m_statusLabel->setText(tr("Forward alignment captured (Pan %1 / Tilt %2)").arg(pan).arg(tilt));
    }
    else if (step.kind == Step::Point)
    {
        f->pointDmx[step.point] = QPair<int, int>(pan, tilt);
        m_statusLabel->setText(tr("Point captured (Pan %1 / Tilt %2)").arg(pan).arg(tilt));
    }
    showStep();
}

void StageGeometry::addReferenceValue()
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
        return;
    const Step &step = m_steps.at(m_stepIndex);
    bool isPan = (step.kind == Step::PanRef);
    if (!isPan && step.kind != Step::TiltRef)
        return;
    int value = isPan ? (m_panSlider != NULL ? m_panSlider->value() : -1)
                      : (m_tiltSlider != NULL ? m_tiltSlider->value() : -1);
    if (value < 0)
        return;

    QList<int> values = isPan ? f->panRefs.value(step.name) : f->tiltRefs.value(step.name);
    if (!values.contains(value))
        values.append(value);
    std::sort(values.begin(), values.end());
    if (isPan)
        f->panRefs[step.name] = values;
    else
        f->tiltRefs[step.name] = values;
    f->confirmed.insert(QString("%1:%2").arg(isPan ? "pan" : "tilt").arg(step.name));
    m_confirmCheck->setChecked(true);
    showStep();
}

void StageGeometry::clearReference()
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
        return;
    const Step &step = m_steps.at(m_stepIndex);
    bool isPan = (step.kind == Step::PanRef);
    if (isPan)
        f->panRefs[step.name] = QList<int>();
    else if (step.kind == Step::TiltRef)
        f->tiltRefs[step.name] = QList<int>();
    else
        return;
    f->confirmed.remove(QString("%1:%2").arg(isPan ? "pan" : "tilt").arg(step.name));
    showStep();
}

void StageGeometry::prevStep()
{
    if (m_stepIndex > 0)
    {
        m_stepIndex--;
        showStep();
    }
}

void StageGeometry::nextStep()
{
    if (m_stepIndex < m_steps.size() - 1)
    {
        m_stepIndex++;
        showStep();
    }
}

void StageGeometry::selectStep(int index)
{
    if (index < 0 || index >= m_steps.size() || index == m_stepIndex)
        return;
    m_stepIndex = index;
    showStep();
}

void StageGeometry::calibrate()
{
    StageGeometryFixture *f = activeFixture();
    if (f == NULL)
        return;
    if (!f->hasFront)
    {
        QMessageBox::warning(this, tr("Alignment missing"),
                             tr("Please capture step 1: the beam horizontally forward."));
        m_stepIndex = 0;
        showStep();
        return;
    }
    StageGeom::Measurement m = f->measurement();
    if (m.panValueCount() < 2 || m.tiltValueCount() < 2)
    {
        QMessageBox::warning(this, tr("Too few references"),
                             tr("At least two DMX reference values are needed for pan AND tilt.\n"
                                "Currently: pan %1, tilt %2.")
                             .arg(m.panValueCount()).arg(m.tiltValueCount()));
        return;
    }
    if (m.points.size() < 2)
    {
        QMessageBox::warning(this, tr("Too few points"),
                             tr("At least 2 stage points (ideally 9) are needed."));
        return;
    }

    try
    {
        StageGeom::CalibrationResult r = StageGeom::calibrate(m);
        f->fixture = r.fixture;
        f->calibrated = true;
        f->lastPan = -1;
        f->lastTilt = -1;
        syncFixtureToMonitor(f);

        QString quality = (r.rmsDeg < 1.5) ? tr("good") : tr("high – check");
        QString note = r.reversedBeam ? tr("\nNote: references described a reversed beam – corrected.") : QString();
        m_calibResult->setText(
            tr("A) Pan %1°±%2  Tilt %3°±%4\n"
               "B) raw x=%5 y=%6 z=%7\n"
               "C) fine x=%8 y=%9 z=%10\n"
               "   ± %11/%12/%13 cm\n"
               "Target error %14° (%15)%16")
            .arg(r.panRangeDeg, 0, 'f', 0).arg(r.panRangeStdDeg, 0, 'f', 1)
            .arg(r.tiltRangeDeg, 0, 'f', 0).arg(r.tiltRangeStdDeg, 0, 'f', 1)
            .arg(r.stagedPosition.x(), 0, 'f', 2).arg(r.stagedPosition.y(), 0, 'f', 2).arg(r.stagedPosition.z(), 0, 'f', 2)
            .arg(r.position.x(), 0, 'f', 2).arg(r.position.y(), 0, 'f', 2).arg(r.position.z(), 0, 'f', 2)
            .arg(r.positionStd.x() * 100, 0, 'f', 0).arg(r.positionStd.y() * 100, 0, 'f', 0).arg(r.positionStd.z() * 100, 0, 'f', 0)
            .arg(r.rmsDeg, 0, 'f', 2).arg(quality).arg(note));
        m_statusLabel->setText(tr("“%1” calibrated").arg(f->name));
        refreshFixtureCombo();
        refreshUsageList();
        drawUsage();
        emit fixturesChanged();
    }
    catch (const std::exception &exc)
    {
        QMessageBox::warning(this, tr("Calibration failed"), QString::fromStdString(exc.what()));
    }
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------

void StageGeometry::refreshUsageList()
{
    if (m_usageList == NULL)
        return;
    m_usageList->clear();
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        QListWidgetItem *item = new QListWidgetItem(
            QString("%1%2").arg(f->name, f->calibrated ? "" : tr("  (not calibrated)")));
        item->setData(Qt::UserRole, ItemLamp);
        item->setData(Qt::UserRole + 1, f->id);
        m_usageList->addItem(item);
    }
    for (FixtureGroup *g : m_doc->fixtureGroups())
    {
        QListWidgetItem *item = new QListWidgetItem(tr("Group: %1").arg(g->name()));
        item->setData(Qt::UserRole, ItemGroup);
        item->setData(Qt::UserRole + 1, g->id());
        m_usageList->addItem(item);
    }
}

QList<StageGeometryFixture *> StageGeometry::controlledFixtures() const
{
    QList<StageGeometryFixture *> result;
    bool anySelected = false;

    for (int i = 0; i < m_usageList->count(); i++)
    {
        QListWidgetItem *item = m_usageList->item(i);
        if (!item->isSelected())
            continue;
        anySelected = true;
        int type = item->data(Qt::UserRole).toInt();
        quint32 id = item->data(Qt::UserRole + 1).toUInt();
        if (type == ItemLamp)
        {
            StageGeometryFixture *f = m_engine->fixtureById(id);
            if (f != NULL && f->calibrated && !result.contains(f))
                result << f;
        }
        else
        {
            FixtureGroup *g = m_doc->fixtureGroup(id);
            if (g == NULL)
                continue;
            for (quint32 fid : g->fixtureList())
            {
                StageGeometryFixture *f = m_engine->fixtureById(fid);
                if (f != NULL && f->calibrated && !result.contains(f))
                    result << f;
            }
        }
    }

    if (!anySelected)
    {
        for (int i = 0; i < m_engine->fixtureCount(); i++)
        {
            StageGeometryFixture *f = m_engine->fixtureAt(i);
            if (f->calibrated)
                result << f;
        }
    }
    return result;
}

void StageGeometry::drawUsage()
{
    if (m_usageView == NULL)
        return;
    m_usageView->setMode(StageGeometryView::TopDown);
    StageGeom::Stage st = m_engine->stage();
    StageGeom::Room rm = m_engine->room();
    m_usageView->setStage(st.sx(), st.sy());
    m_usageView->setRoom(rm.minX(), rm.maxX(), rm.minY(), rm.maxY());
    m_usageView->setGridPoints(QList<QPointF>(), QList<QPointF>());
    m_usageView->setHighlight(QPointF(), false);
    m_usageView->setPanArrow(0, QString(), false);

    QList<StageGeometryFixture *> controlled = controlledFixtures();
    QList<StageGeometryView::Marker> markers;
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        if (!f->calibrated)
            continue;
        StageGeometryView::Marker mk;
        mk.pos = QPointF(f->fixture.x(), f->fixture.y());
        mk.name = f->name;
        mk.color = QColor(FIXTURE_COLORS[i % FIXTURE_COLOR_COUNT]);
        mk.selected = controlled.contains(f);
        mk.calibrated = true;
        markers << mk;
    }
    m_usageView->setMarkers(markers);
    m_usageView->setTarget(m_target2d, m_hasTarget);
    m_usageView->setMessage(m_hasTarget
        ? tr("Target: x=%1 y=%2 z=%3 m").arg(m_target2d.x(), 0, 'f', 2)
          .arg(m_target2d.y(), 0, 'f', 2).arg(m_targetZ, 0, 'f', 2)
        : tr("Click in the plan to choose a target"));
}

void StageGeometry::onUsageClicked(QPointF world)
{
    // Ignore clicks that are outside both the stage and the room.
    StageGeom::Stage stage = m_engine->stage();
    StageGeom::Room room = m_engine->room();
    if (!stage.contains(world) && !room.containsXY(world.x(), world.y()))
        return;

    m_target2d = world;
    m_hasTarget = true;
    updateTarget();
    if (m_applyCheck->isChecked())
        applyTarget(true);
}

void StageGeometry::updateTarget()
{
    m_targetZ = m_heightSlider->value() / 100.0;
    m_heightLabel->setText(tr("z = %1 m").arg(m_targetZ, 0, 'f', 2));

    if (!m_hasTarget)
    {
        drawUsage();
        return;
    }

    QList<StageGeometryFixture *> entries = controlledFixtures();
    if (entries.isEmpty())
    {
        m_usageInfo->setPlainText(tr("No calibrated lamp selected."));
        drawUsage();
        return;
    }

    QString where = m_engine->stage().contains(m_target2d) ? tr("(on stage)") : tr("(in room)");
    QStringList lines;
    lines << tr("Target:  x=%1  y=%2  z=%3 m").arg(m_target2d.x(), 0, 'f', 2)
             .arg(m_target2d.y(), 0, 'f', 2).arg(m_targetZ, 0, 'f', 2) << where << "";
    for (StageGeometryFixture *f : entries)
    {
        QPair<int, int> pair = m_engine->choosePair(f, m_target2d.x(), m_target2d.y(), m_targetZ);
        lines << (pair.first >= 0
            ? tr("%1:  Pan %2  Tilt %3").arg(f->name).arg(pair.first).arg(pair.second)
            : tr("%1:  unreachable").arg(f->name));
    }
    m_usageInfo->setPlainText(lines.join("\n"));
    drawUsage();
}

void StageGeometry::applyTarget(bool silent)
{
    if (!m_hasTarget)
        return;
    QList<StageGeometryFixture *> entries = controlledFixtures();
    int applied = 0;
    for (StageGeometryFixture *f : entries)
    {
        QPair<int, int> pair = m_engine->choosePair(f, m_target2d.x(), m_target2d.y(), m_targetZ);
        if (pair.first < 0)
            continue;
        setDeskValue(f->panAddress(), pair.first);
        setDeskValue(f->tiltAddress(), pair.second);
        if (f->dimmerAddress() >= 0)
            setDeskValue(f->dimmerAddress(), 255);
        applied++;
    }
    if (!silent)
        m_statusLabel->setText(tr("Applied to %1 lamp(s)").arg(applied));
    syncSliderValues();
}

// ---------------------------------------------------------------------------
// Live
// ---------------------------------------------------------------------------

void StageGeometry::applyLiveConfig()
{
    m_engine->setLiveConfig(m_liveUniverse->value(), m_liveXCh->value(),
                            m_liveYCh->value(), m_liveZCh->value(), m_liveAll->isChecked());
}

void StageGeometry::toggleLive()
{
    bool running = !m_engine->isLive();
    applyLiveConfig();
    m_engine->setLive(running);
    m_liveButton->setText(running ? tr("Stop") : tr("Start"));
    m_liveStatus->setText(running ? tr("running…") : tr("stopped"));
}

void StageGeometry::liveTick()
{
    // The room x/y come from the Fixture Monitor; poll for changes there.
    StageGeom::Room r = m_engine->room();
    if (!qFuzzyCompare(r.maxX(), m_cachedRoom.maxX()) ||
        !qFuzzyCompare(r.maxY(), m_cachedRoom.maxY()))
        onGeometryChanged();

    if (!m_engine->isLive())
        return;

    applyLiveConfig();
    QVector3D t;
    if (!m_engine->liveTargetFromInput(t))
    {
        m_liveLog->setPlainText(tr("(waiting for input…)"));
        return;
    }
    m_engine->setTarget(t);
    m_target2d = QPointF(t.x(), t.y());
    m_targetZ = t.z();
    m_hasTarget = true;

    QList<StageGeometryFixture *> targets;
    if (m_liveAll->isChecked())
    {
        for (int i = 0; i < m_engine->fixtureCount(); i++)
        {
            StageGeometryFixture *f = m_engine->fixtureAt(i);
            if (f->calibrated)
                targets << f;
        }
    }
    else
        targets = controlledFixtures();

    QStringList lines;
    lines << tr("Input → target x=%1 y=%2 z=%3 m").arg(t.x(), 0, 'f', 2)
             .arg(t.y(), 0, 'f', 2).arg(t.z(), 0, 'f', 2) << "";
    for (StageGeometryFixture *f : targets)
    {
        QPair<int, int> pair = m_engine->choosePair(f, t.x(), t.y(), t.z());
        if (pair.first >= 0)
        {
            setDeskValue(f->panAddress(), pair.first);
            setDeskValue(f->tiltAddress(), pair.second);
            if (f->dimmerAddress() >= 0)
                setDeskValue(f->dimmerAddress(), 255);
            lines << tr("%1: Pan %2  Tilt %3").arg(f->name).arg(pair.first).arg(pair.second);
        }
        else
            lines << tr("%1: unreachable").arg(f->name);
    }
    m_liveLog->setPlainText(lines.join("\n"));
    syncSliderValues();
    drawUsage();
}

// ---------------------------------------------------------------------------
// Load & Save
// ---------------------------------------------------------------------------

bool StageGeometry::loadXML(QXmlStreamReader &root)
{
    bool ok = m_engine->loadXML(root);
    if (!ok)
        return false;

    m_activeIndex = 0;
    m_stepIndex = 0;
    refreshFixtureCombo();
    rebuildSliders();
    onGeometryChanged();

    m_liveUniverse->setValue(m_engine->liveUniverse());
    m_liveXCh->setValue(m_engine->liveXChannel());
    m_liveYCh->setValue(m_engine->liveYChannel());
    m_liveZCh->setValue(m_engine->liveZChannel());
    m_liveAll->setChecked(m_engine->liveAllFixtures());

    // Push the restored calibrated positions to the Fixture Monitor.
    for (int i = 0; i < m_engine->fixtureCount(); i++)
    {
        StageGeometryFixture *f = m_engine->fixtureAt(i);
        if (f->calibrated)
            syncFixtureToMonitor(f);
    }
    emit fixturesChanged();
    return true;
}

bool StageGeometry::saveXML(QXmlStreamWriter *doc) const
{
    return m_engine->saveXML(doc);
}
