/*
  Q Light Controller Plus
  stagegeometryview.h

  QPainter based top-down / side view used by the Stage Geometry tab. In the
  top-down view the room (the aimable area) is edited by dragging its corners;
  the stage rectangle itself is fixed from the tab.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STAGEGEOMETRYVIEW_H
#define STAGEGEOMETRYVIEW_H

#include <QWidget>
#include <QPointF>
#include <QList>
#include <QColor>
#include <QString>
#include <QPolygonF>

#include "stagegeometrymath.h"

/** @addtogroup ui_stagegeometry UI
 * @{
 */

class StageGeometryView : public QWidget
{
    Q_OBJECT

public:
    struct Marker
    {
        QPointF pos;
        QString name;
        QColor color;
        bool selected;
        bool calibrated;
    };

    enum Mode { TopDown, SideView };

    explicit StageGeometryView(QWidget *parent = NULL);

    void setMode(Mode mode);
    void setStage(double sx, double sy);
    void setRoom(double x0, double x1, double y0, double y1);
    void setGridPoints(const QList<QPointF> &points, const QList<QPointF> &done);
    void setHighlight(const QPointF &point, bool on);
    void setPanArrow(double angle, const QString &label, bool on);
    void setSideArrow(double angle, const QString &label, bool on);
    void setMarkers(const QList<Marker> &markers);
    void setTarget(const QPointF &point, bool on);
    void setMessage(const QString &message);
    void setResizable(bool resizable);

    /** Whitespace kept around the fitted content (pixels). Default 26. */
    void setFitPadding(int padding);
    /** Extra world margin around the content (fraction of the span). Default 0.10. */
    void setFitMargin(double factor);
    /** Stretch the content to fill the widget instead of keeping the aspect ratio. */
    void setStretchToFit(bool stretch);
    bool stretchToFit() const { return m_stretch; }

    /** Show a path polyline (world coordinates). */
    void setPath(const QPolygonF &path);

    /** Show several path polylines at once (e.g. active Path Functions). */
    void setPaths(const QList<QPolygonF> &paths);

    /** Show live preview spots (world coordinates), e.g. running Path Functions. */
    void setSpots(const QList<QPointF> &spots);
    QList<QPointF> spots() const { return m_spots; }

    /** Show an editable shape box (world coordinates) with move/resize. */
    void setEditableBox(const QRectF &box, bool visible);
    void setEditableBoxEnabled(bool enabled);
    /** Set the displayed rotation of the shape (degrees). */
    void setEditableBoxRotation(double degrees);

    /** When enabled, the mouse draws a path instead of emitting clicked(). */
    void setPathDrawingEnabled(bool enable);
    bool pathDrawingEnabled() const { return m_pathDrawing; }

signals:
    void clicked(QPointF world);
    void roomResized(double x0, double x1, double y0, double y1);
    void roomResizeFinished();
    void pathChanged(const QPolygonF &path);
    void editableBoxChanged(const QRectF &box);
    void editableBoxRotated(double degrees);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void computeView();
    QPointF w2p(const QPointF &world) const;
    QPointF p2w(const QPointF &pixel) const;
    int handleAt(const QPointF &pixel) const;   // 0 = min corner, 1 = max corner, -1 = none

    Mode m_mode;
    double m_sx;
    double m_sy;
    double m_roomX0, m_roomX1, m_roomY0, m_roomY1;
    QList<QPointF> m_points;
    QList<QPointF> m_done;
    bool m_hasHighlight;
    QPointF m_highlight;
    bool m_hasPanArrow;
    double m_panAngle;
    QString m_panLabel;
    bool m_hasSideArrow;
    double m_sideAngle;
    QString m_sideLabel;
    QList<Marker> m_markers;
    bool m_hasTarget;
    QPointF m_target;
    QString m_message;
    bool m_resizable;
    int m_dragging;                 // -1 none, 0/1 handle index
    int m_fitPadding;
    double m_fitMargin;
    bool m_stretch;
    QList<QPolygonF> m_paths;
    QList<QPointF> m_spots;
    bool m_pathDrawing;
    bool m_drawing;
    QRectF m_editBox;
    bool m_editBoxVisible;
    bool m_editBoxEnabled;
    double m_editBoxRotation;
    int m_boxDrag;              // -1 none, 0 move, 1 resize, 2 rotate
    QPointF m_boxGrab;

    // world <-> pixel mapping
    double m_scaleX;
    double m_scaleY;
    double m_ox;
    double m_oy;
    double m_x0;
    double m_y0;
    int m_height;
};

/** @} */

#endif // STAGEGEOMETRYVIEW_H
