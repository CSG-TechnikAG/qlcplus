/*
  Q Light Controller Plus
  truss.h

  A configurable lighting truss piece (straight box / triangle / ladder, or a
  corner / T / 4-way / 6-way junction) used by the 3D Preview tab. Geometry is
  generated as line segments (chords + diagonal braces), which keeps rendering
  cheap and scalable. Each piece exposes attachment ports used to link pieces.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TRUSS_H
#define TRUSS_H

#include <QVector3D>
#include <QQuaternion>
#include <QList>
#include <QSet>
#include <QColor>
#include <QString>

/**
 * One line segment of a truss. Chords are drawn thicker than braces.
 */
struct TrussSegment
{
    QVector3D a;
    QVector3D b;
    bool chord;
};

/** An attachment point on a truss, in the piece's local coordinates. */
struct TrussPort
{
    QVector3D localPos;
    QVector3D localDir;   // outward unit direction
};

/** A full 3D rigid transform: rotation + translation. */
struct TrussXform
{
    QQuaternion rot;
    QVector3D t;

    QVector3D apply(const QVector3D &p) const;
    TrussXform compose(const TrussXform &other) const;   // this * other
    TrussXform inverse() const;
};

/**
 * A truss piece, centred on its position and running along its local X axis
 * (rotated around the vertical axis by $rotation degrees).
 */
class Truss
{
public:
    enum Type { Box, Triangle, Ladder, Corner, Tee, Cross4, Cross6 };

    Truss();

    int id;
    Type type;
    Type profile;       // cross-section used by junctions (Box/Triangle/Ladder)
    double length;      // m, along the local X axis (straight pieces)
    double width;       // m, cross-section
    double height;      // m, cross-section
    double bayLength;   // m, spacing of the diagonal braces
    QVector3D pos;      // centre, world metres
    QQuaternion rot;    // full 3D orientation (local X = length axis)
    bool visible;
    QColor color;

    QString typeName() const;
    static QString typeName(Type t);
    static QList<Type> types();
    static bool isJunction(Type t);

    /** Stable (non-translated) id used for saving. */
    static QString typeId(Type t);
    static Type typeFromId(const QString &id);

    /** Yaw (degrees) shown in the panel; setting it resets to a horizontal pose. */
    double yawDegrees() const;
    void setYawDegrees(double deg);

    /** True when the length axis points (mostly) up or down. */
    bool isVertical() const;

    /** Set the pose from the panel fields. */
    void setPose(double yawDeg, bool vertical);

    /** Attachment points in local coordinates. */
    QList<TrussPort> localPorts() const;

    /** All line segments, in world coordinates. When $onlyLinkedPorts is set,
     *  a junction emits stubs only for the ports in $linkedPorts. */
    QList<TrussSegment> segments(const QSet<int> &linkedPorts = QSet<int>(),
                                 bool onlyLinkedPorts = false) const;

    TrussXform xform() const;
    void setXform(const TrussXform &x);
};

#endif // TRUSS_H
