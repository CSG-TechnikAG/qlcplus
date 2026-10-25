/*
  Q Light Controller Plus
  preview3dopenglrenderer.h

  Offscreen OpenGL renderer for the 3D Preview scene. M4 renders the room,
  fixtures, additive beams, shadows, haze and smoke with cached static
  geometry, cached shadow maps, frustum culling and adaptive resolution.
  Preview3DView composites the image and keeps the software rasterizer as a
  guaranteed fallback.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PREVIEW3DOPENGLRENDERER_H
#define PREVIEW3DOPENGLRENDERER_H

#include <QImage>
#include <QMatrix4x4>
#include <QString>

struct Preview3DSceneFrame;
class QOpenGLContext;
class QOffscreenSurface;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLFunctions_3_3_Core;
class QOpenGLVertexArrayObject;
class QOpenGLBuffer;

class Preview3DOpenGLRenderer
{
public:
    Preview3DOpenGLRenderer();
    ~Preview3DOpenGLRenderer();

    /** True when an OpenGL context can be created on this system. */
    static bool isSupported();

    bool initialize(QString *error = NULL);
    void shutdown();
    bool isInitialized() const { return m_initialized; }
    double lastGpuTimeMs() const { return m_lastGpuTimeMs; }
    int staticUploadCount() const { return m_staticUploadCount; }
    int shadowRenderCount() const { return m_shadowRenderCount; }
    QString driverInfo() const { return m_driverInfo; }

    /**
     * Render the environment, fixtures, beams, shadows, haze and smoke.
     */
    bool render(const Preview3DSceneFrame &frame, QImage &image, QString *error = NULL);

private:
    bool ensureFramebuffer(int width, int height, int samples, QString *error);
    bool prepareGeometry(const Preview3DSceneFrame &frame, QString *error);
    bool initShadowResources(QString *error);
    void renderShadowMap(const Preview3DSceneFrame &frame);
    void renderSmoke(const Preview3DSceneFrame &frame, const QMatrix4x4 &mvp);
    void clearGeometry();

    bool m_initialized;
    bool m_failed;

    QOpenGLContext *m_context;
    QOffscreenSurface *m_surface;
    QOpenGLFramebufferObject *m_fbo;
    QOpenGLShaderProgram *m_program;
    QOpenGLShaderProgram *m_shadowProgram;
    QOpenGLShaderProgram *m_smokeProgram;
    QOpenGLFunctions_3_3_Core *m_gl;
    QOpenGLVertexArrayObject *m_vao;
    QOpenGLVertexArrayObject *m_smokeVao;
    QOpenGLBuffer *m_vbo;
    QOpenGLBuffer *m_fixtureVbo;
    QOpenGLBuffer *m_beamVbo;
    QOpenGLBuffer *m_smokeVbo;
    unsigned int m_shadowFbo;
    unsigned int m_shadowTexture;
    int m_shadowSize;
    int m_vertexCount;
    int m_fixtureVertexCount;
    int m_beamVertexCount;
    int m_smokeVertexCount;
    quint64 m_staticSignature;
    quint64 m_fixtureSignature;
    quint64 m_shadowSignature;
    bool m_shadowValid;
    int m_fboWidth;
    int m_fboHeight;
    int m_fboSamples;
    QMatrix4x4 m_lightMvp;
    QString m_driverInfo;
    double m_effectiveScale;
    double m_lastGpuTimeMs;
    unsigned int m_timerQueries[2];
    int m_timerIndex;
    unsigned int m_pendingTimer;
    int m_staticUploadCount;
    int m_shadowRenderCount;
};

#endif // PREVIEW3DOPENGLRENDERER_H
