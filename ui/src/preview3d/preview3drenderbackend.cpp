/*
  Q Light Controller Plus
  preview3drenderbackend.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QSettings>

#include "preview3dopenglrenderer.h"

#include "preview3drenderbackend.h"

#define SETTINGS_RENDERER     QStringLiteral("preview3d/renderer")
#define SETTINGS_QUALITY      QStringLiteral("preview3d/quality")
#define SETTINGS_SHADOWS      QStringLiteral("preview3d/shadows")
#define SETTINGS_VOLUMETRIC   QStringLiteral("preview3d/volumetricBeams")
#define SETTINGS_HAZE         QStringLiteral("preview3d/haze")
#define SETTINGS_SHOW_HINTS   QStringLiteral("preview3d/showHints")
#define SETTINGS_SHOW_STATUS  QStringLiteral("preview3d/showRenderStatus")
#define SETTINGS_SHOW_FPS     QStringLiteral("preview3d/showFps")
#define SETTINGS_RESOLUTION   QStringLiteral("preview3d/resolutionScale")

SoftwarePreview3DRenderBackend::SoftwarePreview3DRenderBackend()
{
}

Preview3DRenderCapabilities SoftwarePreview3DRenderBackend::capabilities() const
{
    Preview3DRenderCapabilities caps;
    caps.id = id();
    caps.displayName = displayName();
    caps.accelerated = false;
    caps.shadows = false;
    caps.volumetricBeams = true;
    caps.haze = true;
    caps.physicallyBased = false;
    caps.maxSupersample = 2;
    caps.notes = QStringLiteral("CPU software rasterizer; guaranteed fallback");
    return caps;
}

bool SoftwarePreview3DRenderBackend::initialize(QString *error)
{
    if (error != NULL)
        error->clear();
    m_initialized = true;
    return true;
}

void SoftwarePreview3DRenderBackend::shutdown()
{
    m_initialized = false;
}

QString SoftwarePreview3DRenderBackend::diagnostics() const
{
    return QStringLiteral("Software rasterizer: CPU");
}

GpuPreview3DRenderBackend::GpuPreview3DRenderBackend()
    : m_renderer(new Preview3DOpenGLRenderer())
{
}

GpuPreview3DRenderBackend::~GpuPreview3DRenderBackend()
{
    shutdown();
    delete m_renderer;
    m_renderer = NULL;
}

Preview3DRenderCapabilities GpuPreview3DRenderBackend::capabilities() const
{
    Preview3DRenderCapabilities caps;
    caps.id = id();
    caps.displayName = displayName();
    caps.accelerated = true;
    caps.shadows = true;
    caps.volumetricBeams = true;
    caps.haze = true;
    caps.physicallyBased = false;
    caps.maxSupersample = 4;
    caps.notes = QStringLiteral("OpenGL meshes, cached shadows, haze, beams, smoke and adaptive quality");
    return caps;
}

bool GpuPreview3DRenderBackend::initialize(QString *error)
{
    if (m_renderer == NULL)
        return false;
    if (m_renderer->initialize(error) == false)
        return false;

    if (Preview3DOpenGLRenderer::isSupported() == false)
    {
        m_renderer->shutdown();
        if (error != NULL)
            *error = QStringLiteral("OpenGL 3.3 core is not available");
        return false;
    }
    return true;
}

void GpuPreview3DRenderBackend::shutdown()
{
    if (m_renderer != NULL)
        m_renderer->shutdown();
}

bool GpuPreview3DRenderBackend::isInitialized() const
{
    return m_renderer != NULL && m_renderer->isInitialized();
}

double GpuPreview3DRenderBackend::lastGpuFrameMs() const
{
    return (m_renderer != NULL) ? m_renderer->lastGpuTimeMs() : 0.0;
}

int GpuPreview3DRenderBackend::staticUploadCount() const
{
    return (m_renderer != NULL) ? m_renderer->staticUploadCount() : 0;
}

int GpuPreview3DRenderBackend::shadowRenderCount() const
{
    return (m_renderer != NULL) ? m_renderer->shadowRenderCount() : 0;
}

QString GpuPreview3DRenderBackend::diagnostics() const
{
    return (m_renderer != NULL) ? m_renderer->driverInfo() : QString();
}

bool GpuPreview3DRenderBackend::renderFrame(const Preview3DSceneFrame &frame,
                                            QImage &image, QString *error)
{
    if (m_renderer == NULL || m_renderer->isInitialized() == false)
        return false;
    return m_renderer->render(frame, image, error);
}

Preview3DRenderSettings Preview3DRenderBackendFactory::loadSettings()
{
    QSettings settings;

    Preview3DRenderSettings out;
    out.renderer = stringToRenderer(settings.value(SETTINGS_RENDERER,
                                        rendererToString(Preview3DRenderer::Auto)).toString());
    out.quality = stringToQuality(settings.value(SETTINGS_QUALITY,
                                      qualityToString(Preview3DQuality::High)).toString());
    out.shadows = settings.value(SETTINGS_SHADOWS, true).toBool();
    out.volumetricBeams = settings.value(SETTINGS_VOLUMETRIC, true).toBool();
    out.haze = settings.value(SETTINGS_HAZE, true).toBool();
    out.showHints = settings.value(SETTINGS_SHOW_HINTS, false).toBool();
    out.showRenderStatus = settings.value(SETTINGS_SHOW_STATUS, false).toBool();
    out.showFps = settings.value(SETTINGS_SHOW_FPS, false).toBool();
    out.resolutionScale = qBound(0.5, settings.value(SETTINGS_RESOLUTION, 1.0).toDouble(), 2.0);
    return out;
}

void Preview3DRenderBackendFactory::saveSettings(const Preview3DRenderSettings &settings)
{
    QSettings out;
    out.setValue(SETTINGS_RENDERER, rendererToString(settings.renderer));
    out.setValue(SETTINGS_QUALITY, qualityToString(settings.quality));
    out.setValue(SETTINGS_SHADOWS, settings.shadows);
    out.setValue(SETTINGS_VOLUMETRIC, settings.volumetricBeams);
    out.setValue(SETTINGS_HAZE, settings.haze);
    out.setValue(SETTINGS_SHOW_HINTS, settings.showHints);
    out.setValue(SETTINGS_SHOW_STATUS, settings.showRenderStatus);
    out.setValue(SETTINGS_SHOW_FPS, settings.showFps);
    out.setValue(SETTINGS_RESOLUTION, settings.resolutionScale);
}

QString Preview3DRenderBackendFactory::rendererToString(Preview3DRenderer renderer)
{
    switch (renderer)
    {
    case Preview3DRenderer::Gpu:
        return QStringLiteral("gpu");
    case Preview3DRenderer::Software:
        return QStringLiteral("software");
    case Preview3DRenderer::Auto:
    default:
        return QStringLiteral("auto");
    }
}

Preview3DRenderer Preview3DRenderBackendFactory::stringToRenderer(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("gpu") || lower == QStringLiteral("opengl"))
        return Preview3DRenderer::Gpu;
    if (lower == QStringLiteral("software") || lower == QStringLiteral("cpu"))
        return Preview3DRenderer::Software;
    return Preview3DRenderer::Auto;
}

QString Preview3DRenderBackendFactory::qualityToString(Preview3DQuality quality)
{
    switch (quality)
    {
    case Preview3DQuality::Low:
        return QStringLiteral("low");
    case Preview3DQuality::Medium:
        return QStringLiteral("medium");
    case Preview3DQuality::Ultra:
        return QStringLiteral("ultra");
    case Preview3DQuality::High:
    default:
        return QStringLiteral("high");
    }
}

Preview3DQuality Preview3DRenderBackendFactory::stringToQuality(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    if (lower == QStringLiteral("low"))
        return Preview3DQuality::Low;
    if (lower == QStringLiteral("medium"))
        return Preview3DQuality::Medium;
    if (lower == QStringLiteral("ultra"))
        return Preview3DQuality::Ultra;
    return Preview3DQuality::High;
}

bool Preview3DRenderBackendFactory::gpuBackendAvailable()
{
    if (qEnvironmentVariableIsSet("QLC_PREVIEW3D_DISABLE_GPU"))
        return false;
    return Preview3DOpenGLRenderer::isSupported();
}

Preview3DRenderBackend *Preview3DRenderBackendFactory::createBackend(
        const Preview3DRenderSettings &settings, Preview3DRenderer *actual, QString *reason)
{
    Preview3DRenderer selected = settings.renderer;
    QString diagnostic;

    if (selected == Preview3DRenderer::Gpu && gpuBackendAvailable() == false)
    {
        diagnostic = QStringLiteral("GPU renderer is unavailable; using the software renderer");
        selected = Preview3DRenderer::Software;
    }
    else if (selected == Preview3DRenderer::Auto)
    {
        /* Prefer hardware when a usable context exists; unsupported systems
         * continue to fall back to the guaranteed software renderer. */
        if (gpuBackendAvailable())
        {
            selected = Preview3DRenderer::Gpu;
            diagnostic = QStringLiteral("Auto selected the OpenGL renderer");
        }
        else
        {
            selected = Preview3DRenderer::Software;
            diagnostic = QStringLiteral("OpenGL is unavailable; Auto uses the software renderer");
        }
    }

    if (actual != NULL)
        *actual = selected;
    if (reason != NULL)
        *reason = diagnostic;

    if (selected == Preview3DRenderer::Gpu)
        return new GpuPreview3DRenderBackend();

    return new SoftwarePreview3DRenderBackend();
}
