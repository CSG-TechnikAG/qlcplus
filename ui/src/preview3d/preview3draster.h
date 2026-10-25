/*
  Q Light Controller Plus
  preview3draster.h

  A tiny software z-buffer rasterizer for the 3D Preview. It fills triangles
  and (dashed) lines with perspective-correct depth testing, supersamples for
  antialiasing and hands back a QImage.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PREVIEW3DRASTER_H
#define PREVIEW3DRASTER_H

#include <QImage>
#include <QVector>
#include <QPointF>
#include <QColor>

class SoftwareRaster
{
public:
    SoftwareRaster();

    /** Start a frame at widget size (supersampled internally). */
    void begin(int width, int height, int supersample = 2);

    /** Paint the background gradient. */
    void fillGradient(const QColor &top, const QColor &bottom);

    /**
     * Rasterize a triangle. Screen points are in widget coordinates; iw is the
     * reciprocal of the camera-space depth (larger = nearer).
     */
    void triangle(const QPointF &p0, const QPointF &p1, const QPointF &p2,
                  double iw0, double iw1, double iw2, const QColor &color);

    /** Rasterize a thick, depth-tested line. */
    void line(const QPointF &p0, const QPointF &p1, double iw0, double iw1,
              const QColor &color, double width);

    /** Rasterize a dashed, depth-tested line (dash/gap in pixels). */
    void dashedLine(const QPointF &p0, const QPointF &p1, double iw0, double iw1,
                    const QColor &color, double width, double dash, double gap);

    /** A soft radial sprite (smoke puff), depth-tested at its centre. */
    void sprite(const QPointF &center, double iw, double radius, const QColor &color);

    /** Downsample and return the finished image. */
    QImage finish();

    /** Enable/disable depth writes (disable for transparent geometry). */
    void setDepthWrite(bool on) { m_depthWrite = on; }

    /** Additive blending (for light beams: overlapping colours add up). */
    void setAdditive(bool on) { m_additive = on; }

    /** Counters exposed for the M0 performance baseline. */
    int triangleCount() const { return m_triangles; }
    int lineCount() const { return m_lines; }
    int spriteCount() const { return m_sprites; }

private:
    int m_ss;
    int m_w, m_h;                 // supersampled dimensions
    QImage m_img;
    QVector<float> m_depth;
    bool m_depthWrite;
    bool m_additive;
    int m_triangles = 0;
    int m_lines = 0;
    int m_sprites = 0;
};

#endif // PREVIEW3DRASTER_H
