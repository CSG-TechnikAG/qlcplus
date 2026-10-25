/*
  Q Light Controller Plus
  preview3dsceneframe.h

  Backend-neutral snapshot of the 3D Preview scene. The software renderer can
  continue using its live QObject models; GPU backends consume this immutable
  frame description. The snapshot now carries static geometry, fixtures,
  beams, smoke and shadow/haze options.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PREVIEW3DSCENEFRAME_H
#define PREVIEW3DSCENEFRAME_H

#include <QColor>
#include <QList>
#include <QSize>
#include <QVector3D>
#include <QQuaternion>

#include "fixturescene.h"

class TrussScene;

/** One renderable truss segment, already transformed to world space. */
struct Preview3DTrussSegmentSnapshot
{
    QVector3D a;
    QVector3D b;
    double radius = 0.02;
    QColor color = QColor(180, 180, 180);
    bool chord = false;
};

/** Snapshot of everything a render backend needs for one frame. */
/** One volumetric smoke puff, already sorted by the backend. */
struct Preview3DSmokeSnapshot
{
    QVector3D pos;
    double size = 0.2;
    QColor color = QColor(206, 211, 221);
    double alpha = 0.0;
    bool haze = false;
};

/** A coupler/collar where two truss ports meet. */
struct Preview3DTrussConnectorSnapshot
{
    QVector3D pos;
    QVector3D direction = QVector3D(1, 0, 0);
    double radius = 0.05;
    double length = 0.09;
    QColor color = QColor(110, 120, 132);
};

struct Preview3DBeamSnapshot
{
    QVector3D origin;
    QQuaternion rotation = QQuaternion();
    double beamDeg = 25.0;
    double length = 4.0;
    QColor color = QColor(255, 255, 255);
    int alpha = 60;
    bool rectangular = false;
    double halfWidth = 0.15;
    double halfHeight = 0.15;
    double spread = 0.3;
};

struct Preview3DSceneFrame
{
    QSize viewport;
    double devicePixelRatio = 1.0;

    // Camera basis and projection
    QVector3D eye;
    QVector3D right;
    QVector3D up;
    QVector3D forward;
    QVector3D target;
    QVector3D lightDirection = QVector3D(-0.35, -0.45, 0.82);
    double yaw = 0.0;
    double pitch = 0.0;
    double distance = 10.0;
    double fovY = 0.733;
    double nearPlane = 0.05;

    // Room and stage (metres)
    double roomX0 = 0.0;
    double roomX1 = 0.0;
    double roomY0 = 0.0;
    double roomY1 = 0.0;
    double zFloor = 0.0;
    double zTop = 0.0;
    double stageX = 0.0;
    double stageY = 0.0;

    bool showGrid = true;
    bool beamPreview = false;
    bool live = false;
    bool fixtureInteractive = false;
    bool shadows = true;
    bool haze = true;
    bool adaptiveQuality = false;
    double resolutionScale = 1.0;
    int msaaSamples = 0;
    double hazeDensity = 0.018;
    double smokeHaze = 0.0;
    double smokeFog = 0.0;
    QColor hazeColor = QColor(11, 17, 24);
    quint32 selectedFixture = 0;

    QList<FixtureInstance> fixtures;
    QList<Preview3DTrussSegmentSnapshot> trussSegments;
    QList<Preview3DBeamSnapshot> beams;
    QList<Preview3DSmokeSnapshot> smoke;
    QList<Preview3DTrussConnectorSnapshot> trussConnectors;
};

#endif // PREVIEW3DSCENEFRAME_H
