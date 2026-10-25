/*
  Q Light Controller Plus
  preview3dview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QPolygonF>
#include <QLineF>
#include <QLinearGradient>
#include <QVarLengthArray>
#include <QRadialGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtMath>
#include <cmath>
#include <algorithm>

#include "preview3dview.h"
#include "trussscene.h"
#include "fixturescene.h"

namespace
{
/** Stage deck thickness and leg size (metres). */
const double kDeckThickness = 0.08;
const double kLegHalf = 0.03;

/** Grid spacing (metres) adapted to the scene size. */
double gridStep(double span)
{
    if (span > 60.0)
        return 10.0;
    if (span > 25.0)
        return 5.0;
    if (span > 12.0)
        return 2.0;
    return 1.0;
}

/** Sutherland-Hodgman clip of a camera-space polygon against z >= near. */
QList<QVector3D> clipNear(const QList<QVector3D> &poly, double near)
{
    QList<QVector3D> out;
    const int n = poly.size();
    for (int i = 0; i < n; ++i)
    {
        const QVector3D &a = poly.at(i);
        const QVector3D &b = poly.at((i + 1) % n);
        const bool aIn = a.z() >= near;
        const bool bIn = b.z() >= near;
        if (aIn)
            out << a;
        if (aIn != bIn)
        {
            const double t = (near - a.z()) / (b.z() - a.z());
            out << a + (b - a) * t;
        }
    }
    return out;
}

void clipNearVar(const QVarLengthArray<QVector3D, 32> &poly,
                 QVarLengthArray<QVector3D, 32> &out, double near)
{
    out.clear();
    const int n = poly.size();
    for (int i = 0; i < n; ++i)
    {
        const QVector3D &a = poly.at(i);
        const QVector3D &b = poly.at((i + 1) % n);
        const bool aIn = a.z() >= near;
        const bool bIn = b.z() >= near;
        if (aIn)
            out.append(a);
        if (aIn != bIn)
        {
            const double t = (near - a.z()) / (b.z() - a.z());
            out.append(a + (b - a) * t);
        }
    }
}

/** Clip a camera-space segment against z >= near. Returns false if invisible. */
bool clipLine(QVector3D &a, QVector3D &b, double near)
{
    const bool aIn = a.z() >= near;
    const bool bIn = b.z() >= near;
    if (!aIn && !bIn)
        return false;
    if (aIn != bIn)
    {
        const double t = (near - a.z()) / (b.z() - a.z());
        const QVector3D c = a + (b - a) * t;
        if (aIn)
            b = c;
        else
            a = c;
    }
    return true;
}

/** Simple diffuse shading for the preview's fixed studio light. */
QColor shadedColor(const QColor &base, const QVector3D &normal,
                   double ambient = 0.76, double diffuse = 0.38)
{
    static const QVector3D light =
        QVector3D(-0.35, -0.45, 0.82).normalized();
    const double ndl = qMax(0.0, double(QVector3D::dotProduct(normal.normalized(), light)));
    const double f = ambient + diffuse * ndl;
    return QColor(qBound(0, qRound(base.red() * f), 255),
                  qBound(0, qRound(base.green() * f), 255),
                  qBound(0, qRound(base.blue() * f), 255),
                  base.alpha());
}

double pointSegmentDist(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-9)
        return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = qBound(0.0, t, 1.0);
    return QLineF(p, a + ab * t).length();
}
} // namespace

Preview3DView::Preview3DView(QWidget *parent)
    : QWidget(parent)
    , m_rx0(-2.5), m_rx1(2.5), m_ry0(-2.5), m_ry1(2.5)
    , m_zFloor(-0.5), m_zTop(4.0)
    , m_stageX(4.0), m_stageY(3.0)
    , m_yaw(qDegreesToRadians(38.0))
    , m_pitch(qDegreesToRadians(32.0))
    , m_dist(10.0)
    , m_target(0.0, 0.0, 0.0)
    , m_fovY(qDegreesToRadians(42.0))
    , m_near(0.05)
    , m_hasFitted(false)
    , m_userMoved(false)
    , m_showGrid(true)
    , m_beamPreview(false)
    , m_dragMode(DragNone)
    , m_scene(NULL)
    , m_trussInteractive(false)
    , m_selectedId(0)
    , m_dragTrussId(0)
    , m_dragPort(-1)
    , m_hoverTrussId(0)
    , m_hoverPort(-1)
    , m_fixtureScene(NULL)
    , m_fixtureInteractive(false)
    , m_selectedFixture(-1)
    , m_dragFixture(false)
    , m_occludersDirty(true)
    , m_fpsFrames(0)
    , m_fps(0.0)
    , m_lastFrameMs(0.0)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);

    m_bgTop = QColor(0, 0, 0);
    m_bgBottom = QColor(0, 0, 0);

    m_floorFill = QColor(34, 43, 54);
    m_floorGrid = QColor(60, 78, 97, 190);
    m_floorEdge = QColor(96, 126, 156);
    m_roomBoxEdge = QColor(124, 156, 188);

    m_stageTop = QColor(30, 92, 138);
    m_stageTopGrid = QColor(130, 200, 240, 150);
    m_stageSide = QColor(18, 50, 77);
    m_stageEdge = QColor(79, 195, 247);

    m_legColor = QColor(84, 98, 113);
    m_legEdge = QColor(120, 140, 160);
    m_trussBrace = QColor(126, 136, 150);
    m_selectColor = QColor(120, 230, 255);

    m_fpsTimer.start();
    updateCamera();
}

Preview3DView::~Preview3DView()
{
}

void Preview3DView::setScene(double roomX0, double roomX1,
                             double roomY0, double roomY1,
                             double zFloor, double zTop,
                             double stageX, double stageY)
{
    m_rx0 = roomX0;
    m_rx1 = roomX1;
    m_ry0 = roomY0;
    m_ry1 = roomY1;
    m_zFloor = zFloor;
    m_zTop = qMax(zTop, zFloor + 0.01);
    m_stageX = qMax(0.0, stageX);
    m_stageY = qMax(0.0, stageY);

    // Keep the whole room in view while the size is being edited, unless the
    // user has already taken control of the camera.
    if (!m_userMoved)
        fitToRoom();

    update();
}

void Preview3DView::fitToRoom()
{
    if (width() <= 0 || height() <= 0)
    {
        m_hasFitted = false;
        return;
    }

    // Fit the room footprint only (the room height is left out of the framing).
    const double hx = (m_rx1 - m_rx0) * 0.5;
    const double hy = (m_ry1 - m_ry0) * 0.5;
    m_target = QVector3D((m_rx0 + m_rx1) * 0.5,
                         (m_ry0 + m_ry1) * 0.5,
                         m_zFloor * 0.5);

    double radius = qMax(qSqrt(hx * hx + hy * hy), 1.0);

    const double aspect = double(width()) / double(height());
    const double fovX = 2.0 * qAtan(qTan(m_fovY * 0.5) * aspect);
    const double fitFov = qMin(m_fovY, fovX);

    m_dist = (radius / qSin(fitFov * 0.5)) * 1.15;
    m_dist = qBound(0.5, m_dist, 5000.0);

    m_hasFitted = true;
    updateCamera();
    update();
    emit viewChanged();
}

void Preview3DView::resetView()
{
    m_yaw = qDegreesToRadians(38.0);
    m_pitch = qDegreesToRadians(32.0);
    m_userMoved = false;
    fitToRoom();
    update();
    emit viewChanged();
}

void Preview3DView::setGridVisible(bool on)
{
    m_showGrid = on;
    update();
}

void Preview3DView::setBeamPreview(bool on)
{
    m_beamPreview = on;
    update();
}

void Preview3DView::setTrussScene(TrussScene *scene)
{
    if (m_scene == scene)
        return;
    if (m_scene != NULL)
        disconnect(m_scene, nullptr, this, nullptr);
    m_scene = scene;
    m_occludersDirty = true;
    if (m_scene != NULL)
        connect(m_scene, &TrussScene::changed, this, [this]() {
            m_occludersDirty = true;
            update();
        });
    update();
}

void Preview3DView::setTrussInteractive(bool on)
{
    if (m_trussInteractive == on)
        return;
    m_trussInteractive = on;
    if (!on)
    {
        m_dragMode = DragNone;
        m_hoverTrussId = 0;
        m_hoverPort = -1;
        unsetCursor();
    }
    update();
}

void Preview3DView::setSelectedTruss(int id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    update();
}

void Preview3DView::setFixtureScene(FixtureScene *scene)
{
    if (m_fixtureScene == scene)
        return;
    if (m_fixtureScene != NULL)
        disconnect(m_fixtureScene, nullptr, this, nullptr);
    m_fixtureScene = scene;
    if (m_fixtureScene != NULL)
    {
        connect(m_fixtureScene, &FixtureScene::changed, this, [this]() { update(); });
        connect(m_fixtureScene, &FixtureScene::liveChanged, this, [this]() { update(); });
    }
    update();
}

void Preview3DView::setFixtureInteractive(bool on)
{
    if (m_fixtureInteractive == on)
        return;
    m_fixtureInteractive = on;
    if (!on)
    {
        if (m_dragFixture)
            m_dragMode = DragNone;
        m_dragFixture = false;
        unsetCursor();
    }
    update();
}

void Preview3DView::setSelectedFixture(int id)
{
    if (m_selectedFixture == id)
        return;
    m_selectedFixture = id;
    update();
}

void Preview3DView::updateCamera()
{
    const double cp = qCos(m_pitch);
    const double sp = qSin(m_pitch);
    const double cy = qCos(m_yaw);
    const double sy = qSin(m_yaw);

    // Direction from the target to the eye.
    const QVector3D dir(sy * cp, -cy * cp, sp);
    m_eye = m_target + dir * m_dist;

    m_fwd = (m_target - m_eye).normalized();
    QVector3D r = QVector3D::crossProduct(m_fwd, QVector3D(0.0, 0.0, 1.0));
    if (r.lengthSquared() < 1e-9)
        r = QVector3D(1.0, 0.0, 0.0);
    m_right = r.normalized();
    m_up = QVector3D::crossProduct(m_right, m_fwd).normalized();
}

QVector3D Preview3DView::toCamera(const QVector3D &world) const
{
    const QVector3D d = world - m_eye;
    return QVector3D(QVector3D::dotProduct(d, m_right),
                     QVector3D::dotProduct(d, m_up),
                     QVector3D::dotProduct(d, m_fwd));
}

QPointF Preview3DView::projectCamera(const QVector3D &cam) const
{
    const double f = (height() * 0.5) / qTan(m_fovY * 0.5);
    const double z = qMax(cam.z(), 1e-6);
    return QPointF(width() * 0.5 + cam.x() * f / z,
                   height() * 0.5 - cam.y() * f / z);
}

bool Preview3DView::project(const QVector3D &world, QPointF &out) const
{
    const QVector3D cam = toCamera(world);
    if (cam.z() < m_near)
        return false;
    out = projectCamera(cam);
    return true;
}

bool Preview3DView::worldToScreen(const QVector3D &world, QPointF &screen) const
{
    return project(world, screen);
}

void Preview3DView::camera(double &yaw, double &pitch, double &dist, QVector3D &target) const
{
    yaw = m_yaw;
    pitch = m_pitch;
    dist = m_dist;
    target = m_target;
}

void Preview3DView::setCamera(double yaw, double pitch, double dist, const QVector3D &target)
{
    m_yaw = yaw;
    m_pitch = qBound(qDegreesToRadians(2.0), pitch, qDegreesToRadians(88.0));
    m_dist = qBound(0.3, dist, 5000.0);
    m_target = target;
    m_userMoved = true;
    m_hasFitted = true;
    updateCamera();
    update();
}

bool Preview3DView::screenToGround(const QPoint &screen, double zPlane, QVector3D &out) const
{
    const double f = (height() * 0.5) / qTan(m_fovY * 0.5);
    const double xc = (screen.x() - width() * 0.5) / f;
    const double yc = -(screen.y() - height() * 0.5) / f;

    const QVector3D dir = (m_right * xc + m_up * yc + m_fwd).normalized();
    if (qAbs(dir.z()) < 1e-6)
        return false;

    const double t = (zPlane - m_eye.z()) / dir.z();
    if (t <= 0.0)
        return false;

    out = m_eye + dir * t;
    return true;
}

void Preview3DView::drawPolygon(QPainter &p, const QList<QVector3D> &world,
                                const QColor &fill, const QColor &edge,
                                double edgeWidth) const
{
    QList<QVector3D> cam;
    cam.reserve(world.size());
    for (const QVector3D &w : world)
        cam << toCamera(w);

    const QList<QVector3D> clipped = clipNear(cam, m_near);
    if (clipped.size() < 3)
        return;

    QPolygonF poly;
    poly.reserve(clipped.size());
    for (const QVector3D &c : clipped)
        poly << projectCamera(c);

    p.setPen(edgeWidth > 0.0 ? QPen(edge, edgeWidth) : Qt::NoPen);
    p.setBrush(fill);
    p.drawPolygon(poly);
}

void Preview3DView::drawLine(QPainter &p, const QVector3D &a, const QVector3D &b,
                             const QColor &color, double width, Qt::PenStyle style) const
{
    QVector3D ca = toCamera(a);
    QVector3D cb = toCamera(b);
    if (!clipLine(ca, cb, m_near))
        return;

    p.setPen(QPen(color, width, style));
    p.drawLine(projectCamera(ca), projectCamera(cb));
}

QList<QPointF> Preview3DView::stageFeet() const
{
    QList<QPointF> feet;
    if (m_stageX <= 0.0 || m_stageY <= 0.0)
        return feet;

    // One foot per metre along an axis; when the length isn't a whole number
    // of metres, add one at the far edge as well.
    auto axis = [](double len) {
        QList<double> pos;
        for (double v = 0.0; v <= len + 1e-6; v += 1.0)
            pos << v;
        if (pos.isEmpty() || qAbs(pos.last() - len) > 1e-6)
            pos << len;
        return pos;
    };

    const QList<double> xs = axis(m_stageX);
    const QList<double> ys = axis(m_stageY);

    for (double x : xs)
    {
        feet << QPointF(x, 0.0);
        feet << QPointF(x, m_stageY);
    }
    for (double y : ys)
    {
        if (y <= 1e-6 || y >= m_stageY - 1e-6)
            continue;                       // corners already added
        feet << QPointF(0.0, y);
        feet << QPointF(m_stageX, y);
    }

    // Inset each foot so it sits *under* the deck (outer face flush with the
    // edge) instead of straddling the edge.
    auto inset = [](double v, double half, double len) {
        if (len <= 2.0 * half)
            return len * 0.5;
        return qBound(half, v, len - half);
    };
    for (QPointF &f : feet)
        f = QPointF(inset(f.x(), kLegHalf, m_stageX),
                    inset(f.y(), kLegHalf, m_stageY));

    return feet;
}

void Preview3DView::drawLeg(QPainter &p, double x, double y, double half,
                            double zTop, double zBottom) const
{
    const QVector3D c[4] = {
        QVector3D(x - half, y - half, zTop),
        QVector3D(x + half, y - half, zTop),
        QVector3D(x + half, y + half, zTop),
        QVector3D(x - half, y + half, zTop)
    };

    for (int i = 0; i < 4; ++i)
    {
        const QVector3D &a = c[i];
        const QVector3D &b = c[(i + 1) % 4];
        const QVector3D edgeDir = b - a;
        QVector3D outward(edgeDir.y(), -edgeDir.x(), 0.0);
        if (outward.lengthSquared() < 1e-9)
            continue;
        outward.normalize();

        const QVector3D faceCenter((a.x() + b.x()) * 0.5,
                                   (a.y() + b.y()) * 0.5,
                                   (zTop + zBottom) * 0.5);
        if (QVector3D::dotProduct(outward, m_eye - faceCenter) <= 0.0)
            continue;

        const QList<QVector3D> side = {
            a, b,
            QVector3D(b.x(), b.y(), zBottom),
            QVector3D(a.x(), a.y(), zBottom)
        };
        drawPolygon(p, side, m_legColor, m_legEdge, 0.8);
    }
}

bool Preview3DView::projectDepth(const QVector3D &world, QPointF &screen, double &invW) const
{
    const QVector3D cam = toCamera(world);
    if (cam.z() < m_near)
        return false;
    screen = projectCamera(cam);
    invW = 1.0 / cam.z();
    return true;
}

void Preview3DView::rasterPolygon(SoftwareRaster &r, const QList<QVector3D> &world,
                                  const QColor &fill, const QColor &edge, double edgeWidth) const
{
    QVarLengthArray<QVector3D, 32> cam;
    cam.reserve(world.size());
    for (const QVector3D &w : world)
        cam.append(toCamera(w));

    QVarLengthArray<QVector3D, 32> clipped;
    clipNearVar(cam, clipped, m_near);
    if (clipped.size() < 3)
        return;

    QVarLengthArray<QPointF, 32> pts;
    QVarLengthArray<double, 32> iws;
    for (const QVector3D &c : clipped)
    {
        pts.append(projectCamera(c));
        iws.append(1.0 / c.z());
    }

    // Convex polygons -> triangle fan.
    for (int i = 1; i + 1 < pts.size(); ++i)
        r.triangle(pts.at(0), pts.at(i), pts.at(i + 1),
                   iws.at(0), iws.at(i), iws.at(i + 1), fill);

    if (edgeWidth > 0.0)
        for (int i = 0; i < pts.size(); ++i)
        {
            const int j = (i + 1) % pts.size();
            r.line(pts.at(i), pts.at(j), iws.at(i), iws.at(j), edge, edgeWidth);
        }
}

void Preview3DView::rasterLine(SoftwareRaster &r, const QVector3D &a, const QVector3D &b,
                               const QColor &color, double width, Qt::PenStyle style) const
{
    QVector3D ca = toCamera(a);
    QVector3D cb = toCamera(b);
    if (!clipLine(ca, cb, m_near))
        return;

    const QPointF pa = projectCamera(ca);
    const QPointF pb = projectCamera(cb);
    const double ia = 1.0 / ca.z();
    const double ib = 1.0 / cb.z();

    if (style == Qt::DashLine)
        r.dashedLine(pa, pb, ia, ib, color, width, 6.0, 4.0);
    else
        r.line(pa, pb, ia, ib, color, width);
}

void Preview3DView::rasterFloor(SoftwareRaster &r) const
{
    const QList<QVector3D> corners = {
        QVector3D(m_rx0, m_ry0, m_zFloor),
        QVector3D(m_rx1, m_ry0, m_zFloor),
        QVector3D(m_rx1, m_ry1, m_zFloor),
        QVector3D(m_rx0, m_ry1, m_zFloor)
    };
    rasterPolygon(r, corners, m_floorFill, QColor(), 0.0);

    if (!m_showGrid)
        return;

    const double step = gridStep(qMax(m_rx1 - m_rx0, m_ry1 - m_ry0));
    for (double x = qCeil(m_rx0 / step) * step; x <= m_rx1 + 1e-6; x += step)
        rasterLine(r, QVector3D(x, m_ry0, m_zFloor), QVector3D(x, m_ry1, m_zFloor),
                   m_floorGrid, 1.0);
    for (double y = qCeil(m_ry0 / step) * step; y <= m_ry1 + 1e-6; y += step)
        rasterLine(r, QVector3D(m_rx0, y, m_zFloor), QVector3D(m_rx1, y, m_zFloor),
                   m_floorGrid, 1.0);
}

void Preview3DView::rasterRoomBox(SoftwareRaster &r) const
{
    const QVector3D b[4] = {
        QVector3D(m_rx0, m_ry0, m_zFloor),
        QVector3D(m_rx1, m_ry0, m_zFloor),
        QVector3D(m_rx1, m_ry1, m_zFloor),
        QVector3D(m_rx0, m_ry1, m_zFloor)
    };
    const QVector3D t[4] = {
        QVector3D(m_rx0, m_ry0, m_zTop),
        QVector3D(m_rx1, m_ry0, m_zTop),
        QVector3D(m_rx1, m_ry1, m_zTop),
        QVector3D(m_rx0, m_ry1, m_zTop)
    };

    for (int i = 0; i < 4; ++i)
    {
        rasterLine(r, b[i], b[(i + 1) % 4], m_roomBoxEdge, 1.3, Qt::DashLine);
        rasterLine(r, t[i], t[(i + 1) % 4], m_roomBoxEdge, 1.3, Qt::DashLine);
        rasterLine(r, b[i], t[i], m_roomBoxEdge, 1.3, Qt::DashLine);
    }
}

void Preview3DView::rasterStage(SoftwareRaster &r) const
{
    if (m_stageX <= 0.0 || m_stageY <= 0.0)
        return;

    const double riser = qMax(0.0, -m_zFloor);        // deck top to room floor
    const double deckT = qMin(kDeckThickness, riser); // the deck is thin
    const double deckBottom = -deckT;

    const QVector3D c[4] = {
        QVector3D(0.0, 0.0, 0.0),
        QVector3D(m_stageX, 0.0, 0.0),
        QVector3D(m_stageX, m_stageY, 0.0),
        QVector3D(0.0, m_stageY, 0.0)
    };

    // Legs: thin boxes, camera-facing sides only (the deck hides the rest).
    if (riser > deckT + 1e-4)
    {
        const QList<QPointF> feet = stageFeet();
        for (const QPointF &f : feet)
        {
            const double x = f.x();
            const double y = f.y();
            const QVector3D lc[4] = {
                QVector3D(x - kLegHalf, y - kLegHalf, deckBottom),
                QVector3D(x + kLegHalf, y - kLegHalf, deckBottom),
                QVector3D(x + kLegHalf, y + kLegHalf, deckBottom),
                QVector3D(x - kLegHalf, y + kLegHalf, deckBottom)
            };
            for (int i = 0; i < 4; ++i)
            {
                const QVector3D &a = lc[i];
                const QVector3D &b = lc[(i + 1) % 4];
                const QVector3D edgeDir = b - a;
                QVector3D outward(edgeDir.y(), -edgeDir.x(), 0.0);
                if (outward.lengthSquared() < 1e-9)
                    continue;
                outward.normalize();
                const QVector3D fc((a.x() + b.x()) * 0.5, (a.y() + b.y()) * 0.5,
                                   (deckBottom + m_zFloor) * 0.5);
                if (QVector3D::dotProduct(outward, m_eye - fc) <= 0.0)
                    continue;
                const QList<QVector3D> side = {
                    a, b,
                    QVector3D(b.x(), b.y(), m_zFloor),
                    QVector3D(a.x(), a.y(), m_zFloor)
                };
                rasterPolygon(r, side, shadedColor(m_legColor, outward), m_legEdge, 0.8);
            }
        }
    }

    // Deck rim, camera-facing only.
    if (deckT > 1e-4)
    {
        for (int i = 0; i < 4; ++i)
        {
            const QVector3D &a = c[i];
            const QVector3D &b = c[(i + 1) % 4];
            const QVector3D edgeDir = b - a;
            QVector3D outward(edgeDir.y(), -edgeDir.x(), 0.0);
            if (outward.lengthSquared() < 1e-9)
                continue;
            outward.normalize();
            const QVector3D fc((a.x() + b.x()) * 0.5, (a.y() + b.y()) * 0.5,
                               deckBottom * 0.5);
            if (QVector3D::dotProduct(outward, m_eye - fc) <= 0.0)
                continue;
            const QList<QVector3D> side = {
                a, b,
                QVector3D(b.x(), b.y(), deckBottom),
                QVector3D(a.x(), a.y(), deckBottom)
            };
            rasterPolygon(r, side, m_stageSide, m_stageEdge, 1.0);
        }
    }

    // Deck top.
    const QList<QVector3D> top = { c[0], c[1], c[2], c[3] };
    rasterPolygon(r, top, m_stageTop, m_stageEdge, 2.0);

    if (!m_showGrid)
        return;

    const double step = gridStep(qMax(m_stageX, m_stageY));
    for (double x = qCeil(0.0 / step) * step; x <= m_stageX + 1e-6; x += step)
        rasterLine(r, QVector3D(x, 0.0, 0.0), QVector3D(x, m_stageY, 0.0),
                   m_stageTopGrid, 1.0);
    for (double y = qCeil(0.0 / step) * step; y <= m_stageY + 1e-6; y += step)
        rasterLine(r, QVector3D(0.0, y, 0.0), QVector3D(m_stageX, y, 0.0),
                   m_stageTopGrid, 1.0);
}

void Preview3DView::rasterTrusses(SoftwareRaster &r) const
{
    if (m_scene == NULL)
        return;

    for (const Truss &t : m_scene->trusses())
    {
        if (!t.visible)
            continue;

        const bool sel = m_trussInteractive && (t.id == m_selectedId);
        const QColor chord = sel ? m_selectColor : t.color;
        const QColor brace = sel ? m_selectColor.darker(115) : t.color.darker(125);
        const double cw = sel ? 3.0 : 2.4;
        const double bw = sel ? 1.8 : 1.3;

        const QList<TrussSegment> segs = t.segments(m_scene->linkedPorts(t.id), true);
        for (const TrussSegment &s : segs)
            rasterLine(r, s.a, s.b, s.chord ? chord : brace, s.chord ? cw : bw);
    }
}

void Preview3DView::rasterBox(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                              const QVector3D &size, const QColor &fill, const QColor &edge) const
{
    const QVector3D h = size * 0.5f;
    const QVector3D lc[8] = {
        QVector3D(-h.x(), -h.y(), -h.z()), QVector3D( h.x(), -h.y(), -h.z()),
        QVector3D( h.x(),  h.y(), -h.z()), QVector3D(-h.x(),  h.y(), -h.z()),
        QVector3D(-h.x(), -h.y(),  h.z()), QVector3D( h.x(), -h.y(),  h.z()),
        QVector3D( h.x(),  h.y(),  h.z()), QVector3D(-h.x(),  h.y(),  h.z())
    };

    QVector3D c[8];
    for (int i = 0; i < 8; ++i)
        c[i] = rot.rotatedVector(lc[i]) + center;

    static const int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {1,2,6,5}, {2,3,7,6}, {3,0,4,7}
    };
    static const QVector3D normals[6] = {
        QVector3D(0,0,-1), QVector3D(0,0,1), QVector3D(0,-1,0),
        QVector3D(1,0,0), QVector3D(0,1,0), QVector3D(-1,0,0)
    };

    for (int f = 0; f < 6; ++f)
    {
        const QVector3D n = rot.rotatedVector(normals[f]);
        const QVector3D fc = (c[faces[f][0]] + c[faces[f][1]] + c[faces[f][2]] + c[faces[f][3]]) * 0.25f;
        if (QVector3D::dotProduct(n, m_eye - fc) <= 0.0)
            continue;

        const QList<QVector3D> poly = {
            c[faces[f][0]], c[faces[f][1]], c[faces[f][2]], c[faces[f][3]]
        };
        rasterPolygon(r, poly, shadedColor(fill, n), edge, 1.0);
    }
}

void Preview3DView::rasterDisk(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                               double radius, const QColor &fill, const QColor &edge) const
{
    const QVector3D n = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
    if (QVector3D::dotProduct(n, m_eye - center) <= 0.0)
        return;

    const int segs = 20;
    QList<QVector3D> ring;
    ring.reserve(segs);
    for (int i = 0; i < segs; ++i)
    {
        const double a = 2.0 * M_PI * i / segs;
        ring << center + rot.rotatedVector(QVector3D(qCos(a) * radius, 0.0, qSin(a) * radius));
    }
    rasterPolygon(r, ring, fill, edge, 1.0);
}

void Preview3DView::rasterCylinder(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                                   double radius, double length, const QColor &fill, const QColor &edge,
                                   int segs) const
{
    const double half = length * 0.5;
    QList<QVector3D> ring0, ring1;
    ring0.reserve(segs);
    ring1.reserve(segs);
    for (int i = 0; i < segs; ++i)
    {
        const double a = 2.0 * M_PI * i / segs;
        const double x = qCos(a) * radius;
        const double z = qSin(a) * radius;
        ring0 << center + rot.rotatedVector(QVector3D(x, -half, z));
        ring1 << center + rot.rotatedVector(QVector3D(x,  half, z));
    }

    const QVector3D axis = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));

    // Side quads (back faces culled against the eye).
    for (int i = 0; i < segs; ++i)
    {
        const int j = (i + 1) % segs;
        const QVector3D mid = (ring0.at(i) + ring0.at(j) + ring1.at(j) + ring1.at(i)) * 0.25f;
        const QVector3D n = (mid - center).normalized();
        if (QVector3D::dotProduct(n, m_eye - mid) <= 0.0)
            continue;
        rasterPolygon(r, { ring0.at(i), ring0.at(j), ring1.at(j), ring1.at(i) },
                      shadedColor(fill, n), edge, 1.0);
    }

    // Caps.
    if (QVector3D::dotProduct(axis, m_eye - (center + axis * half)) > 0.0)
        rasterPolygon(r, ring1, shadedColor(fill.lighter(112), axis, 0.82, 0.3), edge, 1.0);
    if (QVector3D::dotProduct(-axis, m_eye - (center - axis * half)) > 0.0)
        rasterPolygon(r, ring0, shadedColor(fill.darker(118), -axis, 0.82, 0.3), edge, 1.0);
}

void Preview3DView::rasterDome(SoftwareRaster &r, const QVector3D &base, const QQuaternion &rot,
                               double radius, double height, int segs, int rings,
                               const QColor &fill, const QColor &edge) const
{
    QList<QList<QVector3D>> lat;
    for (int j = 0; j <= rings; ++j)
    {
        const double phi = (M_PI * 0.5) * j / rings;
        const double rr = radius * qCos(phi);
        const double hh = height * qSin(phi);
        QList<QVector3D> ring;
        for (int i = 0; i < segs; ++i)
        {
            const double a = 2.0 * M_PI * i / segs;
            ring << base + rot.rotatedVector(QVector3D(qCos(a) * rr, hh, qSin(a) * rr));
        }
        lat << ring;
    }

    for (int j = 0; j < rings; ++j)
    {
        for (int i = 0; i < segs; ++i)
        {
            const int k = (i + 1) % segs;
            const QVector3D mid = (lat.at(j).at(i) + lat.at(j).at(k) +
                                   lat.at(j + 1).at(k) + lat.at(j + 1).at(i)) * 0.25f;
            const QVector3D n = (mid - base).normalized();
            rasterPolygon(r, { lat.at(j).at(i), lat.at(j).at(k),
                               lat.at(j + 1).at(k), lat.at(j + 1).at(i) },
                          shadedColor(fill, n), edge, 1.0);
        }
    }
}

Preview3DView::BeamHit Preview3DView::beamHit(const QVector3D &origin, const QVector3D &dir) const
{
    BeamHit hit;

    // Slab test against the room box: find where the ray leaves it.
    double tmax = 1e9;
    int axis = 2;
    const double o[3] = { origin.x(), origin.y(), origin.z() };
    const double d[3] = { dir.x(), dir.y(), dir.z() };
    const double lo[3] = { m_rx0, m_ry0, m_zFloor };
    const double hi[3] = { m_rx1, m_ry1, m_zTop };
    for (int i = 0; i < 3; ++i)
    {
        if (qAbs(d[i]) < 1e-9)
        {
            if (o[i] < lo[i] || o[i] > hi[i])
            {
                hit.t = 0.05;
                hit.point = origin + dir * 0.05;
                hit.normal = QVector3D(i == 0 ? 1.0 : 0.0, i == 1 ? 1.0 : 0.0, i == 2 ? 1.0 : 0.0);
                return hit;
            }
            continue;
        }
        double t1 = (lo[i] - o[i]) / d[i];
        double t2 = (hi[i] - o[i]) / d[i];
        if (t1 > t2)
            qSwap(t1, t2);
        if (t2 < tmax)
        {
            tmax = t2;
            axis = i;
        }
    }
    if (tmax < 0.05)
        tmax = 0.05;

    hit.t = tmax;
    hit.point = origin + dir * tmax;
    hit.normal = QVector3D(axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0);

    // A downward beam stops on the stage deck (z = 0, inside the footprint) or
    // on the room floor, whichever comes first.
    if (dir.z() < -1e-6)
    {
        if (origin.z() > 0.0)
        {
            const double t = (0.0 - origin.z()) / dir.z();
            if (t > 0.05 && t < hit.t)
            {
                const QVector3D p = origin + dir * t;
                if (p.x() >= 0.0 && p.x() <= m_stageX && p.y() >= 0.0 && p.y() <= m_stageY)
                {
                    hit.t = t;
                    hit.point = p;
                    hit.normal = QVector3D(0.0, 0.0, 1.0);
                }
            }
        }
        if (origin.z() > m_zFloor)
        {
            const double t = (m_zFloor - origin.z()) / dir.z();
            if (t > 0.05 && t < hit.t)
            {
                hit.t = t;
                hit.point = origin + dir * t;
                hit.normal = QVector3D(0.0, 0.0, 1.0);
            }
        }
    }

    return hit;
}

double Preview3DView::beamLength(const QVector3D &origin, const QVector3D &dir) const
{
    return qMax(0.05, beamHit(origin, dir).t);
}

namespace
{
/** Ray vs axis-aligned box; t is the entry (outside) or exit (inside). */
bool rayBoxT(const QVector3D &o, const QVector3D &d,
             const QVector3D &lo, const QVector3D &hi, double &t)
{
    double tmin = -1e9, tmax = 1e9;
    const double ov[3] = { o.x(), o.y(), o.z() };
    const double dv[3] = { d.x(), d.y(), d.z() };
    const double lov[3] = { lo.x(), lo.y(), lo.z() };
    const double hiv[3] = { hi.x(), hi.y(), hi.z() };
    for (int i = 0; i < 3; ++i)
    {
        if (qAbs(dv[i]) < 1e-9)
        {
            if (ov[i] < lov[i] || ov[i] > hiv[i])
                return false;
            continue;
        }
        double t1 = (lov[i] - ov[i]) / dv[i];
        double t2 = (hiv[i] - ov[i]) / dv[i];
        if (t1 > t2)
            qSwap(t1, t2);
        tmin = qMax(tmin, t1);
        tmax = qMin(tmax, t2);
    }
    if (tmax < tmin || tmax < 0.0)
        return false;
    t = (tmin > 0.0) ? tmin : tmax;
    return t > 0.0;
}

/** Ray vs a capsule (segment a-b with `radius`), closest-approach test. */
bool rayCapsule(const QVector3D &o, const QVector3D &d,
                const QVector3D &a, const QVector3D &b, double radius, double &t)
{
    const QVector3D ab = b - a;
    const double C = QVector3D::dotProduct(ab, ab);
    if (C < 1e-9)
        return false;
    const QVector3D w0 = o - a;
    const double D = QVector3D::dotProduct(d, ab);
    const double W = QVector3D::dotProduct(d, w0);
    const double E = QVector3D::dotProduct(ab, w0);
    const double denom = C - D * D;
    double s = (denom > 1e-9) ? (E - W * D) / denom : 0.0;
    s = qBound(0.0, s, 1.0);
    double tc = s * D - W;
    if (tc < 0.0)
        tc = 0.0;
    const QVector3D p = o + d * tc;
    const QVector3D q = a + ab * s;
    if ((p - q).lengthSquared() > radius * radius)
        return false;
    t = tc;
    return t > 0.05;
}
} // namespace

double Preview3DView::beamRay(const QVector3D &origin, const QVector3D &dir) const
{
    double t = 1e9;
    double tb;

    // Room box (the origin is inside, so this is the exit through a wall/floor).
    if (rayBoxT(origin, dir, QVector3D(m_rx0, m_ry0, m_zFloor),
                QVector3D(m_rx1, m_ry1, m_zTop), tb))
        t = qMin(t, tb);

    // Stage deck as a solid box (occludes beams that pass under/beside it).
    if (m_stageX > 0.0 && m_stageY > 0.0)
    {
        const double riser = qMax(0.0, -m_zFloor);
        const double deckT = qMin(kDeckThickness, riser);
        if (rayBoxT(origin, dir, QVector3D(0.0, 0.0, -deckT),
                    QVector3D(m_stageX, m_stageY, 0.0), tb))
            t = qMin(t, tb);
    }

    // Truss shadows.
    for (const Occluder &oc : m_occluders)
    {
        if (rayCapsule(origin, dir, oc.a, oc.b, oc.radius, tb))
            t = qMin(t, tb);
    }

    return qMax(0.05, t);
}

void Preview3DView::rasterFixtureBeam(SoftwareRaster &r, const QVector3D &origin,
                                      const QQuaternion &rot, double beamDeg,
                                      const QColor &color, double length, int alpha) const
{
    const QVector3D dir = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
    const bool autoLen = (length <= 0.0);

    const double tanHalf = qTan(qDegreesToRadians(beamDeg * 0.5));

    QVector3D u = QVector3D::crossProduct(dir, QVector3D(0.0, 0.0, 1.0));
    if (u.lengthSquared() < 1e-6)
        u = QVector3D(1.0, 0.0, 0.0);
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(dir, u).normalized();

    // Collide each cone edge independently, so only the parts of the beam that
    // hit a surface stop there (stage edge, deck sides, walls, trusses).
    const int N = 16;
    QList<QVector3D> base;
    for (int i = 0; i < N; ++i)
    {
        const double a = 2.0 * M_PI * i / N;
        const QVector3D edge = (dir + u * (tanHalf * qCos(a)) + v * (tanHalf * qSin(a))).normalized();
        const double t = autoLen ? beamRay(origin, edge) : length;
        base << origin + edge * (t * 0.99);   // pull off the surface (anti z-fight)
    }

    QColor beamCol = color;
    beamCol.setAlpha(alpha);
    r.setDepthWrite(false);
    for (int i = 0; i < N; ++i)
    {
        const QList<QVector3D> tri = { origin, base.at(i), base.at((i + 1) % N) };
        rasterPolygon(r, tri, beamCol, QColor(), 0.0);
    }
    r.setDepthWrite(true);
}

void Preview3DView::rasterRectBeam(SoftwareRaster &r, const QVector3D &origin,
                                   const QQuaternion &rot, double halfW, double halfH,
                                   double length, double spread, const QColor &color,
                                   int alpha) const
{
    const QVector3D dir = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
    const QVector3D rx = rot.rotatedVector(QVector3D(1.0, 0.0, 0.0));
    const QVector3D rz = rot.rotatedVector(QVector3D(0.0, 0.0, 1.0));
    const double ew = halfW + spread;
    const double eh = halfH + spread;
    const double len = qMax(0.05, length);

    // Collide each far corner independently.
    auto far = [&](double sx, double sz) {
        const QVector3D e = (dir + rx * (ew / len) * sx + rz * (eh / len) * sz).normalized();
        return origin + e * (beamRay(origin, e) * 0.99);
    };

    const QVector3D n0 = origin - rx * halfW - rz * halfH;
    const QVector3D n1 = origin + rx * halfW - rz * halfH;
    const QVector3D n2 = origin + rx * halfW + rz * halfH;
    const QVector3D n3 = origin - rx * halfW + rz * halfH;
    const QVector3D f0 = far(-1.0, -1.0);
    const QVector3D f1 = far( 1.0, -1.0);
    const QVector3D f2 = far( 1.0,  1.0);
    const QVector3D f3 = far(-1.0,  1.0);

    QColor col = color;
    col.setAlpha(alpha);
    r.setDepthWrite(false);
    rasterPolygon(r, { n0, n1, f1, f0 }, col, QColor(), 0.0);
    rasterPolygon(r, { n1, n2, f2, f1 }, col, QColor(), 0.0);
    rasterPolygon(r, { n2, n3, f3, f2 }, col, QColor(), 0.0);
    rasterPolygon(r, { n3, n0, f0, f3 }, col, QColor(), 0.0);
    QColor pool = color;
    pool.setAlpha(qMin(255, alpha + 30));
    rasterPolygon(r, { f0, f1, f2, f3 }, pool, QColor(), 0.0);
    r.setDepthWrite(true);
}

void Preview3DView::rasterBarnDoors(SoftwareRaster &r, const QVector3D &center,
                                    const QQuaternion &rot, double radius,
                                    double length, double tilt) const
{
    const QColor fill = QColor(40, 42, 46);
    const QColor edge = QColor(122, 126, 134);
    const double c = qCos(tilt);
    const double s = qSin(tilt);
    const double R = radius;
    const double L = length;

    auto door = [&](const QVector3D &a0, const QVector3D &a1, const QVector3D &d) {
        const QVector3D p0 = center + rot.rotatedVector(a0);
        const QVector3D p1 = center + rot.rotatedVector(a1);
        const QVector3D p2 = center + rot.rotatedVector(a1 + d * L);
        const QVector3D p3 = center + rot.rotatedVector(a0 + d * L);
        rasterPolygon(r, { p0, p1, p2, p3 }, fill, edge, 1.0);
    };

    door(QVector3D(-R, 0, R),  QVector3D(R, 0, R),  QVector3D(0, c, s));   // top
    door(QVector3D(R, 0, -R),  QVector3D(-R, 0, -R), QVector3D(0, c, -s)); // bottom
    door(QVector3D(-R, 0, -R), QVector3D(-R, 0, R),  QVector3D(-s, c, 0)); // left
    door(QVector3D(R, 0, R),   QVector3D(R, 0, -R),  QVector3D(s, c, 0));  // right
}

void Preview3DView::invalidateOccluders()
{
    m_occludersDirty = true;
}

void Preview3DView::rebuildOccluders() const
{
    if (m_occludersDirty == false)
        return;

    m_occluders.clear();
    if (m_scene != NULL)
    {
        for (const Truss &t : m_scene->trusses())
        {
            if (!t.visible)
                continue;
            const QList<TrussSegment> segs = t.segments(m_scene->linkedPorts(t.id), true);
            for (const TrussSegment &s : segs)
                m_occluders << Occluder{ s.a, s.b, s.chord ? 0.030 : 0.018 };
        }
    }
    m_occludersDirty = false;
}

void Preview3DView::rasterFixtures(SoftwareRaster &r) const
{
    if (m_fixtureScene == NULL)
        return;

    for (const FixtureInstance &fi : m_fixtureScene->fixtures())
    {
        if (!fi.visible)
            continue;

        const bool sel = m_fixtureInteractive && ((int)fi.id == m_selectedFixture);
        const QColor shell = sel ? m_selectColor : QColor(58, 60, 66);
        const QColor shellEdge = sel ? m_selectColor : QColor(96, 100, 108);
        const QColor metal = sel ? m_selectColor.darker(115) : QColor(78, 82, 89);

        // Live (operate mode) brightness and colour.
        const bool live = fi.live;
        const double inten = live ? fi.liveIntensity : 1.0;
        const bool lit = (!live) || (inten > 0.02);
        const QColor gel = sel ? m_selectColor : (live ? fi.liveColor : fi.color);
        const QColor lens = (lit || sel) ? gel : gel.darker(280);

        const QQuaternion rot = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), fi.yaw)
                              * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), fi.pitch);

        switch (fi.model)
        {
        case FixtureModel::Dimmer:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();

            // Chassis.
            rasterBox(r, fi.pos, rot, QVector3D(sx, sy, sz), shell, shellEdge);

            // Heat-sink fins across the top.
            const int fins = 7;
            for (int i = 0; i < fins; ++i)
            {
                const double fx = -sx * 0.5 + sx * (i + 0.5) / fins;
                rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(fx, 0.0, sz * 0.5 + 0.012)),
                          rot, QVector3D(sx / (fins * 2.0), sy * 0.9, 0.024),
                          shell.lighter(118), shellEdge.darker(130));
            }

            // Front panel (facing the beam axis) with indicator LEDs.
            const double py = sy * 0.5 + 0.003;
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, py, 0.0)), rot,
                      QVector3D(sx * 0.9, 0.006, sz * 0.66), shell.darker(150), shellEdge.darker(140));

            const int leds = 4;
            for (int i = 0; i < leds; ++i)
            {
                const double lx = -sx * 0.34 + sx * 0.68 * (i + 0.5) / leds;
                const QColor led = sel ? m_selectColor
                                       : (i == 0 ? gel.lighter(130) : QColor(120, 230, 140));
                rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(lx, py + 0.006, 0.0)), rot,
                           qMin(sx * 0.035, sz * 0.22), led, shellEdge.darker(140));
            }
            break;
        }
        case FixtureModel::Lamp:
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.3);

            // Yoke: base plate + two side arms.
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, -len * 0.1, -(radius + 0.022))),
                      rot, QVector3D(radius * 2.4, len * 0.6, 0.03), metal, metal.darker(130));
            for (double s : { -1.0, 1.0 })
            {
                rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(s * (radius + 0.018), -len * 0.1, 0.0)),
                          rot, QVector3D(0.03, len * 0.6, radius * 1.05), metal, metal.darker(130));
            }

            // Body (can) + rear lamp cap.
            rasterCylinder(r, fi.pos, rot, radius, len, shell, shellEdge, 18);
            rasterCylinder(r, fi.pos + rot.rotatedVector(QVector3D(0.0, -len * 0.5 - 0.025, 0.0)), rot,
                           radius * 0.6, 0.05, shell.darker(115), shellEdge, 14);

            // Front rim + warm lens.
            const QVector3D front = fi.pos + rot.rotatedVector(QVector3D(0.0, len * 0.5, 0.0));
            rasterCylinder(r, front + rot.rotatedVector(QVector3D(0.0, 0.008, 0.0)), rot,
                           radius * 1.08, 0.018, shellEdge, shellEdge.darker(130), 18);
            rasterDisk(r, front + rot.rotatedVector(QVector3D(0.0, 0.022, 0.0)), rot,
                       radius * 0.8, lens, shellEdge.darker(120));

            // Barn doors make it read as a conventional theatre lantern.
            rasterBarnDoors(r, front + rot.rotatedVector(QVector3D(0.0, 0.022, 0.0)), rot,
                            radius * 1.05, radius * 0.85, 0.95);
            break;
        }
        case FixtureModel::Bar:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            const double len = qMax(sy, sz);

            // Housing: a long bar.
            rasterBox(r, fi.pos, rot, QVector3D(sx, len, sz), shell, shellEdge);

            const int cols = qMax(1, fi.ledCols);
            const int rows = qMax(1, fi.ledRows);
            const double dx = sx / cols;
            const double dz = sz / rows;
            const double lr = qMax(0.006, qMin(dx, dz) * 0.36);

            // Lens grid (cols x rows) on the beam face.
            for (int cix = 0; cix < cols; ++cix)
            {
                const double lx = -sx * 0.5 + dx * (cix + 0.5);
                for (int rix = 0; rix < rows; ++rix)
                {
                    const double lz = -sz * 0.5 + dz * (rix + 0.5);
                    QColor pc = lens;
                    if (live && !fi.liveHeads.isEmpty())
                    {
                        const int idx = rix * cols + cix;
                        const QColor hc = fi.liveHeads.at(idx % fi.liveHeads.size());
                        const double hi = (idx < fi.liveHeadIntensity.size())
                                        ? fi.liveHeadIntensity.at(idx) : inten;
                        pc = (hi > 0.02 || sel) ? (sel ? m_selectColor : hc) : hc.darker(280);
                    }
                    rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(lx, len * 0.5 + 0.004, lz)),
                               rot, lr, pc, shellEdge.darker(130));
                }
            }
            break;
        }
        case FixtureModel::Derby:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            const double R = qMax(0.05, 0.4 * qMax(sx, sy));
            const double baseH = qMax(0.03, sz * 0.35);
            const double domeH = qMax(0.05, sz * 0.55);

            // Cylinder axes are local +Y, so spin them to stand up (+Z).
            const QQuaternion up = rot * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), 90.0);
            const QVector3D baseC = fi.pos + rot.rotatedVector(QVector3D(0.0, 0.0, -sz * 0.35));

            // Base + dome.
            rasterCylinder(r, baseC, up, R, baseH, shell, shellEdge, 18);
            rasterDome(r, baseC + rot.rotatedVector(QVector3D(0.0, 0.0, baseH * 0.5)),
                       up, R * 0.95, domeH, 14, 3, shell.lighter(112), shellEdge);
            break;
        }
        case FixtureModel::MovingHead:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            const double bodyR = qMax(0.05, 0.32 * qMin(sx, sy));
            const double drumLen = qMax(bodyR * 1.5, sx * 0.8);

            double mhYaw, mhPitch;
            if (live && fi.liveDirValid)
            {
                // Orient the head so its beam points along the calibrated
                // world direction.
                const QVector3D d = fi.liveDir.normalized();
                mhPitch = qRadiansToDegrees(qAsin(qBound(-1.0, double(d.z()), 1.0)));
                mhYaw = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
            }
            else
            {
                mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
                mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            }
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), mhPitch);
            // The drum axis lies along the tilt axis (local X).
            const QQuaternion drum = head * QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), -90.0);

            // Fixed base at the top.
            rasterBox(r, fi.pos + QVector3D(0.0, 0.0, sz * 0.42), QQuaternion(),
                      QVector3D(sx * 0.85, sy * 0.7, sz * 0.16), shell, shellEdge);

            // Yoke arms (pan only).
            for (double s : { -1.0, 1.0 })
            {
                rasterBox(r, fi.pos + pan.rotatedVector(QVector3D(s * (drumLen * 0.5 + 0.02), 0.0, 0.0)),
                          pan, QVector3D(0.03, sy * 0.7, sz * 0.5), metal, metal.darker(130));
            }

            // Head drum (pan + tilt) with a lens facing the beam.
            rasterCylinder(r, fi.pos, drum, bodyR, drumLen, shell, shellEdge, 18);
            const QVector3D front = fi.pos + head.rotatedVector(QVector3D(0.0, bodyR, 0.0));
            rasterDisk(r, front + head.rotatedVector(QVector3D(0.0, 0.012, 0.0)), head,
                       bodyR * 0.62, lens, shellEdge.darker(120));
            break;
        }
        case FixtureModel::Scanner:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();

            double mhYaw, mhPitch;
            if (live && fi.liveDirValid)
            {
                const QVector3D d = fi.liveDir.normalized();
                mhPitch = qRadiansToDegrees(qAsin(qBound(-1.0, double(d.z()), 1.0)));
                mhYaw = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
            }
            else
            {
                mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
                mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            }
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), mhPitch);

            // Low base and a compact mirror head on top.
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, 0.0, -sz * 0.22)),
                      rot, QVector3D(sx, sy, sz * 0.55), shell, shellEdge);
            for (double side : { -1.0, 1.0 })
            {
                rasterBox(r, fi.pos + pan.rotatedVector(QVector3D(side * sx * 0.32, 0.0, sz * 0.12)),
                          pan, QVector3D(0.025, sy * 0.55, sz * 0.45),
                          metal, metal.darker(130));
            }
            rasterBox(r, fi.pos + QVector3D(0.0, 0.0, sz * 0.24), head,
                      QVector3D(sx * 0.62, sy * 0.58, sz * 0.42), shell, shellEdge);
            const QVector3D mirror = fi.pos + QVector3D(0.0, 0.0, sz * 0.24)
                                   + head.rotatedVector(QVector3D(0.0, sy * 0.3, 0.0));
            rasterDisk(r, mirror + head.rotatedVector(QVector3D(0.0, 0.008, 0.0)),
                       head, qMin(sx, sy) * 0.18, lens, shellEdge.darker(120));
            break;
        }
        case FixtureModel::Strobe:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();

            rasterBox(r, fi.pos, rot, QVector3D(sx, sy, sz), shell, shellEdge);

            // Recessed reflector and a linear xenon tube.
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + 0.004, 0.0)),
                      rot, QVector3D(sx * 0.88, 0.008, sz * 0.62),
                      shell.darker(155), shellEdge.darker(140));
            const QQuaternion tubeRot = rot
                * QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), -90.0);
            rasterCylinder(r, fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + 0.018, 0.0)),
                           tubeRot, qMax(0.006, sz * 0.055), sx * 0.76, lens,
                           shellEdge.darker(120), 12);
            break;
        }
        case FixtureModel::Laser:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();

            rasterBox(r, fi.pos, rot, QVector3D(sx, sy, sz), shell, shellEdge);
            // Front aperture and a small status LED.
            rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + 0.006, 0.0)),
                       rot, qMin(sx, sz) * 0.18, lens, shellEdge.darker(120));
            rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(sx * 0.28, sy * 0.5 + 0.006, -sz * 0.2)),
                       rot, qMin(sx, sz) * 0.035, QColor(120, 230, 140),
                       shellEdge.darker(140));
            break;
        }
        case FixtureModel::Smoke:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();

            // Chassis.
            rasterBox(r, fi.pos, rot, QVector3D(sx, sy, sz), shell, shellEdge);

            // Front control panel with two indicators.
            const double py = sy * 0.5 + 0.003;
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, py, 0.0)), rot,
                      QVector3D(sx * 0.55, 0.006, sz * 0.5), shell.darker(150), shellEdge.darker(140));
            rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(-sx * 0.12, py + 0.006, sz * 0.08)), rot,
                       qMin(sx, sz) * 0.035, sel ? m_selectColor : QColor(120, 230, 140),
                       shellEdge.darker(140));
            rasterDisk(r, fi.pos + rot.rotatedVector(QVector3D(sx * 0.0, py + 0.006, sz * 0.08)), rot,
                       qMin(sx, sz) * 0.035, sel ? m_selectColor : QColor(235, 90, 70),
                       shellEdge.darker(140));

            // Nozzle at the front-top.
            const double nozzleR = qMax(0.015, qMin(sx, sz) * 0.12);
            const QVector3D nozzle = fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + sy * 0.16, sz * 0.26));
            rasterCylinder(r, nozzle, rot, nozzleR, sy * 0.32, metal, metal.darker(130), 12);

            // Fluid bottle on top.
            const QQuaternion up = rot * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), 90.0);
            const double tankR = qMax(0.03, sx * 0.12);
            const double tankH = sz * 0.55;
            rasterCylinder(r, fi.pos + rot.rotatedVector(QVector3D(-sx * 0.3, 0.0, sz * 0.5 + tankH * 0.5)),
                           up, tankR, tankH, QColor(96, 150, 196), shellEdge, 12);
            rasterCylinder(r, fi.pos + rot.rotatedVector(QVector3D(-sx * 0.3, 0.0, sz * 0.5 + tankH * 0.95)),
                           up, tankR * 0.55, tankH * 0.2, shell, shellEdge, 10);

            // Handle.
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(sx * 0.18, 0.0, sz * 0.5 + 0.03)), rot,
                      QVector3D(sx * 0.5, 0.03, 0.03), metal, metal.darker(130));
            break;
        }
        default: // Par
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.1);

            // Yoke: base plate + two side arms (drawn before the can).
            rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(0.0, -len * 0.12, -(radius + 0.022))),
                      rot, QVector3D(radius * 2.4, len * 0.62, 0.03), metal, metal.darker(130));
            for (double s : { -1.0, 1.0 })
            {
                rasterBox(r, fi.pos + rot.rotatedVector(QVector3D(s * (radius + 0.018), -len * 0.12, 0.0)),
                          rot, QVector3D(0.03, len * 0.62, radius * 1.05), metal, metal.darker(130));
            }

            // Can body.
            rasterCylinder(r, fi.pos, rot, radius, len, shell, shellEdge, 18);

            // Front rim and lens.
            const QVector3D front = fi.pos + rot.rotatedVector(QVector3D(0.0, len * 0.5, 0.0));
            rasterCylinder(r, front + rot.rotatedVector(QVector3D(0.0, 0.008, 0.0)), rot,
                           radius * 1.05, 0.016, shellEdge, shellEdge.darker(130), 18);
            rasterDisk(r, front + rot.rotatedVector(QVector3D(0.0, 0.02, 0.0)), rot,
                       radius * 0.82, lens, shellEdge.darker(120));

            break;
        }
        }
    }

    // --- Pass 2: additive light beams --------------------------------------
    // Drawn after all opaque geometry so bodies occlude them, depth-tested but
    // without depth writes so overlapping beams mix additively. Only shown in
    // operate mode, or in design mode when "Beam preview" is enabled.
    const bool liveMode = m_fixtureScene->isLive();
    const bool showBeams = liveMode || m_beamPreview;

    // Truss occluders (beam shadows) are static between truss edits.
    rebuildOccluders();

    r.setAdditive(true);
    r.setDepthWrite(false);

    for (const FixtureInstance &fi : m_fixtureScene->fixtures())
    {
        if (!fi.visible || !showBeams)
            continue;

        const bool sel = m_fixtureInteractive && ((int)fi.id == m_selectedFixture);
        const bool live = fi.live;
        const double inten = live ? fi.liveIntensity : 1.0;
        const bool lit = (!live) || (inten > 0.02) || (fi.model == FixtureModel::Derby);
        if (!lit)
            continue;
        const QColor gel = sel ? m_selectColor : (live ? fi.liveColor : fi.color);

        const QQuaternion rot = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), fi.yaw)
                              * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), fi.pitch);

        switch (fi.model)
        {
        case FixtureModel::Lamp:
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.3);
            const QVector3D front = fi.pos + rot.rotatedVector(QVector3D(0.0, len * 0.5 + 0.022, 0.0));
            rasterFixtureBeam(r, front, rot, fi.beamDeg, gel, 0.0,
                              qBound(0, int(80.0 * inten), 90));
            break;
        }
        case FixtureModel::Bar:
        {
            const double sx = fi.size.x();
            const double sz = fi.size.z();
            const double len = qMax(fi.size.y(), sz);
            const int cols = qMax(1, fi.ledCols);
            const double dx = sx / cols;
            const QVector3D dir = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
            for (int cix = 0; cix < cols; ++cix)
            {
                const double lx = -sx * 0.5 + dx * (cix + 0.5);
                const QVector3D o = fi.pos + rot.rotatedVector(QVector3D(lx, len * 0.5 + 0.006, 0.0));
                const double blen = beamLength(o, dir);
                const double spread = qTan(qDegreesToRadians(qBound(6.0, fi.beamDeg, 45.0) * 0.5)) * blen;
                QColor bc = gel;
                double bi = inten;
                if (live && !fi.liveHeads.isEmpty())
                {
                    const int idx = cix % fi.liveHeads.size();
                    bc = fi.liveHeads.at(idx);
                    if (idx < fi.liveHeadIntensity.size())
                        bi = fi.liveHeadIntensity.at(idx);
                }
                const int a = qBound(0, int(55.0 * bi), 70);
                if (a > 0)
                    rasterRectBeam(r, o, rot, dx * 0.45, sz * 0.5, blen, spread, bc, a);
            }
            break;
        }
        case FixtureModel::Derby:
        {
            const double sz = fi.size.z();
            const double baseH = qMax(0.03, sz * 0.35);
            const double domeH = qMax(0.05, sz * 0.55);
            const QVector3D baseC = fi.pos + rot.rotatedVector(QVector3D(0.0, 0.0, -sz * 0.35));
            const QVector3D domeTop = baseC + rot.rotatedVector(QVector3D(0.0, 0.0, baseH * 0.5 + domeH * 0.8));
            const int beams = 6;
            const double tilt = 0.9;
            const double ct = qCos(tilt);
            const double st = qSin(tilt);
            for (int i = 0; i < beams; ++i)
            {
                const double a = 2.0 * M_PI * i / beams;
                const QVector3D localDir(qCos(a) * st, qSin(a) * st, -ct);
                const QQuaternion dirRot = rot
                    * QQuaternion::rotationTo(QVector3D(0.0, 1.0, 0.0), localDir.normalized());
                const QColor beamCol = sel ? m_selectColor
                                           : (live ? gel
                                                   : QColor::fromHsv((i * 360 / beams + 20) % 360, 200, 255));
                rasterFixtureBeam(r, domeTop, dirRot, qBound(6.0, fi.beamDeg, 18.0), beamCol, 0.0,
                                  qBound(45, int(70.0 * inten), 80));
            }
            break;
        }
        case FixtureModel::MovingHead:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            double mhYaw, mhPitch;
            if (live && fi.liveDirValid)
            {
                const QVector3D d = fi.liveDir.normalized();
                mhPitch = qRadiansToDegrees(qAsin(qBound(-1.0, double(d.z()), 1.0)));
                mhYaw = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
            }
            else
            {
                mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
                mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            }
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), mhPitch);
            const double bodyR = qMax(0.05, 0.32 * qMin(sx, sy));
            const QVector3D front = fi.pos + head.rotatedVector(QVector3D(0.0, bodyR + 0.012, 0.0));
            rasterFixtureBeam(r, front, head, fi.beamDeg, gel, 0.0,
                              qBound(0, int(80.0 * inten), 90));
            break;
        }
        case FixtureModel::Scanner:
        {
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            double mhYaw, mhPitch;
            if (live && fi.liveDirValid)
            {
                const QVector3D d = fi.liveDir.normalized();
                mhPitch = qRadiansToDegrees(qAsin(qBound(-1.0, double(d.z()), 1.0)));
                mhYaw = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
            }
            else
            {
                mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
                mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            }
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), mhPitch);
            const QVector3D front = fi.pos + QVector3D(0.0, 0.0, sz * 0.24)
                                  + head.rotatedVector(QVector3D(0.0, sy * 0.32, 0.0));
            rasterFixtureBeam(r, front, head, fi.beamDeg, gel, 0.0,
                              qBound(0, int(75.0 * inten), 88));
            break;
        }
        case FixtureModel::Strobe:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            const QVector3D dir = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
            const QVector3D origin = fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + 0.01, 0.0));
            const double blen = beamLength(origin, dir);
            const double spread = qTan(qDegreesToRadians(qBound(24.0, fi.beamDeg, 90.0) * 0.5)) * blen;
            const int a = qBound(0, int(60.0 * inten), 75);
            if (a > 0)
                rasterRectBeam(r, origin, rot, sx * 0.42, sz * 0.3, blen, spread, gel, a);
            break;
        }
        case FixtureModel::Laser:
        {
            const double sy = fi.size.y();
            const QVector3D origin = fi.pos + rot.rotatedVector(QVector3D(0.0, sy * 0.5 + 0.012, 0.0));
            const int rays = 5;
            static const QColor laserPalette[] = {
                QColor(80, 255, 140), QColor(255, 70, 210), QColor(80, 190, 255),
                QColor(255, 230, 90), QColor(255, 255, 255)
            };
            for (int i = 0; i < rays; ++i)
            {
                const double angle = (i - (rays - 1) * 0.5) * 4.5;
                const QQuaternion rayRot = rot
                    * QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), angle);
                QColor rayColor = gel;
                if (live && fi.liveHeads.isEmpty() == false)
                    rayColor = fi.liveHeads.at(i % fi.liveHeads.size());
                else if (!live)
                    rayColor = laserPalette[i % 5];
                rasterFixtureBeam(r, origin, rayRot, 2.8, rayColor, 0.0,
                                  qBound(0, int(65.0 * inten), 80));
            }
            break;
        }
        case FixtureModel::Dimmer:
        case FixtureModel::Smoke:
            break;   // no light beam
        default: // Par
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.1);
            const QVector3D front = fi.pos + rot.rotatedVector(QVector3D(0.0, len * 0.5 + 0.02, 0.0));
            rasterFixtureBeam(r, front, rot, fi.beamDeg, gel, 0.0,
                              qBound(0, int(80.0 * inten), 90));
            break;
        }
        }
    }

    r.setDepthWrite(true);
    r.setAdditive(false);

    // --- Pass 3: volumetric smoke (alpha blended, back to front) -----------
    struct Puff
    {
        QPointF sp;
        double iw;
        double radius;
        QColor color;
        double depth;
    };
    QList<Puff> puffs;
    const double focal = (height() * 0.5) / qTan(m_fovY * 0.5);
    for (const FixtureInstance &fi : m_fixtureScene->fixtures())
    {
        if (!liveMode || fi.model != FixtureModel::Smoke || !fi.visible)
            continue;
        for (const FixtureScene::SmokeParticle &p : m_fixtureScene->smokeParticles(fi.id))
        {
            if (p.alpha <= 0.003)
                continue;
            const QVector3D cam = toCamera(p.pos);
            if (cam.z() < m_near)
                continue;
            Puff pf;
            pf.sp = projectCamera(cam);
            pf.iw = 1.0 / cam.z();
            pf.radius = qMin(80.0, p.size * focal / cam.z());
            pf.color = QColor(206, 211, 221);
            pf.color.setAlpha(qBound(0, int(p.alpha * 255.0), 255));
            pf.depth = cam.z();
            puffs << pf;
        }
    }
    std::sort(puffs.begin(), puffs.end(),
              [](const Puff &a, const Puff &b) { return a.depth > b.depth; });
    r.setDepthWrite(false);
    for (const Puff &pf : puffs)
        r.sprite(pf.sp, pf.iw, pf.radius, pf.color);
    r.setDepthWrite(true);
}

bool Preview3DView::pickFixture(const QPoint &screen, int &id, double &dist) const
{
    if (m_fixtureScene == NULL)
        return false;

    const double maxD = 16.0;
    double best = maxD;
    bool found = false;

    for (const FixtureInstance &fi : m_fixtureScene->fixtures())
    {
        if (!fi.visible)
            continue;
        QPointF sp;
        if (!project(fi.pos, sp))
            continue;
        const double d = QLineF(sp, QPointF(screen)).length();
        if (d < best)
        {
            best = d;
            id = fi.id;
            found = true;
        }
    }
    if (found)
        dist = best;
    return found;
}

void Preview3DView::renderTrussGizmos(QPainter &p)
{
    if (!m_trussInteractive || m_scene == NULL)
        return;

    p.setBrush(Qt::NoBrush);

    for (const Truss &t : m_scene->trusses())
    {
        if (!t.visible)
            continue;

        const QList<TrussPort> ports = m_scene->worldPorts(t.id);
        for (int i = 0; i < ports.size(); ++i)
        {
            QPointF sp;
            if (!project(ports.at(i).localPos, sp))
                continue;

            const bool linked = m_scene->isPortLinked(t.id, i);
            const bool hovered = (t.id == m_hoverTrussId && i == m_hoverPort);

            if (linked)
            {
                p.setPen(QPen(QColor(120, 130, 145, 170), 1.2));
                p.drawEllipse(sp, 4.0, 4.0);
            }
            else
            {
                p.setPen(QPen(hovered ? QColor(150, 240, 255) : QColor(79, 195, 247), 1.6));
                p.setBrush(hovered ? QColor(120, 230, 255, 110) : QColor(79, 195, 247, 45));
                p.drawEllipse(sp, hovered ? 7.0 : 5.0, hovered ? 7.0 : 5.0);
                p.setBrush(Qt::NoBrush);
            }
        }
    }

    // The link being dragged.
    if (m_dragMode == DragLink)
    {
        const QList<TrussPort> sp = m_scene->worldPorts(m_dragTrussId);
        QPointF from;
        if (m_dragPort >= 0 && m_dragPort < sp.size()
            && project(sp.at(m_dragPort).localPos, from))
        {
            QPointF to = m_lastMouse;
            if (m_hoverTrussId != 0 && m_hoverPort >= 0)
            {
                const QList<TrussPort> hp = m_scene->worldPorts(m_hoverTrussId);
                if (m_hoverPort < hp.size())
                    project(hp.at(m_hoverPort).localPos, to);
            }
            p.setPen(QPen(QColor(150, 240, 255), 2.0, Qt::DashLine));
            p.drawLine(from, to);
        }
    }
}

void Preview3DView::renderOverlay(QPainter &p)
{
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 110));

    QString hint = tr("Drag: rotate    \u00b7    Right / Middle drag: pan    \u00b7    Wheel: zoom");
    if (m_trussInteractive)
        hint = tr("Drag a truss: move    \u00b7    Drag a ring: link    \u00b7    Empty: rotate    \u00b7    Wheel: zoom");

    QFont f = p.font();
    f.setPointSizeF(qMax(8.0, f.pointSizeF() - 0.5));
    p.setFont(f);

    const QFontMetrics fm(f);
    const int pad = 8;
    const int tw = fm.horizontalAdvance(hint);
    const QRect box(pad, height() - fm.height() - pad * 2, tw + pad * 2, fm.height() + pad);
    p.drawRoundedRect(box, 5, 5);

    p.setPen(QColor(210, 224, 236));
    p.drawText(box, Qt::AlignCenter, hint);

    // FPS counter (top-right).
    const QString fps = tr("FPS %1  \u00b7  %2 ms")
        .arg(m_fps, 0, 'f', 1).arg(m_lastFrameMs, 0, 'f', 1);
    const int fw = fm.horizontalAdvance(fps);
    const QRect fbox(width() - fw - pad * 3, pad, fw + pad * 2, fm.height() + pad);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 110));
    p.drawRoundedRect(fbox, 5, 5);
    p.setPen(QColor(180, 235, 255));
    p.drawText(fbox, Qt::AlignCenter, fps);
}

void Preview3DView::renderScene(QPainter &p)
{
    m_frameTimer.start();
    updateCamera();

    // Render the whole 3D scene with a real z-buffer (proper occlusion).
    // Drop supersampling while the camera/trusses are being dragged or while
    // operate-mode live values are streaming in, so it stays responsive.
    const bool live = (m_fixtureScene != NULL && m_fixtureScene->isLive());
    const int ss = (m_dragMode == DragNone && !live) ? 2 : 1;
    m_raster.begin(width(), height(), ss);
    m_raster.fillGradient(m_bgTop, m_bgBottom);

    rasterFloor(m_raster);
    rasterRoomBox(m_raster);
    rasterStage(m_raster);
    rasterTrusses(m_raster);
    rasterFixtures(m_raster);

    p.drawImage(0, 0, m_raster.finish());

    // Soft vignette for depth. It only depends on the widget size, so build
    // it once instead of recalculating a full-screen radial gradient per frame.
    if (m_vignette.size() != size())
    {
        m_vignette = QImage(size(), QImage::Format_ARGB32_Premultiplied);
        m_vignette.fill(Qt::transparent);
        QPainter vp(&m_vignette);
        QRadialGradient vig(rect().center(), qMax(width(), height()) * 0.75);
        vig.setColorAt(0.55, QColor(0, 0, 0, 0));
        vig.setColorAt(1.0, QColor(0, 0, 0, 90));
        vp.fillRect(rect(), vig);
    }
    p.drawImage(0, 0, m_vignette);

    m_lastFrameMs = m_frameTimer.elapsed();

    m_fpsFrames++;
    if (m_fpsTimer.elapsed() >= 500)
    {
        m_fps = m_fpsFrames * 1000.0 / double(m_fpsTimer.elapsed());
        m_fpsFrames = 0;
        m_fpsTimer.restart();
    }

    renderTrussGizmos(p);
    renderOverlay(p);
}

void Preview3DView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    renderScene(p);
}

void Preview3DView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_hasFitted && width() > 0 && height() > 0)
        resetView();
}

void Preview3DView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_hasFitted && width() > 0 && height() > 0)
        resetView();
}

bool Preview3DView::pickPort(const QPoint &screen, int &trussId, int &port, double &dist) const
{
    if (m_scene == NULL)
        return false;

    const double maxD = 14.0;
    double best = maxD;
    bool found = false;

    for (const Truss &t : m_scene->trusses())
    {
        if (!t.visible)
            continue;

        const QList<TrussPort> ports = m_scene->worldPorts(t.id);
        for (int i = 0; i < ports.size(); ++i)
        {
            if (m_scene->isPortLinked(t.id, i))
                continue;
            QPointF sp;
            if (!project(ports.at(i).localPos, sp))
                continue;
            const double d = QLineF(sp, QPointF(screen)).length();
            if (d < best)
            {
                best = d;
                trussId = t.id;
                port = i;
                found = true;
            }
        }
    }
    if (found)
        dist = best;
    return found;
}

bool Preview3DView::pickTruss(const QPoint &screen, int &trussId, double &dist) const
{
    if (m_scene == NULL)
        return false;

    const double maxD = 12.0;
    double best = maxD;
    bool found = false;

    for (const Truss &t : m_scene->trusses())
    {
        if (!t.visible)
            continue;

        const QList<TrussPort> ports = m_scene->worldPorts(t.id);
        QList<QPointF> pts;
        for (const TrussPort &p : ports)
        {
            QPointF sp;
            if (project(p.localPos, sp))
                pts << sp;
        }

        double d = 1e9;
        if (!Truss::isJunction(t.type) && pts.size() >= 2)
        {
            d = pointSegmentDist(QPointF(screen), pts.first(), pts.last());
        }
        else
        {
            // Junction (or degenerate): use the projected centre.
            QPointF sp;
            if (project(t.pos, sp))
                d = QLineF(sp, QPointF(screen)).length();
        }

        if (d < best)
        {
            best = d;
            trussId = t.id;
            found = true;
        }
    }
    if (found)
        dist = best;
    return found;
}

void Preview3DView::mousePressEvent(QMouseEvent *event)
{
    m_lastMouse = event->pos();

    // Truss interaction (only while the panel is active).
    if (m_trussInteractive && event->button() == Qt::LeftButton
        && !(event->modifiers() & Qt::ShiftModifier))
    {
        int pid = 0, pport = -1, tid = 0;
        double pd = 1e9, td = 1e9;
        const bool hasPort = pickPort(event->pos(), pid, pport, pd);
        const bool hasTruss = pickTruss(event->pos(), tid, td);

        // Link only when the cursor is actually nearer the ring than the body.
        if (hasPort && (!hasTruss || pd <= td))
        {
            m_dragMode = DragLink;
            m_dragTrussId = pid;
            m_dragPort = pport;
            m_hoverTrussId = 0;
            m_hoverPort = -1;
            update();
            return;
        }
        if (hasTruss)
        {
            m_selectedId = tid;
            emit trussSelected(tid);

            const Truss *t = m_scene->trussById(tid);
            if (t != NULL)
                screenToGround(event->pos(), t->pos.z(), m_dragLastGround);

            m_dragMode = DragMove;
            m_dragTrussId = tid;
            m_dragFixture = false;
            setCursor(Qt::SizeAllCursor);
            update();
            return;
        }
    }

    // Fixture interaction (only while the fixture panel is active).
    if (m_fixtureInteractive && event->button() == Qt::LeftButton
        && !(event->modifiers() & Qt::ShiftModifier))
    {
        int id = 0;
        double d = 0.0;
        if (pickFixture(event->pos(), id, d))
        {
            m_selectedFixture = id;
            emit fixtureSelected(id);

            const FixtureInstance *fi = m_fixtureScene->instance(id);
            if (fi != NULL)
                screenToGround(event->pos(), fi->pos.z(), m_dragLastGround);

            m_dragMode = DragMove;
            m_dragFixture = true;
            m_dragTrussId = id;
            setCursor(Qt::SizeAllCursor);
            update();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton
        || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)))
    {
        m_dragMode = DragPan;
    }
    else if (event->button() == Qt::LeftButton)
    {
        m_dragMode = DragOrbit;
    }
    else
    {
        m_dragMode = DragNone;
    }

    if (m_dragMode == DragPan)
        setCursor(Qt::ClosedHandCursor);
    else if (m_dragMode == DragOrbit)
        setCursor(Qt::SizeAllCursor);
}

void Preview3DView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMove)
    {
        m_lastMouse = event->pos();
        if (m_dragFixture && m_fixtureScene != NULL)
        {
            const FixtureInstance *fi = m_fixtureScene->instance(m_dragTrussId);
            QVector3D g;
            if (fi != NULL && screenToGround(event->pos(), fi->pos.z(), g))
            {
                m_fixtureScene->setPosition(m_dragTrussId, fi->pos + (g - m_dragLastGround));
                m_dragLastGround = g;
            }
            return;
        }

        const Truss *t = (m_scene != NULL) ? m_scene->trussById(m_dragTrussId) : NULL;
        QVector3D g;
        if (t != NULL && screenToGround(event->pos(), t->pos.z(), g))
        {
            m_scene->translate(m_dragTrussId, g - m_dragLastGround);
            m_dragLastGround = g;
        }
        return;
    }

    if (m_dragMode == DragLink)
    {
        m_lastMouse = event->pos();
        int id = 0, port = -1;
        double d = 0.0;
        if (pickPort(event->pos(), id, port, d) && id != m_dragTrussId)
        {
            m_hoverTrussId = id;
            m_hoverPort = port;
        }
        else
        {
            m_hoverTrussId = 0;
            m_hoverPort = -1;
        }
        update();
        return;
    }

    if (m_dragMode != DragOrbit && m_dragMode != DragPan)
    {
        m_lastMouse = event->pos();
        return;
    }

    const QPoint delta = event->pos() - m_lastMouse;
    m_lastMouse = event->pos();
    m_userMoved = true;

    if (m_dragMode == DragOrbit)
    {
        // Grab/turntable feel: dragging right turns the room right.
        m_yaw -= delta.x() * 0.008;
        m_pitch += delta.y() * 0.008;
        m_pitch = qBound(qDegreesToRadians(2.0), m_pitch, qDegreesToRadians(88.0));
    }
    else // Pan
    {
        const double scale = (m_dist * qTan(m_fovY * 0.5) * 2.0) / qMax(1, height());
        m_target += (-m_right * delta.x() + m_up * delta.y()) * scale;
    }

    updateCamera();
    update();
    emit viewChanged();
}

void Preview3DView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)

    if (m_dragMode == DragLink && m_scene != NULL)
    {
        if (m_hoverTrussId != 0 && m_hoverPort >= 0)
            m_scene->link(m_dragTrussId, m_dragPort, m_hoverTrussId, m_hoverPort);
    }

    m_dragMode = DragNone;
    m_dragFixture = false;
    m_hoverTrussId = 0;
    m_hoverPort = -1;
    unsetCursor();
    update();
}

void Preview3DView::wheelEvent(QWheelEvent *event)
{
    const int dy = event->angleDelta().y();
    if (dy == 0)
        return;

    m_userMoved = true;
    m_dist *= qPow(0.999, dy);
    m_dist = qBound(0.3, m_dist, 5000.0);

    updateCamera();
    update();
    emit viewChanged();
    event->accept();
}
