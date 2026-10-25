/*
  Q Light Controller Plus
  uistyleproxy.cpp

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

#include <QStyleOptionButton>
#include <QStyleOptionToolButton>
#include <QPainterPath>
#include <QPainter>
#include <QToolButton>
#include <QToolBar>
#include <QWidget>
#include <QPointF>
#include <QColor>
#include <QtMath>

#include "uistyleproxy.h"
#include "uieffects.h"
#include "theme.h"

UiStyleProxy::UiStyleProxy(QStyle *baseStyle)
    : QProxyStyle(baseStyle)
{
}

static QColor blendColor(const QColor &from, const QColor &to, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t);
}

void UiStyleProxy::drawButtonPanel(PrimitiveElement element,
                                   const QStyleOption *option,
                                   QPainter *painter,
                                   const QWidget *widget) const
{
    Theme *theme = Theme::instance();

    /* Classic mode must behave exactly like the stock style */
    if (theme->scheme() == Theme::Classic)
    {
        baseStyle()->drawPrimitive(element, option, painter, widget);
        return;
    }

    const bool isTool = (element == QStyle::PE_PanelButtonTool);
    const bool enabled = (option->state & QStyle::State_Enabled) != 0;
    const bool on = (option->state & QStyle::State_On) != 0;
    const bool sunken = (option->state & QStyle::State_Sunken) != 0;
    const bool mouseOver = (option->state & QStyle::State_MouseOver) != 0;

    bool flat = false;
    bool defaultButton = false;

    if (const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(option))
    {
        flat = (button->features & QStyleOptionButton::Flat) != 0;
        defaultButton = (button->features & QStyleOptionButton::DefaultButton) != 0;
    }

    if (const QToolButton *toolButton = qobject_cast<const QToolButton *>(widget))
    {
        if (toolButton->autoRaise() == true)
            flat = true;
    }

    /* A flat command button (for example the in-row delete button of the
     * Simple Desk cue stack) must stay completely transparent so the item
     * underneath keeps showing through. */
    if (flat == true && element != QStyle::PE_PanelButtonTool)
        return;

    const bool inToolBar = (widget != NULL
                            && qobject_cast<QToolBar *>(widget->parentWidget()) != NULL);
    const QString uiRole = widget != NULL ? widget->property("uiRole").toString() : QString();
    const bool primary = (uiRole == "primary");
    const bool dangerRole = (uiRole == "danger");
    const bool ghost = (uiRole == "ghost" || uiRole == "quiet");
    const bool pill = widget != NULL && widget->property("uiPill").toBool();
    const QColor primaryFill = widget != NULL
                             ? widget->property("uiFillColor").value<QColor>()
                             : QColor();

    qreal hover = UiEffects::instance()->hoverProgress(widget);
    if (theme->animationsEnabled() == false && mouseOver == true)
        hover = 1.0;

    qreal ripple = UiEffects::instance()->rippleProgress(widget);
    const QPointF ripplePos = UiEffects::instance()->rippleOrigin(widget);

    const QColor accent = theme->accent();
    const QColor accentPress = accent.darker(125);
    const QColor accentHover = accent.lighter(112);
    const QColor danger = theme->color(Theme::Danger);
    const QColor background = theme->color(Theme::Background);
    const QColor surface = theme->color(Theme::Surface);
    const QColor surfaceRaised = theme->color(Theme::SurfaceRaised);
    const QColor surfaceHover = theme->color(Theme::SurfaceHover);
    const QColor border = theme->color(Theme::Border);
    const QColor borderStrong = theme->color(Theme::BorderStrong);

    QColor parentBg = inToolBar ? surface : option->palette.color(QPalette::Window);
    if (parentBg.isValid() == false)
        parentBg = background;

    QColor fill;
    QColor stroke(Qt::transparent);

    if (primary == true && enabled == true)
    {
        const QColor base = primaryFill.isValid() ? primaryFill : accent;
        if (sunken == true)
        {
            fill = base.darker(115);
            stroke = fill;
        }
        else
        {
            fill = blendColor(base, base.lighter(112), hover);
            stroke = fill;
        }
    }
    else if (dangerRole == true && enabled == true)
    {
        if (isTool == true)
        {
            /* Destructive toolbar actions only fill when active, but they
             * still get a red-tinted hover so the intent is clear. */
            fill = parentBg;
            if (on == true || sunken == true)
            {
                fill = sunken ? danger.darker(115)
                              : blendColor(danger, danger.lighter(112), hover);
                stroke = fill;
            }
            else if (hover > 0.001)
            {
                fill = blendColor(parentBg, danger, 0.12 * hover);
                stroke = blendColor(parentBg, danger, 0.55 * hover);
            }
        }
        else
        {
            fill = sunken ? danger.darker(115)
                          : blendColor(danger, danger.lighter(112), hover);
            stroke = fill;
        }
    }
    else if (ghost == true && enabled == true && isTool == false)
    {
        if (sunken == true)
        {
            fill = blendColor(parentBg, accent, 0.16);
            stroke = accent;
        }
        else if (hover > 0.001)
        {
            fill = blendColor(parentBg, surfaceHover, hover);
            stroke = blendColor(parentBg, border, hover);
        }
        else
        {
            fill = Qt::transparent;
            stroke = Qt::transparent;
        }
    }
    else if (isTool == true)
    {
        /* Tool buttons are transparent unless they are hovered, checked or
         * pressed. Their parent background is used as the base color so the
         * opaque panel cannot leave alpha trails behind. */
        fill = parentBg;

        if (enabled == false)
        {
            fill = parentBg;
        }
        else if (on == true)
        {
            fill = blendColor(parentBg, accent, 0.20);
            stroke = accent;
        }
        else if (sunken == true)
        {
            fill = accentPress;
            stroke = accentPress;
        }
        else if (hover > 0.001)
        {
            fill = blendColor(parentBg, surfaceHover, hover);
            stroke = blendColor(parentBg, border, hover);
        }
    }
    else
    {
        /* Command buttons are always opaque */
        if (enabled == false)
        {
            fill = surface;
            stroke = border;
        }
        else if (on == true)
        {
            fill = blendColor(surfaceRaised, accent, 0.14);
            stroke = accent;
        }
        else if (defaultButton == true)
        {
            fill = blendColor(accent, accentHover, hover);
            stroke = fill;
        }
        else if (sunken == true)
        {
            fill = accentPress;
            stroke = accentPress;
        }
        else
        {
            fill = blendColor(surfaceRaised, surfaceHover, hover);
            stroke = blendColor(borderStrong, accent, hover);
        }
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    qreal radius = isTool ? qMax(4.0, qreal(theme->radius() - 2))
                          : qreal(theme->radius());
    if (pill == true)
        radius = option->rect.height() / 2.0;
    const QRectF rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);

    const qreal strokeWidth = (on == true) ? 1.5 : 1.0;
    painter->setPen(stroke.alpha() > 0 ? QPen(stroke, strokeWidth) : QPen(Qt::NoPen));
    painter->setBrush(fill);
    painter->drawPath(path);

    /* Soft press ripple, clipped to the rounded panel */
    if (ripple > 0.001 && ripple < 0.999)
    {
        painter->save();
        painter->setClipPath(path);

        QColor rippleColor;
        if (fill.alpha() < 16)
        {
            rippleColor = accent;
        }
        else if (on == true || defaultButton == true || sunken == true
                 || primary == true || dangerRole == true)
        {
            rippleColor = Theme::contrastingText(fill);
        }
        else
        {
            rippleColor = accent;
        }
        rippleColor.setAlphaF(0.28 * (1.0 - ripple));

        const qreal maxRadius = qSqrt(qreal(option->rect.width()) * option->rect.width()
                                    + qreal(option->rect.height()) * option->rect.height());
        const qreal r = maxRadius * ripple;

        painter->setPen(Qt::NoPen);
        painter->setBrush(rippleColor);
        painter->drawEllipse(QPointF(ripplePos - option->rect.topLeft()), r, r);
        painter->restore();
    }

    /* Keyboard focus ring, drawn inside the panel so it never changes size */
    if ((option->state & QStyle::State_HasFocus) && enabled == true)
    {
        QColor focus;
        if (fill.alpha() < 16)
            focus = accent;
        else if (on == true || defaultButton == true || primary == true || dangerRole == true)
            focus = Theme::contrastingText(fill);
        else
            focus = (theme->scheme() == Theme::Light) ? accent.darker(140) : accent.lighter(130);
        focus.setAlphaF(0.75);
        painter->setPen(QPen(focus, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(2, 2, -2, -2), radius - 2, radius - 2);
    }

    painter->restore();
}

void UiStyleProxy::drawPrimitive(PrimitiveElement element,
                                 const QStyleOption *option,
                                 QPainter *painter,
                                 const QWidget *widget) const
{
    switch (element)
    {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel:
    case PE_PanelButtonTool:
        drawButtonPanel(element, option, painter, widget);
        return;

    case PE_IndicatorRadioButton:
        drawRadioIndicator(option, painter, widget);
        return;

    case PE_IndicatorItemViewItemCheck:
        drawItemViewCheck(option, painter, widget);
        return;

    default:
        baseStyle()->drawPrimitive(element, option, painter, widget);
        return;
    }
}

void UiStyleProxy::drawItemViewCheck(const QStyleOption *option, QPainter *painter,
                                     const QWidget *widget) const
{
    Theme *theme = Theme::instance();
    if (theme->scheme() == Theme::Classic)
    {
        baseStyle()->drawPrimitive(PE_IndicatorItemViewItemCheck, option, painter, widget);
        return;
    }

    const bool enabled = (option->state & QStyle::State_Enabled) != 0;
    const bool on = (option->state & QStyle::State_On) != 0;
    const bool partial = (option->state & QStyle::State_NoChange) != 0;
    const QColor accent = theme->accent();
    const QColor border = theme->color(Theme::BorderStrong);
    const QColor surface = theme->color(Theme::Surface);
    const QColor disabled = theme->color(Theme::TextDisabled);

    const int side = qMin(20, qMin(option->rect.width(), option->rect.height()));
    if (side < 10)
    {
        baseStyle()->drawPrimitive(PE_IndicatorItemViewItemCheck, option, painter, widget);
        return;
    }

    QRectF box(option->rect.center().x() - side / 2.0,
               option->rect.center().y() - side / 2.0,
               side, side);
    box.adjust(1.0, 1.0, -1.0, -1.0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor fill = on ? accent : surface;
    QColor stroke = on ? accent : border;
    if (enabled == false)
    {
        fill = theme->color(Theme::SurfaceRaised);
        stroke = theme->color(Theme::Border);
    }

    painter->setPen(QPen(stroke, 1.2));
    painter->setBrush(fill);
    painter->drawRoundedRect(box, 4.0, 4.0);

    if (on || partial)
    {
        QColor check = enabled ? Theme::contrastingText(accent) : disabled;
        painter->setPen(QPen(check, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        if (partial)
        {
            path.moveTo(box.left() + box.width() * 0.28, box.center().y());
            path.lineTo(box.right() - box.width() * 0.28, box.center().y());
        }
        else
        {
            path.moveTo(box.left() + box.width() * 0.24, box.center().y());
            path.lineTo(box.left() + box.width() * 0.43, box.bottom() - box.height() * 0.28);
            path.lineTo(box.right() - box.width() * 0.22, box.top() + box.height() * 0.30);
        }
        painter->drawPath(path);
    }

    painter->restore();
}

void UiStyleProxy::drawRadioIndicator(const QStyleOption *option, QPainter *painter,
                                      const QWidget *widget) const
{
    Theme *theme = Theme::instance();
    if (theme->scheme() == Theme::Classic)
    {
        baseStyle()->drawPrimitive(PE_IndicatorRadioButton, option, painter, widget);
        return;
    }

    const bool enabled = (option->state & QStyle::State_Enabled) != 0;
    const bool on = (option->state & QStyle::State_On) != 0;
    const bool mouseOver = (option->state & QStyle::State_MouseOver) != 0;
    const QColor accent = theme->accent();
    const QColor surface = theme->color(Theme::Surface);
    const QColor surfaceRaised = theme->color(Theme::SurfaceRaised);
    const QColor border = theme->color(Theme::Border);
    const QColor borderStrong = theme->color(Theme::BorderStrong);

    QRectF rect = QRectF(option->rect).adjusted(1, 1, -1, -1);
    if (rect.width() < 6 || rect.height() < 6)
        rect = QRectF(option->rect);
    const qreal side = qMin(rect.width(), rect.height());
    rect = QRectF(rect.center().x() - side / 2.0,
                  rect.center().y() - side / 2.0, side, side);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (on == true)
    {
        QColor ring = enabled ? accent : borderStrong;
        if (enabled && mouseOver)
            ring = accent.lighter(112);
        painter->setPen(Qt::NoPen);
        painter->setBrush(ring);
        painter->drawEllipse(rect);

        const qreal inner = qMax(3.0, side * 0.34);
        QRectF hole(rect.center().x() - inner / 2.0,
                    rect.center().y() - inner / 2.0, inner, inner);
        painter->setBrush(enabled ? surface : surfaceRaised);
        painter->drawEllipse(hole);
    }
    else
    {
        QColor stroke = enabled ? borderStrong : border;
        if (enabled && mouseOver)
            stroke = accent;
        painter->setPen(QPen(stroke, 1.2));
        painter->setBrush(enabled ? surface : surfaceRaised);
        painter->drawEllipse(rect.adjusted(0.6, 0.6, -0.6, -0.6));
    }

    painter->restore();
}

int UiStyleProxy::pixelMetric(PixelMetric metric, const QStyleOption *option,
                              const QWidget *widget) const
{
    Theme *theme = Theme::instance();
    if (theme->scheme() != Theme::Classic)
    {
        if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight
            || metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
        {
            return 20;
        }
        if (metric == PM_ToolBarExtensionExtent)
            return 26;
    }

    return baseStyle()->pixelMetric(metric, option, widget);
}
