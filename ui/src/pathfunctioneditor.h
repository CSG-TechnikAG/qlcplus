/*
  Q Light Controller Plus
  pathfunctioneditor.h

  Editor for the Path Function: draw a path on the stage map and choose the
  calibrated lamps that follow it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PATHFUNCTIONEDITOR_H
#define PATHFUNCTIONEDITOR_H

#include <QWidget>
#include <QPolygonF>

class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QDoubleSpinBox;
class QComboBox;
class QSlider;
class QLabel;
class QLineEdit;
class QTimer;
class QCheckBox;
class StageGeometryView;
class PathFunction;
class Doc;

class PathFunctionEditor : public QWidget
{
    Q_OBJECT

public:
    PathFunctionEditor(QWidget* parent, PathFunction* function, Doc* doc);
    ~PathFunctionEditor();

private slots:
    void slotPathChanged(const QPolygonF& path);
    void slotClearPath();
    void slotApply();
    void slotPreviewTick();
    void slotPresetChanged(int index);
    void slotBoxChanged(const QRectF& box);
    void slotBoxRotated(double degrees);

private:
    void updateView();
    double previewParam() const;
    void generatePreset();

    PathFunction* m_function;
    Doc* m_doc;
    QLineEdit* m_nameEdit;
    StageGeometryView* m_view;
    QTableWidget* m_fixtures;
    QComboBox* m_presetCombo;
    QDoubleSpinBox* m_zSpin;
    QDoubleSpinBox* m_durationSpin;
    QComboBox* m_easingCombo;
    QComboBox* m_modeCombo;
    QCheckBox* m_invertCheck;
    QSlider* m_manualSlider;
    QLabel* m_manualLabel;
    QTimer* m_previewTimer;
    double m_previewT;
    bool m_loading;
    QPolygonF m_path;
    QRectF m_shapeBox;
    bool m_shapeActive;
    double m_shapeRotation;
};

#endif // PATHFUNCTIONEDITOR_H
