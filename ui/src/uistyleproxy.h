/*
  Q Light Controller Plus
  uistyleproxy.h

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

#ifndef UISTYLEPROXY_H
#define UISTYLEPROXY_H

#include <QProxyStyle>

/** @addtogroup ui UI
 * @{
 */

/**
 * Base style used by the built-in QLC+ themes.
 *
 * Only button panels are drawn here; everything else is left to the
 * stylesheet and to the platform style. Drawings happen during the widget's
 * normal paint pass, so hover/ripple animations are free of overlay widgets
 * and alpha accumulation.
 */
class UiStyleProxy final : public QProxyStyle
{
public:
    explicit UiStyleProxy(QStyle *baseStyle);

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const override;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = NULL,
                    const QWidget *widget = NULL) const override;

private:
    void drawButtonPanel(PrimitiveElement element, const QStyleOption *option,
                         QPainter *painter, const QWidget *widget) const;
    void drawRadioIndicator(const QStyleOption *option, QPainter *painter,
                            const QWidget *widget) const;
    void drawItemViewCheck(const QStyleOption *option, QPainter *painter,
                           const QWidget *widget) const;
};

/** @} */

#endif // UISTYLEPROXY_H
