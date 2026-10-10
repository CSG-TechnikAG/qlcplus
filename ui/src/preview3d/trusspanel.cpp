/*
  Q Light Controller Plus
  trusspanel.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QFrame>

#include "trusspanel.h"
#include "trussscene.h"

TrussPanel::TrussPanel(TrussScene *scene, QWidget *parent)
    : QWidget(parent)
    , m_scene(scene)
    , m_list(NULL)
    , m_addButton(NULL)
    , m_removeButton(NULL)
    , m_unlinkButton(NULL)
    , m_floorButton(NULL)
    , m_form(NULL)
    , m_typeCombo(NULL)
    , m_profileCombo(NULL)
    , m_lengthSpin(NULL)
    , m_widthSpin(NULL)
    , m_heightSpin(NULL)
    , m_baySpin(NULL)
    , m_xSpin(NULL)
    , m_ySpin(NULL)
    , m_zSpin(NULL)
    , m_rotSpin(NULL)
    , m_visibleCheck(NULL)
    , m_verticalCheck(NULL)
    , m_colorButton(NULL)
    , m_updating(false)
    , m_floorZ(0.0)
{
    setMinimumWidth(250);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel *title = new QLabel(tr("Trusses"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    QHBoxLayout *buttons = new QHBoxLayout();
    m_addButton = new QPushButton(tr("Add"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_removeButton);
    layout->addLayout(buttons);

    m_unlinkButton = new QPushButton(tr("Unlink"), this);
    layout->addWidget(m_unlinkButton);

    m_floorButton = new QPushButton(tr("Place on floor"), this);
    m_floorButton->setToolTip(tr("Drop the piece (and everything linked to it) onto the floor"));
    layout->addWidget(m_floorButton);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    QLabel *hint = new QLabel(tr("Drag a truss to move it, drag from a ring to "
                                 "link pieces."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hint);

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    m_form = new QWidget(this);
    QFormLayout *form = new QFormLayout(m_form);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_typeCombo = new QComboBox(m_form);
    for (Truss::Type t : Truss::types())
        m_typeCombo->addItem(Truss::typeName(t), int(t));
    form->addRow(tr("Type"), m_typeCombo);

    m_profileCombo = new QComboBox(m_form);
    m_profileCombo->addItem(Truss::typeName(Truss::Box), int(Truss::Box));
    m_profileCombo->addItem(Truss::typeName(Truss::Triangle), int(Truss::Triangle));
    m_profileCombo->addItem(Truss::typeName(Truss::Ladder), int(Truss::Ladder));
    m_profileCombo->setToolTip(tr("Cross-section used by junction pieces"));
    form->addRow(tr("Profile"), m_profileCombo);

    auto makeSpin = [this](double lo, double hi, double step, const QString &suffix) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(m_form);
        sb->setRange(lo, hi);
        sb->setSingleStep(step);
        sb->setDecimals(2);
        sb->setSuffix(suffix);
        return sb;
    };

    m_lengthSpin = makeSpin(0.1, 100.0, 0.5, tr(" m"));
    m_widthSpin = makeSpin(0.05, 2.0, 0.05, tr(" m"));
    m_heightSpin = makeSpin(0.05, 2.0, 0.05, tr(" m"));
    m_baySpin = makeSpin(0.1, 5.0, 0.1, tr(" m"));
    m_xSpin = makeSpin(-50.0, 50.0, 0.1, tr(" m"));
    m_ySpin = makeSpin(-50.0, 50.0, 0.1, tr(" m"));
    m_zSpin = makeSpin(-5.0, 50.0, 0.1, tr(" m"));
    m_rotSpin = makeSpin(-180.0, 180.0, 5.0, tr(" \u00b0"));

    form->addRow(tr("Length"), m_lengthSpin);
    form->addRow(tr("Width"), m_widthSpin);
    form->addRow(tr("Height"), m_heightSpin);
    form->addRow(tr("Bay length"), m_baySpin);
    form->addRow(tr("X"), m_xSpin);
    form->addRow(tr("Y"), m_ySpin);
    form->addRow(tr("Z"), m_zSpin);
    form->addRow(tr("Rotation"), m_rotSpin);

    m_visibleCheck = new QCheckBox(tr("Visible"), m_form);
    form->addRow(QString(), m_visibleCheck);

    m_verticalCheck = new QCheckBox(tr("Vertical"), m_form);
    m_verticalCheck->setToolTip(tr("Stand the truss on end (for towers and vertical rigs)"));
    form->addRow(QString(), m_verticalCheck);

    m_colorButton = new QPushButton(tr("Colour"), m_form);
    form->addRow(tr("Colour"), m_colorButton);

    layout->addWidget(m_form);

    connect(m_addButton, &QPushButton::clicked, this, &TrussPanel::slotAdd);
    connect(m_removeButton, &QPushButton::clicked, this, &TrussPanel::slotRemove);
    connect(m_unlinkButton, &QPushButton::clicked, this, &TrussPanel::slotUnlink);
    connect(m_floorButton, &QPushButton::clicked, this, &TrussPanel::slotPlaceOnFloor);
    connect(m_list, &QListWidget::currentRowChanged, this, &TrussPanel::slotSelectionChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrussPanel::slotFieldChanged);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrussPanel::slotFieldChanged);
    connect(m_visibleCheck, &QCheckBox::toggled, this, &TrussPanel::slotFieldChanged);
    connect(m_verticalCheck, &QCheckBox::toggled, this, &TrussPanel::slotFieldChanged);
    connect(m_colorButton, &QPushButton::clicked, this, &TrussPanel::slotColorClicked);
    for (QDoubleSpinBox *sb : { m_lengthSpin, m_widthSpin, m_heightSpin, m_baySpin,
                                m_xSpin, m_ySpin, m_zSpin, m_rotSpin })
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &TrussPanel::slotFieldChanged);

    if (m_scene != NULL)
        connect(m_scene, &TrussScene::changed, this, &TrussPanel::slotSceneChanged);

    rebuildList();
}

int TrussPanel::currentIndex() const
{
    return m_list->currentRow();
}

int TrussPanel::selectedId() const
{
    const int idx = currentIndex();
    if (m_scene == NULL || idx < 0 || idx >= m_scene->trusses().size())
        return 0;
    return m_scene->trusses().at(idx).id;
}

void TrussPanel::setSelectedId(int id)
{
    if (m_scene == NULL)
        return;
    for (int i = 0; i < m_scene->trusses().size(); ++i)
    {
        if (m_scene->trusses().at(i).id == id)
        {
            m_updating = true;
            m_list->setCurrentRow(i);
            m_updating = false;
            loadFields();
            return;
        }
    }
}

void TrussPanel::slotSceneChanged()
{
    if (m_scene == NULL)
        return;

    // While dragging, the piece count doesn't change: only refresh the labels
    // and fields instead of rebuilding the whole list every frame.
    if (m_scene->trusses().size() == m_list->count())
    {
        const QList<Truss> &ts = m_scene->trusses();
        m_updating = true;
        for (int i = 0; i < ts.size(); ++i)
        {
            QListWidgetItem *item = m_list->item(i);
            if (item == NULL)
                continue;
            QString label = tr("%1: %2").arg(i + 1).arg(ts.at(i).typeName());
            if (!Truss::isJunction(ts.at(i).type))
                label += tr(" %1 m").arg(ts.at(i).length, 0, 'f', 1);
            if (ts.at(i).isVertical())
                label += tr("  [V]");
            if (m_scene->hasLinks(ts.at(i).id))
                label += tr("  \u26ad");
            item->setText(label);
        }
        m_updating = false;
        loadFields();
        return;
    }

    rebuildList();
}

void TrussPanel::rebuildList()
{
    if (m_scene == NULL)
        return;

    m_updating = true;
    const int keep = m_list->currentRow();
    m_list->clear();
    const QList<Truss> &trusses = m_scene->trusses();
    for (int i = 0; i < trusses.size(); ++i)
    {
        const Truss &t = trusses.at(i);
        QString label = tr("%1: %2").arg(i + 1).arg(t.typeName());
        if (!Truss::isJunction(t.type))
            label += tr(" %1 m").arg(t.length, 0, 'f', 1);
        if (t.isVertical())
            label += tr("  [V]");
        if (m_scene->hasLinks(t.id))
            label += tr("  \u26ad");
        m_list->addItem(label);
    }
    if (keep >= 0 && keep < m_list->count())
        m_list->setCurrentRow(keep);
    else if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    m_updating = false;

    loadFields();
}

void TrussPanel::loadFields()
{
    if (m_scene == NULL)
        return;

    const int idx = currentIndex();
    const bool has = (idx >= 0 && idx < m_scene->trusses().size());

    m_form->setEnabled(has);
    m_removeButton->setEnabled(has);
    m_unlinkButton->setEnabled(has && m_scene->hasLinks(selectedId()));
    if (!has)
        return;

    const Truss &t = m_scene->trusses().at(idx);
    const bool junction = Truss::isJunction(t.type);

    m_updating = true;
    m_typeCombo->setCurrentIndex(m_typeCombo->findData(int(t.type)));
    m_profileCombo->setCurrentIndex(m_profileCombo->findData(int(t.profile)));
    m_lengthSpin->setValue(t.length);
    m_widthSpin->setValue(t.width);
    m_heightSpin->setValue(t.height);
    m_baySpin->setValue(t.bayLength);
    m_xSpin->setValue(t.pos.x());
    m_ySpin->setValue(t.pos.y());
    m_zSpin->setValue(t.pos.z());
    m_rotSpin->setValue(t.yawDegrees());
    m_visibleCheck->setChecked(t.visible);
    m_verticalCheck->setChecked(t.isVertical());
    m_updating = false;

    m_lengthSpin->setEnabled(!junction);
    m_baySpin->setEnabled(!junction);
    m_profileCombo->setEnabled(junction);

    updateColorButton();
}

void TrussPanel::updateColorButton()
{
    const int idx = currentIndex();
    if (m_scene == NULL || idx < 0 || idx >= m_scene->trusses().size())
        return;

    const QColor c = m_scene->trusses().at(idx).color;
    if (m_colorButton->text() == c.name())
        return;
    m_colorButton->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                 .arg(c.name(),
                                      (c.lightness() > 128) ? "#000000" : "#ffffff"));
    m_colorButton->setText(c.name());
}

void TrussPanel::slotAdd()
{
    if (m_scene == NULL)
        return;

    Truss t;
    t.pos = QVector3D(0.0, 0.0, 3.5);
    const int id = m_scene->addTruss(t);
    setSelectedId(id);
    emit selectionChanged(id);
}

void TrussPanel::slotRemove()
{
    if (m_scene == NULL)
        return;
    const int id = selectedId();
    if (id == 0)
        return;
    m_scene->removeTruss(id);
    emit selectionChanged(selectedId());
}

void TrussPanel::slotUnlink()
{
    if (m_scene == NULL)
        return;
    const int id = selectedId();
    if (id == 0)
        return;
    m_scene->unlinkAll(id);
    loadFields();
}

void TrussPanel::slotPlaceOnFloor()
{
    if (m_scene == NULL)
        return;
    const int id = selectedId();
    if (id == 0)
        return;
    m_scene->placeOnFloor(id, m_floorZ);
    loadFields();
}

void TrussPanel::setFloorZ(double z)
{
    m_floorZ = z;
}

void TrussPanel::slotSelectionChanged()
{
    if (m_updating)
        return;
    loadFields();
    emit selectionChanged(selectedId());
}

void TrussPanel::slotFieldChanged()
{
    if (m_updating || m_scene == NULL)
        return;

    const int idx = currentIndex();
    if (idx < 0 || idx >= m_scene->trusses().size())
        return;

    const Truss old = m_scene->trusses().at(idx);

    Truss t = old;
    t.type = Truss::Type(m_typeCombo->currentData().toInt());
    t.profile = Truss::Type(m_profileCombo->currentData().toInt());
    t.length = m_lengthSpin->value();
    t.width = m_widthSpin->value();
    t.height = m_heightSpin->value();
    t.bayLength = m_baySpin->value();
    t.pos = QVector3D(m_xSpin->value(), m_ySpin->value(), m_zSpin->value());
    if (!qFuzzyCompare(m_rotSpin->value(), old.yawDegrees())
        || m_verticalCheck->isChecked() != old.isVertical())
        t.setPose(m_rotSpin->value(), m_verticalCheck->isChecked());
    t.visible = m_visibleCheck->isChecked();

    // Changing the shape/type moves the ports, so drop stale links.
    if (t.type != old.type || !qFuzzyCompare(t.length, old.length))
        m_scene->unlinkAll(t.id);

    m_scene->replaceTruss(t);
    loadFields();
}

void TrussPanel::slotColorClicked()
{
    if (m_scene == NULL)
        return;

    const int idx = currentIndex();
    if (idx < 0 || idx >= m_scene->trusses().size())
        return;

    const QColor c = QColorDialog::getColor(m_scene->trusses().at(idx).color, this, tr("Truss colour"));
    if (!c.isValid())
        return;

    Truss t = m_scene->trusses().at(idx);
    t.color = c;
    m_scene->replaceTruss(t);
    updateColorButton();
}
