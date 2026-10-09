/*
  Q Light Controller Plus
  vcstagemap.h

  A Virtual Console widget that shows the Stage Geometry stage/room as a
  clickable 2D map, with a dynamic lamp selector at the bottom. Clicking the
  map aims the selected calibrated lamps at that point (through the Simple
  Desk).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef VCSTAGEMAP_H
#define VCSTAGEMAP_H

#include <QHash>
#include <QList>
#include <QPointF>

#include "vcwidget.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class StageGeometryView;
class QToolButton;
class QTimer;
class FlowLayout;
class QSlider;
class QLabel;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCStageMap       QStringLiteral("StageMap")
#define KXMLQLCVCStageMapLamps  QStringLiteral("Lamps")
#define KXMLQLCVCStageMapLamp   QStringLiteral("Lamp")
#define KXMLQLCVCStageMapHeight QStringLiteral("Height")
#define KXMLQLCVCStageMapStretch QStringLiteral("Stretch")
#define KXMLQLCVCStageMapXInput QStringLiteral("XInput")
#define KXMLQLCVCStageMapYInput QStringLiteral("YInput")
#define KXMLQLCVCStageMapZInput QStringLiteral("ZInput")
#define KXMLQLCVCStageMapInvertX QStringLiteral("InvertX")
#define KXMLQLCVCStageMapInvertY QStringLiteral("InvertY")
#define KXMLQLCVCStageMapInvertZ QStringLiteral("InvertZ")

class VCStageMap final : public VCWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(VCStageMap)

public:
    VCStageMap(QWidget *parent, Doc *doc);
    ~VCStageMap();

    static const quint8 xInputSourceId;
    static const quint8 yInputSourceId;
    static const quint8 zInputSourceId;

    /*************************************************************************
     * VCWidget interface
     *************************************************************************/
public:
    void enableWidgetUI(bool enable) override;
    VCWidget *createCopy(VCWidget *parent) const override;
    void editProperties() override;
    void updateFeedback() override;
    void setCaption(const QString &text) override;

protected:
    bool copyFrom(const VCWidget *widget) override;

public:
    /*************************************************************************
     * Load & Save
     *************************************************************************/
public:
    bool loadXML(QXmlStreamReader &root) override;
    bool saveXML(QXmlStreamWriter *doc) override;

protected slots:
    void slotModeChanged(Doc::Mode mode) override;

private slots:
    void slotStageChanged();
    void slotFixturesChanged();
    void slotViewClicked(QPointF world);
    void slotLampToggled(bool checked);
    void slotHeightChanged(int value);
    void slotPreview();
    void slotClearAim();

protected slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value) override;

private:
    void rebuildLamps();
    void updateView();
    void updatePosition();
    void applyTarget();
    QList<quint32> selectedLampIds() const;

private:
    StageGeometryView *m_view;
    QLabel *m_captionLabel;
    QSlider *m_height;
    QLabel *m_heightLabel;
    QToolButton *m_clearButton;
    QWidget *m_lampsHost;
    FlowLayout *m_lampsLayout;

    QHash<quint32, QToolButton *> m_lamps;   // fixture id -> toggle button
    QList<quint32> m_selected;               // persisted selection

    bool m_stretch;                          // stretch-to-fit vs scale-to-fit

    bool m_hasTarget;
    QPointF m_target;
    double m_targetZ;

    // external input steering
    uchar m_xInput;
    uchar m_yInput;
    uchar m_zInput;
    bool m_invertX;
    bool m_invertY;
    bool m_invertZ;

    QTimer* m_previewTimer;
};

/** @} */

#endif // VCSTAGEMAP_H
