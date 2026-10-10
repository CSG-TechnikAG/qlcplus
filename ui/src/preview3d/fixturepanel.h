/*
  Q Light Controller Plus
  fixturepanel.h

  Fixture mode panel for the 3D Preview: lists every placed dimmer/LED and lets
  you move it (x/y/z), rotate it (yaw/pitch) and hide it. Horizontal placement
  and yaw stay in sync with the Fixture Monitor.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREPANEL_H
#define FIXTUREPANEL_H

#include <QWidget>

class FixtureScene;
class QListWidget;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;

class FixturePanel final : public QWidget
{
    Q_OBJECT

public:
    explicit FixturePanel(FixtureScene *scene, QWidget *parent = NULL);

    int selectedId() const;
    void setSelectedId(int id);

    /** Room floor height: the lowest placeable fixture height. */
    void setFloorZ(double z);

signals:
    void selectionChanged(int id);

private slots:
    void slotSceneChanged();
    void slotSelectionChanged();
    void slotFieldChanged();

private:
    void rebuildList();
    void loadFields();
    int currentIndex() const;

    FixtureScene *m_scene;
    QListWidget *m_list;
    QDoubleSpinBox *m_xSpin;
    QDoubleSpinBox *m_ySpin;
    QDoubleSpinBox *m_zSpin;
    QDoubleSpinBox *m_yawSpin;
    QDoubleSpinBox *m_pitchSpin;
    QCheckBox *m_visibleCheck;
    int m_updating;
    bool m_applying;
};

#endif // FIXTUREPANEL_H
