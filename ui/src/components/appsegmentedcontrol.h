/*
  Q Light Controller Plus
  appsegmentedcontrol.h

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef APPSEGMENTEDCONTROL_H
#define APPSEGMENTEDCONTROL_H

#include <QWidget>
#include <QColor>
#include <QList>
#include <QSize>

class QVariantAnimation;

/** @addtogroup ui UI
 * @{
 */

/**
 * Animated segmented control. Used for the Design/Operate switch and for
 * compact settings such as the navigation position.
 */
class AppSegmentedControl final : public QWidget
{
    Q_OBJECT

public:
    explicit AppSegmentedControl(QWidget *parent = NULL);

    void addSegment(const QString &text, const QString &iconName = QString(),
                    const QColor &activeColor = QColor());
    void setSegmentColor(int index, const QColor &color);

    int count() const;
    int currentIndex() const;

    /** Update the selection without emitting currentChanged(). */
    void setCurrentIndex(int index, bool animate = true);

    QSize sizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    struct Segment
    {
        QString text;
        QString iconName;
        QColor color;
    };

    int segmentAt(const QPoint &pos) const;
    int segmentWidth() const;
    void activate(int index);

private:
    QList<Segment> m_segments;
    int m_current;
    int m_hover;
    int m_pressed;
    qreal m_indicatorX;
    QVariantAnimation *m_animation;
};

/** @} */

#endif // APPSEGMENTEDCONTROL_H
