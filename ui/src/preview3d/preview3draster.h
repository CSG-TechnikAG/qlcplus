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

    /** Downsample and return the finished image. */
    QImage finish();

    /** Enable/disable depth writes (disable for transparent geometry). */
    void setDepthWrite(bool on) { m_depthWrite = on; }

private:
    void blend(int x, int y, float iw, const QColor &color);

    int m_ss;
    int m_w, m_h;                 // supersampled dimensions
    QImage m_img;
    QVector<float> m_depth;
    bool m_depthWrite;
};

#endif // PREVIEW3DRASTER_H
