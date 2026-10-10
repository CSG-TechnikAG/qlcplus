/*
  Q Light Controller Plus
  preview3draster.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPainter>
#include <QLinearGradient>
#include <QtMath>

#include "preview3draster.h"

SoftwareRaster::SoftwareRaster()
    : m_ss(1)
    , m_w(0)
    , m_h(0)
    , m_depthWrite(true)
{
}

void SoftwareRaster::begin(int width, int height, int supersample)
{
    m_ss = qMax(1, supersample);
    m_w = qMax(1, width) * m_ss;
    m_h = qMax(1, height) * m_ss;

    if (m_img.width() != m_w || m_img.height() != m_h)
    {
        m_img = QImage(m_w, m_h, QImage::Format_ARGB32_Premultiplied);
        m_depth.resize(m_w * m_h);
    }
    if (m_img.isNull())
    {
        // Allocation failed (huge viewport): render nothing rather than crash.
        m_w = 0;
        m_h = 0;
        return;
    }
    m_img.fill(Qt::transparent);
    m_depth.fill(0.0f);   // 0 = infinitely far (invW)
}

void SoftwareRaster::fillGradient(const QColor &top, const QColor &bottom)
{
    QPainter p(&m_img);
    QLinearGradient g(0, 0, 0, m_h);
    g.setColorAt(0.0, top);
    g.setColorAt(1.0, bottom);
    p.fillRect(m_img.rect(), g);
}

void SoftwareRaster::blend(int x, int y, float iw, const QColor &color)
{
    if (x < 0 || y < 0 || x >= m_w || y >= m_h)
        return;

    const int idx = y * m_w + x;
    if (iw < m_depth[idx])
        return;
    if (m_depthWrite)
        m_depth[idx] = iw;

    QRgb *row = reinterpret_cast<QRgb *>(m_img.scanLine(y));
    if (color.alpha() == 255)
    {
        row[x] = color.rgb();
    }
    else
    {
        const int a = color.alpha();
        const QRgb old = row[x];
        const int ia = 255 - a;
        row[x] = qRgb((qRed(old) * ia + color.red() * a) / 255,
                      (qGreen(old) * ia + color.green() * a) / 255,
                      (qBlue(old) * ia + color.blue() * a) / 255);
    }
}

void SoftwareRaster::triangle(const QPointF &p0, const QPointF &p1, const QPointF &p2,
                              double iw0, double iw1, double iw2, const QColor &color)
{
    const double x0 = p0.x() * m_ss, y0 = p0.y() * m_ss;
    const double x1 = p1.x() * m_ss, y1 = p1.y() * m_ss;
    const double x2 = p2.x() * m_ss, y2 = p2.y() * m_ss;

    const double area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (qAbs(area) < 1e-9)
        return;
    const double invArea = 1.0 / area;

    int minX = int(qFloor(qMin(x0, qMin(x1, x2))));
    int maxX = int(qCeil(qMax(x0, qMax(x1, x2))));
    int minY = int(qFloor(qMin(y0, qMin(y1, y2))));
    int maxY = int(qCeil(qMax(y0, qMax(y1, y2))));
    minX = qMax(0, minX);
    maxX = qMin(m_w - 1, maxX);
    minY = qMax(0, minY);
    maxY = qMin(m_h - 1, maxY);
    if (minX > maxX || minY > maxY)
        return;

    // Incremental barycentric coordinates (edge-function gradients).
    const double A0 = y1 - y2, B0 = x2 - x1, C0 = x1 * y2 - x2 * y1;
    const double A1 = y2 - y0, B1 = x0 - x2, C1 = x2 * y0 - x0 * y2;
    const double dw0dx = A0 * invArea;
    const double dw1dx = A1 * invArea;
    const double startX = minX + 0.5;

    for (int y = minY; y <= maxY; ++y)
    {
        const double py = y + 0.5;
        double w0 = (A0 * startX + B0 * py + C0) * invArea;
        double w1 = (A1 * startX + B1 * py + C1) * invArea;
        for (int x = minX; x <= maxX; ++x)
        {
            if (w0 >= 0.0 && w1 >= 0.0 && (w0 + w1) <= 1.0)
                blend(x, y, float(w0 * iw0 + w1 * iw1 + (1.0 - w0 - w1) * iw2), color);
            w0 += dw0dx;
            w1 += dw1dx;
        }
    }
}

void SoftwareRaster::line(const QPointF &p0, const QPointF &p1, double iw0, double iw1,
                          const QColor &color, double width)
{
    QPointF d = p1 - p0;
    const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
    const double h = qMax(0.5, width) * 0.5;

    if (len < 1e-6)
    {
        triangle(QPointF(p0.x() - h, p0.y() - h), QPointF(p0.x() + h, p0.y() - h),
                 QPointF(p0.x() + h, p0.y() + h), iw0, iw0, iw0, color);
        triangle(QPointF(p0.x() - h, p0.y() - h), QPointF(p0.x() + h, p0.y() + h),
                 QPointF(p0.x() - h, p0.y() + h), iw0, iw0, iw0, color);
        return;
    }

    const QPointF n(-d.y() / len, d.x() / len);
    const QPointF a = p0 + n * h;
    const QPointF b = p0 - n * h;
    const QPointF c = p1 - n * h;
    const QPointF e = p1 + n * h;

    triangle(a, b, c, iw0, iw0, iw1, color);
    triangle(a, c, e, iw0, iw1, iw1, color);
}

void SoftwareRaster::dashedLine(const QPointF &p0, const QPointF &p1, double iw0, double iw1,
                                const QColor &color, double width, double dash, double gap)
{
    QPointF d = p1 - p0;
    const double len = qSqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-6)
        return;

    const QPointF dir(d.x() / len, d.y() / len);
    double t = 0.0;
    while (t < len)
    {
        const double t0 = t;
        const double t1 = qMin(t + dash, len);
        const QPointF a = p0 + dir * t0;
        const QPointF b = p0 + dir * t1;
        const double f0 = t0 / len;
        const double f1 = t1 / len;
        line(a, b, iw0 + (iw1 - iw0) * f0, iw0 + (iw1 - iw0) * f1, color, width);
        t = t1 + gap;
    }
}

QImage SoftwareRaster::finish()
{
    if (m_ss == 1)
        return m_img;
    return m_img.scaled(m_w / m_ss, m_h / m_ss,
                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
