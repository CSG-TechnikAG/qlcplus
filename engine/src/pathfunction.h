/*
  Q Light Controller Plus
  pathfunction.h

  A Function that moves calibrated moving heads along a path drawn in the
  Stage Geometry plan. It stores a calibration snapshot per controlled
  fixture so it can run in the engine independently of the UI.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PATHFUNCTION_H
#define PATHFUNCTION_H

#include <QPolygonF>
#include <QPointF>
#include <QMap>
#include <QList>
#include <QMutex>
#include <QSharedPointer>

#include "function.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class GenericFader;
class MasterTimer;

/** @addtogroup engine_functions Functions
 * @{
 */

#define KXMLQLCPathPoints       QStringLiteral("Points")
#define KXMLQLCPathPoint        QStringLiteral("Point")
#define KXMLQLCPathZ            QStringLiteral("Z")
#define KXMLQLCPathMode         QStringLiteral("Mode")
#define KXMLQLCPathEasing       QStringLiteral("Easing")
#define KXMLQLCPathManualPos    QStringLiteral("ManualPosition")
#define KXMLQLCPathFixture      QStringLiteral("Fixture")
#define KXMLQLCPathFixtureID    QStringLiteral("ID")
#define KXMLQLCPathPanK         QStringLiteral("PanK")
#define KXMLQLCPathPanT         QStringLiteral("PanT")
#define KXMLQLCPathTiltK        QStringLiteral("TiltK")
#define KXMLQLCPathTiltT        QStringLiteral("TiltT")
#define KXMLQLCPathPanCh        QStringLiteral("PanCh")
#define KXMLQLCPathTiltCh       QStringLiteral("TiltCh")
#define KXMLQLCPathDimCh        QStringLiteral("DimCh")

class PathFunction final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(PathFunction)

public:
    /** How the path is played back */
    enum PathMode { Repeat = 0, PingPong, Once, Manual };
    Q_ENUM(PathMode)

    /** Interpolation easing */
    enum Easing { Linear = 0, EaseIn, EaseOut, EaseInOut };
    Q_ENUM(Easing)

    /** A controlled moving head + the calibration needed to aim it. */
    struct PathFixture
    {
        quint32 id = 0;
        double x = 0, y = 0, z = 0;
        double panK = 0, panT = 0, tiltK = 0, tiltT = 0;
        int panCh = -1, tiltCh = -1, dimCh = -1;
        bool inverted = false;      // traverse the path in the opposite direction
        double offsetDeg = 0.0;     // phase offset along the path
        // runtime only
        int lastPan = -1, lastTilt = -1;
        QPointF currentTarget;      // last aimed point (for previews)
    };

    PathFunction(Doc* doc);
    ~PathFunction();

    /** @reimp */
    QIcon getIcon() const override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** @reimp */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Path
     *********************************************************************/
public:
    QPolygonF path() const;
    void setPath(const QPolygonF& path);

    double pathZ() const;
    void setPathZ(double z);

    /*********************************************************************
     * Fixtures
     *********************************************************************/
public:
    QList<PathFixture> fixtures() const;
    void setFixtures(const QList<PathFixture>& fixtures);
    void clearFixtures();

    /** @reimp */
    QList<quint32> components() const override;

public slots:
    /** @reimp */
    void slotFixtureRemoved(quint32 fxi_id) override;

    /*********************************************************************
     * Playback
     *********************************************************************/
public:
    PathMode pathMode() const;
    void setPathMode(PathMode mode);

    Easing easing() const;
    void setEasing(Easing easing);

    double manualPosition() const;
    void setManualPosition(double pos);

    /** Traverse the path in the opposite direction. */
    bool inverted() const;
    void setInverted(bool invert);

    /** Last computed spot along the path (for live previews). */
    QPointF currentPoint() const;

    /** Current target point of a controlled lamp (its offset/invert applied). */
    QPointF fixtureTarget(quint32 id) const;

    /** Interpolate the path point at parameter $t (0..1). */
    QPointF pointAt(double t) const;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    void preRun(MasterTimer* timer) override;
    void write(MasterTimer* timer, QList<Universe*> universes) override;
    void postRun(MasterTimer* timer, QList<Universe*> universes) override;

    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId = 0) override;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool saveXML(QXmlStreamWriter *doc) const override;
    bool loadXML(QXmlStreamReader &root) override;
    void postLoad() override;

private:
    /** Position along the path (0..1) for the current tick. */
    double currentParam() const;
    double applyEasing(double t) const;

    void writeFixtures(QList<Universe*> universes, double t);
    QSharedPointer<GenericFader> getFader(QList<Universe*> universes, quint32 universeID);

private:
    QPolygonF m_path;
    double m_pathZ;
    QList<PathFixture> m_fixtures;
    PathMode m_mode;
    Easing m_easing;
    double m_manualPos;
    bool m_inverted;

    mutable QMutex m_pointMutex;
    QPointF m_currentPoint;

    QMap<quint32, QSharedPointer<GenericFader> > m_fadersMap;
};

/** @} */

#endif // PATHFUNCTION_H
