/*
  Q Light Controller Plus
  stagegeometrymath.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "stagegeometrymath.h"

#include <QtMath>
#include <QSet>

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace StageGeom
{

double angleDifference(double a, double b)
{
    return qAtan2(qSin(a - b), qCos(a - b));
}

QVector3D beamDirection(double pan, double tilt)
{
    return QVector3D(qSin(tilt) * qCos(pan),
                     qSin(tilt) * qSin(pan),
                     -qCos(tilt));
}

QPair<double, double> mirroredAngles(double pan, double tilt)
{
    return QPair<double, double>(pan + M_PI, -tilt);
}

QPair<double, double> beamAngles(double fx, double fy, double fz,
                                 double px, double py, double pz)
{
    double r = qSqrt((px - fx) * (px - fx) + (py - fy) * (py - fy));
    double pan = qAtan2(py - fy, px - fx);
    double tilt = qAtan2(r, fz - pz);
    return QPair<double, double>(pan, tilt);
}

// ---------------------------------------------------------------------------
// Stage
// ---------------------------------------------------------------------------

Stage::Stage(double sx, double sy)
    : m_sx(sx)
    , m_sy(sy)
{
}

QList<QPointF> Stage::points() const
{
    double hx = m_sx / 2.0;
    double hy = m_sy / 2.0;
    QList<QPointF> list;
    list << QPointF(0, 0)    << QPointF(hx, 0)   << QPointF(m_sx, 0)
         << QPointF(0, hy)   << QPointF(hx, hy)  << QPointF(m_sx, hy)
         << QPointF(0, m_sy) << QPointF(hx, m_sy) << QPointF(m_sx, m_sy);
    return list;
}

QList<QPointF> Stage::corners() const
{
    QList<QPointF> list;
    list << QPointF(0, 0) << QPointF(m_sx, 0)
         << QPointF(m_sx, m_sy) << QPointF(0, m_sy);
    return list;
}

bool Stage::contains(const QPointF &point) const
{
    return point.x() >= 0.0 && point.x() <= m_sx &&
           point.y() >= 0.0 && point.y() <= m_sy;
}

// ---------------------------------------------------------------------------
// Room
// ---------------------------------------------------------------------------

Room::Room(double x0, double x1, double y0, double y1, double z0, double z1)
    : m_x0(x0), m_x1(x1)
    , m_y0(y0), m_y1(y1)
    , m_z0(z0), m_z1(z1)
{
}

Room Room::aroundStage(const Stage &stage, double z0, double z1)
{
    return Room(0.0, stage.sx(), 0.0, stage.sy(), z0, z1);
}

bool Room::isValid() const
{
    return m_x1 > m_x0 && m_y1 > m_y0 && m_z1 > m_z0;
}

QPair<double, double> Room::bounds(const QString &axis) const
{
    if (axis == QLatin1String("x"))
        return QPair<double, double>(m_x0, m_x1);
    if (axis == QLatin1String("y"))
        return QPair<double, double>(m_y0, m_y1);
    return QPair<double, double>(m_z0, m_z1);
}

bool Room::contains(const QVector3D &point) const
{
    return point.x() >= m_x0 && point.x() <= m_x1 &&
           point.y() >= m_y0 && point.y() <= m_y1 &&
           point.z() >= m_z0 && point.z() <= m_z1;
}

QVector3D Room::clamp(const QVector3D &point) const
{
    return QVector3D(qBound(m_x0, double(point.x()), m_x1),
                     qBound(m_y0, double(point.y()), m_y1),
                     qBound(m_z0, double(point.z()), m_z1));
}

// ---------------------------------------------------------------------------
// AxisCalibration
// ---------------------------------------------------------------------------

AxisCalibration::AxisCalibration(double k, double t)
    : m_k(k)
    , m_t(t)
{
}

AxisCalibration AxisCalibration::fromRangeZero(double rangeDeg, double zeroDeg)
{
    return AxisCalibration(qDegreesToRadians(rangeDeg) / 255.0,
                           -qDegreesToRadians(zeroDeg));
}

double AxisCalibration::rangeDeg() const
{
    return qRadiansToDegrees(m_k * 255.0);
}

double AxisCalibration::zeroDeg() const
{
    return -qRadiansToDegrees(m_t);
}

double AxisCalibration::angle(double dmx) const
{
    return m_k * dmx + m_t;
}

// ---------------------------------------------------------------------------
// DMX conversion
// ---------------------------------------------------------------------------

QList<int> angleToDmxCandidates(double angle, const AxisCalibration &cal,
                                int dmin, int dmax)
{
    QList<int> candidates;
    double k = cal.k();
    double t = cal.t();

    if (k == 0.0)
        return candidates;

    double n_a = (angle - t - k * dmin) / (2.0 * M_PI);
    double n_b = (angle - t - k * dmax) / (2.0 * M_PI);
    int n_min = int(qCeil(qMin(n_a, n_b)));
    int n_max = int(qFloor(qMax(n_a, n_b)));

    for (int n = n_min; n <= n_max; n++)
    {
        double value = (angle - t - 2.0 * M_PI * n) / k;
        int dmx = int(std::nearbyint(value));   // banker's rounding, like Python round()
        if (dmx < dmin || dmx > dmax)
            continue;

        double error = qAbs(k * dmx + t + 2.0 * M_PI * n - angle);
        if (error <= qAbs(k) / 2.0 + 1e-9)
            candidates.append(dmx);
    }

    // sorted, unique
    QList<int> unique;
    for (int v : candidates)
        if (!unique.contains(v))
            unique.append(v);
    std::sort(unique.begin(), unique.end());
    return unique;
}

int requireDmxValue(double angle, const AxisCalibration &cal)
{
    QList<int> values = angleToDmxCandidates(angle, cal);
    if (values.isEmpty())
        return -1;
    return values.first();
}

QPair<int, int> selectPair(const QList<QPair<int, int> > &pairs,
                           const QPair<int, int> &previous, int center)
{
    if (pairs.isEmpty())
        return QPair<int, int>(-1, -1);

    auto toCenter = [center](const QPair<int, int> &pt) {
        return qAbs(pt.first - center) + qAbs(pt.second - center);
    };

    if (previous.first < 0 || previous.second < 0)
    {
        QPair<int, int> best = pairs.first();
        for (const QPair<int, int> &pt : pairs)
            if (toCenter(pt) < toCenter(best))
                best = pt;
        return best;
    }

    const int ppan = previous.first;
    const int ptilt = previous.second;
    QPair<int, int> nearest = pairs.first();
    int nearestCost = qAbs(nearest.first - ppan) + qAbs(nearest.second - ptilt);
    for (const QPair<int, int> &pt : pairs)
    {
        int cost = qAbs(pt.first - ppan) + qAbs(pt.second - ptilt);
        if (cost < nearestCost)
        {
            nearestCost = cost;
            nearest = pt;
        }
    }

    // Forced jump detection: smallest spacing between distinct candidate pans.
    QList<int> pans;
    for (const QPair<int, int> &pt : pairs)
        if (!pans.contains(pt.first))
            pans.append(pt.first);
    std::sort(pans.begin(), pans.end());
    if (pans.size() >= 2)
    {
        int spacing = pans.at(1) - pans.at(0);
        for (int i = 2; i < pans.size(); i++)
            spacing = qMin(spacing, pans.at(i) - pans.at(i - 1));
        if (qAbs(nearest.first - ppan) > spacing / 2)
        {
            QPair<int, int> best = pairs.first();
            for (const QPair<int, int> &pt : pairs)
                if (toCenter(pt) < toCenter(best))
                    best = pt;
            return best;
        }
    }
    return nearest;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

Fixture::Fixture()
    : m_x(0), m_y(0), m_z(0)
    , m_pan(1.0, 0.0)
    , m_tilt(1.0, 0.0)
    , m_valid(false)
{
}

Fixture::Fixture(double x, double y, double z,
                 const AxisCalibration &pan, const AxisCalibration &tilt)
    : m_x(x), m_y(y), m_z(z)
    , m_pan(pan)
    , m_tilt(tilt)
    , m_valid(true)
{
}

Fixture Fixture::fromRanges(double x, double y, double z,
                            double panRange, double panZero,
                            double tiltRange, double tiltZero)
{
    return Fixture(x, y, z,
                   AxisCalibration::fromRangeZero(panRange, panZero),
                   AxisCalibration::fromRangeZero(tiltRange, tiltZero));
}

QPair<double, double> Fixture::beamAngles(double px, double py, double pz) const
{
    return StageGeom::beamAngles(m_x, m_y, m_z, px, py, pz);
}

QVector3D Fixture::beamDirection(int panDmx, int tiltDmx) const
{
    return StageGeom::beamDirection(m_pan.angle(panDmx), m_tilt.angle(tiltDmx));
}

QList<QPair<int, int> > Fixture::aim(double px, double py, double pz) const
{
    QList<QPair<int, int> > pairs;
    if (!m_valid)
        return pairs;

    QPair<double, double> angles = beamAngles(px, py, pz);

    const QPair<double, double> branches[2] = {
        angles,
        mirroredAngles(angles.first, angles.second)
    };

    for (const QPair<double, double> &branch : branches)
    {
        QList<int> panDmx = angleToDmxCandidates(branch.first, m_pan);
        QList<int> tiltDmx = angleToDmxCandidates(branch.second, m_tilt);
        for (int p : panDmx)
            for (int t : tiltDmx)
                pairs.append(QPair<int, int>(p, t));
    }

    const int mid = 127;
    std::sort(pairs.begin(), pairs.end(),
              [mid](const QPair<int, int> &a, const QPair<int, int> &b) {
                  return qAbs(a.first - mid) + qAbs(a.second - mid) <
                         qAbs(b.first - mid) + qAbs(b.second - mid);
              });
    return pairs;
}

QPair<int, int> Fixture::aimDmx(double px, double py, double pz) const
{
    QList<QPair<int, int> > pairs = aim(px, py, pz);
    if (pairs.isEmpty())
        return QPair<int, int>(-1, -1);
    return pairs.first();
}

} // namespace StageGeom
