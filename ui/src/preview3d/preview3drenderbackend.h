/*
  Q Light Controller Plus
  preview3drenderbackend.h

  GPU 3D Preview foundation:
  backend-neutral renderer selection, settings, capabilities and fallback.

  Decision: the GPU backend uses an offscreen OpenGL 3.3 core context and an
  FBO composited by Preview3DView. This keeps the existing picking/panels and
  camera interaction stable while the software renderer remains the
  guaranteed fallback. QRhi remains an option for a future Qt6-only backend.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PREVIEW3DRENDERBACKEND_H
#define PREVIEW3DRENDERBACKEND_H

#include <QString>
#include <QList>
#include <QSize>

struct Preview3DSceneFrame;
class Preview3DOpenGLRenderer;

/** Preferred renderer for the 3D Preview. */
enum class Preview3DRenderer
{
    Auto = 0,
    Gpu,
    Software
};

/** Quality preset shared by all backends. */
enum class Preview3DQuality
{
    Low = 0,
    Medium,
    High,
    Ultra
};

/** Persistent 3D Preview render settings. */
struct Preview3DRenderSettings
{
    Preview3DRenderer renderer = Preview3DRenderer::Auto;
    Preview3DQuality quality = Preview3DQuality::High;
    bool shadows = true;
    bool volumetricBeams = true;
    bool haze = true;
    bool showHints = false;
    bool showRenderStatus = false;
    bool showFps = false;
    double resolutionScale = 1.0;
};

/** What a backend can actually do. */
struct Preview3DRenderCapabilities
{
    QString id;
    QString displayName;
    bool accelerated = false;
    bool shadows = false;
    bool volumetricBeams = false;
    bool haze = false;
    bool physicallyBased = false;
    int maxSupersample = 1;
    QString notes;
};

/** Per-frame profiling counters. */
struct Preview3DRenderStats
{
    QSize viewport;
    int triangles = 0;
    int lines = 0;
    int sprites = 0;
    int fixtures = 0;
    int trussSegments = 0;
    double frameMs = 0.0;
    double gpuFrameMs = 0.0;
    bool gpu = false;
};

/** Abstract backend lifecycle. Scene frames arrive through renderFrame(). */
class Preview3DRenderBackend
{
public:
    virtual ~Preview3DRenderBackend() {}

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual Preview3DRenderCapabilities capabilities() const = 0;

    virtual bool initialize(QString *error = NULL) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;

    /** Last measured GPU frame time in milliseconds (0 for software). */
    virtual double lastGpuFrameMs() const { return 0.0; }
    virtual int staticUploadCount() const { return 0; }
    virtual int shadowRenderCount() const { return 0; }
    virtual QString diagnostics() const { return QString(); }

    /** Render a scene frame into an image. Software backends may return
     *  false; the view then uses its direct software path. */
    virtual bool renderFrame(const Preview3DSceneFrame &frame, QImage &image,
                             QString *error = NULL)
    {
        Q_UNUSED(frame)
        Q_UNUSED(image)
        Q_UNUSED(error)
        return false;
    }
};

/** Existing CPU rasterizer exposed through the backend contract. */
class SoftwarePreview3DRenderBackend final : public Preview3DRenderBackend
{
public:
    SoftwarePreview3DRenderBackend();

    QString id() const override { return QStringLiteral("software"); }
    QString displayName() const override { return QStringLiteral("Software renderer"); }
    Preview3DRenderCapabilities capabilities() const override;
    bool initialize(QString *error = NULL) override;
    void shutdown() override;
    bool isInitialized() const override { return m_initialized; }
    QString diagnostics() const override;

private:
    bool m_initialized = false;
};

/** Offscreen OpenGL backend for the M4 scene, fixtures, beams and effects. */
class GpuPreview3DRenderBackend final : public Preview3DRenderBackend
{
public:
    GpuPreview3DRenderBackend();
    ~GpuPreview3DRenderBackend() override;

    QString id() const override { return QStringLiteral("opengl"); }
    QString displayName() const override { return QStringLiteral("OpenGL renderer"); }
    Preview3DRenderCapabilities capabilities() const override;
    bool initialize(QString *error = NULL) override;
    void shutdown() override;
    bool isInitialized() const override;
    double lastGpuFrameMs() const override;
    int staticUploadCount() const override;
    int shadowRenderCount() const override;
    QString diagnostics() const override;
    bool renderFrame(const Preview3DSceneFrame &frame, QImage &image,
                     QString *error = NULL) override;

private:
    Preview3DOpenGLRenderer *m_renderer;
};

/** Settings persistence and backend resolution. */
class Preview3DRenderBackendFactory
{
public:
    static Preview3DRenderSettings loadSettings();
    static void saveSettings(const Preview3DRenderSettings &settings);

    static QString rendererToString(Preview3DRenderer renderer);
    static Preview3DRenderer stringToRenderer(const QString &value);
    static QString qualityToString(Preview3DQuality quality);
    static Preview3DQuality stringToQuality(const QString &value);

    /** True when the build includes OpenGL and a context can be created here. */
    static bool gpuBackendAvailable();

    /**
     * Resolve the requested backend, falling back to software when needed.
     * The returned backend is owned by the caller.
     *
     * @param settings Requested renderer and quality
     * @param actual Receives the backend type that was actually selected
     * @param reason Receives a diagnostic message when fallback occurred
     */
    static Preview3DRenderBackend *createBackend(const Preview3DRenderSettings &settings,
                                                  Preview3DRenderer *actual = NULL,
                                                  QString *reason = NULL);
};

#endif // PREVIEW3DRENDERBACKEND_H
