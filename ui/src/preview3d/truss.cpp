/*
  Q Light Controller Plus
  truss.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtMath>

#include "truss.h"

/****************************************************************************
 * TrussXform
 ****************************************************************************/

QVector3D TrussXform::apply(const QVector3D &p) const
{
    return rot.rotatedVector(p) + t;
}

TrussXform TrussXform::compose(const TrussXform &o) const
{
    TrussXform r;
    r.rot = rot * o.rot;
    r.t = apply(o.t);
    return r;
}

TrussXform TrussXform::inverse() const
{
    TrussXform r;
    r.rot = rot.conjugated();
    r.t = -r.rot.rotatedVector(t);
    return r;
}

/****************************************************************************
 * Truss
 ****************************************************************************/

Truss::Truss()
    : id(0)
    , type(Box)
    , profile(Box)
    , length(4.0)
    , width(0.30)
    , height(0.30)
    , bayLength(0.5)
    , pos(0.0, 0.0, 3.5)
    , rot()
    , visible(true)
    , color(168, 178, 190)
{
}

QString Truss::typeName(Type t)
{
    switch (t)
    {
    case Triangle: return QStringLiteral("Triangle");
    case Ladder:   return QStringLiteral("Ladder");
    case Corner:   return QStringLiteral("Corner");
    case Tee:      return QStringLiteral("T");
    case Cross4:   return QStringLiteral("4-way");
    case Cross6:   return QStringLiteral("6-way");
    default:       return QStringLiteral("Box");
    }
}

QString Truss::typeName() const
{
    return typeName(type);
}

QList<Truss::Type> Truss::types()
{
    return QList<Type>() << Box << Triangle << Ladder
                         << Corner << Tee << Cross4 << Cross6;
}

bool Truss::isJunction(Type t)
{
    return t == Corner || t == Tee || t == Cross4 || t == Cross6;
}

QString Truss::typeId(Type t)
{
    switch (t)
    {
    case Triangle: return QStringLiteral("triangle");
    case Ladder:   return QStringLiteral("ladder");
    case Corner:   return QStringLiteral("corner");
    case Tee:      return QStringLiteral("tee");
    case Cross4:   return QStringLiteral("cross4");
    case Cross6:   return QStringLiteral("cross6");
    default:       return QStringLiteral("box");
    }
}

Truss::Type Truss::typeFromId(const QString &id)
{
    if (id == QStringLiteral("triangle")) return Triangle;
    if (id == QStringLiteral("ladder"))   return Ladder;
    if (id == QStringLiteral("corner"))   return Corner;
    if (id == QStringLiteral("tee"))      return Tee;
    if (id == QStringLiteral("cross4"))   return Cross4;
    if (id == QStringLiteral("cross6"))   return Cross6;
    return Box;
}

QList<TrussPort> Truss::localPorts() const
{
    QList<TrussPort> ports;

    if (!isJunction(type))
    {
        const double halfL = qMax(0.1, length) * 0.5;
        ports << TrussPort{ QVector3D(-halfL, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D( halfL, 0.0, 0.0), QVector3D( 1.0, 0.0, 0.0) };
        return ports;
    }

    const double ns = qMax(0.08, width * 0.5);
    switch (type)
    {
    case Corner:
        ports << TrussPort{ QVector3D(-ns, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0,  ns, 0.0), QVector3D( 0.0, 1.0, 0.0) };
        break;
    case Tee:
        ports << TrussPort{ QVector3D(-ns, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D( ns, 0.0, 0.0), QVector3D( 1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0,  ns, 0.0), QVector3D( 0.0, 1.0, 0.0) };
        break;
    case Cross4:
        ports << TrussPort{ QVector3D(-ns, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D( ns, 0.0, 0.0), QVector3D( 1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0, -ns, 0.0), QVector3D( 0.0, -1.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0,  ns, 0.0), QVector3D( 0.0, 1.0, 0.0) };
        break;
    case Cross6:
        ports << TrussPort{ QVector3D(-ns, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D( ns, 0.0, 0.0), QVector3D( 1.0, 0.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0, -ns, 0.0), QVector3D( 0.0, -1.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0,  ns, 0.0), QVector3D( 0.0, 1.0, 0.0) };
        ports << TrussPort{ QVector3D(0.0, 0.0, -ns), QVector3D( 0.0, 0.0, -1.0) };
        ports << TrussPort{ QVector3D(0.0, 0.0,  ns), QVector3D( 0.0, 0.0, 1.0) };
        break;
    default:
        break;
    }
    return ports;
}

static void addBoxEdges(QList<TrussSegment> &segs, double hx, double hy, double hz)
{
    const QVector3D c[8] = {
        QVector3D(-hx, -hy, -hz), QVector3D( hx, -hy, -hz),
        QVector3D( hx,  hy, -hz), QVector3D(-hx,  hy, -hz),
        QVector3D(-hx, -hy,  hz), QVector3D( hx, -hy,  hz),
        QVector3D( hx,  hy,  hz), QVector3D(-hx,  hy,  hz)
    };
    static const int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i)
        segs << TrussSegment{ c[e[i][0]], c[e[i][1]], true };
}

/** Straight truss geometry in local coordinates (centred, along X). */
static QList<TrussSegment> straightLocal(Truss::Type type, double length,
                                         double width, double height, double bayLength)
{
    QList<TrussSegment> segs;
    const double L = qMax(0.1, length);
    const double halfL = L * 0.5;
    const double w = qMax(0.05, width);
    const double h = qMax(0.05, height);

    // Cross-section chord positions (y, z).
    QList<QPointF> chords;
    switch (type)
    {
    case Truss::Triangle:
        chords << QPointF(-w * 0.5, -h * 0.5)
               << QPointF( w * 0.5, -h * 0.5)
               << QPointF( 0.0,        h * 0.5);
        break;
    case Truss::Ladder:
        chords << QPointF(0.0, -h * 0.5)
               << QPointF(0.0,  h * 0.5);
        break;
    case Truss::Box:
    default:
        chords << QPointF(-w * 0.5, -h * 0.5)
               << QPointF( w * 0.5, -h * 0.5)
               << QPointF( w * 0.5,  h * 0.5)
               << QPointF(-w * 0.5,  h * 0.5);
        break;
    }

    const int nc = chords.size();

    // End frames.
    const int edges = (type == Truss::Ladder) ? 1 : nc;
    for (int e = 0; e < 2; ++e)
    {
        const double x = (e == 0) ? -halfL : halfL;
        for (int i = 0; i < edges; ++i)
        {
            const QPointF &a = chords.at(i);
            const QPointF &b = chords.at((i + 1) % nc);
            segs << TrussSegment{ QVector3D(x, a.x(), a.y()),
                                  QVector3D(x, b.x(), b.y()), true };
        }
    }

    // Longitudinal chords.
    for (const QPointF &c : chords)
        segs << TrussSegment{ QVector3D(-halfL, c.x(), c.y()),
                              QVector3D( halfL, c.x(), c.y()), true };

    // Diagonal braces (zigzag on every face).
    const int bays = qMax(1, int(qRound(L / qMax(0.1, bayLength))));
    const double bl = L / bays;
    const int faces = (type == Truss::Ladder) ? 1 : nc;
    for (int f = 0; f < faces; ++f)
    {
        const QPointF &a = chords.at(f);
        const QPointF &b = chords.at((f + 1) % nc);
        for (int i = 0; i < bays; ++i)
        {
            const double x0 = -halfL + i * bl;
            const double x1 = -halfL + (i + 1) * bl;
            if ((i & 1) == 0)
                segs << TrussSegment{ QVector3D(x0, a.x(), a.y()),
                                      QVector3D(x1, b.x(), b.y()), false };
            else
                segs << TrussSegment{ QVector3D(x0, b.x(), b.y()),
                                      QVector3D(x1, a.x(), a.y()), false };
        }
    }

    // Ladder rungs.
    if (type == Truss::Ladder)
    {
        for (int i = 0; i <= bays; ++i)
        {
            const double x = -halfL + i * bl;
            segs << TrussSegment{ QVector3D(x, chords.at(0).x(), chords.at(0).y()),
                                  QVector3D(x, chords.at(1).x(), chords.at(1).y()), false };
        }
    }

    return segs;
}

QList<TrussSegment> Truss::segments(const QSet<int> &linkedPorts, bool onlyLinkedPorts) const
{
    QList<TrussSegment> segs;

    if (isJunction(type))
    {
        // Each port gets a short truss stub of the junction's own profile,
        // reaching from the centre outwards so the pieces join up cleanly.
        // With $onlyLinkedPorts, unconnected sides are left empty.
        const double ns = qMax(0.08, width * 0.5);
        const double L = ns + 0.30;
        const QList<TrussPort> ports = localPorts();
        for (int i = 0; i < ports.size(); ++i)
        {
            if (onlyLinkedPorts && !linkedPorts.contains(i))
                continue;

            const TrussPort &p = ports.at(i);
            const QList<TrussSegment> sub = straightLocal(profile, L, width, height, bayLength);
            const QQuaternion r = QQuaternion::rotationTo(QVector3D(1.0, 0.0, 0.0), p.localDir);
            const QVector3D center = p.localDir * (L * 0.5);
            for (const TrussSegment &s : sub)
                segs << TrussSegment{ r.rotatedVector(s.a) + center,
                                      r.rotatedVector(s.b) + center, s.chord };
        }

        // Small centre node, so an unconnected junction is still visible.
        addBoxEdges(segs, ns * 0.45, ns * 0.45, ns * 0.45);
    }
    else
    {
        segs = straightLocal(type, length, width, height, bayLength);
    }

    // Place into the world.
    for (TrussSegment &s : segs)
    {
        s.a = rot.rotatedVector(s.a) + pos;
        s.b = rot.rotatedVector(s.b) + pos;
    }

    return segs;
}

bool Truss::isVertical() const
{
    return qAbs(rot.rotatedVector(QVector3D(1.0, 0.0, 0.0)).z()) > 0.7;
}

double Truss::yawDegrees() const
{
    double yaw;
    if (isVertical())
    {
        // Length axis is vertical; use the cross-section to recover the yaw.
        const QVector3D y = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
        yaw = qRadiansToDegrees(qAtan2(-y.x(), y.y()));
    }
    else
    {
        const QVector3D x = rot.rotatedVector(QVector3D(1.0, 0.0, 0.0));
        yaw = qRadiansToDegrees(qAtan2(x.y(), x.x()));
    }
    while (yaw > 180.0)
        yaw -= 360.0;
    while (yaw < -180.0)
        yaw += 360.0;
    return yaw;
}

void Truss::setYawDegrees(double deg)
{
    setPose(deg, isVertical());
}

void Truss::setPose(double yawDeg, bool vertical)
{
    QQuaternion base;
    if (vertical)
        base = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 1.0, 0.0), -90.0);
    rot = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), yawDeg) * base;
}

TrussXform Truss::xform() const
{
    TrussXform x;
    x.rot = rot;
    x.t = pos;
    return x;
}

void Truss::setXform(const TrussXform &x)
{
    rot = x.rot.normalized();
    pos = x.t;
}
