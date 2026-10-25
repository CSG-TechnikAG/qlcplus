/*
  Q Light Controller Plus
  preview3d_test.cpp

  Tests for backend selection, fallback, software counters and M2-M4 GPU rendering.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QSettings>

#include "preview3d_test.h"
#include "preview3drenderbackend.h"
#include "preview3draster.h"
#include "preview3dsceneframe.h"

void Preview3D_Test::backendNamesRoundTrip()
{
    QCOMPARE(Preview3DRenderBackendFactory::rendererToString(Preview3DRenderer::Auto),
             QString("auto"));
    QCOMPARE(Preview3DRenderBackendFactory::rendererToString(Preview3DRenderer::Gpu),
             QString("gpu"));
    QCOMPARE(Preview3DRenderBackendFactory::rendererToString(Preview3DRenderer::Software),
             QString("software"));

    QCOMPARE(Preview3DRenderBackendFactory::stringToRenderer("auto"), Preview3DRenderer::Auto);
    QCOMPARE(Preview3DRenderBackendFactory::stringToRenderer("opengl"), Preview3DRenderer::Gpu);
    QCOMPARE(Preview3DRenderBackendFactory::stringToRenderer("cpu"), Preview3DRenderer::Software);
    QCOMPARE(Preview3DRenderBackendFactory::stringToRenderer("nonsense"), Preview3DRenderer::Auto);

    QCOMPARE(Preview3DRenderBackendFactory::qualityToString(Preview3DQuality::Ultra),
             QString("ultra"));
    QCOMPARE(Preview3DRenderBackendFactory::stringToQuality("low"), Preview3DQuality::Low);
}

void Preview3D_Test::settingsRoundTrip()
{
    QSettings settings;
    const QVariant oldRenderer = settings.value("preview3d/renderer");
    const QVariant oldQuality = settings.value("preview3d/quality");

    Preview3DRenderSettings saved;
    saved.renderer = Preview3DRenderer::Gpu;
    saved.quality = Preview3DQuality::Ultra;
    saved.shadows = false;
    saved.volumetricBeams = false;
    saved.haze = false;
    saved.showHints = true;
    saved.showRenderStatus = true;
    saved.showFps = true;
    saved.resolutionScale = 1.5;
    Preview3DRenderBackendFactory::saveSettings(saved);

    const Preview3DRenderSettings loaded = Preview3DRenderBackendFactory::loadSettings();
    QCOMPARE(loaded.renderer, Preview3DRenderer::Gpu);
    QCOMPARE(loaded.quality, Preview3DQuality::Ultra);
    QCOMPARE(loaded.shadows, false);
    QCOMPARE(loaded.volumetricBeams, false);
    QCOMPARE(loaded.haze, false);
    QCOMPARE(loaded.showHints, true);
    QCOMPARE(loaded.showRenderStatus, true);
    QCOMPARE(loaded.showFps, true);
    QCOMPARE(loaded.resolutionScale, 1.5);

    if (oldRenderer.isValid())
        settings.setValue("preview3d/renderer", oldRenderer);
    else
        settings.remove("preview3d/renderer");
    if (oldQuality.isValid())
        settings.setValue("preview3d/quality", oldQuality);
    else
        settings.remove("preview3d/quality");
}

void Preview3D_Test::autoFallsBackToSoftware()
{
    Preview3DRenderSettings settings;
    settings.renderer = Preview3DRenderer::Auto;

    Preview3DRenderer actual = Preview3DRenderer::Auto;
    QString reason;
    Preview3DRenderBackend *backend =
        Preview3DRenderBackendFactory::createBackend(settings, &actual, &reason);

    QVERIFY(backend != NULL);
    QVERIFY(reason.isEmpty() == false);
    if (Preview3DRenderBackendFactory::gpuBackendAvailable())
    {
        QCOMPARE(actual, Preview3DRenderer::Gpu);
        QCOMPARE(backend->id(), QString("opengl"));
    }
    else
    {
        QCOMPARE(actual, Preview3DRenderer::Software);
        QCOMPARE(backend->id(), QString("software"));
    }
    QVERIFY(backend->initialize() == true);
    QVERIFY(backend->capabilities().accelerated ==
             (actual == Preview3DRenderer::Gpu));
    backend->shutdown();
    delete backend;
}

void Preview3D_Test::explicitGpuSelection()
{
    Preview3DRenderSettings settings;
    settings.renderer = Preview3DRenderer::Gpu;

    Preview3DRenderer actual = Preview3DRenderer::Auto;
    QString reason;
    Preview3DRenderBackend *backend =
        Preview3DRenderBackendFactory::createBackend(settings, &actual, &reason);

    QVERIFY(backend != NULL);
    if (Preview3DRenderBackendFactory::gpuBackendAvailable())
    {
        QCOMPARE(actual, Preview3DRenderer::Gpu);
        QCOMPARE(backend->id(), QString("opengl"));
        QString error;
        const bool initialized = backend->initialize(&error);
        if (initialized == false)
            QSKIP(qPrintable(QString("OpenGL backend unavailable on this host: %1").arg(error)));
        QVERIFY(backend->isInitialized());
        QVERIFY(backend->diagnostics().contains("OpenGL"));
    }
    else
    {
        QCOMPARE(actual, Preview3DRenderer::Software);
        QVERIFY(reason.contains("unavailable"));
    }
    delete backend;
}

void Preview3D_Test::forcedGpuFallback()
{
    const char *name = "QLC_PREVIEW3D_DISABLE_GPU";
    const bool hadValue = qEnvironmentVariableIsSet(name);
    const QByteArray oldValue = qgetenv(name);
    qputenv(name, "1");

    QVERIFY(Preview3DRenderBackendFactory::gpuBackendAvailable() == false);

    Preview3DRenderSettings settings;
    settings.renderer = Preview3DRenderer::Gpu;
    Preview3DRenderer actual = Preview3DRenderer::Gpu;
    QString reason;
    Preview3DRenderBackend *backend =
            Preview3DRenderBackendFactory::createBackend(settings, &actual, &reason);
    QVERIFY(backend != NULL);
    QCOMPARE(actual, Preview3DRenderer::Software);
    QCOMPARE(backend->id(), QString("software"));
    QVERIFY(reason.contains("unavailable"));
    QVERIFY(backend->initialize() == true);
    delete backend;

    if (hadValue)
        qputenv(name, oldValue);
    else
        qunsetenv(name);
}

void Preview3D_Test::gpuContextLoss()
{
    if (Preview3DRenderBackendFactory::gpuBackendAvailable() == false)
        QSKIP("OpenGL 3.3 core is not available on this host");

    Preview3DSceneFrame frame;
    frame.viewport = QSize(64, 48);
    frame.eye = QVector3D(3, -4, 2);
    frame.target = QVector3D(0, 0, 0.5);
    frame.up = QVector3D(0, 0, 1);
    frame.roomX0 = -2;
    frame.roomX1 = 2;
    frame.roomY0 = -2;
    frame.roomY1 = 2;
    frame.zFloor = -0.5;
    frame.zTop = 3;

    GpuPreview3DRenderBackend backend;
    QString error;
    if (backend.initialize(&error) == false)
        QSKIP(qPrintable(QString("OpenGL backend unavailable on this host: %1").arg(error)));

    QImage image;
    QVERIFY(backend.renderFrame(frame, image, &error));
    backend.shutdown();
    QVERIFY(backend.isInitialized() == false);
    image = QImage();
    QVERIFY(backend.renderFrame(frame, image, &error) == false);
    QVERIFY(image.isNull());
}

void Preview3D_Test::gpuStaticFrame()
{
    if (Preview3DRenderBackendFactory::gpuBackendAvailable() == false)
        QSKIP("OpenGL 3.3 core is not available on this host");

    Preview3DSceneFrame frame;
    frame.viewport = QSize(160, 120);
    frame.eye = QVector3D(4.5, -5.5, 3.0);
    frame.target = QVector3D(0.0, 0.0, 0.5);
    frame.up = QVector3D(0, 0, 1);
    frame.fovY = qDegreesToRadians(42.0);
    frame.nearPlane = 0.05;
    frame.roomX0 = -3.0;
    frame.roomX1 = 3.0;
    frame.roomY0 = -3.0;
    frame.roomY1 = 3.0;
    frame.zFloor = -0.5;
    frame.zTop = 4.0;
    frame.stageX = 4.0;
    frame.stageY = 3.0;
    frame.showGrid = true;

    Preview3DTrussSegmentSnapshot segment;
    segment.a = QVector3D(-1.5, 0.5, 3.0);
    segment.b = QVector3D(1.5, 0.5, 3.0);
    segment.radius = 0.03;
    segment.color = QColor(190, 190, 190);
    segment.chord = true;
    frame.trussSegments << segment;

    FixtureInstance par;
    par.id = 1;
    par.name = "Test Par";
    par.pos = QVector3D(-1.2, 0.0, 2.2);
    par.size = QVector3D(0.3, 0.35, 0.3);
    par.color = QColor(255, 120, 60);
    par.model = FixtureModel::Par;
    par.beamDeg = 25.0;
    frame.fixtures << par;

    FixtureInstance head;
    head.id = 2;
    head.name = "Test Head";
    head.pos = QVector3D(0.8, 0.5, 2.8);
    head.size = QVector3D(0.3, 0.3, 0.45);
    head.color = QColor(80, 180, 255);
    head.model = FixtureModel::MovingHead;
    head.beamDeg = 18.0;
    frame.fixtures << head;

    Preview3DBeamSnapshot beam;
    beam.origin = par.pos + QVector3D(0, 0.2, 0);
    beam.rotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 180.0);
    beam.beamDeg = 24.0;
    beam.length = 3.0;
    beam.color = par.color;
    beam.alpha = 35;
    frame.beams << beam;

    GpuPreview3DRenderBackend backend;
    QString error;
    if (backend.initialize(&error) == false)
        QSKIP(qPrintable(QString("OpenGL backend unavailable on this host: %1").arg(error)));

    QImage image;
    QVERIFY2(backend.renderFrame(frame, image, &error),
             qPrintable(QString("GPU frame failed: %1").arg(error)));
    QVERIFY(image.isNull() == false);
    QCOMPARE(image.size() / image.devicePixelRatio(), QSizeF(160, 120).toSize());

    /* The test par/beam is warm orange; its presence proves that the M2
     * fixture and beam passes reached the framebuffer. */
    bool warmPixel = false;
    for (int y = 0; y < image.height() && warmPixel == false; ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > pixel.blue() + 30 && pixel.red() > 80)
            {
                warmPixel = true;
                break;
            }
        }
    }
    QVERIFY(warmPixel);
}

void Preview3D_Test::gpuEffectsFrame()
{
    if (Preview3DRenderBackendFactory::gpuBackendAvailable() == false)
        QSKIP("OpenGL 3.3 core is not available on this host");

    Preview3DSceneFrame frame;
    frame.viewport = QSize(240, 180);
    frame.eye = QVector3D(4.5, -5.5, 3.0);
    frame.target = QVector3D(0.0, 0.0, 0.5);
    frame.up = QVector3D(0, 0, 1);
    frame.fovY = qDegreesToRadians(42.0);
    frame.nearPlane = 0.05;
    frame.roomX0 = -3.0;
    frame.roomX1 = 3.0;
    frame.roomY0 = -3.0;
    frame.roomY1 = 3.0;
    frame.zFloor = -0.5;
    frame.zTop = 4.0;
    frame.stageX = 4.0;
    frame.stageY = 3.0;

    Preview3DTrussSegmentSnapshot segment;
    segment.a = QVector3D(-1.5, 0.5, 3.0);
    segment.b = QVector3D(1.5, 0.5, 3.0);
    segment.radius = 0.04;
    segment.color = QColor(190, 190, 190);
    segment.chord = true;
    frame.trussSegments << segment;

    FixtureInstance par;
    par.id = 1;
    par.pos = QVector3D(-1.2, 0.0, 2.2);
    par.size = QVector3D(0.3, 0.35, 0.3);
    par.color = QColor(255, 120, 60);
    par.model = FixtureModel::Par;
    frame.fixtures << par;

    GpuPreview3DRenderBackend backend;
    QString error;
    if (backend.initialize(&error) == false)
        QSKIP(qPrintable(QString("OpenGL backend unavailable on this host: %1").arg(error)));

    frame.shadows = false;
    frame.haze = false;
    QImage base;
    QVERIFY2(backend.renderFrame(frame, base, &error),
             qPrintable(QString("Baseline frame failed: %1").arg(error)));

    frame.shadows = true;
    QImage shadowed;
    QVERIFY2(backend.renderFrame(frame, shadowed, &error),
             qPrintable(QString("Shadow frame failed: %1").arg(error)));
    QVERIFY(shadowed != base);

    frame.shadows = false;
    frame.haze = true;
    QImage hazy;
    QVERIFY2(backend.renderFrame(frame, hazy, &error),
             qPrintable(QString("Haze frame failed: %1").arg(error)));
    QVERIFY(hazy != base);

    frame.haze = false;
    Preview3DSmokeSnapshot smoke;
    smoke.pos = QVector3D(0.0, 0.5, 1.5);
    smoke.size = 0.6;
    smoke.alpha = 0.5;
    frame.smoke << smoke;
    QImage smoky;
    QVERIFY2(backend.renderFrame(frame, smoky, &error),
             qPrintable(QString("Smoke frame failed: %1").arg(error)));
    QVERIFY(smoky != base);
    QVERIFY(backend.lastGpuFrameMs() > 0.0);

    // Static geometry and the shadow map must be reused for an unchanged scene.
    frame.smoke.clear();
    frame.haze = false;
    frame.shadows = true;
    QImage cached;
    QVERIFY2(backend.renderFrame(frame, cached, &error),
             qPrintable(QString("Cached frame failed: %1").arg(error)));
    const int staticUploads = backend.staticUploadCount();
    const int shadowRenders = backend.shadowRenderCount();
    QVERIFY(backend.renderFrame(frame, cached, &error));
    QCOMPARE(backend.staticUploadCount(), staticUploads);
    QCOMPARE(backend.shadowRenderCount(), shadowRenders);

    // Moving a fixture invalidates the shadow map but not static room geometry.
    frame.fixtures[0].pos.setX(frame.fixtures[0].pos.x() + 0.1f);
    QVERIFY(backend.renderFrame(frame, cached, &error));
    QCOMPARE(backend.staticUploadCount(), staticUploads);
    QVERIFY(backend.shadowRenderCount() > shadowRenders);

    // Resolution scaling preserves logical output size for adaptive quality.
    frame.adaptiveQuality = false;
    frame.resolutionScale = 0.5;
    QImage halfRes;
    QVERIFY2(backend.renderFrame(frame, halfRes, &error),
             qPrintable(QString("Half-resolution frame failed: %1").arg(error)));
    QVERIFY(halfRes.width() < 240);
    QCOMPARE(halfRes.size() / halfRes.devicePixelRatio(), QSizeF(240, 180).toSize());
}

void Preview3D_Test::softwareRasterCounters()
{
    SoftwareRaster raster;
    raster.begin(64, 64, 1);
    raster.fillGradient(QColor(0, 0, 0), QColor(32, 32, 48));
    raster.triangle(QPointF(4, 4), QPointF(50, 4), QPointF(4, 50),
                    1.0, 1.0, 1.0, QColor(200, 80, 40));
    raster.line(QPointF(2, 60), QPointF(60, 60), 1.0, 1.0,
                QColor(255, 255, 255), 2.0);
    raster.sprite(QPointF(32, 32), 1.0, 8.0, QColor(255, 255, 255, 120));

    QCOMPARE(raster.lineCount(), 1);
    QCOMPARE(raster.spriteCount(), 1);
    QVERIFY(raster.triangleCount() >= 3); // line is rendered as two triangles
    QVERIFY(raster.finish().size() == QSize(64, 64));
}

QTEST_MAIN(Preview3D_Test)
