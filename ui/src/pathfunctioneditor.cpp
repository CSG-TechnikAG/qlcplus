/*
  Q Light Controller Plus
  pathfunctioneditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QSplitter>
#include <QLabel>
#include <QSlider>
#include <QLineEdit>
#include <QTimer>
#include <QCheckBox>
#include <QColor>
#include <QtMath>
#include <cmath>

#include "pathfunctioneditor.h"
#include "stagegeometryview.h"
#include "stagegeometry.h"
#include "pathfunction.h"
#include "doc.h"

static QPolygonF makeShape(int preset, const QRectF& box, double rotationDeg)
{
    QPolygonF path;
    const int N = 48;
    double l = box.left(), r = box.right(), t = box.top(), b = box.bottom();
    double cx = box.center().x(), cy = box.center().y();

    switch (preset)
    {
    case 4: // Line
        path << QPointF(l, cy) << QPointF(r, cy);
        break;
    case 5: // Circle
        for (int i = 0; i <= N; i++)
        {
            double a = 2.0 * M_PI * i / N;
            path << QPointF(cx + (box.width() / 2.0) * qCos(a),
                            cy + (box.height() / 2.0) * qSin(a));
        }
        break;
    case 6: // Semicircle (upper half)
        for (int i = 0; i <= N; i++)
        {
            double a = M_PI * i / N;
            path << QPointF(cx + (box.width() / 2.0) * qCos(a),
                            cy + (box.height() / 2.0) * qSin(a));
        }
        break;
    case 7: // Triangle
        path << QPointF(cx, t) << QPointF(r, b) << QPointF(l, b) << QPointF(cx, t);
        break;
    case 8: // Rectangle
        path << QPointF(l, t) << QPointF(r, t) << QPointF(r, b)
             << QPointF(l, b) << QPointF(l, t);
        break;
    default:
        break;
    }

    if (rotationDeg != 0.0 && path.isEmpty() == false)
    {
        double rad = qDegreesToRadians(rotationDeg);
        double cs = qCos(rad), sn = qSin(rad);
        for (QPointF& p : path)
        {
            QPointF d = p - QPointF(cx, cy);
            p = QPointF(cx, cy) + QPointF(d.x() * cs - d.y() * sn, d.x() * sn + d.y() * cs);
        }
    }

    return path;
}

PathFunctionEditor::PathFunctionEditor(QWidget* parent, PathFunction* function, Doc* doc)
    : QWidget(parent)
    , m_function(function)
    , m_doc(doc)
    , m_nameEdit(NULL)
    , m_view(NULL)
    , m_fixtures(NULL)
    , m_presetCombo(NULL)
    , m_zSpin(NULL)
    , m_durationSpin(NULL)
    , m_easingCombo(NULL)
    , m_modeCombo(NULL)
    , m_invertCheck(NULL)
    , m_manualSlider(NULL)
    , m_manualLabel(NULL)
    , m_previewTimer(NULL)
    , m_previewT(0.0)
    , m_loading(true)
    , m_shapeActive(false)
    , m_shapeRotation(0.0)
{
    QVBoxLayout *main = new QVBoxLayout(this);

    QHBoxLayout *nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(tr("Name:"), this));
    m_nameEdit = new QLineEdit(function->name(), this);
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        m_function->setName(text);
    });
    nameRow->addWidget(m_nameEdit, 1);
    main->addLayout(nameRow);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    main->addWidget(splitter, 1);

    // Stage map / path drawing
    QGroupBox *mapBox = new QGroupBox(tr("Draw the path (hold and drag the mouse)"), splitter);
    QVBoxLayout *mapLayout = new QVBoxLayout(mapBox);
    m_view = new StageGeometryView(mapBox);
    m_view->setResizable(false);
    m_view->setFitPadding(4);
    m_view->setFitMargin(0.02);
    m_view->setPathDrawingEnabled(true);
    connect(m_view, &StageGeometryView::pathChanged, this, &PathFunctionEditor::slotPathChanged);
    connect(m_view, &StageGeometryView::editableBoxChanged, this, &PathFunctionEditor::slotBoxChanged);
    connect(m_view, &StageGeometryView::editableBoxRotated, this, &PathFunctionEditor::slotBoxRotated);
    mapLayout->addWidget(m_view);
    QPushButton *clearBtn = new QPushButton(tr("Clear path"), mapBox);
    connect(clearBtn, &QPushButton::clicked, this, &PathFunctionEditor::slotClearPath);
    mapLayout->addWidget(clearBtn);
    splitter->addWidget(mapBox);

    // Settings panel
    QWidget *right = new QWidget(splitter);
    QVBoxLayout *rightLayout = new QVBoxLayout(right);

    QGroupBox *fixtureBox = new QGroupBox(tr("Lamps following the path"), right);
    QVBoxLayout *fixtureLayout = new QVBoxLayout(fixtureBox);
    m_fixtures = new QTableWidget(fixtureBox);
    m_fixtures->setColumnCount(3);
    m_fixtures->setHorizontalHeaderLabels(QStringList() << tr("Lamp") << tr("Invert") << tr("Offset (°)"));
    m_fixtures->verticalHeader()->setVisible(false);
    m_fixtures->horizontalHeader()->setStretchLastSection(true);
    m_fixtures->setColumnWidth(0, 130);
    m_fixtures->setColumnWidth(1, 50);
    connect(m_fixtures, &QTableWidget::itemChanged, this, &PathFunctionEditor::slotApply);
    fixtureLayout->addWidget(m_fixtures);
    rightLayout->addWidget(fixtureBox, 1);

    QFormLayout *form = new QFormLayout();

    m_presetCombo = new QComboBox(right);
    m_presetCombo->addItem(tr("(free drawing)"));
    m_presetCombo->addItem(tr("Circle on stage"));
    m_presetCombo->addItem(tr("Stage edge"));
    m_presetCombo->addItem(tr("Room edge"));
    m_presetCombo->addItem(tr("Line"));
    m_presetCombo->addItem(tr("Circle"));
    m_presetCombo->addItem(tr("Semicircle"));
    m_presetCombo->addItem(tr("Triangle"));
    m_presetCombo->addItem(tr("Rectangle"));
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PathFunctionEditor::slotPresetChanged);
    form->addRow(tr("Preset:"), m_presetCombo);

    m_zSpin = new QDoubleSpinBox(right);
    m_zSpin->setRange(-1000.0, 1000.0);
    m_zSpin->setDecimals(2);
    m_zSpin->setSuffix(tr(" m"));
    connect(m_zSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PathFunctionEditor::slotApply);
    form->addRow(tr("Target height (z):"), m_zSpin);

    m_durationSpin = new QDoubleSpinBox(right);
    m_durationSpin->setRange(0.1, 3600.0);
    m_durationSpin->setDecimals(2);
    m_durationSpin->setSuffix(tr(" s"));
    connect(m_durationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PathFunctionEditor::slotApply);
    form->addRow(tr("Cycle time:"), m_durationSpin);

    m_easingCombo = new QComboBox(right);
    m_easingCombo->addItem(tr("Linear"));
    m_easingCombo->addItem(tr("Ease in"));
    m_easingCombo->addItem(tr("Ease out"));
    m_easingCombo->addItem(tr("Ease in/out"));
    connect(m_easingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PathFunctionEditor::slotApply);
    form->addRow(tr("Easing:"), m_easingCombo);

    m_modeCombo = new QComboBox(right);
    m_modeCombo->addItem(tr("Repeat"));
    m_modeCombo->addItem(tr("Forward - Back"));
    m_modeCombo->addItem(tr("Once"));
    m_modeCombo->addItem(tr("Manual"));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PathFunctionEditor::slotApply);
    form->addRow(tr("Run mode:"), m_modeCombo);

    m_invertCheck = new QCheckBox(tr("Invert direction"), right);
    connect(m_invertCheck, &QCheckBox::toggled, this, &PathFunctionEditor::slotApply);
    form->addRow(QString(), m_invertCheck);

    rightLayout->addLayout(form);

    QGroupBox *manualBox = new QGroupBox(tr("Manual position"), right);
    QVBoxLayout *manualLayout = new QVBoxLayout(manualBox);
    m_manualSlider = new QSlider(Qt::Horizontal, manualBox);
    m_manualSlider->setRange(0, 1000);
    connect(m_manualSlider, &QSlider::valueChanged, this, &PathFunctionEditor::slotApply);
    manualLayout->addWidget(m_manualSlider);
    m_manualLabel = new QLabel(manualBox);
    manualLayout->addWidget(m_manualLabel);
    rightLayout->addWidget(manualBox);

    rightLayout->addStretch();

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 0);

    // Populate from the function
    m_path = function->path();
    m_zSpin->setValue(function->pathZ());
    m_durationSpin->setValue(function->duration() / 1000.0);
    m_easingCombo->setCurrentIndex(int(function->easing()));
    m_modeCombo->setCurrentIndex(int(function->pathMode()));
    m_invertCheck->setChecked(function->inverted());
    m_manualSlider->setValue(int(function->manualPosition() * 1000.0));

    StageGeometry *sg = StageGeometry::instance();
    QList<quint32> controlled;
    for (const PathFunction::PathFixture& pf : function->fixtures())
        controlled << pf.id;

    if (sg != NULL)
    {
        m_fixtures->blockSignals(true);
        for (quint32 id : sg->calibratedFixtureIds())
        {
            int row = m_fixtures->rowCount();
            m_fixtures->insertRow(row);

            QTableWidgetItem *nameItem = new QTableWidgetItem(sg->fixtureName(id));
            nameItem->setData(Qt::UserRole, id);
            nameItem->setFlags(nameItem->flags() | Qt::ItemIsUserCheckable);
            nameItem->setCheckState(controlled.contains(id) ? Qt::Checked : Qt::Unchecked);
            m_fixtures->setItem(row, 0, nameItem);

            bool inv = false;
            double off = 0.0;
            for (const PathFunction::PathFixture& pfx : function->fixtures())
            {
                if (pfx.id == id)
                {
                    inv = pfx.inverted;
                    off = pfx.offsetDeg;
                }
            }

            QTableWidgetItem *invItem = new QTableWidgetItem();
            invItem->setFlags(invItem->flags() | Qt::ItemIsUserCheckable);
            invItem->setCheckState(inv ? Qt::Checked : Qt::Unchecked);
            m_fixtures->setItem(row, 1, invItem);

            QDoubleSpinBox *offSpin = new QDoubleSpinBox(m_fixtures);
            offSpin->setRange(0.0, 360.0);
            offSpin->setDecimals(1);
            offSpin->setValue(off);
            connect(offSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &PathFunctionEditor::slotApply);
            m_fixtures->setCellWidget(row, 2, offSpin);
        }
        m_fixtures->blockSignals(false);
    }

    m_manualSlider->setEnabled(function->pathMode() == PathFunction::Manual);
    m_manualLabel->setText(tr("Position: %1").arg(m_manualSlider->value() / 1000.0, 0, 'f', 3));

    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout, this, &PathFunctionEditor::slotPreviewTick);
    m_previewTimer->start(40);

    m_loading = false;
    updateView();
}

PathFunctionEditor::~PathFunctionEditor()
{
}

void PathFunctionEditor::updateView()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL || m_view == NULL)
        return;

    StageGeom::Stage st = sg->stage();
    StageGeom::Room r = sg->room();
    m_view->setStage(st.sx(), st.sy());
    m_view->setRoom(r.minX(), r.maxX(), r.minY(), r.maxY());
    m_view->setPath(m_path);

    QList<StageGeometryView::Marker> markers;
    for (int row = 0; row < m_fixtures->rowCount(); row++)
    {
        QTableWidgetItem *item = m_fixtures->item(row, 0);
        if (item == NULL)
            continue;
        quint32 id = item->data(Qt::UserRole).toUInt();
        double x = 0, y = 0, z = 0;
        if (!sg->fixturePosition(id, x, y, z))
            continue;
        StageGeometryView::Marker mk;
        mk.pos = QPointF(x, y);
        mk.name = sg->fixtureName(id);
        mk.selected = item->checkState() == Qt::Checked;
        mk.color = mk.selected ? QColor(255, 220, 0) : QColor(140, 140, 140);
        mk.calibrated = true;
        markers << mk;
    }
    m_view->setMarkers(markers);
}

void PathFunctionEditor::slotPathChanged(const QPolygonF& path)
{
    m_path = path;
    m_view->setPath(m_path);
    slotApply();
}

void PathFunctionEditor::slotClearPath()
{
    m_path.clear();
    m_view->setPath(m_path);
    m_shapeActive = false;
    m_view->setEditableBox(QRectF(), false);
    m_view->setPathDrawingEnabled(true);
    slotApply();
}

void PathFunctionEditor::slotPresetChanged(int index)
{
    Q_UNUSED(index)
    generatePreset();
}

void PathFunctionEditor::slotBoxChanged(const QRectF& box)
{
    m_shapeBox = box;
    if (m_shapeActive)
        generatePreset();
}

void PathFunctionEditor::slotBoxRotated(double degrees)
{
    m_shapeRotation = degrees;
    if (m_shapeActive)
        generatePreset();
}

void PathFunctionEditor::generatePreset()
{
    StageGeometry *sg = StageGeometry::instance();
    if (sg == NULL || m_view == NULL)
        return;

    StageGeom::Stage st = sg->stage();
    StageGeom::Room r = sg->room();
    int preset = m_presetCombo->currentIndex();
    const int N = 48;
    QPolygonF path;

    switch (preset)
    {
    case 1: // Circle on stage
    {
        double cx = st.sx() / 2.0, cy = st.sy() / 2.0;
        double rad = 0.45 * qMin(st.sx(), st.sy());
        for (int i = 0; i <= N; i++)
        {
            double a = 2.0 * M_PI * i / N;
            path << QPointF(cx + rad * qCos(a), cy + rad * qSin(a));
        }
        m_shapeActive = false;
        break;
    }
    case 2: // Stage edge
        path << QPointF(0, 0) << QPointF(st.sx(), 0) << QPointF(st.sx(), st.sy())
             << QPointF(0, st.sy()) << QPointF(0, 0);
        m_shapeActive = false;
        break;
    case 3: // Room edge
        path << QPointF(r.minX(), r.minY()) << QPointF(r.maxX(), r.minY())
             << QPointF(r.maxX(), r.maxY()) << QPointF(r.minX(), r.maxY())
             << QPointF(r.minX(), r.minY());
        m_shapeActive = false;
        break;
    default:
        break;
    }

    if (preset >= 4)
    {
        if (!m_shapeActive)
        {
            double w = 0.6 * st.sx();
            double h = 0.6 * st.sy();
            m_shapeBox = QRectF(st.sx() / 2.0 - w / 2.0, st.sy() / 2.0 - h / 2.0, w, h);
            m_shapeRotation = 0.0;
            m_shapeActive = true;
        }
        path = makeShape(preset, m_shapeBox, m_shapeRotation);
    }

    m_path = path;
    m_view->setPath(m_path);
    m_view->setEditableBox(m_shapeBox, m_shapeActive);
    m_view->setEditableBoxEnabled(m_shapeActive);
    m_view->setEditableBoxRotation(m_shapeRotation);
    m_view->setPathDrawingEnabled(!m_shapeActive);
    slotApply();
}

double PathFunctionEditor::previewParam() const
{
    int mode = m_modeCombo->currentIndex();
    double t = 0.0;

    if (mode == int(PathFunction::Manual))
    {
        t = m_manualSlider->value() / 1000.0;
    }
    else
    {
        double dur = m_durationSpin->value();
        if (dur <= 0.0)
            dur = 1.0;
        double cycle = m_previewT / dur;
        if (mode == int(PathFunction::Once))
            t = qMin(cycle, 1.0);
        else if (mode == int(PathFunction::PingPong))
        {
            double ph = std::fmod(cycle, 2.0);
            t = ph <= 1.0 ? ph : 2.0 - ph;
        }
        else
            t = cycle - std::floor(cycle);
    }

    t = qBound(0.0, t, 1.0);
    switch (m_easingCombo->currentIndex())
    {
    case 1: t = t * t; break;
    case 2: t = t * (2.0 - t); break;
    case 3: t = t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t; break;
    default: break;
    }

    if (m_invertCheck != NULL && m_invertCheck->isChecked())
        t = 1.0 - t;

    return t;
}

void PathFunctionEditor::slotPreviewTick()
{
    if (m_modeCombo->currentIndex() != int(PathFunction::Manual))
        m_previewT += 0.04;

    // Base parameter of the function (function-level invert already applied).
    double t = previewParam();

    // Show one spot per selected lamp, honouring its per-lamp invert/offset.
    QList<QPointF> spots;
    for (int row = 0; row < m_fixtures->rowCount(); row++)
    {
        QTableWidgetItem *nameItem = m_fixtures->item(row, 0);
        if (nameItem == NULL || nameItem->checkState() != Qt::Checked)
            continue;

        QTableWidgetItem *invItem = m_fixtures->item(row, 1);
        bool inv = (invItem != NULL && invItem->checkState() == Qt::Checked);
        QDoubleSpinBox *offSpin = qobject_cast<QDoubleSpinBox *>(m_fixtures->cellWidget(row, 2));
        double off = (offSpin != NULL) ? offSpin->value() : 0.0;

        double local = t + off / 360.0;
        local -= std::floor(local);
        if (inv)
            local = 1.0 - local;

        spots << m_function->pointAt(local);
    }

    // With no lamp selected, still preview the bare path.
    if (spots.isEmpty())
        spots << m_function->pointAt(t);

    m_view->setSpots(spots);
}

void PathFunctionEditor::slotApply()
{
    if (m_loading)
        return;

    StageGeometry *sg = StageGeometry::instance();

    QList<PathFunction::PathFixture> list;
    for (int row = 0; row < m_fixtures->rowCount(); row++)
    {
        QTableWidgetItem *nameItem = m_fixtures->item(row, 0);
        if (nameItem == NULL || nameItem->checkState() != Qt::Checked || sg == NULL)
            continue;
        quint32 id = nameItem->data(Qt::UserRole).toUInt();
        PathFunction::PathFixture pf;
        pf.id = id;
        if (!sg->fixtureAimData(id, pf.x, pf.y, pf.z, pf.panK, pf.panT,
                                pf.tiltK, pf.tiltT, pf.panCh, pf.tiltCh, pf.dimCh))
            continue;
        QTableWidgetItem *invItem = m_fixtures->item(row, 1);
        pf.inverted = (invItem != NULL && invItem->checkState() == Qt::Checked);
        QDoubleSpinBox *offSpin = qobject_cast<QDoubleSpinBox *>(m_fixtures->cellWidget(row, 2));
        pf.offsetDeg = (offSpin != NULL) ? offSpin->value() : 0.0;
        list << pf;
    }

    m_function->setPath(m_path);
    m_function->setPathZ(m_zSpin->value());
    m_function->setFixtures(list);
    m_function->setDuration(uint(m_durationSpin->value() * 1000.0));
    m_function->setEasing(PathFunction::Easing(m_easingCombo->currentIndex()));
    m_function->setPathMode(PathFunction::PathMode(m_modeCombo->currentIndex()));
    m_function->setManualPosition(m_manualSlider->value() / 1000.0);
    m_function->setInverted(m_invertCheck->isChecked());

    m_manualSlider->setEnabled(m_modeCombo->currentIndex() == int(PathFunction::Manual));
    m_manualLabel->setText(tr("Position: %1").arg(m_manualSlider->value() / 1000.0, 0, 'f', 3));

    updateView();
}
