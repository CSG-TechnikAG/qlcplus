/*
  Q Light Controller Plus
  stagegeometryengine.h

  Show model for the Stage Geometry tab. Lamps are NOT created by hand: they
  are the moving head fixtures patched in QLC+. The pan/tilt (and dimmer)
  channel addresses are gathered automatically from the fixture definition.
  Output is routed through the Simple Desk engine, so aiming and calibration
  drive the very same sliders the Simple Desk tab shows.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGEOMETRYENGINE_H
#define STAGEGEOMETRYENGINE_H

#include <QObject>
#include <QHash>
#include <QMap>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QStringList>
#include <QVector3D>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QByteArray>

#include "stagegeometrymath.h"
#include "stagegeometrysolver.h"

class QLCChannel;
class MasterTimer;
class Universe;
class Fixture;
class Doc;

/** @addtogroup ui_stagegeometry UI
 * @{
 */

#define KXMLQLCStageGeometry          QStringLiteral("StageGeometry")
#define KXMLQLCStageGeometryStage     QStringLiteral("Stage")
#define KXMLQLCStageGeometryRoom      QStringLiteral("Room")
#define KXMLQLCStageGeometryLive      QStringLiteral("Live")
#define KXMLQLCStageGeometryFixtures  QStringLiteral("Fixtures")
#define KXMLQLCStageGeometryFixture   QStringLiteral("Fixture")

/**
 * The calibration state of ONE QLC+ moving head fixture. The fixture itself
 * (name, universe, channel addresses) is owned by QLC+ and re-read whenever
 * the fixture changes; only the calibration input/solution lives here.
 */
class StageGeometryFixture
{
public:
    explicit StageGeometryFixture(quint32 id = 0xFFFFFFFF);

    /** Rebuild the solver input from the raw wizard values. */
    StageGeom::Measurement measurement() const;

    bool isMovingHead() const { return panChannel >= 0 && tiltChannel >= 0; }

    /** Absolute DMX addresses (universe * 512 + address + channel). */
    int panAddress() const { return universe * 512 + baseAddress + panChannel; }
    int tiltAddress() const { return universe * 512 + baseAddress + tiltChannel; }
    int dimmerAddress() const { return dimmerChannel >= 0 ? universe * 512 + baseAddress + dimmerChannel : -1; }

    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

    quint32 id;
    QString name;
    int universe;
    int baseAddress;
    int panChannel;      // relative channel index within the fixture, -1 if none
    int tiltChannel;
    int dimmerChannel;

    bool calibrated;
    StageGeom::Fixture fixture;

    // raw calibration input
    bool hasFront;
    int frontPan;
    int frontTilt;
    QMap<QPointF, QPair<int, int> > pointDmx;
    QMap<QString, QList<int> > panRefs;
    QMap<QString, QList<int> > tiltRefs;
    QSet<QString> confirmed;

    // runtime only: last sent pair, for jump-free tracking
    int lastPan;
    int lastTilt;
};

/**
 * The Stage Geometry model. It owns the stage/room, mirrors the QLC+ moving
 * head fixtures and caches incoming DMX for the live coordinate server.
 */
class StageGeometryEngine : public QObject
{
    Q_OBJECT

public:
    StageGeometryEngine(Doc *doc);
    ~StageGeometryEngine();

    /*********************************************************************
     * Geometry
     *********************************************************************/
    double stageWidth() const;
    double stageDepth() const;
    double stageOffsetX() const;
    double stageOffsetY() const;
    /** Height of the stage above the room floor (metres). The stage plane is
     *  z=0; the room floor is at -stageHeight. */
    double stageHeight() const;
    double roomHeight() const;
    StageGeom::Stage stage() const;
    /** The room: x/y extent comes from the Fixture Monitor grid and the stage
     *  offset, z from the room height. The stage sits at (0,0)..(sx,sy) and
     *  the room is placed around it by the offset. */
    StageGeom::Room room() const;

    /** Set the stage footprint (metres). Emits geometryChanged(). */
    void setStage(double sx, double sy);
    /** Set the stage position inside the room (metres from the room origin).
     *  Emits geometryChanged(). */
    void setStageOffset(double ox, double oy);
    /** Set the stage height above the room floor (metres). Emits geometryChanged(). */
    void setStageHeight(double h);
    /** Set the room height above the stage (z1); the floor is -stageHeight.
     *  Emits geometryChanged(). */
    void setRoomHeight(double z1);

    /*********************************************************************
     * Fixtures (auto-generated from the QLC+ moving heads)
     *********************************************************************/
    int fixtureCount() const;
    StageGeometryFixture *fixtureAt(int index) const;
    StageGeometryFixture *fixtureById(quint32 id) const;

    /**
     * Pick a jump-free (pan, tilt) DMX pair that aims the fixture at a world
     * point, remembering the choice for continuity. Returns (-1,-1) when
     * unreachable. Pure computation: nothing is sent here.
     */
    QPair<int, int> choosePair(StageGeometryFixture *fixture, double x, double y, double z) const;

    /*********************************************************************
     * Live coordinate server
     *********************************************************************/
    void setLive(bool running);
    bool isLive() const;
    void setLiveConfig(int universe, int xCh, int yCh, int zCh, bool all);
    int liveUniverse() const;
    int liveXChannel() const;
    int liveYChannel() const;
    int liveZChannel() const;
    bool liveAllFixtures() const;
    void setTarget(const QVector3D &target);
    QVector3D target() const;
    bool liveTargetFromInput(QVector3D &target) const;

    /*********************************************************************
     * Input
     *********************************************************************/
    int inputValue(int universe, int channel) const;

    /*********************************************************************
     * Reference directions
     *********************************************************************/
    static QStringList panDirectionNames();
    static double panDirectionAngle(const QString &name);
    static QStringList tiltDirectionNames();
    static double tiltDirectionAngle(const QString &name);

    /** True if the fixture has both a Pan and a Tilt channel. */
    static bool isMovingHead(Fixture *fixture);
    /** Relative index of the first channel with the given preset, or -1. */
    static int findChannel(Fixture *fixture, int preset, int finePreset = 0);

    /*********************************************************************
     * Load & Save
     *********************************************************************/
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

signals:
    /** The set of moving head fixtures changed (added/removed/renamed). */
    void fixturesChanged();
    /** Stage or room changed. */
    void geometryChanged();

private slots:
    void slotFixturesChanged();
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key);

private:
    void rebuildFixtures();
    void updateFixtureInfo(StageGeometryFixture *f, Fixture *fx);

    Doc *m_doc;
    bool m_clearing;
    mutable QMutex m_mutex;
    double m_sx;
    double m_sy;
    double m_stageOffX;
    double m_stageOffY;
    double m_stageHeight;
    double m_roomHeight;
    /** Compute the room (assumes m_mutex is held). */
    StageGeom::Room computeRoom() const;
    QMap<quint32, StageGeometryFixture *> m_fixtures;   // keyed by QLC+ fixture id
    QList<quint32> m_order;                             // display order
    bool m_live;
    QVector3D m_target;
    int m_liveUniverse;
    int m_liveXCh, m_liveYCh, m_liveZCh;
    bool m_liveAll;
    QHash<quint32, QByteArray> m_input;
};

/** @} */

#endif // STAGEGEOMETRYENGINE_H
