/*
  Q Light Controller Plus
  stagegeometry.h

  Stage Geometry tab: moving head calibration & 3D mapping. The stage is
  edited in the plan view, lamps are the QLC+ moving head fixtures and all
  pan/tilt I/O goes through the Simple Desk sliders.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGEOMETRY_H
#define STAGEGEOMETRY_H

#include <QWidget>
#include <QList>
#include <QPointF>
#include <QString>

#include "stagegeometryengine.h"

class StageGeometryView;
class ConsoleChannel;
class QXmlStreamReader;
class QXmlStreamWriter;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QListWidget;
class QHBoxLayout;
class QVBoxLayout;
class QTextEdit;
class QComboBox;
class QCheckBox;
class QTabWidget;
class QSlider;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui_stagegeometry UI
 * @{
 */

class StageGeometry : public QWidget
{
    Q_OBJECT

public:
    StageGeometry(QWidget *parent, Doc *doc);
    ~StageGeometry();

    /** Get the singleton instance */
    static StageGeometry *instance();

    /** Stage footprint (metres) — also drawn in the Fixture Monitor. */
    double stageWidth() const;
    double stageDepth() const;
    double stageOffsetX() const;
    double stageOffsetY() const;
    double stageHeight() const;
    double roomHeight() const;

    /** Geometry setters used by the Fixture Monitor setup fields. */
    void setStageSize(double sx, double sy);
    void setStageOffset(double ox, double oy);
    void setStageHeight(double h);
    void setRoomHeight(double h);

    /** The current room (x/y from the Monitor grid + stage offset, z height). */
    StageGeom::Room room() const;

    /** The stage footprint. */
    StageGeom::Stage stage() const;

    /** Name of a lamp, or an empty string. */
    QString fixtureName(quint32 id) const;

    /** Aim the given calibrated lamps at a world point (writes Simple Desk). */
    void aimAt(const QList<quint32> &ids, double x, double y, double z);

    /** Clear the Simple Desk overrides (manual aim) of the given lamps. */
    void clearAim(const QList<quint32> &ids);

    /** True if any of the given lamps has a manual Simple Desk aim override. */
    bool isAimOverridden(const QList<quint32> &ids) const;

    /** Raw aim data of a calibrated lamp (for the Path Function editor). */
    bool fixtureAimData(quint32 id, double &x, double &y, double &z,
                        double &panK, double &panT, double &tiltK, double &tiltT,
                        int &panCh, int &tiltCh, int &dimCh) const;

    /** Position of a calibrated lamp (stage-relative metres). False if not calibrated. */
    bool fixturePosition(quint32 id, double &x, double &y, double &z) const;

    /** Move a calibrated lamp (from the Fixture Monitor drag). */
    void setFixturePosition(quint32 id, double x, double y);

    /** IDs of the calibrated lamps. */
    QList<quint32> calibratedFixtureIds() const;

    /** Start from scratch; clear everything */
    void clearContents();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

signals:
    /** The stage or room changed (so the Fixture Monitor can redraw). */
    void stageChanged();
    /** A calibrated lamp moved (so the Fixture Monitor can follow it). */
    void fixturePositionChanged(quint32 id, double x, double y, double z);
    /** The set of moving-head lamps changed (added/removed/renamed). */
    void fixturesChanged();

private:
    struct Step
    {
        enum Kind { Front, PanRef, TiltRef, Point };
        Kind kind;
        QString name;
        double angle;
        QPointF point;
    };

    void initUi();
    void initTopBar();
    void initCalibrationTab();
    void initUsageTab();
    void initLiveTab();

    /*********************************************************************
     * State updates
     *********************************************************************/
    void refreshFixtureCombo();
    void rebuildSliders();
    void updateRoomLabel();
    void syncFixtureToMonitor(StageGeometryFixture *f);
    void onFixtureSelected(int index);
    void onGeometryChanged();
    void onFixturesChanged();
    void onUniverseWritten(quint32 idx, const QByteArray &ua);
    void syncSliderValues();

    StageGeometryFixture *activeFixture() const;

    /*********************************************************************
     * Simple Desk bridge
     *********************************************************************/
    void setDeskValue(int address, int value);
    int deskValue(int address) const;
    void slotSliderValueChanged(quint32 fid, quint32 chan, uchar value);

    /*********************************************************************
     * Calibration wizard
     *********************************************************************/
    void buildSteps();
    void showStep();
    void refreshStepList();
    void prevStep();
    void nextStep();
    void selectStep(int index);
    void captureStep();
    void addReferenceValue();
    void clearReference();
    void calibrate();

    bool stepComplete(const Step &step) const;
    QString stepShort(const Step &step) const;
    QString pointLabel(const QPointF &point) const;
    QString frontHint(const Step &step) const;

    /*********************************************************************
     * Usage
     *********************************************************************/
    void refreshUsageList();
    void drawUsage();
    void onUsageClicked(QPointF world);
    void updateTarget();
    void applyTarget(bool silent);
    QList<StageGeometryFixture *> controlledFixtures() const;

    /*********************************************************************
     * Live
     *********************************************************************/
    void applyLiveConfig();
    void toggleLive();
    void liveTick();

private:
    static StageGeometry *s_instance;
    Doc *m_doc;
    StageGeometryEngine *m_engine;
    int m_activeIndex;
    QList<Step> m_steps;
    int m_stepIndex;
    bool m_updating;

    QTabWidget *m_tabs;

    // top bar
    QLabel *m_infoLabel;
    QComboBox *m_fixtureCombo;
    QLabel *m_statusLabel;

    // calibration
    QListWidget *m_stepList;
    StageGeometryView *m_calibView;
    StageGeometryView *m_sideView;
    QLabel *m_stepTitle;
    QLabel *m_stepInstr;
    QLabel *m_frontHint;
    QWidget *m_sliderHost;
    QHBoxLayout *m_sliderLayout;
    ConsoleChannel *m_panSlider;
    ConsoleChannel *m_tiltSlider;
    QPushButton *m_captureButton;
    QPushButton *m_addRefButton;
    QPushButton *m_clearRefButton;
    QLabel *m_refValues;
    QCheckBox *m_confirmCheck;
    QPushButton *m_calibrateButton;
    QLabel *m_calibResult;

    // usage
    StageGeometryView *m_usageView;
    QListWidget *m_usageList;
    QSlider *m_heightSlider;
    QLabel *m_heightLabel;
    QTextEdit *m_usageInfo;
    QCheckBox *m_applyCheck;
    bool m_hasTarget;
    QPointF m_target2d;
    double m_targetZ;
    StageGeom::Room m_cachedRoom;

    // live
    QSpinBox *m_liveUniverse, *m_liveXCh, *m_liveYCh, *m_liveZCh;
    QCheckBox *m_liveAll;
    QPushButton *m_liveButton;
    QLabel *m_liveStatus;
    QTextEdit *m_liveLog;
    QTimer *m_timer;
};

/** @} */

#endif // STAGEGEOMETRY_H
