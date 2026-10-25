/*
  Q Light Controller Plus
  appswitch.cpp

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
#include <QPainter>
#include <QStyle>
#include <QWidget>
#include <QEvent>

#include "appswitch.h"
#include "theme.h"

static QColor mixSwitchColors(const QColor &from, const QColor &to, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t);
}

AppSwitch::AppSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_animation(new QVariantAnimation(this))
    , m_progress(0.0)
    , m_hovered(false)
{
    setCheckable(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant &value)
    {
        m_progress = value.toReal();
        update();
    });

    connect(this, &QAbstractButton::toggled, this, [this](bool checked)
    {
        animateTo(checked);
    });

    connect(Theme::instance(), &Theme::changed, this, [this]() { update(); });
}

QSize AppSwitch::sizeHint() const
{
    return QSize(44, 24);
}

void AppSwitch::animateTo(bool checked)
{
    Theme *theme = Theme::instance();
    if (theme->animationsEnabled() == false || theme->scheme() == Theme::Classic)
    {
        m_animation->stop();
        m_progress = checked ? 1.0 : 0.0;
        update();
        return;
    }

    m_animation->stop();
    m_animation->setDuration(160);
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    m_animation->start();
}

void AppSwitch::paintEvent(QPaintEvent *)
{
    Theme *theme = Theme::instance();
    const QColor accent = theme->accent();
    const QColor surface = theme->color(Theme::SurfaceHover);
    const QColor border = theme->color(Theme::BorderStrong);
    const bool enabled = isEnabled();

    QColor track = enabled ? mixSwitchColors(surface, accent, m_progress)
                           : theme->color(Theme::SurfaceRaised);
    QColor stroke = enabled ? mixSwitchColors(border, accent, m_progress)
                            : theme->color(Theme::Border);
    if (enabled == true && m_hovered == true && m_progress < 0.5)
        stroke = mixSwitchColors(stroke, accent, 0.65);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF trackRect = QRectF(rect()).adjusted(1, 2, -1, -2);
    painter.setPen(QPen(stroke, 1));
    painter.setBrush(track);
    painter.drawRoundedRect(trackRect, trackRect.height() / 2.0,
                            trackRect.height() / 2.0);

    const qreal knobSize = height() - 8.0;
    const qreal knobX = 4.0 + m_progress * (width() - 8.0 - knobSize);
    const QRectF knob(knobX, 4.0, knobSize, knobSize);
    painter.setPen(QPen(theme->color(Theme::BorderStrong), 1));
    painter.setBrush(enabled ? QColor("#FFFFFF") : theme->color(Theme::TextDisabled));
    painter.drawEllipse(knob);

    if (hasFocus() == true)
    {
        painter.setPen(QPen(accent, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(trackRect.adjusted(1, 1, -1, -1),
                                (trackRect.height() - 2) / 2.0,
                                (trackRect.height() - 2) / 2.0);
    }
}

bool AppSwitch::hitButton(const QPoint &pos) const
{
    return rect().contains(pos);
}

bool AppSwitch::event(QEvent *event)
{
    if (event->type() == QEvent::Enter)
    {
        m_hovered = true;
        update();
    }
    else if (event->type() == QEvent::Leave)
    {
        m_hovered = false;
        update();
    }

    return QAbstractButton::event(event);
}
