/*
  Q Light Controller Plus
  stagegeometrysolver.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "stagegeometrysolver.h"

#include <QtMath>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace StageGeom
{

namespace
{

// ---------------------------------------------------------------------------
// tiny linear algebra helpers
// ---------------------------------------------------------------------------

bool solve2(double a00, double a01, double a10, double a11,
            double b0, double b1, double &x0, double &x1)
{
    double det = a00 * a11 - a01 * a10;
    if (qAbs(det) < 1e-15)
        return false;
    x0 = (b0 * a11 - a01 * b1) / det;
    x1 = (a00 * b1 - b0 * a10) / det;
    return true;
}

bool solve3(const double A[3][3], const double b[3], double x[3])
{
    double det =
        A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
        A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
        A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (qAbs(det) < 1e-18)
        return false;

    for (int col = 0; col < 3; col++)
    {
        double M[3][3];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                M[r][c] = (c == col) ? b[r] : A[r][c];
        double d =
            M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
            M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
            M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
        x[col] = d / det;
    }
    return true;
}

// Symmetric 3x3 pseudo-inverse via Jacobi eigen-decomposition.
void symmetricPseudoInverse3(const double in[3][3], double out[3][3])
{
    double a[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            a[i][j] = in[i][j];

    double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    for (int sweep = 0; sweep < 50; sweep++)
    {
        double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-24)
            break;
        for (int p = 0; p < 2; p++)
        {
            for (int q = p + 1; q < 3; q++)
            {
                if (qAbs(a[p][q]) < 1e-18)
                    continue;
                double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                double t = (theta >= 0 ? 1.0 : -1.0) /
                           (qAbs(theta) + qSqrt(theta * theta + 1.0));
                double c = 1.0 / qSqrt(t * t + 1.0);
                double s = t * c;

                for (int k = 0; k < 3; k++)
                {
                    double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; k++)
                {
                    double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; k++)
                {
                    double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }

    // a is now diagonal (eigenvalues), v columns are eigenvectors.
    double maxAbs = 0.0;
    for (int i = 0; i < 3; i++)
        maxAbs = qMax(maxAbs, qAbs(a[i][i]));
    double tol = maxAbs * 1e-9;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = 0.0;

    for (int e = 0; e < 3; e++)
    {
        if (qAbs(a[e][e]) <= tol)
            continue;                    // treat near-zero eigenvalue as null
        double inv = 1.0 / a[e][e];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                out[i][j] += inv * v[i][e] * v[j][e];
    }
}

// ---------------------------------------------------------------------------
// solver core
// ---------------------------------------------------------------------------

// plausible axis slope |k| = radians per DMX step for ~90..1200 deg over 255
const double K_MIN = qDegreesToRadians(90.0) / 255.0;
const double K_MAX = qDegreesToRadians(1200.0) / 255.0;

struct AxisFit
{
    double k, t, cost, kStd;
};

AxisFit fitAxis(const QList<QPair<double, QList<int> > > &references, int nSeeds = 120)
{
    QVector<double> vs, th;
    for (const QPair<double, QList<int> > &ref : references)
        for (int v : ref.second)
        {
            vs.append(double(v));
            th.append(ref.first);
        }

    if (vs.size() < 2)
        throw std::runtime_error("Für jede Achse werden mindestens zwei Referenzwerte benötigt.");

    int n = vs.size();

    // design matrix columns [v, 1] -> normal equations A = D^T D, b = D^T y
    auto lstsq = [&](const QVector<double> &y, double &k, double &t) {
        double svv = 0, sv = 0, sy = 0, svy = 0;
        for (int i = 0; i < n; i++)
        {
            svv += vs[i] * vs[i];
            sv += vs[i];
            sy += y[i];
            svy += vs[i] * y[i];
        }
        solve2(svv, sv, sv, double(n), svy, sy, k, t);
    };

    QVector<double> seeds;
    for (int i = 0; i < nSeeds; i++)
    {
        double k = K_MIN + (K_MAX - K_MIN) * i / double(nSeeds - 1);
        seeds.append(k);
        seeds.append(-k);
    }

    double bestCost = -1, bestK = 0, bestT = 0;
    QVector<double> y(n);
    for (double seed : seeds)
    {
        double k = seed;
        double t;
        {
            double ss = 0, sc = 0;
            for (int i = 0; i < n; i++)
            {
                double d = th[i] - k * vs[i];
                ss += qSin(d);
                sc += qCos(d);
            }
            t = qAtan2(ss, sc);
        }
        for (int iter = 0; iter < 8; iter++)
        {
            for (int i = 0; i < n; i++)
            {
                double nwrap = qRound((k * vs[i] + t - th[i]) / (2.0 * M_PI));
                y[i] = th[i] + 2.0 * M_PI * nwrap;
            }
            lstsq(y, k, t);
        }
        double cost = 0;
        for (int i = 0; i < n; i++)
        {
            double r = qAtan2(qSin(k * vs[i] + t - th[i]),
                              qCos(k * vs[i] + t - th[i]));
            cost += r * r;
        }
        if (bestCost < 0 || cost < bestCost)
        {
            bestCost = cost;
            bestK = k;
            bestT = t;
        }
    }

    double dof = qMax(1, n - 2);
    double svv = 0, sv = 0;
    for (int i = 0; i < n; i++)
    {
        svv += vs[i] * vs[i];
        sv += vs[i];
    }
    double det = svv * double(n) - sv * sv;
    double cov00 = 0.0;
    if (qAbs(det) > 1e-18)
        cov00 = (bestCost / dof) * (double(n) / det);

    AxisFit fit;
    fit.k = bestK;
    fit.t = bestT;
    fit.cost = bestCost;
    fit.kStd = qSqrt(qMax(cov00, 0.0));
    return fit;
}

struct Beam
{
    QVector3D point;
    QVector3D dir;
};

QVector<Beam> pointBeams(const Measurement &m, const AxisCalibration &pan,
                         const AxisCalibration &tilt)
{
    QVector<Beam> beams;
    for (auto it = m.points.constBegin(); it != m.points.constEnd(); ++it)
    {
        Beam b;
        b.point = QVector3D(it.key().x(), it.key().y(), 0.0);
        b.dir = beamDirection(pan.angle(it.value().first),
                              tilt.angle(it.value().second));
        beams.append(b);
    }
    return beams;
}

void solvePosition(const QVector<Beam> &beams, QVector3D &F, double cov[3][3])
{
    double A[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    double b[3] = {0, 0, 0};

    for (const Beam &bm : beams)
    {
        double d[3] = {bm.dir.x(), bm.dir.y(), bm.dir.z()};
        double P[3] = {bm.point.x(), bm.point.y(), bm.point.z()};
        double M[3][3];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                M[i][j] = (i == j ? 1.0 : 0.0) - d[i] * d[j];
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++)
                A[i][j] += M[i][j];
            b[i] += M[i][0] * P[0] + M[i][1] * P[1] + M[i][2] * P[2];
        }
    }

    double x[3] = {0, 0, 0};
    solve3(A, b, x);
    F = QVector3D(x[0], x[1], x[2]);

    double ssr = 0;
    for (const Beam &bm : beams)
    {
        double d[3] = {bm.dir.x(), bm.dir.y(), bm.dir.z()};
        QVector3D diff = F - bm.point;
        double off[3];
        for (int i = 0; i < 3; i++)
            off[i] = diff.x() * ((i == 0 ? 1.0 : 0.0) - d[0] * d[i]) +
                     diff.y() * ((i == 1 ? 1.0 : 0.0) - d[1] * d[i]) +
                     diff.z() * ((i == 2 ? 1.0 : 0.0) - d[2] * d[i]);
        ssr += off[0] * off[0] + off[1] * off[1] + off[2] * off[2];
    }
    double dof = qMax(1, 2 * beams.size() - 3);
    double pinv[3][3];
    symmetricPseudoInverse3(A, pinv);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            cov[i][j] = (ssr / dof) * pinv[i][j];
}

bool beamsPointBackwards(const QVector3D &F, const QVector<Beam> &beams)
{
    if (beams.isEmpty())
        return false;
    for (const Beam &bm : beams)
        if (QVector3D::dotProduct(bm.point - F, bm.dir) >= 0.0)
            return false;
    return true;
}

QVector<double> positionResidual(const QVector3D &F, const QVector<Beam> &beams)
{
    QVector<double> r;
    r.reserve(beams.size() * 3);
    for (const Beam &bm : beams)
    {
        QVector3D v = bm.point - F;
        double n = v.length();
        QVector3D unit = (n > 1e-9) ? v / n : QVector3D(0, 0, 0);
        r.append(unit.x() - bm.dir.x());
        r.append(unit.y() - bm.dir.y());
        r.append(unit.z() - bm.dir.z());
    }
    return r;
}

double residualCost(const QVector<double> &r)
{
    double c = 0;
    for (double v : r)
        c += v * v;
    return c;
}

double rmsDeg(const QVector<double> &residual)
{
    if (residual.isEmpty())
        return 0.0;
    double sum = 0;
    int n = residual.size() / 3;
    for (int i = 0; i < n; i++)
    {
        double dx = residual[3 * i];
        double dy = residual[3 * i + 1];
        double dz = residual[3 * i + 2];
        double chord = qSqrt(dx * dx + dy * dy + dz * dz);
        double angle = 2.0 * qAsin(qBound(0.0, chord / 2.0, 1.0));
        sum += angle * angle;
    }
    return qRadiansToDegrees(qSqrt(sum / n));
}

// Gauss-Newton / Levenberg-Marquardt refinement of the position only.
QVector3D refinePosition(const QVector3D &seed, const QVector<Beam> &beams)
{
    QVector3D pos = seed;
    double cost = residualCost(positionResidual(pos, beams));
    double lambda = 1e-3;

    const double eps = 1e-5;

    for (int iter = 0; iter < 50; iter++)
    {
        QVector<double> r0 = positionResidual(pos, beams);
        int N = beams.size();

        // Jacobian by central finite differences (3N x 3)
        QVector<double> J;
        J.reserve(N * 9);
        for (int c = 0; c < 3; c++)
        {
            QVector3D p1 = pos, p2 = pos;
            if (c == 0) { p1.setX(pos.x() + eps); p2.setX(pos.x() - eps); }
            if (c == 1) { p1.setY(pos.y() + eps); p2.setY(pos.y() - eps); }
            if (c == 2) { p1.setZ(pos.z() + eps); p2.setZ(pos.z() - eps); }
            QVector<double> rp = positionResidual(p1, beams);
            QVector<double> rm = positionResidual(p2, beams);
            for (int i = 0; i < N * 3; i++)
                J.append((rp[i] - rm[i]) / (2.0 * eps));
        }

        double H[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        double g[3] = {0, 0, 0};
        for (int a = 0; a < 3; a++)
        {
            for (int b = 0; b < 3; b++)
            {
                double s = 0;
                for (int i = 0; i < N * 3; i++)
                    s += J[a * N * 3 + i] * J[b * N * 3 + i];
                H[a][b] = s;
            }
            double s = 0;
            for (int i = 0; i < N * 3; i++)
                s += J[a * N * 3 + i] * r0[i];
            g[a] = s;
        }

        double Hd[3][3];
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++)
                Hd[a][b] = H[a][b] + (a == b ? lambda : 0.0);

        double delta[3];
        if (!solve3(Hd, g, delta))
            break;

        QVector3D trial(pos.x() - delta[0], pos.y() - delta[1], pos.z() - delta[2]);
        double trialCost = residualCost(positionResidual(trial, beams));
        if (trialCost < cost)
        {
            double improvement = cost - trialCost;
            pos = trial;
            cost = trialCost;
            lambda = qMax(lambda * 0.5, 1e-9);
            if (improvement < 1e-18)
                break;
        }
        else
        {
            lambda *= 2.0;
            if (lambda > 1e9)
                break;
        }
    }
    return pos;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------

int Measurement::panValueCount() const
{
    int n = 0;
    for (const QPair<double, QList<int> > &r : panReferences)
        n += r.second.size();
    return n;
}

int Measurement::tiltValueCount() const
{
    int n = 0;
    for (const QPair<double, QList<int> > &r : tiltReferences)
        n += r.second.size();
    return n;
}

bool Measurement::isValid() const
{
    return panValueCount() >= 2 && tiltValueCount() >= 2 && points.size() >= 2;
}

// ---------------------------------------------------------------------------
// calibrate
// ---------------------------------------------------------------------------

CalibrationResult calibrate(const Measurement &measurement, bool refine)
{
    AxisFit panFit = fitAxis(measurement.panReferences);
    AxisFit tiltFit = fitAxis(measurement.tiltReferences);
    AxisCalibration pan(panFit.k, panFit.t);
    AxisCalibration tilt(tiltFit.k, tiltFit.t);

    if (measurement.points.size() < 2)
        throw std::runtime_error("Für die Position werden mindestens zwei Bühnenpunkte benötigt.");

    QVector<Beam> beams = pointBeams(measurement, pan, tilt);
    QVector3D F;
    double cov[3][3];
    solvePosition(beams, F, cov);

    bool reversed = beamsPointBackwards(F, beams);
    if (reversed)
    {
        pan = AxisCalibration(pan.k(), pan.t() + M_PI);
        tilt = AxisCalibration(-tilt.k(), M_PI - tilt.t());
        beams = pointBeams(measurement, pan, tilt);
    }

    QVector3D staged = F;
    QVector3D posStd(qSqrt(qMax(cov[0][0], 0.0)),
                     qSqrt(qMax(cov[1][1], 0.0)),
                     qSqrt(qMax(cov[2][2], 0.0)));
    bool didRefine = false;

    if (refine)
    {
        QVector3D refined = refinePosition(F, beams);
        if (qSqrt((refined.x() - F.x()) * (refined.x() - F.x()) +
                  (refined.y() - F.y()) * (refined.y() - F.y())) +
            qAbs(refined.z() - F.z()) < 50.0)
        {
            F = refined;
            didRefine = true;
        }
    }

    Fixture fixture(F.x(), F.y(), F.z(), pan, tilt);
    QVector<double> residual = positionResidual(F, beams);
    double cost = residualCost(residual);

    const double degPerStep = 255.0 * 180.0 / M_PI;

    CalibrationResult result;
    result.fixture = fixture;
    result.panRangeDeg = fixture.pan().rangeDeg();
    result.panRangeStdDeg = panFit.kStd * degPerStep;
    result.tiltRangeDeg = qAbs(fixture.tilt().rangeDeg());
    result.tiltRangeStdDeg = tiltFit.kStd * degPerStep;
    result.position = F;
    result.positionStd = posStd;
    result.stagedPosition = staged;
    result.cost = cost;
    result.rmsDeg = rmsDeg(residual);
    result.reversedBeam = reversed;
    result.refined = didRefine;
    return result;
}

} // namespace StageGeom
