/*
  Q Light Controller Plus
  uieffects.cpp

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

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QMouseEvent>
#include <QWidget>
#include <QEvent>
#include <QPoint>
#include <QtMath>
#include <QDebug>

#include "uieffects.h"
#include "theme.h"

#define HOVER_FADE_IN_MS    140
#define HOVER_FADE_OUT_MS   110
#define RIPPLE_DURATION_MS  320
#define ANIMATION_FRAME_MS  16

static UiEffects *s_effects = NULL;

UiEffects *UiEffects::instance()
{
    if (s_effects == NULL)
        s_effects = new UiEffects();
    return s_effects;
}

UiEffects::UiEffects()
    : QObject(NULL)
{
    m_clock.start();

    m_timer.setInterval(ANIMATION_FRAME_MS);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(slotTick()));

    if (qApp != NULL)
        qApp->installEventFilter(this);

    connect(Theme::instance(), &Theme::changed,
            this, &UiEffects::slotThemeChanged);
}

bool UiEffects::effectsActive() const
{
    return Theme::instance()->animationsEnabled()
        && Theme::instance()->scheme() != Theme::Classic;
}

bool UiEffects::isEffectButton(QWidget *widget) const
{
    if (widget == NULL || widget->isEnabled() == false || widget->isVisible() == false)
        return false;

    if (widget->property("qlcNoEffects").toBool() == true)
        return false;

    if (widget->testAttribute(Qt::WA_TransparentForMouseEvents) == true)
        return false;

    /* Ripples on check boxes and radio buttons look out of place */
    if (qobject_cast<QCheckBox *>(widget) != NULL
        || qobject_cast<QRadioButton *>(widget) != NULL)
        return false;

    /* Flat command buttons are painted transparently (e.g. the cue stack
     * delete button); animating them would require a repaint underneath. */
    if (QPushButton *button = qobject_cast<QPushButton *>(widget))
    {
        if (button->isFlat() == true)
            return false;
    }

    return qobject_cast<QAbstractButton *>(widget) != NULL;
}

qreal UiEffects::hoverValue(const HoverInfo &info) const
{
    if (info.duration <= 0)
        return info.to;

    const qreal t = qBound(0.0, qreal(m_clock.elapsed() - info.start) / info.duration, 1.0);
    const qreal eased = 1.0 - qPow(1.0 - t, 3.0); // OutCubic
    return info.from + (info.to - info.from) * eased;
}

qreal UiEffects::rippleValue(const RippleInfo &info) const
{
    const qreal t = qBound(0.0, qreal(m_clock.elapsed() - info.start) / RIPPLE_DURATION_MS, 1.0);
    return 1.0 - qPow(1.0 - t, 3.0); // OutCubic
}

void UiEffects::startHover(QWidget *widget)
{
    HoverInfo info;
    info.start = m_clock.elapsed();
    info.duration = HOVER_FADE_IN_MS;
    info.from = m_hover.contains(widget) ? hoverValue(m_hover.value(widget)) : 0.0;
    info.to = 1.0;
    m_hover.insert(widget, info);

    widget->update();
    if (m_timer.isActive() == false)
        m_timer.start();
}

void UiEffects::stopHover(QWidget *widget)
{
    if (m_hover.contains(widget) == false)
        return;

    HoverInfo info;
    info.start = m_clock.elapsed();
    info.duration = HOVER_FADE_OUT_MS;
    info.from = hoverValue(m_hover.value(widget));
    info.to = 0.0;
    m_hover.insert(widget, info);

    widget->update();
    if (m_timer.isActive() == false)
        m_timer.start();
}

void UiEffects::startRipple(QWidget *widget, const QPoint &pos)
{
    RippleInfo info;
    info.start = m_clock.elapsed();
    info.origin = pos;
    m_ripple.insert(widget, info);

    widget->update();
    if (m_timer.isActive() == false)
        m_timer.start();
}

void UiEffects::removeWidget(QWidget *widget)
{
    m_hover.remove(widget);
    m_ripple.remove(widget);
}

qreal UiEffects::hoverProgress(const QWidget *widget) const
{
    if (widget == NULL || m_hover.contains(widget) == false)
        return 0.0;

    return hoverValue(m_hover.value(widget));
}

qreal UiEffects::rippleProgress(const QWidget *widget) const
{
    if (widget == NULL || m_ripple.contains(widget) == false)
        return 0.0;

    return rippleValue(m_ripple.value(widget));
}

QPointF UiEffects::rippleOrigin(const QWidget *widget) const
{
    if (widget == NULL || m_ripple.contains(widget) == false)
        return QPointF();

    return m_ripple.value(widget).origin;
}

void UiEffects::slotTick()
{
    if (m_clock.isValid() == false)
        m_clock.start();

    QList<const QWidget *> hoverDone;
    QList<const QWidget *> rippleDone;
    bool running = false;

    QHash<const QWidget *, HoverInfo>::const_iterator hit = m_hover.constBegin();
    while (hit != m_hover.constEnd())
    {
        const QWidget *widget = hit.key();
        const HoverInfo &info = hit.value();
        const qreal value = hoverValue(info);

        if (qAbs(value - info.to) > 0.001)
        {
            running = true;
            if (widget != NULL)
                const_cast<QWidget *>(widget)->update();
        }
        else
        {
            /* Paint the final state once (clears a finishing ripple or a
             * faded-out hover) before dropping the entry. */
            if (widget != NULL)
                const_cast<QWidget *>(widget)->update();

            if (info.to <= 0.0)
                hoverDone.append(widget);
        }

        ++hit;
    }

    QHash<const QWidget *, RippleInfo>::const_iterator rit = m_ripple.constBegin();
    while (rit != m_ripple.constEnd())
    {
        const QWidget *widget = rit.key();
        const RippleInfo &info = rit.value();

        if (rippleValue(info) >= 0.999)
        {
            if (widget != NULL)
                const_cast<QWidget *>(widget)->update();
            rippleDone.append(widget);
        }
        else
        {
            running = true;
            if (widget != NULL)
                const_cast<QWidget *>(widget)->update();
        }

        ++rit;
    }

    foreach (const QWidget *widget, hoverDone)
        m_hover.remove(widget);
    foreach (const QWidget *widget, rippleDone)
        m_ripple.remove(widget);

    if (running == false)
        m_timer.stop();
}

void UiEffects::clearEffects()
{
    QList<const QWidget *> widgets;

    QHash<const QWidget *, HoverInfo>::const_iterator hit = m_hover.constBegin();
    while (hit != m_hover.constEnd())
    {
        widgets.append(hit.key());
        ++hit;
    }

    QHash<const QWidget *, RippleInfo>::const_iterator rit = m_ripple.constBegin();
    while (rit != m_ripple.constEnd())
    {
        widgets.append(rit.key());
        ++rit;
    }

    m_hover.clear();
    m_ripple.clear();
    m_timer.stop();

    foreach (const QWidget *widget, widgets)
    {
        if (widget != NULL)
            const_cast<QWidget *>(widget)->update();
    }
}

void UiEffects::slotThemeChanged()
{
    clearEffects();
}

bool UiEffects::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();

    /* This filter is installed on the whole application, so bail out as
     * early and as cheaply as possible for the vast majority of events. */
    if (type != QEvent::Enter && type != QEvent::Leave
        && type != QEvent::MouseButtonPress && type != QEvent::Resize
        && type != QEvent::Hide && type != QEvent::Destroy)
        return false;

    if (effectsActive() == false)
    {
        if (m_hover.isEmpty() == false)
            clearEffects();
        return false;
    }

    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (widget == NULL)
        return false;

    switch (type)
    {
    case QEvent::Enter:
        if (isEffectButton(widget) == true)
            startHover(widget);
        break;

    case QEvent::Leave:
        if (m_hover.contains(widget) == true)
            stopHover(widget);
        break;

    case QEvent::MouseButtonPress:
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && isEffectButton(widget) == true)
            startRipple(widget, mouseEvent->pos());
        break;
    }

    case QEvent::Resize:
        if (m_hover.contains(widget) == true || m_ripple.contains(widget) == true)
            widget->update();
        break;

    case QEvent::Hide:
        if (m_hover.contains(widget) == true || m_ripple.contains(widget) == true)
            removeWidget(widget);
        break;

    case QEvent::Destroy:
        removeWidget(widget);
        break;

    default:
        break;
    }

    return false;
}
