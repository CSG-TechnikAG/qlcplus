/*
  Q Light Controller Plus
  appswitch.h

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

#ifndef APPSWITCH_H
#define APPSWITCH_H

#include <QAbstractButton>

class QVariantAnimation;

/** @addtogroup ui UI
 * @{
 */

/** Animated switch used for boolean settings. */
class AppSwitch final : public QAbstractButton
{
public:
    explicit AppSwitch(QWidget *parent = NULL);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
    bool event(QEvent *event) override;

private:
    void animateTo(bool checked);

private:
    QVariantAnimation *m_animation;
    qreal m_progress;
    bool m_hovered;
};

/** @} */

#endif // APPSWITCH_H
