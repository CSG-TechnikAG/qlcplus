/*
  Q Light Controller Plus
  trusspanel.h

  Configuration panel for the 3D Preview trusses: add/remove trusses and edit
  their type, size, position, rotation and colour. Also unlink pieces.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TRUSSPANEL_H
#define TRUSSPANEL_H

#include <QWidget>

#include "truss.h"

class TrussScene;
class QListWidget;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QWidget;

class TrussPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TrussPanel(TrussScene *scene, QWidget *parent = NULL);

    int selectedId() const;
    void setSelectedId(int id);

    /** Height of the room floor, used by "Place on floor". */
    void setFloorZ(double z);

signals:
    void selectionChanged(int id);

private slots:
    void slotSceneChanged();
    void slotAdd();
    void slotRemove();
    void slotUnlink();
    void slotPlaceOnFloor();
    void slotSelectionChanged();
    void slotFieldChanged();
    void slotColorClicked();

private:
    void rebuildList();
    void loadFields();
    void updateColorButton();
    int currentIndex() const;

    TrussScene *m_scene;
    QListWidget *m_list;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_unlinkButton;
    QPushButton *m_floorButton;
    QWidget *m_form;
    QComboBox *m_typeCombo;
    QComboBox *m_profileCombo;
    QDoubleSpinBox *m_lengthSpin;
    QDoubleSpinBox *m_widthSpin;
    QDoubleSpinBox *m_heightSpin;
    QDoubleSpinBox *m_baySpin;
    QDoubleSpinBox *m_xSpin;
    QDoubleSpinBox *m_ySpin;
    QDoubleSpinBox *m_zSpin;
    QDoubleSpinBox *m_rotSpin;
    QCheckBox *m_visibleCheck;
    QCheckBox *m_verticalCheck;
    QPushButton *m_colorButton;
    bool m_updating;
    double m_floorZ;
};

#endif // TRUSSPANEL_H
