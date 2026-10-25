/*
  Q Light Controller Plus
  appsegmentedcontrol.cpp

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

#include <QVariantAnimation>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QWidget>

#include "appsegmentedcontrol.h"
#include "appicons.h"
#include "theme.h"

static int segmentMotionDuration()
{
    Theme *theme = Theme::instance();
    return (theme->animationsEnabled() && theme->scheme() != Theme::Classic) ? 180 : 0;
}

AppSegmentedControl::AppSegmentedControl(QWidget *parent)
    : QWidget(parent)
    , m_current(0)
    , m_hover(-1)
    , m_pressed(-1)
    , m_indicatorX(0.0)
    , m_animation(new QVariantAnimation(this))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(34);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant &value)
    {
        m_indicatorX = value.toReal();
        update();
    });

    connect(Theme::instance(), &Theme::changed, this, [this]() { update(); });
}

void AppSegmentedControl::addSegment(const QString &text, const QString &iconName,
                                     const QColor &activeColor)
{
    Segment segment;
    segment.text = text;
    segment.iconName = iconName;
    segment.color = activeColor;
    m_segments.append(segment);

    if (m_segments.size() == 1)
        m_current = 0;

    updateGeometry();
    update();
}

void AppSegmentedControl::setSegmentColor(int index, const QColor &color)
{
    if (index < 0 || index >= m_segments.size())
        return;

    m_segments[index].color = color;
    update();
}

int AppSegmentedControl::count() const
{
    return m_segments.size();
}

int AppSegmentedControl::currentIndex() const
{
    return m_current;
}

int AppSegmentedControl::segmentWidth() const
{
    if (m_segments.isEmpty())
        return width();

    return width() / m_segments.size();
}

void AppSegmentedControl::setCurrentIndex(int index, bool animate)
{
    if (m_segments.isEmpty())
        return;

    index = qBound(0, index, m_segments.size() - 1);
    m_current = index;

    const qreal target = index * segmentWidth() + 3.0;
    if (animate == false || segmentMotionDuration() == 0 || m_indicatorX < 0.0)
    {
        m_animation->stop();
        m_indicatorX = target;
        update();
        return;
    }

    m_animation->stop();
    m_animation->setDuration(segmentMotionDuration());
    m_animation->setStartValue(m_indicatorX);
    m_animation->setEndValue(target);
    m_animation->start();
}

void AppSegmentedControl::activate(int index)
{
    if (index < 0 || index >= m_segments.size() || index == m_current)
        return;

    setCurrentIndex(index, true);
    emit currentChanged(index);
}

QSize AppSegmentedControl::sizeHint() const
{
    QFontMetrics fm(font());
    int width = 0;
    for (int i = 0; i < m_segments.size(); i++)
    {
        int segment = fm.horizontalAdvance(m_segments.at(i).text) + 28;
        if (m_segments.at(i).iconName.isEmpty() == false)
            segment += 24;
        width += qMax(72, segment);
    }

    return QSize(qMax(120, width), 34);
}

void AppSegmentedControl::paintEvent(QPaintEvent *)
{
    if (m_segments.isEmpty())
        return;

    Theme *theme = Theme::instance();
    const bool enabled = isEnabled();
    const QColor surface = theme->color(Theme::SurfaceRaised);
    const QColor border = theme->color(Theme::Border);
    const QColor text = theme->color(Theme::Text);
    const QColor textDim = theme->color(Theme::TextDim);
    const QColor textDisabled = theme->color(Theme::TextDisabled);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF background = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(border, 1));
    painter.setBrush(surface);
    painter.drawRoundedRect(background, background.height() / 2.0,
                            background.height() / 2.0);

    /* The animated active pill */
    const qreal segWidth = segmentWidth();
    const QRectF pill(m_indicatorX, 3.0, segWidth - 6.0, height() - 6.0);
    QColor pillColor = m_segments.at(m_current).color;
    if (pillColor.isValid() == false)
        pillColor = theme->accent();
    if (enabled == false)
        pillColor = theme->color(Theme::SurfaceHover);

    painter.setPen(Qt::NoPen);
    painter.setBrush(pillColor);
    painter.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);

    QFont segmentFont = font();
    segmentFont.setPixelSize(12);
    segmentFont.setWeight(QFont::Medium);
    painter.setFont(segmentFont);
    const QFontMetrics fm(segmentFont);

    for (int i = 0; i < m_segments.size(); i++)
    {
        const bool active = (i == m_current);
        const bool hovered = (enabled == true && i == m_hover && active == false);

        if (hovered == true)
        {
            const QRectF hoverRect(i * segWidth + 3.0, 3.0,
                                   segWidth - 6.0, height() - 6.0);
            QColor hoverColor = theme->color(Theme::SurfaceHover);
            hoverColor.setAlphaF(0.55);
            painter.setPen(Qt::NoPen);
            painter.setBrush(hoverColor);
            painter.drawRoundedRect(hoverRect, hoverRect.height() / 2.0,
                                    hoverRect.height() / 2.0);
        }

        QColor labelColor;
        if (enabled == false)
            labelColor = textDisabled;
        else if (active == true)
            labelColor = Theme::contrastingText(pillColor);
        else
            labelColor = hovered ? text : textDim;

        const QString &label = m_segments.at(i).text;
        const QPixmap icon = m_segments.at(i).iconName.isEmpty()
                          ? QPixmap()
                          : AppIcons::pixmap(m_segments.at(i).iconName, 16,
                                             labelColor, devicePixelRatioF());
        const int textWidth = fm.horizontalAdvance(label);
        const int iconWidth = icon.isNull() ? 0 : 16;
        const int gap = icon.isNull() ? 0 : 6;
        const int totalWidth = textWidth + iconWidth + gap;
        const int startX = qRound(i * segWidth + (segWidth - totalWidth) / 2.0);
        const int centerY = height() / 2;

        if (icon.isNull() == false)
            painter.drawPixmap(startX, centerY - 8, icon);

        painter.setPen(labelColor);
        const QRect textRect(startX + iconWidth + gap, 0,
                             textWidth + 2, height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    if (hasFocus() == true)
    {
        painter.setPen(QPen(theme->accent(), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(background.adjusted(1, 1, -1, -1),
                                (background.height() - 2) / 2.0,
                                (background.height() - 2) / 2.0);
    }
}

int AppSegmentedControl::segmentAt(const QPoint &pos) const
{
    if (m_segments.isEmpty())
        return -1;

    const int width = segmentWidth();
    if (width <= 0)
        return -1;

    const int index = pos.x() / width;
    return (index >= 0 && index < m_segments.size()) ? index : -1;
}

void AppSegmentedControl::mouseMoveEvent(QMouseEvent *event)
{
    const int hover = segmentAt(event->pos());
    if (hover != m_hover)
    {
        m_hover = hover;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void AppSegmentedControl::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_pressed = segmentAt(event->pos());
        update();
    }
    QWidget::mousePressEvent(event);
}

void AppSegmentedControl::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const int index = segmentAt(event->pos());
        const int pressed = m_pressed;
        m_pressed = -1;
        update();
        if (index >= 0 && index == pressed)
            activate(index);
    }
    QWidget::mouseReleaseEvent(event);
}

void AppSegmentedControl::leaveEvent(QEvent *event)
{
    m_hover = -1;
    update();
    QWidget::leaveEvent(event);
}

void AppSegmentedControl::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left)
        activate(qMax(0, m_current - 1));
    else if (event->key() == Qt::Key_Right)
        activate(qMin(m_segments.size() - 1, m_current + 1));
    else
        QWidget::keyPressEvent(event);
}

void AppSegmentedControl::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_animation->stop();
    m_indicatorX = m_current * segmentWidth() + 3.0;
}

void AppSegmentedControl::focusInEvent(QFocusEvent *event)
{
    update();
    QWidget::focusInEvent(event);
}

void AppSegmentedControl::focusOutEvent(QFocusEvent *event)
{
    update();
    QWidget::focusOutEvent(event);
}
