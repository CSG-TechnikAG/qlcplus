/*
  Q Light Controller Plus
  uieffects.h

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

#ifndef UIEFFECTS_H
#define UIEFFECTS_H

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QObject>
#include <QTimer>

class QWidget;

/** @addtogroup ui UI
 * @{
 */

/**
 * Tiny animation state tracker for the Qt Widgets UI.
 *
 * This class intentionally does not paint anything by itself. It only keeps
 * the current hover/ripple progress of interactive widgets and asks those
 * widgets to repaint while an animation is running. The actual painting is
 * done by UiStyleProxy during the widget's own paint pass, which means:
 *
 *  - no extra transparent widgets / native windows
 *  - no alpha accumulation and no QGraphicsEffect
 *  - only the animated button is repainted (typically at 60 fps for < 200 ms)
 *  - when animations are disabled or Classic is selected, a single timer is
 *    never started.
 */
class UiEffects final : public QObject
{
    Q_OBJECT

public:
    static UiEffects *instance();

    /** Progress (0..1) of the hover fade for the given widget. */
    qreal hoverProgress(const QWidget *widget) const;

    /** Progress (0..1) of the press ripple for the given widget. */
    qreal rippleProgress(const QWidget *widget) const;

    /** Origin of the press ripple, in widget coordinates. */
    QPointF rippleOrigin(const QWidget *widget) const;

    /** Stop all animations and forget all widgets. */
    void clearEffects();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    UiEffects();

    struct HoverInfo
    {
        qint64 start;
        int duration;
        qreal from;
        qreal to;
    };

    struct RippleInfo
    {
        qint64 start;
        QPointF origin;
    };

    bool effectsActive() const;
    bool isEffectButton(QWidget *widget) const;

    qreal hoverValue(const HoverInfo &info) const;
    qreal rippleValue(const RippleInfo &info) const;

    void startHover(QWidget *widget);
    void stopHover(QWidget *widget);
    void startRipple(QWidget *widget, const QPoint &pos);
    void removeWidget(QWidget *widget);

private slots:
    void slotTick();
    void slotThemeChanged();

private:
    QHash<const QWidget *, HoverInfo> m_hover;
    QHash<const QWidget *, RippleInfo> m_ripple;

    QElapsedTimer m_clock;
    QTimer m_timer;

    Q_DISABLE_COPY(UiEffects)
};

/** @} */

#endif // UIEFFECTS_H
