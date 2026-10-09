/*
  Q Light Controller Plus
  stagegeometrysolver.h

  Staged calibration solver (port of stage_geometry/solver.py).
  Each stage is well posed:

    A) axis calibration (k, t) per axis, from the reference directions only,
       closed form (wrapped linear regression).
    B) position (x, y, z) from the beam DIRECTIONS, a linear least squares.
    C) optional position-only refinement with the axes fixed.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGEOMETRYSOLVER_H
#define STAGEGEOMETRYSOLVER_H

#include <QMap>
#include <QPair>
#include <QPointF>
#include <QVector3D>
#include <QList>

#include "stagegeometrymath.h"

namespace StageGeom
{

/**
 * The calibration measurement for one fixture: the stage points (one pan/tilt
 * DMX value each) plus the reference directions (global angle -> all DMX
 * values reaching it). Points and references need not be in the same branch.
 */
class Measurement
{
public:
    QMap<QPointF, QPair<int, int> > points;              // stage point -> (pan,tilt)
    QList<QPair<double, QList<int> > > panReferences;    // (angle, [dmx...])
    QList<QPair<double, QList<int> > > tiltReferences;

    int panValueCount() const;
    int tiltValueCount() const;
    bool isValid() const;
};

struct CalibrationResult
{
    Fixture fixture;
    double panRangeDeg = 0.0;
    double panRangeStdDeg = 0.0;
    double tiltRangeDeg = 0.0;
    double tiltRangeStdDeg = 0.0;
    QVector3D position;
    QVector3D positionStd;
    QVector3D stagedPosition;
    double cost = 0.0;
    double rmsDeg = 0.0;
    bool reversedBeam = false;
    bool refined = false;
};

/**
 * Staged calibration. Throws std::runtime_error (with a user-readable message)
 * when there is not enough input. `refine` enables the optional Stage C.
 */
CalibrationResult calibrate(const Measurement &measurement, bool refine = true);

} // namespace StageGeom

#endif // STAGEGEOMETRYSOLVER_H
