/*
  Q Light Controller Plus
  stagegeometrymath.h

  Port of the "stage_geometry" moving head calibration model to C++.
  Geometry, DMX <-> axis-angle conversion and the calibrated Fixture.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGEOMETRYMATH_H
#define STAGEGEOMETRYMATH_H

#include <QVector3D>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QString>

/**
 * QPointF has no operator< in Qt6, but it is used as a QMap key for the 3x3
 * stage grid. Provide a strict weak ordering (lexicographic).
 */
inline bool operator<(const QPointF &a, const QPointF &b)
{
    return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
}

/** @addtogroup ui_stagegeometry UI
 * @{
 */

namespace StageGeom
{

/** Smallest signed difference between two angles, in [-pi, pi]. */
double angleDifference(double a, double b);

/**
 * THE beam model: unit direction of the beam for axis angles (pan, tilt).
 *
 * World frame: x = right, y = forward, z = up. For an ideally mounted head
 * (pan axis vertical) the beam points (sinT cosP, sinT sinP, -cosT), with pan
 * P (azimuth) and tilt T measured from straight down. Defined for every
 * (P, T): (P, T) and (P+pi, -T) give the SAME direction.
 */
QVector3D beamDirection(double pan, double tilt);

/** The other axis position producing the identical beam: (pan+pi, -tilt). */
QPair<double, double> mirroredAngles(double pan, double tilt);

/**
 * Inverse of beamDirection: the pan/tilt that aims a fixture at a point.
 * tilt convention: 0 = straight down, 90 = horizontal, >90 = above.
 * Returns (pan, tilt) in radians.
 */
QPair<double, double> beamAngles(double fx, double fy, double fz,
                                 double px, double py, double pz = 0.0);

/** Rectangular stage; (0,0) .. (sx,sy) with y pointing "forward". */
class Stage
{
public:
    Stage(double sx = 4.0, double sy = 3.0);

    double sx() const { return m_sx; }
    double sy() const { return m_sy; }
    bool isValid() const { return m_sx > 0.0 && m_sy > 0.0; }

    /** The 3x3 calibration grid, as world points on the stage plane. */
    QList<QPointF> points() const;
    QList<QPointF> corners() const;
    bool contains(const QPointF &point) const;

private:
    double m_sx, m_sy;
};

/**
 * The room the fixtures may aim into: an axis aligned box given relative to
 * the stage origin (0,0,0). May start behind/below the stage.
 */
class Room
{
public:
    Room(double x0 = 0.0, double x1 = 1.0,
         double y0 = 0.0, double y1 = 1.0,
         double z0 = 0.0, double z1 = 4.0);

    static Room aroundStage(const Stage &stage, double z0 = 0.0, double z1 = 4.0);

    double minX() const { return m_x0; }
    double maxX() const { return m_x1; }
    double minY() const { return m_y0; }
    double maxY() const { return m_y1; }
    double minZ() const { return m_z0; }
    double maxZ() const { return m_z1; }

    bool isValid() const;

    /** Bounds of the axis "x", "y" or "z" as (lo, hi). */
    QPair<double, double> bounds(const QString &axis) const;

    bool contains(const QVector3D &point) const;
    /** True if the (x, y) plan position is inside the room footprint. */
    bool containsXY(double x, double y) const
    {
        return x >= m_x0 && x <= m_x1 && y >= m_y0 && y <= m_y1;
    }
    QVector3D clamp(const QVector3D &point) const;

private:
    double m_x0, m_x1, m_y0, m_y1, m_z0, m_z1;
};

/**
 * Linear DMX -> axis angle calibration:  angle = k * dmx + t.
 * k = radians per DMX step, t = offset in radians.
 */
class AxisCalibration
{
public:
    AxisCalibration(double k = 1.0, double t = 0.0);

    /** Build from mechanical description (range/zero in degrees). */
    static AxisCalibration fromRangeZero(double rangeDeg, double zeroDeg);

    double k() const { return m_k; }
    double t() const { return m_t; }

    /** Total mechanical travel represented by DMX, in degrees (can be negative). */
    double rangeDeg() const;
    /** Axis angle where global zero occurs, in degrees (need not be reachable). */
    double zeroDeg() const;

    /** Convert a DMX value to an axis angle. */
    double angle(double dmx) const;

    bool isValid() const { return m_k != 0.0; }

private:
    double m_k, m_t;
};

/**
 * All integer DMX values in [dmin, dmax] that can represent an axis angle,
 * accounting for multi-turn wraps. Sorted, unique.
 */
QList<int> angleToDmxCandidates(double angle, const AxisCalibration &cal,
                                int dmin = 0, int dmax = 255);

/** Like angleToDmxCandidates, but throws (returns first) if unreachable. */
int requireDmxValue(double angle, const AxisCalibration &cal);

/**
 * Pick ONE (pan, tilt) DMX pair from the candidates, jump-free.
 * previous is the pair currently sent (-1/-1 for none). Returns (-1,-1) when
 * the list is empty.
 */
QPair<int, int> selectPair(const QList<QPair<int, int> > &pairs,
                           const QPair<int, int> &previous = QPair<int, int>(-1, -1),
                           int center = 127);

/**
 * A calibrated moving head: physical position + pan/tilt axis calibration.
 * Geometric authority for aiming.
 */
class Fixture
{
public:
    Fixture();
    Fixture(double x, double y, double z,
            const AxisCalibration &pan, const AxisCalibration &tilt);

    static Fixture fromRanges(double x, double y, double z,
                              double panRange, double panZero,
                              double tiltRange, double tiltZero);

    bool isValid() const { return m_valid; }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }
    QVector3D position() const { return QVector3D(m_x, m_y, m_z); }

    /** Move the fixture (used when the Monitor drags it). */
    void setPosition(double x, double y, double z)
    {
        m_x = x;
        m_y = y;
        m_z = z;
    }

    const AxisCalibration &pan() const { return m_pan; }
    const AxisCalibration &tilt() const { return m_tilt; }

    /** Global (pan, tilt) angles to aim at a world point, in radians. */
    QPair<double, double> beamAngles(double px, double py, double pz = 0.0) const;

    /** Unit direction actually emitted for a pan/tilt DMX pair. */
    QVector3D beamDirection(int panDmx, int tiltDmx) const;

    /**
     * All pan/tilt DMX pairs that aim the beam at a world point, best first
     * (axes nearest mid travel). Empty means out of reach.
     */
    QList<QPair<int, int> > aim(double px, double py, double pz = 0.0) const;

    /** The single best pair, or (-1,-1) when unreachable. */
    QPair<int, int> aimDmx(double px, double py, double pz = 0.0) const;

private:
    double m_x, m_y, m_z;
    AxisCalibration m_pan, m_tilt;
    bool m_valid;
};

} // namespace StageGeom

/** @} */

#endif // STAGEGEOMETRYMATH_H
