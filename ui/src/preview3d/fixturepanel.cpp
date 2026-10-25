/*
  Q Light Controller Plus
  fixturepanel.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <cstdio>
#include <cstdio>

#include "fixturepanel.h"
#include "fixturescene.h"

FixturePanel::FixturePanel(FixtureScene *scene, QWidget *parent)
    : QWidget(parent)
    , m_scene(scene)
    , m_list(NULL)
    , m_xSpin(NULL)
    , m_ySpin(NULL)
    , m_zSpin(NULL)
    , m_yawSpin(NULL)
    , m_pitchSpin(NULL)
    , m_visibleCheck(NULL)
    , m_updating(0)
    , m_applying(false)
{
    setMinimumWidth(250);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel *title = new QLabel(tr("Fixtures"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    QLabel *hint = new QLabel(tr("Drag a fixture in the view to move it. "
                                 "X/Y and yaw are shared with the Fixture Monitor."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hint);

    QWidget *form = new QWidget(this);
    QFormLayout *fl = new QFormLayout(form);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeSpin = [form](double lo, double hi, double step, const QString &suffix) {
        QDoubleSpinBox *sb = new QDoubleSpinBox(form);
        sb->setRange(lo, hi);
        sb->setSingleStep(step);
        sb->setDecimals(2);
        sb->setSuffix(suffix);
        return sb;
    };

    m_xSpin = makeSpin(-50.0, 50.0, 0.1, tr(" m"));
    m_ySpin = makeSpin(-50.0, 50.0, 0.1, tr(" m"));
    m_zSpin = makeSpin(-50.0, 20.0, 0.1, tr(" m"));
    m_yawSpin = makeSpin(-180.0, 180.0, 5.0, tr(" \u00b0"));
    m_pitchSpin = makeSpin(-90.0, 90.0, 5.0, tr(" \u00b0"));

    fl->addRow(tr("X"), m_xSpin);
    fl->addRow(tr("Y"), m_ySpin);
    fl->addRow(tr("Height"), m_zSpin);
    fl->addRow(tr("Yaw"), m_yawSpin);
    fl->addRow(tr("Pitch"), m_pitchSpin);

    m_visibleCheck = new QCheckBox(tr("Visible"), form);
    fl->addRow(QString(), m_visibleCheck);

    layout->addWidget(form);

    connect(m_list, &QListWidget::currentRowChanged, this, &FixturePanel::slotSelectionChanged);
    connect(m_visibleCheck, &QCheckBox::toggled, this, &FixturePanel::slotFieldChanged);
    for (QDoubleSpinBox *sb : { m_xSpin, m_ySpin, m_zSpin, m_yawSpin, m_pitchSpin })
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &FixturePanel::slotFieldChanged);

    if (m_scene != NULL)
        connect(m_scene, &FixtureScene::changed, this, &FixturePanel::slotSceneChanged);

    rebuildList();
}

int FixturePanel::currentIndex() const
{
    return m_list->currentRow();
}

int FixturePanel::selectedId() const
{
    const int idx = currentIndex();
    if (m_scene == NULL || idx < 0 || idx >= m_scene->fixtures().size())
        return -1;
    return int(m_scene->fixtures().at(idx).id);
}

void FixturePanel::setSelectedId(int id)
{
    if (m_scene == NULL)
        return;
    if (id < 0)
    {
        m_updating++;
        m_list->setCurrentRow(-1);
        m_updating--;
        loadFields();
        return;
    }
    for (int i = 0; i < m_scene->fixtures().size(); ++i)
    {
        if ((int)m_scene->fixtures().at(i).id == id)
        {
            m_updating++;
            m_list->setCurrentRow(i);
            m_updating--;
            loadFields();
            return;
        }
    }
}

void FixturePanel::setFloorZ(double z)
{
    if (m_zSpin != NULL)
        m_zSpin->setMinimum(z);
}

void FixturePanel::slotSceneChanged()
{
    if (m_scene == NULL || m_applying)
        return;

    if (m_scene->fixtures().size() == m_list->count())
    {
        m_updating++;
        const QList<FixtureInstance> &fs = m_scene->fixtures();
        for (int i = 0; i < fs.size(); ++i)
            if (QListWidgetItem *item = m_list->item(i))
                item->setText(tr("%1: %2").arg(i + 1).arg(fs.at(i).name));
        m_updating--;
        loadFields();
        return;
    }

    rebuildList();
}

void FixturePanel::rebuildList()
{
    if (m_scene == NULL)
        return;

    m_updating++;
    const int keep = m_list->currentRow();
    m_list->clear();
    const QList<FixtureInstance> &fs = m_scene->fixtures();
    for (int i = 0; i < fs.size(); ++i)
        m_list->addItem(tr("%1: %2").arg(i + 1).arg(fs.at(i).name));
    if (keep >= 0 && keep < m_list->count())
        m_list->setCurrentRow(keep);
    else if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    m_updating--;

    loadFields();
}

void FixturePanel::loadFields()
{
    if (m_scene == NULL)
        return;

    const int idx = currentIndex();
    const bool has = (idx >= 0 && idx < m_scene->fixtures().size());

    if (!has)
        return;

    const FixtureInstance &fi = m_scene->fixtures().at(idx);

    m_updating++;
    m_xSpin->setValue(fi.pos.x());
    m_ySpin->setValue(fi.pos.y());
    m_zSpin->setValue(fi.pos.z());
    m_yawSpin->setValue(fi.yaw);
    m_pitchSpin->setValue(fi.pitch);
    m_visibleCheck->setChecked(fi.visible);
    m_updating--;
}

void FixturePanel::slotSelectionChanged()
{
    if (m_updating > 0)
        return;
    loadFields();
    emit selectionChanged(selectedId());
}

void FixturePanel::slotFieldChanged()
{
    if (m_updating > 0 || m_scene == NULL)
        return;

    const int id = selectedId();
    if (id < 0)
        return;

    // Read every field first, then block the widgets while pushing the change:
    // the scene emits changed() which reloads the fields, and without blocking
    // that reload would feed straight back into this slot.
    const double x = m_xSpin->value();
    const double y = m_ySpin->value();
    const double z = m_zSpin->value();
    const double yaw = m_yawSpin->value();
    const double pitch = m_pitchSpin->value();
    const bool visible = m_visibleCheck->isChecked();

    const FixtureInstance *cur = m_scene->instance(id);
    const bool wasVisible = (cur != NULL) ? cur->visible : visible;

    m_applying = true;
    m_scene->setPosition(id, QVector3D(x, y, z));
    m_scene->setOrientation(id, yaw, pitch);
    // Only touch the Monitor placement when the state actually changed,
    // otherwise hiding/unplacing would wipe the position and rotation.
    if (visible != wasVisible)
        m_scene->setVisible(id, visible);
    m_applying = false;
}
