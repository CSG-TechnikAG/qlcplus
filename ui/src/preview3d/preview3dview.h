/*
  Q Light Controller Plus
  preview3dview.h

  A lightweight, dependency-free 3D viewport for the "3D Preview" tab.
  It software-renders the room, the stage deck and the trusses with an orbit
  camera (rotate / pan / zoom) and auto-fits the room on first show.

  When the truss panel is active the viewport also handles truss interaction:
  dragging a piece moves it (linked pieces follow) and dragging from an
  attachment ring links two pieces together.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PREVIEW3DVIEW_H
#define PREVIEW3DVIEW_H

#include <QWidget>
#include <QVector3D>
#include <QPointF>
#include <QList>
#include <QColor>
#include <QString>
#include <QElapsedTimer>

#include "preview3draster.h"

class QPainter;
class TrussScene;
class FixtureScene;

/**
 * A small software 3D renderer used by the "3D Preview" tab.
 *
 * World frame (metres): x = right, y = forward, z = up. The stage plane is
 * z = 0 and the room floor is at z = -stageHeight, matching the Stage
 * Geometry model.
 *
 * Controls:
 *   - left drag                   -> orbit (yaw / pitch)
 *   - middle / right / shift drag -> pan
 *   - mouse wheel                 -> zoom
 *   - left drag on a truss        -> move it (panel active)
 *   - left drag from a ring       -> link two trusses (panel active)
 */
class Preview3DView final : public QWidget
{
    Q_OBJECT

public:
    explicit Preview3DView(QWidget *parent = NULL);
    ~Preview3DView();

    /** Room footprint + floor/top heights, and the stage footprint (metres). */
    void setScene(double roomX0, double roomX1,
                  double roomY0, double roomY1,
                  double zFloor, double zTop,
                  double stageX, double stageY);

    /** Re-fit the current room into the viewport (keeps the orientation). */
    void fitToRoom();
    /** Restore the default orientation and fit the room. */
    void resetView();

    bool isGridVisible() const { return m_showGrid; }
    void setGridVisible(bool on);

    /** Trusses to render. */
    void setTrussScene(TrussScene *scene);
    /** Enable/disable truss picking, dragging and linking. */
    void setTrussInteractive(bool on);
    bool isTrussInteractive() const { return m_trussInteractive; }

    void setSelectedTruss(int id);
    int selectedTruss() const { return m_selectedId; }

    /** Fixtures to render. */
    void setFixtureScene(FixtureScene *scene);
    /** Enable/disable fixture picking and dragging. */
    void setFixtureInteractive(bool on);
    bool isFixtureInteractive() const { return m_fixtureInteractive; }
    void setSelectedFixture(int id);
    int selectedFixture() const { return m_selectedFixture; }

    /** Project a world point to widget coordinates (false if behind the camera). */
    bool worldToScreen(const QVector3D &world, QPointF &screen) const;

    /** Camera state, for saving/restoring the view. */
    void camera(double &yaw, double &pitch, double &dist, QVector3D &target) const;
    void setCamera(double yaw, double pitch, double dist, const QVector3D &target);

signals:
    /** Emitted when the user changes the camera (rotate / pan / zoom). */
    void viewChanged();
    /** Emitted when the selected truss changes (click in the viewport). */
    void trussSelected(int id);
    /** Emitted when the selected fixture changes (click in the viewport). */
    void fixtureSelected(int id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    enum DragMode { DragNone, DragOrbit, DragPan, DragMove, DragLink };

    void updateCamera();
    QVector3D toCamera(const QVector3D &world) const;
    QPointF projectCamera(const QVector3D &cam) const;
    bool project(const QVector3D &world, QPointF &out) const;
    bool screenToGround(const QPoint &screen, double zPlane, QVector3D &out) const;

    void drawPolygon(QPainter &p, const QList<QVector3D> &world,
                     const QColor &fill, const QColor &edge, double edgeWidth) const;
    void drawLine(QPainter &p, const QVector3D &a, const QVector3D &b,
                  const QColor &color, double width,
                  Qt::PenStyle style = Qt::SolidLine) const;
    /** Draw one vertical stage leg (a thin box) with back-face culling. */
    void drawLeg(QPainter &p, double x, double y, double half,
                 double zTop, double zBottom) const;
    /** Perimeter feet: one every metre, plus the edge when it isn't a whole metre. */
    QList<QPointF> stageFeet() const;

    bool pickPort(const QPoint &screen, int &trussId, int &port, double &dist) const;
    bool pickTruss(const QPoint &screen, int &trussId, double &dist) const;

    void renderScene(QPainter &p);

    bool projectDepth(const QVector3D &world, QPointF &screen, double &invW) const;
    void rasterPolygon(SoftwareRaster &r, const QList<QVector3D> &world,
                       const QColor &fill, const QColor &edge, double edgeWidth) const;
    void rasterLine(SoftwareRaster &r, const QVector3D &a, const QVector3D &b,
                    const QColor &color, double width,
                    Qt::PenStyle style = Qt::SolidLine) const;
    void rasterFloor(SoftwareRaster &r) const;
    /** Dashed wireframe of the room volume (12 edges), to show the height. */
    void rasterRoomBox(SoftwareRaster &r) const;
    void rasterStage(SoftwareRaster &r) const;
    void rasterTrusses(SoftwareRaster &r) const;
    void rasterFixtures(SoftwareRaster &r) const;
    void rasterBox(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                   const QVector3D &size, const QColor &fill, const QColor &edge) const;
    /** A capped cylinder whose axis is the rotated local +Y (beam direction). */
    void rasterCylinder(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                        double radius, double length, const QColor &fill, const QColor &edge,
                        int segments) const;
    /** A hemisphere whose up axis is the rotated local +Y. */
    void rasterDome(SoftwareRaster &r, const QVector3D &base, const QQuaternion &rot,
                    double radius, double height, int segments, int rings,
                    const QColor &fill, const QColor &edge) const;
    /** A flat disk facing the rotated local +Y (a lens / indicator). */
    void rasterDisk(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                    double radius, const QColor &fill, const QColor &edge) const;
    /** A translucent beam cone emitted along the rotated local +Y. */
    void rasterFixtureBeam(SoftwareRaster &r, const QVector3D &origin, const QQuaternion &rot,
                           double beamDeg, const QColor &color, double length = 3.0,
                           int alpha = 50) const;
    /** A translucent rectangular beam (frustum) emitted along the rotated local +Y. */
    void rasterRectBeam(SoftwareRaster &r, const QVector3D &origin, const QQuaternion &rot,
                        double halfW, double halfH, double length, double spread,
                        const QColor &color, int alpha = 30) const;
    /** Four barn-door flaps hinged around a lens (theatre lantern). */
    void rasterBarnDoors(SoftwareRaster &r, const QVector3D &center, const QQuaternion &rot,
                         double radius, double length, double tilt) const;
    bool pickFixture(const QPoint &screen, int &id, double &dist) const;
    void renderTrussGizmos(QPainter &p);
    void renderOverlay(QPainter &p);

    // geometry (metres)
    double m_rx0, m_rx1, m_ry0, m_ry1;
    double m_zFloor, m_zTop;
    double m_stageX, m_stageY;

    // orbit camera
    double m_yaw, m_pitch, m_dist;
    QVector3D m_target;
    QVector3D m_eye, m_right, m_up, m_fwd;
    double m_fovY, m_near;

    bool m_hasFitted;
    bool m_userMoved;
    bool m_showGrid;

    DragMode m_dragMode;
    QPoint m_lastMouse;

    // truss interaction
    TrussScene *m_scene;
    bool m_trussInteractive;
    int m_selectedId;
    int m_dragTrussId;
    int m_dragPort;
    QVector3D m_dragLastGround;
    int m_hoverTrussId;
    int m_hoverPort;

    // fixture interaction
    FixtureScene *m_fixtureScene;
    bool m_fixtureInteractive;
    int m_selectedFixture;
    bool m_dragFixture;

    // palette
    QColor m_bgTop, m_bgBottom;
    QColor m_floorFill, m_floorGrid, m_floorEdge, m_roomBoxEdge;
    QColor m_stageTop, m_stageTopGrid, m_stageSide, m_stageEdge;
    QColor m_legColor, m_legEdge;
    QColor m_trussBrace;
    QColor m_selectColor;

    SoftwareRaster m_raster;

    QElapsedTimer m_fpsTimer;
    QElapsedTimer m_frameTimer;
    int m_fpsFrames;
    double m_fps;
    double m_lastFrameMs;
};

#endif // PREVIEW3DVIEW_H
