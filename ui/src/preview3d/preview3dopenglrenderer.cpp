/*
  Q Light Controller Plus
  preview3dopenglrenderer.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtGlobal>
#include <QCoreApplication>
#include <QtMath>
#include <QVector>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QPointF>
#include <QVector4D>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QDebug>
#include <algorithm>
#include <cstring>

#include "preview3dopenglrenderer.h"
#include "preview3dsceneframe.h"

#if defined(QLC_PREVIEW3D_OPENGL) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define PREVIEW3D_GL_ENABLED 1
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#else
#define PREVIEW3D_GL_ENABLED 0
#endif

namespace
{
struct GLVertex
{
    float x, y, z;
    float nx, ny, nz;
    float r, g, b, a;
};

struct SmokeVertex
{
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

quint64 hashValue(quint64 h, quint64 v)
{
    return (h ^ v) * 1099511628211ULL;
}

quint64 hashDouble(quint64 h, double v)
{
    quint64 bits = 0;
    std::memcpy(&bits, &v, sizeof(double));
    return hashValue(h, bits);
}

quint64 hashColor(quint64 h, const QColor &color)
{
    return hashValue(h, quint64(color.rgba()));
}

struct Frustum
{
    QVector4D planes[6];

    bool intersectsSphere(const QVector3D &center, double radius) const
    {
        for (int i = 0; i < 6; ++i)
        {
            if (planes[i].x() * center.x() + planes[i].y() * center.y() +
                planes[i].z() * center.z() + planes[i].w() < -radius)
                return false;
        }
        return true;
    }
};

Frustum makeFrustum(const QMatrix4x4 &matrix)
{
    Frustum f;
    const QVector4D r0 = matrix.row(0);
    const QVector4D r1 = matrix.row(1);
    const QVector4D r2 = matrix.row(2);
    const QVector4D r3 = matrix.row(3);
    f.planes[0] = r3 + r0;
    f.planes[1] = r3 - r0;
    f.planes[2] = r3 + r1;
    f.planes[3] = r3 - r1;
    f.planes[4] = r3 + r2;
    f.planes[5] = r3 - r2;
    for (int i = 0; i < 6; ++i)
        f.planes[i].normalize();
    return f;
}

void addTriangle(QVector<GLVertex> &out,
                 const QVector3D &a, const QVector3D &b, const QVector3D &c,
                 const QVector3D &normal, const QColor &color)
{
    const GLVertex va = { a.x(), a.y(), a.z(), normal.x(), normal.y(), normal.z(),
                          float(color.redF()), float(color.greenF()),
                          float(color.blueF()), float(color.alphaF()) };
    const GLVertex vb = { b.x(), b.y(), b.z(), normal.x(), normal.y(), normal.z(),
                          va.r, va.g, va.b, va.a };
    const GLVertex vc = { c.x(), c.y(), c.z(), normal.x(), normal.y(), normal.z(),
                          va.r, va.g, va.b, va.a };
    out << va << vb << vc;
}

void addQuad(QVector<GLVertex> &out,
             const QVector3D &a, const QVector3D &b,
             const QVector3D &c, const QVector3D &d,
             const QVector3D &normal, const QColor &color)
{
    addTriangle(out, a, b, c, normal, color);
    addTriangle(out, a, c, d, normal, color);
}

void addTriangleRGBA(QVector<GLVertex> &out,
                     const QVector3D &a, const QVector3D &b, const QVector3D &c,
                     const QVector3D &normal,
                     const QColor &ca, const QColor &cb, const QColor &cc)
{
    const GLVertex va = { a.x(), a.y(), a.z(), normal.x(), normal.y(), normal.z(),
                          float(ca.redF()), float(ca.greenF()), float(ca.blueF()), float(ca.alphaF()) };
    const GLVertex vb = { b.x(), b.y(), b.z(), normal.x(), normal.y(), normal.z(),
                          float(cb.redF()), float(cb.greenF()), float(cb.blueF()), float(cb.alphaF()) };
    const GLVertex vc = { c.x(), c.y(), c.z(), normal.x(), normal.y(), normal.z(),
                          float(cc.redF()), float(cc.greenF()), float(cc.blueF()), float(cc.alphaF()) };
    out << va << vb << vc;
}

void addQuadRGBA(QVector<GLVertex> &out,
                 const QVector3D &a, const QVector3D &b,
                 const QVector3D &c, const QVector3D &d,
                 const QVector3D &normal,
                 const QColor &ca, const QColor &cb,
                 const QColor &cc, const QColor &cd)
{
    addTriangleRGBA(out, a, b, c, normal, ca, cb, cc);
    addTriangleRGBA(out, a, c, d, normal, ca, cc, cd);
}

void addHorizontalLine(QVector<GLVertex> &out,
                       double x0, double y0, double x1, double y1,
                       double z, double halfWidth, const QColor &color)
{
    const QVector3D a(x0, y0, z);
    const QVector3D b(x1, y1, z);
    QVector3D d = b - a;
    if (d.lengthSquared() < 1e-9)
        return;
    d.normalize();
    const QVector3D n(-d.y() * halfWidth, d.x() * halfWidth, 0.0);
    addQuad(out, a - n, a + n, b + n, b - n, QVector3D(0, 0, 1), color);
}

void addPrism(QVector<GLVertex> &out, const QVector3D &a, const QVector3D &b,
              double radius, const QColor &color)
{
    QVector3D axis = b - a;
    if (axis.lengthSquared() < 1e-9)
        return;
    axis.normalize();

    QVector3D reference = qAbs(axis.z()) < 0.9 ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0);
    QVector3D u = QVector3D::crossProduct(axis, reference).normalized();
    QVector3D v = QVector3D::crossProduct(axis, u).normalized();
    const int segments = 6;

    for (int i = 0; i < segments; ++i)
    {
        const double a0 = 2.0 * M_PI * i / segments;
        const double a1 = 2.0 * M_PI * (i + 1) / segments;
        const QVector3D n0 = u * qCos(a0) + v * qSin(a0);
        const QVector3D n1 = u * qCos(a1) + v * qSin(a1);
        const QVector3D p0 = a + n0 * radius;
        const QVector3D p1 = a + n1 * radius;
        const QVector3D p2 = b + n1 * radius;
        const QVector3D p3 = b + n0 * radius;
        addQuad(out, p0, p1, p2, p3, (n0 + n1).normalized(), color);
    }
}

void addDisk(QVector<GLVertex> &out, const QVector3D &center,
             const QQuaternion &rot, double radius, const QColor &color);

void addCylinder(QVector<GLVertex> &out, const QVector3D &center,
                 const QQuaternion &rot, double radius, double length,
                 const QColor &color, int segments = 10)
{
    const QVector3D axis = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
    const QVector3D a = center - axis * (length * 0.5);
    const QVector3D b = center + axis * (length * 0.5);
    addPrism(out, a, b, radius, color);
    addDisk(out, b, rot, radius, color.lighter(108));
    addDisk(out, a, rot * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), 180.0),
            radius, color.darker(112));
    Q_UNUSED(segments)
}

void addDisk(QVector<GLVertex> &out, const QVector3D &center,
             const QQuaternion &rot, double radius, const QColor &color)
{
    const int segments = 12;
    const QVector3D normal = rot.rotatedVector(QVector3D(0.0, 1.0, 0.0));
    for (int i = 0; i < segments; ++i)
    {
        const double a0 = 2.0 * M_PI * i / segments;
        const double a1 = 2.0 * M_PI * (i + 1) / segments;
        const QVector3D p0 = center + rot.rotatedVector(QVector3D(qCos(a0) * radius, 0.0, qSin(a0) * radius));
        const QVector3D p1 = center + rot.rotatedVector(QVector3D(qCos(a1) * radius, 0.0, qSin(a1) * radius));
        addTriangle(out, center, p0, p1, normal, color);
    }
}

void addBox(QVector<GLVertex> &out, const QVector3D &center, const QQuaternion &rot,
            const QVector3D &size, const QColor &color)
{
    const QVector3D h = size * 0.5f;
    const QVector3D local[8] = {
        QVector3D(-h.x(), -h.y(), -h.z()), QVector3D( h.x(), -h.y(), -h.z()),
        QVector3D( h.x(),  h.y(), -h.z()), QVector3D(-h.x(),  h.y(), -h.z()),
        QVector3D(-h.x(), -h.y(),  h.z()), QVector3D( h.x(), -h.y(),  h.z()),
        QVector3D( h.x(),  h.y(),  h.z()), QVector3D(-h.x(),  h.y(),  h.z())
    };
    QVector3D p[8];
    for (int i = 0; i < 8; ++i)
        p[i] = center + rot.rotatedVector(local[i]);

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
        addQuad(out, p[faces[f][0]], p[faces[f][1]], p[faces[f][2]], p[faces[f][3]], n, color);
    }
}

void addCone(QVector<GLVertex> &out, const Preview3DBeamSnapshot &beam)
{
    if (beam.alpha <= 0 || beam.length <= 0.01)
        return;
    const QVector3D dir = beam.rotation.rotatedVector(QVector3D(0.0, 1.0, 0.0)).normalized();
    QVector3D u = QVector3D::crossProduct(dir, QVector3D(0, 0, 1));
    if (u.lengthSquared() < 1e-6)
        u = QVector3D(1, 0, 0);
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(dir, u).normalized();
    const double radius = qTan(qDegreesToRadians(beam.beamDeg * 0.5)) * beam.length;
    const int segments = 12;
    QColor nearCol = beam.color;
    nearCol.setAlpha(qBound(0, beam.alpha, 255));
    QColor farCol = beam.color;
    farCol.setAlpha(qBound(0, beam.alpha / 4, 255));

    QVector3D ring[segments];
    for (int i = 0; i < segments; ++i)
    {
        const double a = 2.0 * M_PI * i / segments;
        ring[i] = beam.origin + dir * beam.length + u * (qCos(a) * radius) + v * (qSin(a) * radius);
    }
    for (int i = 0; i < segments; ++i)
    {
        const QVector3D &p1 = ring[i];
        const QVector3D &p2 = ring[(i + 1) % segments];
        addTriangleRGBA(out, beam.origin, p1, p2, dir, nearCol, farCol, farCol);
    }
}

void addRectBeam(QVector<GLVertex> &out, const Preview3DBeamSnapshot &beam)
{
    if (beam.alpha <= 0 || beam.length <= 0.01)
        return;
    const QVector3D dir = beam.rotation.rotatedVector(QVector3D(0.0, 1.0, 0.0)).normalized();
    const QVector3D rx = beam.rotation.rotatedVector(QVector3D(1.0, 0.0, 0.0));
    const QVector3D rz = beam.rotation.rotatedVector(QVector3D(0.0, 0.0, 1.0));
    const double ew = beam.halfWidth + beam.spread;
    const double eh = beam.halfHeight + beam.spread;

    const QVector3D n0 = beam.origin - rx * beam.halfWidth - rz * beam.halfHeight;
    const QVector3D n1 = beam.origin + rx * beam.halfWidth - rz * beam.halfHeight;
    const QVector3D n2 = beam.origin + rx * beam.halfWidth + rz * beam.halfHeight;
    const QVector3D n3 = beam.origin - rx * beam.halfWidth + rz * beam.halfHeight;
    const QVector3D end = beam.origin + dir * beam.length;
    const QVector3D f0 = end - rx * ew - rz * eh;
    const QVector3D f1 = end + rx * ew - rz * eh;
    const QVector3D f2 = end + rx * ew + rz * eh;
    const QVector3D f3 = end - rx * ew + rz * eh;

    QColor nearCol = beam.color;
    nearCol.setAlpha(qBound(0, beam.alpha, 255));
    QColor farCol = beam.color;
    farCol.setAlpha(qBound(0, beam.alpha / 5, 255));
    addQuadRGBA(out, n0, n1, f1, f0, dir, nearCol, nearCol, farCol, farCol);
    addQuadRGBA(out, n1, n2, f2, f1, dir, nearCol, nearCol, farCol, farCol);
    addQuadRGBA(out, n2, n3, f3, f2, dir, nearCol, nearCol, farCol, farCol);
    addQuadRGBA(out, n3, n0, f0, f3, dir, nearCol, nearCol, farCol, farCol);
    addQuadRGBA(out, f0, f1, f2, f3, dir, farCol, farCol, farCol, farCol);
}
} // namespace

Preview3DOpenGLRenderer::Preview3DOpenGLRenderer()
    : m_initialized(false)
    , m_failed(false)
    , m_context(NULL)
    , m_surface(NULL)
    , m_fbo(NULL)
    , m_program(NULL)
    , m_shadowProgram(NULL)
    , m_smokeProgram(NULL)
    , m_gl(NULL)
    , m_vao(NULL)
    , m_smokeVao(NULL)
    , m_vbo(NULL)
    , m_fixtureVbo(NULL)
    , m_beamVbo(NULL)
    , m_smokeVbo(NULL)
    , m_shadowFbo(0)
    , m_shadowTexture(0)
    , m_shadowSize(1024)
    , m_vertexCount(0)
    , m_fixtureVertexCount(0)
    , m_beamVertexCount(0)
    , m_smokeVertexCount(0)
    , m_staticSignature(0)
    , m_fixtureSignature(0)
    , m_shadowSignature(0)
    , m_shadowValid(false)
    , m_fboWidth(0)
    , m_fboHeight(0)
    , m_fboSamples(0)
    , m_effectiveScale(-1.0)
    , m_lastGpuTimeMs(0.0)
    , m_timerIndex(0)
    , m_pendingTimer(0)
    , m_staticUploadCount(0)
    , m_shadowRenderCount(0)
{
    m_timerQueries[0] = 0;
    m_timerQueries[1] = 0;
}

Preview3DOpenGLRenderer::~Preview3DOpenGLRenderer()
{
    shutdown();
}

bool Preview3DOpenGLRenderer::isSupported()
{
#if PREVIEW3D_GL_ENABLED
    if (QCoreApplication::instance() == NULL)
        return false;

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    QOpenGLContext context;
    context.setFormat(format);
    if (context.create() == false)
        return false;

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    return surface.isValid();
#else
    return false;
#endif
}

bool Preview3DOpenGLRenderer::initialize(QString *error)
{
    if (m_initialized)
        return true;
    if (m_failed)
    {
        if (error != NULL)
            *error = QStringLiteral("The OpenGL renderer previously failed to initialize");
        return false;
    }

#if PREVIEW3D_GL_ENABLED
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    m_surface = new QOffscreenSurface();
    m_surface->setFormat(format);
    m_surface->create();
    if (m_surface->isValid() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an offscreen OpenGL surface");
        return false;
    }

    m_context = new QOpenGLContext();
    m_context->setFormat(format);
    if (m_context->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL 3.3 context");
        return false;
    }

    if (m_context->makeCurrent(m_surface) == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot activate the OpenGL context");
        return false;
    }

    m_gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(m_context);
    if (m_gl == NULL)
    {
        m_failed = true;
        m_context->doneCurrent();
        if (error != NULL)
            *error = QStringLiteral("OpenGL 3.3 core functions are not available");
        return false;
    }
    m_gl->initializeOpenGLFunctions();
    m_gl->glGenQueries(2, m_timerQueries);

    const GLubyte *vendor = m_gl->glGetString(GL_VENDOR);
    const GLubyte *renderer = m_gl->glGetString(GL_RENDERER);
    const GLubyte *version = m_gl->glGetString(GL_VERSION);
    const GLubyte *glsl = m_gl->glGetString(GL_SHADING_LANGUAGE_VERSION);
    m_driverInfo = QStringLiteral("OpenGL vendor: %1\nOpenGL renderer: %2\nOpenGL version: %3\nGLSL: %4")
            .arg(vendor != NULL ? QString::fromLatin1(reinterpret_cast<const char *>(vendor))
                                : QStringLiteral("unknown"))
            .arg(renderer != NULL ? QString::fromLatin1(reinterpret_cast<const char *>(renderer))
                                  : QStringLiteral("unknown"))
            .arg(version != NULL ? QString::fromLatin1(reinterpret_cast<const char *>(version))
                                 : QStringLiteral("unknown"))
            .arg(glsl != NULL ? QString::fromLatin1(reinterpret_cast<const char *>(glsl))
                              : QStringLiteral("unknown"));

    m_program = new QOpenGLShaderProgram();
    const char *vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uMvp;\n"
        "out vec3 vNormal;\n"
        "out vec4 vColor;\n"
        "out vec3 vWorldPos;\n"
        "void main() {\n"
        "    gl_Position = uMvp * vec4(aPosition, 1.0);\n"
        "    vNormal = aNormal;\n"
        "    vColor = aColor;\n"
        "    vWorldPos = aPosition;\n"
        "}\n";
    const char *fragmentSource =
        "#version 330 core\n"
        "in vec3 vNormal;\n"
        "in vec4 vColor;\n"
        "in vec3 vWorldPos;\n"
        "uniform vec3 uLightDir;\n"
        "uniform vec3 uCameraPos;\n"
        "uniform vec3 uHazeColor;\n"
        "uniform float uHazeDensity;\n"
        "uniform float uSmokeScatter;\n"
        "uniform mat4 uLightMvp;\n"
        "uniform sampler2D uShadowMap;\n"
        "uniform int uHasShadows;\n"
        "out vec4 fragColor;\n"
        "float shadowFactor(vec3 world, vec3 n) {\n"
        "    if (uHasShadows == 0) return 1.0;\n"
        "    vec4 lp = uLightMvp * vec4(world, 1.0);\n"
        "    vec3 p = lp.xyz / lp.w;\n"
        "    p = p * 0.5 + 0.5;\n"
        "    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;\n"
        "    float bias = max(0.0012 * (1.0 - dot(n, normalize(uLightDir))), 0.0004);\n"
        "    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));\n"
        "    float sum = 0.0;\n"
        "    for (int x = -1; x <= 1; ++x) {\n"
        "        for (int y = -1; y <= 1; ++y) {\n"
        "            float d = texture(uShadowMap, p.xy + vec2(x, y) * texel).r;\n"
        "            sum += (p.z - bias > d) ? 0.0 : 1.0;\n"
        "        }\n"
        "    }\n"
        "    return sum / 9.0;\n"
        "}\n"
        "void main() {\n"
        "    if (vColor.a < 0.99) {\n"
        "        fragColor = vec4(vColor.rgb * (1.0 + uSmokeScatter), vColor.a);\n"
        "        return;\n"
        "    }\n"
        "    vec3 n = normalize(vNormal);\n"
        "    float diffuse = max(dot(n, normalize(uLightDir)), 0.0);\n"
        "    float shadow = shadowFactor(vWorldPos, n);\n"
        "    float shade = 0.5 + 0.55 * diffuse * mix(0.25, 1.0, shadow);\n"
        "    vec3 color = vColor.rgb * shade;\n"
        "    if (uHazeDensity > 0.0) {\n"
        "        float dist = length(vWorldPos - uCameraPos);\n"
        "        float fog = 1.0 - exp(-dist * uHazeDensity);\n"
        "        color = mix(color, uHazeColor, fog);\n"
        "    }\n"
        "    fragColor = vec4(color, vColor.a);\n"
        "}\n";
    if (m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource) == false ||
        m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource) == false ||
        m_program->link() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("OpenGL shader compilation failed: %1").arg(m_program->log());
        m_context->doneCurrent();
        return false;
    }

    m_shadowProgram = new QOpenGLShaderProgram();
    const char *shadowVertex =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "uniform mat4 uMvp;\n"
        "void main() { gl_Position = uMvp * vec4(aPosition, 1.0); }\n";
    const char *shadowFragment =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0); }\n";
    if (m_shadowProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, shadowVertex) == false ||
        m_shadowProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, shadowFragment) == false ||
        m_shadowProgram->link() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("OpenGL shadow shader compilation failed: %1")
                    .arg(m_shadowProgram->log());
        m_context->doneCurrent();
        return false;
    }

    m_smokeProgram = new QOpenGLShaderProgram();
    const char *smokeVertex =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec2 aUv;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "uniform mat4 uMvp;\n"
        "out vec2 vUv;\n"
        "out vec4 vColor;\n"
        "out vec3 vWorldPos;\n"
        "void main() {\n"
        "    gl_Position = uMvp * vec4(aPosition, 1.0);\n"
        "    vUv = aUv;\n"
        "    vColor = aColor;\n"
        "    vWorldPos = aPosition;\n"
        "}\n";
    const char *smokeFragment =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "in vec4 vColor;\n"
        "in vec3 vWorldPos;\n"
        "uniform vec3 uRoomMin;\n"
        "uniform vec3 uRoomMax;\n"
        "out vec4 fragColor;\n"
        "float hash31(vec3 p) {\n"
        "    p = fract(p * 0.1031);\n"
        "    p += dot(p, p.yzx + 33.33);\n"
        "    return fract((p.x + p.y) * p.z);\n"
        "}\n"
        "float vnoise(vec3 p) {\n"
        "    vec3 i = floor(p);\n"
        "    vec3 f = fract(p);\n"
        "    f = f * f * (3.0 - 2.0 * f);\n"
        "    float n000 = hash31(i + vec3(0,0,0));\n"
        "    float n100 = hash31(i + vec3(1,0,0));\n"
        "    float n010 = hash31(i + vec3(0,1,0));\n"
        "    float n110 = hash31(i + vec3(1,1,0));\n"
        "    float n001 = hash31(i + vec3(0,0,1));\n"
        "    float n101 = hash31(i + vec3(1,0,1));\n"
        "    float n011 = hash31(i + vec3(0,1,1));\n"
        "    float n111 = hash31(i + vec3(1,1,1));\n"
        "    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),\n"
        "               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);\n"
        "}\n"
        "void main() {\n"
        "    if (vWorldPos.x < uRoomMin.x || vWorldPos.x > uRoomMax.x ||\n"
        "        vWorldPos.y < uRoomMin.y || vWorldPos.y > uRoomMax.y ||\n"
        "        vWorldPos.z < uRoomMin.z || vWorldPos.z > uRoomMax.z) discard;\n"
        "    float d = length(vUv * 2.0 - 1.0);\n"
        "    float edge = 1.0 - smoothstep(0.08, 1.0, d);\n"
        "    vec3 np = vWorldPos * 2.4 + vec3(vUv * 1.7, vWorldPos.z * 0.3);\n"
        "    float wisp = vnoise(np) * 0.72 + vnoise(np * 2.13 + 7.0) * 0.28;\n"
        "    float a = vColor.a * edge * smoothstep(0.12, 0.72, wisp);\n"
        "    if (a <= 0.002) discard;\n"
        "    fragColor = vec4(vColor.rgb, a);\n"
        "}\n";
    if (m_smokeProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, smokeVertex) == false ||
        m_smokeProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, smokeFragment) == false ||
        m_smokeProgram->link() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("OpenGL smoke shader compilation failed: %1")
                    .arg(m_smokeProgram->log());
        m_context->doneCurrent();
        return false;
    }

    if (initShadowResources(error) == false)
    {
        m_context->doneCurrent();
        return false;
    }

    m_vao = new QOpenGLVertexArrayObject();
    if (m_vao->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL vertex array object");
        m_context->doneCurrent();
        return false;
    }

    m_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (m_vbo->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL vertex buffer");
        m_context->doneCurrent();
        return false;
    }

    m_fixtureVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (m_fixtureVbo->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create the fixture vertex buffer");
        m_context->doneCurrent();
        return false;
    }

    m_beamVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (m_beamVbo->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL beam vertex buffer");
        m_context->doneCurrent();
        return false;
    }

    m_smokeVao = new QOpenGLVertexArrayObject();
    if (m_smokeVao->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL smoke vertex array object");
        m_context->doneCurrent();
        return false;
    }

    m_smokeVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (m_smokeVbo->create() == false)
    {
        m_failed = true;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL smoke vertex buffer");
        m_context->doneCurrent();
        return false;
    }

    m_context->doneCurrent();
    m_initialized = true;
    return true;
#else
    if (error != NULL)
        *error = QStringLiteral("This build does not include the OpenGL backend");
    return false;
#endif
}

void Preview3DOpenGLRenderer::shutdown()
{
#if PREVIEW3D_GL_ENABLED
    if (m_context != NULL && m_context->isValid())
        m_context->makeCurrent(m_surface);
    delete m_fbo;
    m_fbo = NULL;
    delete m_vbo;
    m_vbo = NULL;
    delete m_fixtureVbo;
    m_fixtureVbo = NULL;
    delete m_beamVbo;
    m_beamVbo = NULL;
    delete m_smokeVbo;
    m_smokeVbo = NULL;
    delete m_vao;
    m_vao = NULL;
    delete m_smokeVao;
    m_smokeVao = NULL;
    delete m_program;
    m_program = NULL;
    delete m_shadowProgram;
    m_shadowProgram = NULL;
    delete m_smokeProgram;
    m_smokeProgram = NULL;
    if (m_gl != NULL)
    {
        if (m_timerQueries[0] != 0 || m_timerQueries[1] != 0)
            m_gl->glDeleteQueries(2, m_timerQueries);
        if (m_shadowFbo != 0)
            m_gl->glDeleteFramebuffers(1, &m_shadowFbo);
        if (m_shadowTexture != 0)
            m_gl->glDeleteTextures(1, &m_shadowTexture);
    }
    m_timerQueries[0] = 0;
    m_timerQueries[1] = 0;
    m_shadowFbo = 0;
    m_shadowTexture = 0;
    if (m_context != NULL)
        m_context->doneCurrent();
    delete m_context;
    m_context = NULL;
    delete m_surface;
    m_surface = NULL;
#endif
    m_initialized = false;
}

bool Preview3DOpenGLRenderer::ensureFramebuffer(int width, int height, int samples, QString *error)
{
#if PREVIEW3D_GL_ENABLED
    if (width <= 0 || height <= 0)
    {
        if (error != NULL)
            *error = QStringLiteral("Invalid OpenGL framebuffer size");
        return false;
    }
    if (m_fbo != NULL && m_fboWidth == width && m_fboHeight == height &&
        m_fboSamples == samples)
        return true;

    delete m_fbo;
    m_fbo = NULL;

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(qBound(0, samples, 4));
    m_fbo = new QOpenGLFramebufferObject(width, height, format);
    if (m_fbo->isValid() == false && format.samples() > 0)
    {
        delete m_fbo;
        format.setSamples(0);
        m_fbo = new QOpenGLFramebufferObject(width, height, format);
    }
    if (m_fbo->isValid() == false)
    {
        delete m_fbo;
        m_fbo = NULL;
        if (error != NULL)
            *error = QStringLiteral("Cannot create an OpenGL framebuffer object");
        return false;
    }
    m_fboWidth = width;
    m_fboHeight = height;
    m_fboSamples = format.samples();
    return true;
#else
    Q_UNUSED(width)
    Q_UNUSED(height)
    Q_UNUSED(samples)
    Q_UNUSED(error)
    return false;
#endif
}

bool Preview3DOpenGLRenderer::initShadowResources(QString *error)
{
#if PREVIEW3D_GL_ENABLED
    Q_UNUSED(error)
    if (m_shadowFbo != 0)
        return true;

    m_gl->glGenTextures(1, &m_shadowTexture);
    m_gl->glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
    m_gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_shadowSize, m_shadowSize,
                       0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_gl->glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    m_gl->glBindTexture(GL_TEXTURE_2D, 0);

    m_gl->glGenFramebuffers(1, &m_shadowFbo);
    m_gl->glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    m_gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_TEXTURE_2D, m_shadowTexture, 0);
    m_gl->glDrawBuffer(GL_NONE);
    m_gl->glReadBuffer(GL_NONE);
    const GLenum status = m_gl->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    m_gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        qWarning() << "Shadow map framebuffer is incomplete:" << status;
        m_gl->glDeleteFramebuffers(1, &m_shadowFbo);
        m_gl->glDeleteTextures(1, &m_shadowTexture);
        m_shadowFbo = 0;
        m_shadowTexture = 0;
    }
    return true;
#else
    Q_UNUSED(error)
    return false;
#endif
}

void Preview3DOpenGLRenderer::clearGeometry()
{
    m_vertexCount = 0;
    m_fixtureVertexCount = 0;
    m_beamVertexCount = 0;
    m_smokeVertexCount = 0;
#if PREVIEW3D_GL_ENABLED
    if (m_vbo != NULL && m_vbo->isCreated())
        m_vbo->destroy();
    if (m_fixtureVbo != NULL && m_fixtureVbo->isCreated())
        m_fixtureVbo->destroy();
    if (m_beamVbo != NULL && m_beamVbo->isCreated())
        m_beamVbo->destroy();
    if (m_smokeVbo != NULL && m_smokeVbo->isCreated())
        m_smokeVbo->destroy();
#endif
}

bool Preview3DOpenGLRenderer::prepareGeometry(const Preview3DSceneFrame &frame, QString *error)
{
#if PREVIEW3D_GL_ENABLED
    QVector<GLVertex> vertices;
    vertices.reserve(4096);

    // Floor.
    const QColor floorColor(34, 43, 54);
    const QColor floorGrid(60, 78, 97);
    addQuad(vertices,
            QVector3D(frame.roomX0, frame.roomY0, frame.zFloor),
            QVector3D(frame.roomX1, frame.roomY0, frame.zFloor),
            QVector3D(frame.roomX1, frame.roomY1, frame.zFloor),
            QVector3D(frame.roomX0, frame.roomY1, frame.zFloor),
            QVector3D(0, 0, 1), floorColor);

    if (frame.showGrid)
    {
        const double step = 1.0;
        for (double x = qCeil(frame.roomX0 / step) * step; x <= frame.roomX1; x += step)
            addHorizontalLine(vertices, x, frame.roomY0, x, frame.roomY1,
                              frame.zFloor + 0.001, 0.006, floorGrid);
        for (double y = qCeil(frame.roomY0 / step) * step; y <= frame.roomY1; y += step)
            addHorizontalLine(vertices, frame.roomX0, y, frame.roomX1, y,
                              frame.zFloor + 0.001, 0.006, floorGrid);
    }

    // Room boundary. The software renderer uses a dashed 12-edge box; the GPU
    // path keeps the same readable enclosure with thin solid rail geometry.
    {
        const QVector3D bottom[4] = {
            QVector3D(frame.roomX0, frame.roomY0, frame.zFloor),
            QVector3D(frame.roomX1, frame.roomY0, frame.zFloor),
            QVector3D(frame.roomX1, frame.roomY1, frame.zFloor),
            QVector3D(frame.roomX0, frame.roomY1, frame.zFloor)
        };
        const QVector3D top[4] = {
            QVector3D(frame.roomX0, frame.roomY0, frame.zTop),
            QVector3D(frame.roomX1, frame.roomY0, frame.zTop),
            QVector3D(frame.roomX1, frame.roomY1, frame.zTop),
            QVector3D(frame.roomX0, frame.roomY1, frame.zTop)
        };
        const QColor roomEdge(96, 126, 156);
        for (int i = 0; i < 4; ++i)
        {
            const int n = (i + 1) % 4;
            addPrism(vertices, bottom[i], bottom[n], 0.004, roomEdge);
            addPrism(vertices, top[i], top[n], 0.004, roomEdge);
            addPrism(vertices, bottom[i], top[i], 0.004, roomEdge);
        }
    }

    // Stage deck: top and four sides, with a very simple solid depth.
    if (frame.stageX > 0.0 && frame.stageY > 0.0)
    {
        const double deckBottom = qMax(frame.zFloor, -0.08);
        const QColor top(45, 54, 63);
        const QColor side(25, 30, 36);
        const QColor trim(108, 120, 132);
        const QColor seam(31, 37, 43);
        addQuad(vertices,
                QVector3D(0, 0, 0), QVector3D(frame.stageX, 0, 0),
                QVector3D(frame.stageX, frame.stageY, 0), QVector3D(0, frame.stageY, 0),
                QVector3D(0, 0, 1), top);
        addQuad(vertices, QVector3D(0, 0, 0), QVector3D(frame.stageX, 0, 0),
                QVector3D(frame.stageX, 0, deckBottom), QVector3D(0, 0, deckBottom),
                QVector3D(0, -1, 0), side);
        addQuad(vertices, QVector3D(frame.stageX, 0, 0), QVector3D(frame.stageX, frame.stageY, 0),
                QVector3D(frame.stageX, frame.stageY, deckBottom), QVector3D(frame.stageX, 0, deckBottom),
                QVector3D(1, 0, 0), side);
        addQuad(vertices, QVector3D(frame.stageX, frame.stageY, 0), QVector3D(0, frame.stageY, 0),
                QVector3D(0, frame.stageY, deckBottom), QVector3D(frame.stageX, frame.stageY, deckBottom),
                QVector3D(0, 1, 0), side);
        addQuad(vertices, QVector3D(0, frame.stageY, 0), QVector3D(0, 0, 0),
                QVector3D(0, 0, deckBottom), QVector3D(0, frame.stageY, deckBottom),
                QVector3D(-1, 0, 0), side);

        const double riser = deckBottom - frame.zFloor;
        if (riser > 1e-4)
        {
            const double legHalf = 0.03;
            QList<double> xs;
            for (double x = 0.0; x <= frame.stageX + 1e-6; x += 1.0)
                xs << x;
            if (xs.isEmpty() || qAbs(xs.last() - frame.stageX) > 1e-6)
                xs << frame.stageX;
            QList<double> ys;
            for (double y = 0.0; y <= frame.stageY + 1e-6; y += 1.0)
                ys << y;
            if (ys.isEmpty() || qAbs(ys.last() - frame.stageY) > 1e-6)
                ys << frame.stageY;

            QList<QPointF> feet;
            for (double x : xs)
            {
                feet << QPointF(x, 0.0);
                feet << QPointF(x, frame.stageY);
            }
            for (double y : ys)
            {
                if (y <= 1e-6 || y >= frame.stageY - 1e-6)
                    continue;
                feet << QPointF(0.0, y);
                feet << QPointF(frame.stageX, y);
            }
            auto inset = [](double v, double half, double len) {
                if (len <= 2.0 * half)
                    return len * 0.5;
                return qBound(half, v, len - half);
            };
            const QColor legColor(42, 50, 58);
            for (const QPointF &f : feet)
            {
                const double x = inset(f.x(), legHalf, frame.stageX);
                const double y = inset(f.y(), legHalf, frame.stageY);
                addBox(vertices, QVector3D(x, y, frame.zFloor + riser * 0.5), QQuaternion(),
                       QVector3D(legHalf * 2.0, legHalf * 2.0, riser), legColor);
            }
        }

        // Layered floor boards and a restrained perimeter trim.
        for (double y = 0.5; y < frame.stageY - 1e-6; y += 0.5)
            addHorizontalLine(vertices, 0.0, y, frame.stageX, y, 0.002, 0.0035, seam);
        addHorizontalLine(vertices, 0.0, 0.0, frame.stageX, 0.0, 0.004, 0.012, trim);
        addHorizontalLine(vertices, 0.0, frame.stageY, frame.stageX, frame.stageY, 0.004, 0.012, trim);
        addHorizontalLine(vertices, 0.0, 0.0, 0.0, frame.stageY, 0.004, 0.012, trim);
        addHorizontalLine(vertices, frame.stageX, 0.0, frame.stageX, frame.stageY, 0.004, 0.012, trim);

        if (frame.showGrid)
        {
            const QColor stageGrid(91, 105, 119);
            for (double x = 0.0; x <= frame.stageX + 1e-6; x += 1.0)
                addHorizontalLine(vertices, x, 0.0, x, frame.stageY, 0.001, 0.005, stageGrid);
            for (double y = 0.0; y <= frame.stageY + 1e-6; y += 1.0)
                addHorizontalLine(vertices, 0.0, y, frame.stageX, y, 0.001, 0.005, stageGrid);
        }
    }

    // Real truss volume: every render segment becomes a six-sided tube.
    for (const Preview3DTrussSegmentSnapshot &segment : frame.trussSegments)
        addPrism(vertices, segment.a, segment.b, segment.radius, segment.color);

    for (const Preview3DTrussConnectorSnapshot &connector : frame.trussConnectors)
    {
        const QQuaternion rot = QQuaternion::rotationTo(QVector3D(0, 1, 0),
                                                        connector.direction.normalized());
        addCylinder(vertices, connector.pos, rot, connector.radius,
                    connector.length, connector.color);
    }

    quint64 staticSig = 1469598103934665603ULL;
    staticSig = hashValue(staticSig, frame.showGrid ? 1 : 0);
    staticSig = hashDouble(staticSig, frame.roomX0);
    staticSig = hashDouble(staticSig, frame.roomX1);
    staticSig = hashDouble(staticSig, frame.roomY0);
    staticSig = hashDouble(staticSig, frame.roomY1);
    staticSig = hashDouble(staticSig, frame.zFloor);
    staticSig = hashDouble(staticSig, frame.zTop);
    staticSig = hashDouble(staticSig, frame.stageX);
    staticSig = hashDouble(staticSig, frame.stageY);
    for (const Preview3DTrussSegmentSnapshot &segment : frame.trussSegments)
    {
        staticSig = hashDouble(staticSig, segment.a.x());
        staticSig = hashDouble(staticSig, segment.a.y());
        staticSig = hashDouble(staticSig, segment.a.z());
        staticSig = hashDouble(staticSig, segment.b.x());
        staticSig = hashDouble(staticSig, segment.b.y());
        staticSig = hashDouble(staticSig, segment.b.z());
        staticSig = hashDouble(staticSig, segment.radius);
        staticSig = hashColor(staticSig, segment.color);
    }
    for (const Preview3DTrussConnectorSnapshot &connector : frame.trussConnectors)
    {
        staticSig = hashDouble(staticSig, connector.pos.x());
        staticSig = hashDouble(staticSig, connector.pos.y());
        staticSig = hashDouble(staticSig, connector.pos.z());
        staticSig = hashDouble(staticSig, connector.direction.x());
        staticSig = hashDouble(staticSig, connector.direction.y());
        staticSig = hashDouble(staticSig, connector.direction.z());
        staticSig = hashDouble(staticSig, connector.radius);
        staticSig = hashDouble(staticSig, connector.length);
        staticSig = hashColor(staticSig, connector.color);
    }

    QVector<GLVertex> fixtureVertices;
    fixtureVertices.reserve(2048);

    // Fixture bodies, heads, yokes and lenses.
    const QColor selectColor(80, 180, 230);
    const QColor defaultShell(58, 60, 66);
    const QColor defaultMetal(78, 82, 89);
    for (const FixtureInstance &fi : frame.fixtures)
    {
        if (!fi.visible)
            continue;
        const bool selected = frame.fixtureInteractive && (frame.selectedFixture == fi.id);
        const QColor shell = selected ? selectColor : defaultShell;
        const QColor metal = selected ? selectColor.darker(115) : defaultMetal;
        const bool live = fi.live;
        const double inten = live ? fi.liveIntensity : 1.0;
        // A lens follows exactly what is visibly emitting: live output in
        // operate mode, or the static beam preview in design mode. Otherwise
        // it stays dark.
        const bool lensLit = selected ||
                (live ? (fi.model == FixtureModel::Smoke
                         ? fi.lightIntensity > 0.02 : inten > 0.02)
                      : frame.beamPreview);
        const QColor gel = selected ? selectColor
                                    : (live ? (fi.model == FixtureModel::Smoke && fi.hasLight
                                               ? fi.lightColor : fi.liveColor)
                                            : fi.color);
        const QColor lensGel = (live && fi.model == FixtureModel::Laser &&
                                fi.liveHeads.isEmpty() == false)
                ? fi.liveHeads.first() : gel;
        const QColor lens = selected ? selectColor
                                     : (lensLit ? lensGel : QColor(10, 10, 12));
        const QQuaternion rot = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), fi.yaw)
                              * QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), fi.pitch);

        switch (fi.model)
        {
        case FixtureModel::Dimmer:
            addBox(fixtureVertices, fi.pos, rot, fi.size, shell);
            addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0.0, fi.size.y() * 0.5 + 0.004, 0.0)),
                   rot, QVector3D(fi.size.x() * 0.8, 0.008, fi.size.z() * 0.65), shell.darker(145));
            break;
        case FixtureModel::Lamp:
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.3);
            addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0.0, -len * 0.1, -(radius + 0.022))),
                   rot, QVector3D(radius * 2.4, len * 0.6, 0.03), metal);
            for (double side : { -1.0, 1.0 })
                addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(side * (radius + 0.018), -len * 0.1, 0.0)),
                       rot, QVector3D(0.03, len * 0.6, radius * 1.05), metal);
            addCylinder(fixtureVertices, fi.pos, rot, radius, len, shell);
            addDisk(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0.0, len * 0.5 + 0.022, 0.0)),
                    rot, radius * 0.82, lens);
            break;
        }
        case FixtureModel::Bar:
        {
            const double sx = fi.size.x();
            const double sz = fi.size.z();
            const double len = qMax(fi.size.y(), sz);
            const int cols = qMax(1, fi.ledCols);
            const int rows = qMax(1, fi.ledRows);
            const double dx = sx / cols;
            const double dz = sz / rows;
            addBox(fixtureVertices, fi.pos, rot, QVector3D(sx, len, sz), shell);
            for (int c = 0; c < cols; ++c)
            {
                const double lx = -sx * 0.5 + dx * (c + 0.5);
                for (int r = 0; r < rows; ++r)
                {
                    const double lz = -sz * 0.5 + dz * (r + 0.5);
                    QColor pc = lens;
                    if (live && fi.liveHeads.isEmpty() == false)
                    {
                        const int idx = r * cols + c;
                        const QColor headColor = fi.liveHeads.at(idx % fi.liveHeads.size());
                        double headIntensity = inten;
                        if (idx < fi.liveHeadIntensity.size())
                            headIntensity = fi.liveHeadIntensity.at(idx);
                        pc = selected ? selectColor
                                      : (headIntensity > 0.02 ? headColor : QColor(10, 10, 12));
                    }
                    addDisk(fixtureVertices,
                            fi.pos + rot.rotatedVector(QVector3D(lx, len * 0.5 + 0.005, lz)),
                            rot, qMax(0.006, qMin(dx, dz) * 0.36), pc);
                }
            }
            break;
        }
        case FixtureModel::Derby:
        {
            const double sz = fi.size.z();
            const double baseH = qMax(0.03, sz * 0.35);
            const double domeH = qMax(0.05, sz * 0.55);
            const QQuaternion up = rot * QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 90.0);
            const QVector3D baseC = fi.pos + rot.rotatedVector(QVector3D(0.0, 0.0, -sz * 0.35));
            addCylinder(fixtureVertices, baseC, up, qMax(0.05, 0.4 * qMax(fi.size.x(), fi.size.y())),
                        baseH, shell);
            addDisk(fixtureVertices, baseC + rot.rotatedVector(QVector3D(0.0, 0.0, baseH * 0.5 + domeH * 0.5)),
                    up, qMax(0.05, 0.38 * qMax(fi.size.x(), fi.size.y())), shell.lighter(112));
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
                const QVector3D d = fi.liveDir.normalized();
                mhPitch = qRadiansToDegrees(qAsin(qBound(-1.0, double(d.z()), 1.0)));
                mhYaw = qRadiansToDegrees(qAtan2(-double(d.x()), double(d.y())));
            }
            else
            {
                mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
                mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            }
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), mhPitch);
            addBox(fixtureVertices, fi.pos + QVector3D(0, 0, sz * 0.42), QQuaternion(),
                   QVector3D(sx * 0.85, sy * 0.7, sz * 0.16), shell);
            for (double side : { -1.0, 1.0 })
                addBox(fixtureVertices, fi.pos + pan.rotatedVector(QVector3D(side * (drumLen * 0.5 + 0.02), 0, 0)),
                       pan, QVector3D(0.03, sy * 0.7, sz * 0.5), metal);
            addCylinder(fixtureVertices, fi.pos, head * QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -90.0),
                        bodyR, drumLen, shell);
            addDisk(fixtureVertices, fi.pos + head.rotatedVector(QVector3D(0.0, bodyR + 0.012, 0.0)),
                    head, bodyR * 0.62, lens);
            break;
        }
        case FixtureModel::Scanner:
        {
            const double sx = fi.size.x();
            const double sy = fi.size.y();
            const double sz = fi.size.z();
            double mhYaw = live ? fi.yaw + fi.livePan : fi.yaw;
            double mhPitch = live ? fi.pitch + fi.liveTilt : fi.pitch;
            const QQuaternion pan = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), mhYaw);
            const QQuaternion head = pan * QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), mhPitch);
            addBox(fixtureVertices, fi.pos + QVector3D(0, 0, -sz * 0.22), rot,
                   QVector3D(sx, sy, sz * 0.55), shell);
            addBox(fixtureVertices, fi.pos + QVector3D(0, 0, sz * 0.24), head,
                   QVector3D(sx * 0.62, sy * 0.58, sz * 0.42), shell);
            addDisk(fixtureVertices, fi.pos + QVector3D(0, 0, sz * 0.24)
                    + head.rotatedVector(QVector3D(0, sy * 0.3 + 0.008, 0)),
                    head, qMin(sx, sy) * 0.18, lens);
            break;
        }
        case FixtureModel::Strobe:
        {
            addBox(fixtureVertices, fi.pos, rot, fi.size, shell);
            addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, fi.size.y() * 0.5 + 0.004, 0)),
                   rot, QVector3D(fi.size.x() * 0.88, 0.008, fi.size.z() * 0.62), shell.darker(155));
            addCylinder(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, fi.size.y() * 0.5 + 0.018, 0)),
                        rot * QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), -90.0),
                        qMax(0.006, fi.size.z() * 0.055), fi.size.x() * 0.76, lens, 8);
            break;
        }
        case FixtureModel::Laser:
            addBox(fixtureVertices, fi.pos, rot, fi.size, shell);
            addDisk(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, fi.size.y() * 0.5 + 0.006, 0)),
                    rot, qMin(fi.size.x(), fi.size.z()) * 0.18, lens);
            break;
        case FixtureModel::Smoke:
            addBox(fixtureVertices, fi.pos, rot, fi.size, shell);
            addCylinder(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, fi.size.y() * 0.5 + fi.size.y() * 0.16,
                                                                      fi.size.z() * 0.26)),
                        rot, qMax(0.015, qMin(fi.size.x(), fi.size.z()) * 0.12),
                        fi.size.y() * 0.32, metal);
            if (fi.hasLight)
            {
                addDisk(fixtureVertices, fi.pos + rot.rotatedVector(
                            QVector3D(0, fi.size.y() * 0.5 + 0.008, fi.size.z() * 0.18)),
                        rot, qMax(0.018, qMin(fi.size.x(), fi.size.z()) * 0.13), lens);
            }
            break;
        default: // Par
        {
            const double radius = qMax(0.05, 0.5 * qMax(fi.size.x(), fi.size.z()));
            const double len = qMax(fi.size.y(), radius * 1.1);
            addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, -len * 0.12, -(radius + 0.022))),
                   rot, QVector3D(radius * 2.4, len * 0.62, 0.03), metal);
            for (double side : { -1.0, 1.0 })
                addBox(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(side * (radius + 0.018), -len * 0.12, 0)),
                       rot, QVector3D(0.03, len * 0.62, radius * 1.05), metal);
            addCylinder(fixtureVertices, fi.pos, rot, radius, len, shell);
            addDisk(fixtureVertices, fi.pos + rot.rotatedVector(QVector3D(0, len * 0.5 + 0.02, 0)),
                    rot, radius * 0.82, lens);
            break;
        }
        }
    }

    quint64 fixtureSig = 1469598103934665603ULL;
    fixtureSig = hashValue(fixtureSig, frame.fixtureInteractive ? 1 : 0);
    fixtureSig = hashValue(fixtureSig, frame.beamPreview ? 1 : 0);
    fixtureSig = hashValue(fixtureSig, quint64(frame.selectedFixture));
    for (const FixtureInstance &fi : frame.fixtures)
    {
        fixtureSig = hashValue(fixtureSig, quint64(fi.id));
        fixtureSig = hashValue(fixtureSig, fi.visible ? 1 : 0);
        fixtureSig = hashValue(fixtureSig, quint64(fi.model));
        fixtureSig = hashDouble(fixtureSig, fi.pos.x());
        fixtureSig = hashDouble(fixtureSig, fi.pos.y());
        fixtureSig = hashDouble(fixtureSig, fi.pos.z());
        fixtureSig = hashDouble(fixtureSig, fi.size.x());
        fixtureSig = hashDouble(fixtureSig, fi.size.y());
        fixtureSig = hashDouble(fixtureSig, fi.size.z());
        fixtureSig = hashDouble(fixtureSig, fi.yaw);
        fixtureSig = hashDouble(fixtureSig, fi.pitch);
        fixtureSig = hashColor(fixtureSig, fi.color);
        fixtureSig = hashValue(fixtureSig, fi.live ? 1 : 0);
        fixtureSig = hashDouble(fixtureSig, fi.liveIntensity);
        fixtureSig = hashColor(fixtureSig, fi.liveColor);
        fixtureSig = hashValue(fixtureSig, fi.hasLight ? 1 : 0);
        fixtureSig = hashColor(fixtureSig, fi.lightColor);
        fixtureSig = hashDouble(fixtureSig, fi.lightIntensity);
        fixtureSig = hashDouble(fixtureSig, fi.livePan);
        fixtureSig = hashDouble(fixtureSig, fi.liveTilt);
        fixtureSig = hashValue(fixtureSig, fi.liveDirValid ? 1 : 0);
        fixtureSig = hashDouble(fixtureSig, fi.liveDir.x());
        fixtureSig = hashDouble(fixtureSig, fi.liveDir.y());
        fixtureSig = hashDouble(fixtureSig, fi.liveDir.z());
        for (const QColor &color : fi.liveHeads)
            fixtureSig = hashColor(fixtureSig, color);
        for (double value : fi.liveHeadIntensity)
            fixtureSig = hashDouble(fixtureSig, value);
    }
    m_fixtureSignature = fixtureSig;


    QVector3D cameraUp = frame.up;
    if (cameraUp.lengthSquared() < 1e-6)
        cameraUp = QVector3D(0, 0, 1);
    QMatrix4x4 cullView;
    cullView.lookAt(frame.eye, frame.target, cameraUp);
    QMatrix4x4 cullProjection;
    const float cullAspect = float(qMax(1, frame.viewport.width())) /
                             float(qMax(1, frame.viewport.height()));
    cullProjection.perspective(float(qRadiansToDegrees(frame.fovY)), cullAspect,
                               float(frame.nearPlane), 1000.0f);
    const Frustum frustum = makeFrustum(cullProjection * cullView);

    QVector<GLVertex> beamVertices;
    const double hazeScatter = qBound(0.0, frame.smokeHaze * 0.9 + frame.smokeFog * 0.35, 1.0);
    for (const Preview3DBeamSnapshot &beam : frame.beams)
    {
        const QVector3D dir = beam.rotation.rotatedVector(QVector3D(0, 1, 0)).normalized();
        const double spread = beam.rectangular
                ? beam.spread + qMax(beam.halfWidth, beam.halfHeight)
                : qTan(qDegreesToRadians(beam.beamDeg * 0.5)) * beam.length;
        const QVector3D center = beam.origin + dir * (beam.length * 0.5);
        if (frustum.intersectsSphere(center, beam.length * 0.5 + spread) == false)
            continue;
        if (beam.rectangular)
            addRectBeam(beamVertices, beam);
        else
            addCone(beamVertices, beam);

        // In haze/fog, add a wide, very soft scattering envelope around the
        // core beam. This is what makes a shaft visible in the air rather than
        // merely brightening the background.
        if (hazeScatter > 0.02)
        {
            Preview3DBeamSnapshot halo = beam;
            halo.alpha = qBound(2, int(beam.alpha * (0.10 + 0.14 * hazeScatter)), 22);
            if (beam.rectangular)
            {
                halo.halfWidth = beam.halfWidth + 0.10 + 0.30 * hazeScatter;
                halo.halfHeight = beam.halfHeight + 0.10 + 0.30 * hazeScatter;
                halo.spread = beam.spread + 0.15 * hazeScatter;
                addRectBeam(beamVertices, halo);
            }
            else
            {
                halo.beamDeg = qMin(170.0, beam.beamDeg * (1.35 + 0.45 * hazeScatter) + 4.0);
                addCone(beamVertices, halo);
            }
        }
    }

    QVector<SmokeVertex> smokeVertices;
    if (frame.smoke.isEmpty() == false)
    {
        QList<Preview3DSmokeSnapshot> puffs = frame.smoke;
        const QVector3D eye = frame.eye;
        std::sort(puffs.begin(), puffs.end(),
                  [&eye](const Preview3DSmokeSnapshot &a, const Preview3DSmokeSnapshot &b) {
            return (a.pos - eye).lengthSquared() > (b.pos - eye).lengthSquared();
        });

        QVector3D forward = frame.forward;
        if (forward.lengthSquared() < 1e-6)
        {
            forward = frame.target - frame.eye;
            if (forward.lengthSquared() < 1e-6)
                forward = QVector3D(0, 1, 0);
        }
        forward.normalize();
        QVector3D up = frame.up;
        if (up.lengthSquared() < 1e-6)
            up = QVector3D(0, 0, 1);
        up.normalize();
        QVector3D right = frame.right;
        if (right.lengthSquared() < 1e-6)
            right = QVector3D::crossProduct(forward, up);
        if (right.lengthSquared() < 1e-6)
            right = QVector3D(1, 0, 0);
        right.normalize();
        up = QVector3D::crossProduct(right, forward).normalized();

        for (const Preview3DSmokeSnapshot &puff : puffs)
        {
            if (QVector3D::dotProduct(puff.pos - eye, forward) <= frame.nearPlane)
                continue;
            if (frustum.intersectsSphere(puff.pos, qMax(0.01, puff.size)) == false)
                continue;
            const QVector3D r = right * qMax(0.01, puff.size);
            const QVector3D u = up * qMax(0.01, puff.size);
            const QVector3D p0 = puff.pos - r - u;
            const QVector3D p1 = puff.pos + r - u;
            const QVector3D p2 = puff.pos + r + u;
            const QVector3D p3 = puff.pos - r + u;
            QColor color = puff.color;
            color.setAlphaF(qBound(0.0, puff.alpha, 1.0));
            const float cr = float(color.redF());
            const float cg = float(color.greenF());
            const float cb = float(color.blueF());
            const float ca = float(color.alphaF());
            const SmokeVertex a = { p0.x(), p0.y(), p0.z(), 0.0f, 0.0f, cr, cg, cb, ca };
            const SmokeVertex b = { p1.x(), p1.y(), p1.z(), 1.0f, 0.0f, cr, cg, cb, ca };
            const SmokeVertex c = { p2.x(), p2.y(), p2.z(), 1.0f, 1.0f, cr, cg, cb, ca };
            const SmokeVertex d = { p3.x(), p3.y(), p3.z(), 0.0f, 1.0f, cr, cg, cb, ca };
            smokeVertices << a << b << c << a << c << d;
        }
    }

    if (vertices.isEmpty())
    {
        if (error != NULL)
            *error = QStringLiteral("No static geometry to upload");
        return false;
    }

    if (m_vbo->isCreated() == false && m_vbo->create() == false)
    {
        if (error != NULL)
            *error = QStringLiteral("Cannot create the static geometry vertex buffer");
        return false;
    }

    // Static room/stage/truss geometry is uploaded only when it changes.
    if (m_staticSignature != staticSig || m_vertexCount == 0)
    {
        m_vbo->bind();
        m_vbo->allocate(vertices.constData(), vertices.size() * int(sizeof(GLVertex)));
        m_vbo->release();
        m_staticSignature = staticSig;
        ++m_staticUploadCount;
    }
    m_vertexCount = vertices.size();

    if (m_fixtureVbo->isCreated() == false && m_fixtureVbo->create() == false)
    {
        if (error != NULL)
            *error = QStringLiteral("Cannot create the fixture vertex buffer");
        return false;
    }
    // Fixture bodies contain live lenses and selection highlighting; upload
    // them every frame rather than risking a stale lens/colour state. The
    // much larger room/stage/truss batch above remains cached.
    if (fixtureVertices.isEmpty() == false)
    {
        m_fixtureVbo->bind();
        m_fixtureVbo->allocate(fixtureVertices.constData(),
                               fixtureVertices.size() * int(sizeof(GLVertex)));
        m_fixtureVbo->release();
    }
    m_fixtureSignature = fixtureSig;
    m_fixtureVertexCount = fixtureVertices.size();

    if (beamVertices.isEmpty() == false)
    {
        if (m_beamVbo->isCreated() == false && m_beamVbo->create() == false)
        {
            if (error != NULL)
                *error = QStringLiteral("Cannot create the beam geometry vertex buffer");
            return false;
        }
        m_beamVbo->bind();
        m_beamVbo->allocate(beamVertices.constData(), beamVertices.size() * int(sizeof(GLVertex)));
        m_beamVbo->release();
    }
    m_beamVertexCount = beamVertices.size();

    if (smokeVertices.isEmpty() == false)
    {
        if (m_smokeVbo->isCreated() == false && m_smokeVbo->create() == false)
        {
            if (error != NULL)
                *error = QStringLiteral("Cannot create the smoke geometry vertex buffer");
            return false;
        }
        m_smokeVbo->bind();
        m_smokeVbo->allocate(smokeVertices.constData(), smokeVertices.size() * int(sizeof(SmokeVertex)));
        m_smokeVbo->release();
    }
    m_smokeVertexCount = smokeVertices.size();
    return true;
#else
    Q_UNUSED(frame)
    Q_UNUSED(error)
    return false;
#endif
}

void Preview3DOpenGLRenderer::renderShadowMap(const Preview3DSceneFrame &frame)
{
#if PREVIEW3D_GL_ENABLED
    if (frame.shadows == false || m_shadowFbo == 0 || m_shadowProgram == NULL ||
        (m_vertexCount <= 0 && m_fixtureVertexCount <= 0))
        return;

    quint64 shadowSig = m_staticSignature;
    shadowSig = hashValue(shadowSig, m_fixtureSignature);
    shadowSig = hashDouble(shadowSig, frame.lightDirection.x());
    shadowSig = hashDouble(shadowSig, frame.lightDirection.y());
    shadowSig = hashDouble(shadowSig, frame.lightDirection.z());
    if (m_shadowValid && shadowSig == m_shadowSignature)
        return;

    const QVector3D center((frame.roomX0 + frame.roomX1) * 0.5,
                           (frame.roomY0 + frame.roomY1) * 0.5,
                           (frame.zFloor + frame.zTop) * 0.5);
    const double dx = frame.roomX1 - frame.roomX0;
    const double dy = frame.roomY1 - frame.roomY0;
    const double dz = frame.zTop - frame.zFloor;
    const double radius = qMax(1.0, 0.5 * qSqrt(dx * dx + dy * dy + dz * dz));
    const double extent = radius * 1.15;

    QVector3D light = frame.lightDirection.normalized();
    if (light.lengthSquared() < 1e-6)
        light = QVector3D(-0.35, -0.45, 0.82).normalized();
    QVector3D up(0.0, 0.0, 1.0);
    if (qAbs(QVector3D::dotProduct(light, up)) > 0.95)
        up = QVector3D(0.0, 1.0, 0.0);

    QMatrix4x4 view;
    view.lookAt(center + light * (radius * 2.5 + 2.0), center, up);
    QMatrix4x4 projection;
    projection.ortho(-extent, extent, -extent, extent, 0.1f, float(radius * 5.0 + 4.0));
    QMatrix4x4 bias;
    bias.translate(0.5f, 0.5f, 0.5f);
    bias.scale(0.5f, 0.5f, 0.5f);
    m_lightMvp = bias * projection * view;

    m_gl->glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    m_gl->glViewport(0, 0, m_shadowSize, m_shadowSize);
    m_gl->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    m_gl->glEnable(GL_DEPTH_TEST);
    m_gl->glDepthFunc(GL_LESS);
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
    m_gl->glClear(GL_DEPTH_BUFFER_BIT);

    m_shadowProgram->bind();
    m_shadowProgram->setUniformValue("uMvp", m_lightMvp);
    m_vao->bind();
    m_vbo->bind();
    m_shadowProgram->enableAttributeArray(0);
    m_shadowProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(GLVertex));
    if (m_vertexCount > 0)
        m_gl->glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    m_vbo->release();
    if (m_fixtureVertexCount > 0)
    {
        m_fixtureVbo->bind();
        m_shadowProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(GLVertex));
        m_gl->glDrawArrays(GL_TRIANGLES, 0, m_fixtureVertexCount);
        m_fixtureVbo->release();
    }
    m_shadowProgram->disableAttributeArray(0);
    m_vao->release();
    m_shadowProgram->release();
    m_shadowSignature = shadowSig;
    m_shadowValid = true;
    ++m_shadowRenderCount;

    m_gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    m_gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

void Preview3DOpenGLRenderer::renderSmoke(const Preview3DSceneFrame &frame, const QMatrix4x4 &mvp)
{
#if PREVIEW3D_GL_ENABLED
    if (m_smokeVertexCount <= 0 || m_smokeProgram == NULL || m_smokeVao == NULL || m_smokeVbo == NULL)
        return;

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_gl->glDepthMask(GL_FALSE);
    m_smokeProgram->bind();
    m_smokeProgram->setUniformValue("uMvp", mvp);
    m_smokeProgram->setUniformValue("uRoomMin",
                                    QVector3D(frame.roomX0, frame.roomY0, frame.zFloor));
    m_smokeProgram->setUniformValue("uRoomMax",
                                    QVector3D(frame.roomX1, frame.roomY1, frame.zTop));
    m_smokeVao->bind();
    m_smokeVbo->bind();
    m_smokeProgram->enableAttributeArray(0);
    m_smokeProgram->enableAttributeArray(1);
    m_smokeProgram->enableAttributeArray(2);
    m_smokeProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(SmokeVertex));
    m_smokeProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 2, sizeof(SmokeVertex));
    m_smokeProgram->setAttributeBuffer(2, GL_FLOAT, 5 * sizeof(float), 4, sizeof(SmokeVertex));
    m_gl->glDrawArrays(GL_TRIANGLES, 0, m_smokeVertexCount);
    m_smokeProgram->disableAttributeArray(0);
    m_smokeProgram->disableAttributeArray(1);
    m_smokeProgram->disableAttributeArray(2);
    m_smokeVbo->release();
    m_smokeVao->release();
    m_smokeProgram->release();
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
#endif
}

bool Preview3DOpenGLRenderer::render(const Preview3DSceneFrame &frame, QImage &image, QString *error)
{
    if (m_initialized == false)
    {
        if (error != NULL)
            *error = QStringLiteral("The OpenGL renderer is not initialized");
        return false;
    }
    if (frame.viewport.width() <= 0 || frame.viewport.height() <= 0)
        return false;

#if PREVIEW3D_GL_ENABLED
    if (m_context->makeCurrent(m_surface) == false)
    {
        if (error != NULL)
            *error = QStringLiteral("Cannot activate the OpenGL context");
        return false;
    }

    if (m_pendingTimer != 0)
    {
        GLuint64 elapsedNs = 0;
        m_gl->glGetQueryObjectui64v(m_pendingTimer, GL_QUERY_RESULT, &elapsedNs);
        m_lastGpuTimeMs = double(elapsedNs) / 1000000.0;
        m_pendingTimer = 0;
    }

    const double requestedScale = qBound(0.5, frame.resolutionScale, 2.0);
    if (frame.adaptiveQuality == false)
        m_effectiveScale = requestedScale;
    else
    {
        if (m_effectiveScale < 0.5)
            m_effectiveScale = requestedScale;
        if (m_effectiveScale > requestedScale)
            m_effectiveScale = requestedScale;
        if (m_lastGpuTimeMs > 20.0 && m_effectiveScale > 0.5)
            m_effectiveScale = qMax(0.5, m_effectiveScale - 0.05);
        else if (m_lastGpuTimeMs > 0.0 && m_lastGpuTimeMs < 9.0 &&
                 m_effectiveScale < requestedScale)
            m_effectiveScale = qMin(requestedScale, m_effectiveScale + 0.03);
    }

    const double dpr = qMax(1.0, frame.devicePixelRatio);
    const int fboW = qMax(1, int(qRound(frame.viewport.width() * dpr * m_effectiveScale)));
    const int fboH = qMax(1, int(qRound(frame.viewport.height() * dpr * m_effectiveScale)));
    if (ensureFramebuffer(fboW, fboH, frame.msaaSamples, error) == false)
    {
        m_context->doneCurrent();
        return false;
    }

    if (prepareGeometry(frame, error) == false)
    {
        m_context->doneCurrent();
        return false;
    }

    const GLuint timerQuery = m_timerQueries[m_timerIndex];
    const bool timing = timerQuery != 0;
    if (timing)
        m_gl->glBeginQuery(GL_TIME_ELAPSED, timerQuery);

    renderShadowMap(frame);

    m_fbo->bind();
    m_gl->glViewport(0, 0, fboW, fboH);
    m_gl->glDisable(GL_BLEND);
    m_gl->glEnable(GL_DEPTH_TEST);
    m_gl->glDepthFunc(GL_LESS);
    const QColor clearColor = frame.haze ? frame.hazeColor : QColor(0, 0, 0);
    m_gl->glClearColor(float(clearColor.redF()), float(clearColor.greenF()),
                       float(clearColor.blueF()), 1.0f);
    m_gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 projection;
    const float aspect = float(fboW) / float(qMax(1, fboH));
    projection.perspective(float(qRadiansToDegrees(frame.fovY)), aspect,
                           float(frame.nearPlane), 1000.0f);

    QMatrix4x4 view;
    QVector3D up = frame.up;
    if (up.lengthSquared() < 1e-6)
        up = QVector3D(0, 0, 1);
    view.lookAt(frame.eye, frame.target, up);
    const QMatrix4x4 mvp = projection * view;

    m_program->bind();
    m_program->setUniformValue("uMvp", mvp);
    m_program->setUniformValue("uLightDir", frame.lightDirection.normalized());
    m_program->setUniformValue("uLightMvp", m_lightMvp);
    m_program->setUniformValue("uCameraPos", frame.eye);
    m_program->setUniformValue("uHazeColor",
                               QVector3D(float(frame.hazeColor.redF()),
                                         float(frame.hazeColor.greenF()),
                                         float(frame.hazeColor.blueF())));
    m_program->setUniformValue("uHazeDensity",
                               float(frame.haze ? frame.hazeDensity : 0.0));
    m_program->setUniformValue("uSmokeScatter",
                               float(frame.smokeHaze * 0.9 + frame.smokeFog * 0.45));
    const bool shadows = frame.shadows && (m_shadowTexture != 0);
    m_program->setUniformValue("uHasShadows", shadows ? 1 : 0);
    m_program->setUniformValue("uShadowMap", 1);
    if (shadows)
    {
        m_gl->glActiveTexture(GL_TEXTURE1);
        m_gl->glBindTexture(GL_TEXTURE_2D, m_shadowTexture);
        m_gl->glActiveTexture(GL_TEXTURE0);
    }

    m_vao->bind();
    m_vbo->bind();
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->enableAttributeArray(2);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(GLVertex));
    m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, sizeof(GLVertex));
    m_program->setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 4, sizeof(GLVertex));

    m_gl->glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    m_vbo->release();

    if (m_fixtureVertexCount > 0)
    {
        m_fixtureVbo->bind();
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(GLVertex));
        m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, sizeof(GLVertex));
        m_program->setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 4, sizeof(GLVertex));
        m_gl->glDrawArrays(GL_TRIANGLES, 0, m_fixtureVertexCount);
        m_fixtureVbo->release();
    }

    if (m_beamVertexCount > 0)
    {
        m_gl->glEnable(GL_BLEND);
        m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        m_gl->glDepthMask(GL_FALSE);
        m_beamVbo->bind();
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(GLVertex));
        m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, sizeof(GLVertex));
        m_program->setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 4, sizeof(GLVertex));
        m_gl->glDrawArrays(GL_TRIANGLES, 0, m_beamVertexCount);
        m_beamVbo->release();
        m_gl->glDepthMask(GL_TRUE);
        m_gl->glDisable(GL_BLEND);
    }

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);
    m_program->disableAttributeArray(2);
    m_vao->release();
    m_program->release();

    renderSmoke(frame, mvp);

    if (timing)
    {
        m_gl->glEndQuery(GL_TIME_ELAPSED);
        m_pendingTimer = timerQuery;
        m_timerIndex = 1 - m_timerIndex;
    }

    m_fbo->release();

    image = m_fbo->toImage();
    image.setDevicePixelRatio(double(fboW) / double(qMax(1, frame.viewport.width())));
    m_context->doneCurrent();
    return image.isNull() == false;
#else
    Q_UNUSED(frame)
    Q_UNUSED(image)
    Q_UNUSED(error)
    return false;
#endif
}
