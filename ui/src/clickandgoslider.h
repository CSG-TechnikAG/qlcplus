/*
  Q Light Controller Plus
  clickandgoslider.h

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


#ifndef CLICKNGOSLIDER_H
#define CLICKNGOSLIDER_H

#include <QMouseEvent>
#include <QSlider>

/** @addtogroup ui UI
 * @{
 */

/** Theme-aware vertical slider style used by the Simple Desk and VC widgets. */
QString cngDefaultStyle(bool inverted = false, int handleHeight = 32,
                        qreal handleOpacity = 1.0, int trackWidth = 30,
                        int fillWidth = 4);
QString cngSubmasterStyle(bool inverted = false, int handleHeight = 32,
                          qreal handleOpacity = 1.0, int trackWidth = 30,
                          int fillWidth = 4);

class ClickAndGoSlider final : public QSlider
{
    Q_OBJECT
public:
    ClickAndGoSlider(QWidget *parent = 0);
    void setSliderStyleSheet(const QString& styleSheet);
    void setShadowLevel(int level);
    void setHandleValueLine(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void showEvent(QShowEvent *e) override;

signals:
    void controlClicked();

private:
    QString m_styleSheet;
    float m_shadowLevel;
    bool m_handleValueLine;
};

/** @} */

#endif

