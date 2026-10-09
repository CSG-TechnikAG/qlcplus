/*
  Q Light Controller Plus
  stagegeometryview.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "stagegeometryview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPalette>
#include <QtMath>

StageGeometryView::StageGeometryView(QWidget *parent)
    : QWidget(parent)
    , m_mode(TopDown)
    , m_sx(4.0)
    , m_sy(3.0)
    , m_roomX0(0.0)
    , m_roomX1(5.0)
    , m_roomY0(0.0)
    , m_roomY1(5.0)
    , m_hasHighlight(false)
    , m_highlight()
    , m_hasPanArrow(false)
    , m_panAngle(0)
    , m_hasSideArrow(false)
    , m_sideAngle(0)
    , m_hasTarget(false)
    , m_resizable(true)
    , m_dragging(-1)
    , m_fitPadding(26)
    , m_fitMargin(0.10)
    , m_stretch(false)
    , m_pathDrawing(false)
    , m_drawing(false)
    , m_editBoxVisible(false)
    , m_editBoxEnabled(false)
    , m_editBoxRotation(0.0)
    , m_boxDrag(-1)
    , m_scaleX(1)
    , m_scaleY(1)
    , m_ox(0)
    , m_oy(0)
    , m_x0(0)
    , m_y0(0)
    , m_height(1)
{
    setMinimumSize(360, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void StageGeometryView::setMode(Mode mode)
{
    m_mode = mode;
    setCursor(mode == TopDown ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void StageGeometryView::setStage(double sx, double sy)
{
    m_sx = sx;
    m_sy = sy;
    update();
}

void StageGeometryView::setRoom(double x0, double x1, double y0, double y1)
{
    m_roomX0 = x0;
    m_roomX1 = x1;
    m_roomY0 = y0;
    m_roomY1 = y1;
    update();
}

void StageGeometryView::setGridPoints(const QList<QPointF> &points, const QList<QPointF> &done)
{
    m_points = points;
    m_done = done;
    update();
}

void StageGeometryView::setHighlight(const QPointF &point, bool on)
{
    m_highlight = point;
    m_hasHighlight = on;
    update();
}

void StageGeometryView::setPanArrow(double angle, const QString &label, bool on)
{
    m_panAngle = angle;
    m_panLabel = label;
    m_hasPanArrow = on;
    update();
}

void StageGeometryView::setSideArrow(double angle, const QString &label, bool on)
{
    m_sideAngle = angle;
    m_sideLabel = label;
    m_hasSideArrow = on;
    update();
}

void StageGeometryView::setMarkers(const QList<Marker> &markers)
{
    m_markers = markers;
    update();
}

void StageGeometryView::setTarget(const QPointF &point, bool on)
{
    m_target = point;
    m_hasTarget = on;
    update();
}

void StageGeometryView::setMessage(const QString &message)
{
    m_message = message;
    update();
}

void StageGeometryView::setResizable(bool resizable)
{
    m_resizable = resizable;
    update();
}

void StageGeometryView::setFitPadding(int padding)
{
    m_fitPadding = qMax(0, padding);
    update();
}

void StageGeometryView::setFitMargin(double factor)
{
    m_fitMargin = qMax(0.0, factor);
    update();
}

void StageGeometryView::setStretchToFit(bool stretch)
{
    m_stretch = stretch;
    update();
}

void StageGeometryView::setPath(const QPolygonF &path)
{
    m_paths.clear();
    if (path.isEmpty() == false)
        m_paths.append(path);
    update();
}

void StageGeometryView::setPaths(const QList<QPolygonF> &paths)
{
    m_paths = paths;
    update();
}

void StageGeometryView::setSpots(const QList<QPointF> &spots)
{
    m_spots = spots;
    update();
}

void StageGeometryView::setEditableBox(const QRectF &box, bool visible)
{
    m_editBox = box;
    m_editBoxVisible = visible;
    update();
}

void StageGeometryView::setEditableBoxEnabled(bool enabled)
{
    m_editBoxEnabled = enabled;
    update();
}

void StageGeometryView::setEditableBoxRotation(double degrees)
{
    m_editBoxRotation = degrees;
    update();
}

void StageGeometryView::setPathDrawingEnabled(bool enable)
{
    m_pathDrawing = enable;
    setCursor(enable ? Qt::CrossCursor : (m_mode == TopDown ? Qt::CrossCursor : Qt::ArrowCursor));
}

void StageGeometryView::computeView()
{
    int w = width();
    int h = height();
    m_height = h;

    if (m_mode == SideView)
    {
        m_scaleX = m_scaleY = 1.0;
        m_ox = m_oy = m_x0 = m_y0 = 0.0;
        return;
    }

    double xmin = qMin(0.0, qMin(m_roomX0, m_sx));
    double xmax = qMax(m_sx, m_roomX1);
    double ymin = qMin(0.0, qMin(m_roomY0, m_sy));
    double ymax = qMax(m_sy, m_roomY1);

    for (const Marker &mk : m_markers)
    {
        xmin = qMin(xmin, mk.pos.x());
        xmax = qMax(xmax, mk.pos.x());
        ymin = qMin(ymin, mk.pos.y());
        ymax = qMax(ymax, mk.pos.y());
    }
    if (m_hasTarget)
    {
        xmin = qMin(xmin, m_target.x());
        xmax = qMax(xmax, m_target.x());
        ymin = qMin(ymin, m_target.y());
        ymax = qMax(ymax, m_target.y());
    }

    double m = qMax(0.0, m_fitMargin * qMax(xmax - xmin, ymax - ymin));
    xmin -= m;
    xmax += m;
    ymin -= m;
    ymax += m;

    double pad = m_fitPadding;
    double sx = (w - 2 * pad) / qMax(1e-6, xmax - xmin);
    double sy = (h - 2 * pad) / qMax(1e-6, ymax - ymin);
    if (m_stretch)
    {
        m_scaleX = sx;
        m_scaleY = sy;
    }
    else
    {
        m_scaleX = m_scaleY = qMin(sx, sy);
    }

    double cw = (xmax - xmin) * m_scaleX;
    double ch = (ymax - ymin) * m_scaleY;
    m_ox = (w - cw) / 2.0;
    m_oy = (h - ch) / 2.0;
    m_x0 = xmin;
    m_y0 = ymin;
}

QPointF StageGeometryView::w2p(const QPointF &world) const
{
    return QPointF(m_ox + (world.x() - m_x0) * m_scaleX,
                   m_height - m_oy - (world.y() - m_y0) * m_scaleY);
}

QPointF StageGeometryView::p2w(const QPointF &pixel) const
{
    return QPointF(m_x0 + (pixel.x() - m_ox) / m_scaleX,
                   m_y0 + (m_height - m_oy - pixel.y()) / m_scaleY);
}

int StageGeometryView::handleAt(const QPointF &pixel) const
{
    QPointF minH = w2p(QPointF(m_roomX0, m_roomY0));
    QPointF maxH = w2p(QPointF(m_roomX1, m_roomY1));
    if (QLineF(pixel, maxH).length() < 14.0)
        return 1;
    if (QLineF(pixel, minH).length() < 14.0)
        return 0;
    return -1;
}

void StageGeometryView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = palette();
    QColor bg = pal.color(QPalette::Base);
    QColor text = pal.color(QPalette::Text);
    QColor accent = pal.color(QPalette::Highlight);
    QColor grid = pal.color(QPalette::Mid);
    QColor roomCol(90, 140, 190);
    QColor targetCol("#e0533b");
    QColor doneCol("#3aa655");
    QColor arrowCol("#e0a020");

    p.fillRect(rect(), bg);

    computeView();

    int w = width();
    int h = height();

    if (m_mode == SideView)
    {
        double groundY = h - 44.0;
        p.setPen(QPen(accent, 2));
        p.drawLine(40, int(groundY), w - 40, int(groundY));
        p.setPen(text);
        p.drawText(w - 60, int(groundY) + 16, tr("Floor"));

        double fx = w * 0.5;
        double fy = h * 0.30;
        p.setPen(QPen(arrowCol, 2));
        p.setBrush(pal.color(QPalette::Button));
        p.drawRect(QRectF(fx - 12, fy - 10, 24, 20));
        p.setPen(text);
        p.drawText(QPointF(fx + 18, fy + 4), tr("Moving head"));

        p.setPen(QPen(grid, 1, Qt::DashLine));
        p.drawLine(QPointF(fx, fy + 10), QPointF(fx, groundY));

        if (m_hasSideArrow)
        {
            double length = qMin(w, h) * 0.30;
            double ex = fx + qSin(m_sideAngle) * length;
            double ey = fy + qCos(m_sideAngle) * length;
            p.setPen(QPen(targetCol, 4));
            p.drawLine(QPointF(fx, fy), QPointF(ex, ey));
            p.setPen(targetCol);
            p.drawText(QPointF(ex + 6, ey), m_sideLabel);
        }

        p.setPen(text);
        p.drawText(QRectF(0, h - 20, w, 18), Qt::AlignHCenter, m_message);
        return;
    }

    // ---- top down ----
    // room (aimable area) - dashed
    QRectF roomRect(QPointF(w2p(QPointF(m_roomX0, m_roomY1))),
                    QPointF(w2p(QPointF(m_roomX1, m_roomY0))));
    QColor roomFill = roomCol;
    roomFill.setAlpha(18);
    p.setPen(QPen(roomCol, 2, Qt::DashLine));
    p.setBrush(roomFill);
    p.drawRect(roomRect);
    p.setPen(roomCol);
    p.drawText(roomRect.topLeft() + QPointF(3, 14), tr("Room"));

    // stage - solid, highlighted
    QRectF stageRect(QPointF(w2p(QPointF(0, m_sy))), QPointF(w2p(QPointF(m_sx, 0))));
    QColor fill = accent;
    fill.setAlpha(36);
    p.setPen(QPen(accent, 2));
    p.setBrush(fill);
    p.drawRect(stageRect);

    p.setPen(QPen(grid, 1));
    for (int gx = 1; gx < int(qCeil(m_sx)); gx++)
        p.drawLine(w2p(QPointF(gx, 0)), w2p(QPointF(gx, m_sy)));
    for (int gy = 1; gy < int(qCeil(m_sy)); gy++)
        p.drawLine(w2p(QPointF(0, gy)), w2p(QPointF(m_sx, gy)));

    p.setPen(text);
    p.drawText(w2p(QPointF(0, m_sy)) + QPointF(-2, 15), tr("stage y"));
    p.drawText(w2p(QPointF(m_sx, 0)) + QPointF(-58, -6), tr("stage x"));

    for (const QPointF &pt : m_points)
    {
        QPointF px = w2p(pt);
        p.setPen(Qt::NoPen);
        p.setBrush(m_done.contains(pt) ? doneCol : grid);
        p.drawEllipse(px, 3.5, 3.5);
    }

    if (m_hasHighlight)
    {
        QPointF px = w2p(m_highlight);
        p.setPen(QPen(targetCol, 3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(px, 12, 12);
        p.setPen(targetCol);
        p.drawText(px + QPointF(-38, -18), tr("aim here"));
    }

    if (m_hasPanArrow)
    {
        QPointF center = w2p(QPointF(m_sx / 2.0, m_sy / 2.0));
        double length = 0.42 * qMin(m_sx, m_sy) + 0.6;
        QPointF end = w2p(QPointF(m_sx / 2.0 + qCos(m_panAngle) * length,
                                  m_sy / 2.0 + qSin(m_panAngle) * length));
        p.setPen(QPen(arrowCol, 4));
        p.drawLine(center, end);
        p.setPen(arrowCol);
        p.drawText(end + QPointF(-10, -10), m_panLabel);
    }

    for (const Marker &mk : m_markers)
    {
        QPointF px = w2p(mk.pos);
        // Draw the fixture position as a small "x".
        p.setPen(QPen(mk.color, mk.selected ? 2 : 1));
        p.drawLine(px + QPointF(-6, -6), px + QPointF(6, 6));
        p.drawLine(px + QPointF(-6, 6), px + QPointF(6, -6));
        p.setPen(mk.color);
        p.drawText(px + QPointF(9, -9), mk.name);
        // Dotted line from a controlled lamp to the target position.
        if (mk.selected && m_hasTarget)
        {
            p.setPen(QPen(mk.color, 1, Qt::DotLine));
            p.drawLine(px, w2p(m_target));
        }
    }

    if (m_hasTarget)
    {
        QPointF px = w2p(m_target);
        p.setPen(QPen(targetCol, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(px, 5, 5);
    }

    // Live preview spots (e.g. running Path Functions)
    for (const QPointF &spot : m_spots)
    {
        QPointF px = w2p(spot);
        p.setPen(QPen(targetCol, 1, Qt::DotLine));
        for (const Marker &mk : m_markers)
            if (mk.selected)
                p.drawLine(w2p(mk.pos), px);
        p.setPen(QPen(targetCol, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(px, 6, 6);
    }

    // Editable shape box (transformable path presets)
    if (m_editBoxVisible)
    {
        QRectF px = QRectF(w2p(m_editBox.topLeft()), w2p(m_editBox.bottomRight())).normalized();
        QColor boxCol(120, 120, 255);
        p.setPen(QPen(boxCol, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(px);
        if (m_editBoxEnabled)
        {
            QPointF handle = w2p(m_editBox.bottomRight());
            p.setPen(QPen(boxCol, 2));
            p.setBrush(boxCol);
            p.drawRect(QRectF(handle.x() - 5, handle.y() - 5, 10, 10));

            // Rotation handle above the box
            QPointF topCenter = w2p(QPointF(m_editBox.center().x(), m_editBox.bottom())) + QPointF(0, -22);
            QPointF anchor = w2p(QPointF(m_editBox.center().x(), m_editBox.bottom()));
            p.setPen(QPen(boxCol, 1, Qt::DashLine));
            p.drawLine(anchor, topCenter);
            p.setPen(QPen(boxCol, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(topCenter, 5, 5);
        }
    }

    // Path polylines
    for (const QPolygonF &path : m_paths)
    {
        if (path.size() < 2)
            continue;
        QPolygonF poly;
        for (const QPointF &wp : path)
            poly << w2p(wp);
        p.setPen(QPen(QColor(90, 200, 120), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(90, 200, 120));
        p.drawEllipse(poly.first(), 4, 4);
        p.drawEllipse(poly.last(), 4, 4);
    }

    if (m_resizable)
    {
        QPointF h1 = w2p(QPointF(m_roomX0, m_roomY0));
        QPointF h2 = w2p(QPointF(m_roomX1, m_roomY1));
        p.setPen(QPen(roomCol, 2));
        p.setBrush(roomCol);
        p.drawRect(QRectF(h1.x() - 5, h1.y() - 5, 10, 10));
        p.drawRect(QRectF(h2.x() - 5, h2.y() - 5, 10, 10));
    }

    p.setPen(text);
    p.drawText(QRectF(0, h - 20, w, 18), Qt::AlignHCenter, m_message);
}

void StageGeometryView::mousePressEvent(QMouseEvent *event)
{
    if (m_mode != TopDown)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF pos = event->position();
#else
    QPointF pos = event->localPos();
#endif

    if (m_editBoxVisible && m_editBoxEnabled && event->button() == Qt::LeftButton)
    {
        QPointF rotHandle = w2p(QPointF(m_editBox.center().x(), m_editBox.bottom())) + QPointF(0, -22);
        if (QLineF(pos, rotHandle).length() < 14.0)
        {
            m_boxDrag = 2;
            setCursor(Qt::CrossCursor);
            return;
        }
        QPointF handle = w2p(m_editBox.bottomRight());
        if (QLineF(pos, handle).length() < 14.0)
        {
            m_boxDrag = 1;
            setCursor(Qt::SizeFDiagCursor);
            return;
        }
        QRectF px = QRectF(w2p(m_editBox.topLeft()), w2p(m_editBox.bottomRight())).normalized();
        if (px.contains(pos))
        {
            m_boxDrag = 0;
            m_boxGrab = p2w(pos) - m_editBox.center();
            setCursor(Qt::SizeAllCursor);
            return;
        }
    }

    if (m_pathDrawing && event->button() == Qt::LeftButton)
    {
        m_drawing = true;
        QPolygonF path;
        path.append(p2w(pos));
        setPath(path);
        emit pathChanged(path);
        return;
    }

    if (m_resizable && event->button() == Qt::LeftButton)
    {
        int handle = handleAt(pos);
        if (handle >= 0)
        {
            m_dragging = handle;
            setCursor(Qt::SizeFDiagCursor);
            return;
        }
    }

    if (event->button() == Qt::LeftButton)
        emit clicked(p2w(pos));
}

void StageGeometryView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mode != TopDown)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF pos = event->position();
#else
    QPointF pos = event->localPos();
#endif

    if (m_boxDrag >= 0)
    {
        if (m_boxDrag == 2)
        {
            QPointF d = p2w(pos) - m_editBox.center();
            double deg = qRadiansToDegrees(qAtan2(d.y(), d.x())) - 90.0;
            emit editableBoxRotated(deg);
            return;
        }

        QPointF world = p2w(pos);
        if (m_boxDrag == 0)
            m_editBox.moveCenter(world - m_boxGrab);
        else
        {
            double w = qMax(0.5, world.x() - m_editBox.left());
            double h = qMax(0.5, world.y() - m_editBox.top());
            m_editBox.setWidth(w);
            m_editBox.setHeight(h);
        }
        update();
        emit editableBoxChanged(m_editBox);
        return;
    }

    if (m_drawing)
    {
        QPolygonF path = m_paths.isEmpty() ? QPolygonF() : m_paths.first();
        path.append(p2w(pos));
        setPath(path);
        emit pathChanged(path);
        return;
    }

    if (m_dragging < 0)
    {
        if (m_resizable)
            setCursor(handleAt(pos) >= 0 ? Qt::SizeFDiagCursor : Qt::CrossCursor);
        return;
    }

    QPointF world = p2w(pos);
    double x = qRound(world.x() * 10.0) / 10.0;
    double y = qRound(world.y() * 10.0) / 10.0;

    double x0 = m_roomX0, x1 = m_roomX1, y0 = m_roomY0, y1 = m_roomY1;
    if (m_dragging == 0)
    {
        x0 = qMin(x, x1 - 0.5);
        y0 = qMin(y, y1 - 0.5);
    }
    else
    {
        x1 = qMax(x, x0 + 0.5);
        y1 = qMax(y, y0 + 0.5);
    }

    if (qFuzzyCompare(x0, m_roomX0) && qFuzzyCompare(x1, m_roomX1) &&
        qFuzzyCompare(y0, m_roomY0) && qFuzzyCompare(y1, m_roomY1))
        return;

    m_roomX0 = x0;
    m_roomX1 = x1;
    m_roomY0 = y0;
    m_roomY1 = y1;
    update();
    emit roomResized(x0, x1, y0, y1);
}

void StageGeometryView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (m_boxDrag >= 0)
    {
        m_boxDrag = -1;
        setCursor(Qt::CrossCursor);
        return;
    }
    if (m_drawing)
    {
        m_drawing = false;
        return;
    }
    if (m_dragging >= 0)
    {
        m_dragging = -1;
        setCursor(Qt::CrossCursor);
        emit roomResizeFinished();
    }
}

void StageGeometryView::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    if (m_dragging < 0 && m_mode == TopDown)
        setCursor(Qt::CrossCursor);
}
