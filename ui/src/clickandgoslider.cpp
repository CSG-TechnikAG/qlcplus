/*
  Q Light Controller Plus
  clickandgoslider.cpp

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

#include "clickandgoslider.h"
#include "theme.h"
#include <QStyleOptionSlider>
#include <QPainter>
#include <QStyle>

ClickAndGoSlider::ClickAndGoSlider(QWidget *parent) : QSlider(parent)
{
    m_shadowLevel = -1;
    m_handleValueLine = false;
}

void ClickAndGoSlider::setSliderStyleSheet(const QString &styleSheet)
{
    if (isVisible())
        QSlider::setStyleSheet(styleSheet);
    else
        m_styleSheet = styleSheet;
}

void ClickAndGoSlider::setShadowLevel(int level)
{
    m_shadowLevel = level;
    update();
}

void ClickAndGoSlider::setHandleValueLine(bool enabled)
{
    m_handleValueLine = enabled;
    update();
}

void ClickAndGoSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->modifiers() == Qt::ControlModifier)
    {
        emit controlClicked();
        return;
    }

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    if (e->button() == Qt::LeftButton && // react only to left button press
        sr.contains(e->pos()) == false) // check if the click is not over the slider's handle
    {
        int newVal = 0;
        if (orientation() == Qt::Vertical)
            newVal = minimum() + ((maximum() - minimum()) * (height() - e->pos().y())) / height();
        else
            newVal = minimum() + ((maximum() - minimum()) * e->pos().x()) / width();

        setSliderDown(true);
        if (invertedAppearance() == true)
            setValue(maximum() - newVal);
        else
            setValue(newVal);
        setSliderDown(false);

        e->accept();
    }
    QSlider::mousePressEvent(e);
}

void ClickAndGoSlider::wheelEvent(QWheelEvent *e)
{
    setSliderDown(true);
    QSlider::wheelEvent(e);
    setSliderDown(false);
}

void ClickAndGoSlider::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
        setSliderDown(true);
    QSlider::keyPressEvent(e);
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
        setSliderDown(false);
}

void ClickAndGoSlider::paintEvent(QPaintEvent *e)
{
    if (m_shadowLevel >= 0)
    {
        QPainter p(this);
        Theme *theme = Theme::instance();
        int levHeight = ((float)height() / 255.0) * m_shadowLevel;
        p.fillRect(width() - 6, 0, width(), height(), theme->color(Theme::Border));
        p.fillRect(width() - 5, 0, width() - 1, height(), theme->color(Theme::SurfaceHover));
        if (invertedAppearance())
            p.fillRect(width() - 5, 0, width() - 1, levHeight, theme->color(Theme::Success));
        else
            p.fillRect(width() - 5, height() - levHeight, width() - 1, height(),
                       theme->color(Theme::Success));
    }

    QSlider::paintEvent(e);

    if (m_handleValueLine == true && orientation() == Qt::Vertical)
    {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect hr = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                 QStyle::SC_SliderHandle, this);
        if (hr.isValid() && hr.width() > 8)
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, true);
            QColor line = Theme::instance()->color(Theme::Background);
            p.setPen(QPen(line, 2.0, Qt::SolidLine, Qt::RoundCap));
            const int inset = qMax(5, hr.width() / 4);
            const int y = hr.center().y();
            p.drawLine(hr.left() + inset, y, hr.right() - inset, y);
        }
    }
}

void ClickAndGoSlider::showEvent(QShowEvent *)
{
    if (m_styleSheet.isEmpty() == false)
    {
        setSliderStyleSheet(m_styleSheet);
        m_styleSheet = "";
    }
}


QString cngDefaultStyle(bool inverted, int handleHeight, qreal handleOpacity,
                        int trackWidth, int fillWidth)
{
    Theme *theme = Theme::instance();
    const QColor handle = theme->color(Theme::Text);
    const QColor track = theme->color(Theme::SurfaceHover);
    const QColor accent = theme->accent();
    const QColor border = theme->color(Theme::BorderStrong);
    const QColor disabled = theme->color(Theme::TextDisabled);

    const int fillMargin = qMax(0, (trackWidth - fillWidth) / 2);
    const QString trackPage = QString("background: %1; border: 1px solid %2; margin: 0 1px; border-radius: 3px;")
                              .arg(track.name(), border.name());
    const QString accentPage = QString("background: %1; border: none; margin: 0 %2px; border-radius: 2px;")
                               .arg(accent.name()).arg(fillMargin);
    const QString handleColor = (handleOpacity < 0.999)
        ? QString("rgba(%1, %2, %3, %4)").arg(handle.red()).arg(handle.green())
          .arg(handle.blue()).arg(QString::number(handleOpacity, 'f', 2))
        : handle.name();
    const QString handleStyle = QString("background: %1; border: 1px solid %2; "
                                        "border-radius: 5px; margin: 0 -2px; "
                                        "width: 34px; height: %3px;")
                                .arg(handleColor, border.name()).arg(handleHeight);

    return QString(
        "QSlider::groove:vertical { background: transparent; width: %1px; } "
        "QSlider::sub-page:vertical { %2 } "
        "QSlider::add-page:vertical { %3 } "
        "QSlider::handle:vertical { %4 } "
        "QSlider::handle:vertical:hover { background: %5; border-color: %5; } "
        "QSlider::handle:vertical:disabled { background: %6; border-color: %7; }")
        .arg(trackWidth)
        .arg(inverted ? accentPage : trackPage,
             inverted ? trackPage : accentPage,
             handleStyle, accent.name(), disabled.name(), border.name());
}

QString cngSubmasterStyle(bool inverted, int handleHeight, qreal handleOpacity,
                          int trackWidth, int fillWidth)
{
    Theme *theme = Theme::instance();
    const QColor handle = theme->color(Theme::Text);
    const QColor track = theme->color(Theme::SurfaceHover);
    const QColor success = theme->color(Theme::Success);
    const QColor border = theme->color(Theme::BorderStrong);
    const QColor disabled = theme->color(Theme::TextDisabled);

    const int fillMargin = qMax(0, (trackWidth - fillWidth) / 2);
    const QString trackPage = QString("background: %1; border: 1px solid %2; margin: 0 1px; border-radius: 3px;")
                              .arg(track.name(), border.name());
    const QString accentPage = QString("background: %1; border: none; margin: 0 %2px; border-radius: 2px;")
                               .arg(success.name()).arg(fillMargin);
    const QString handleColor = (handleOpacity < 0.999)
        ? QString("rgba(%1, %2, %3, %4)").arg(handle.red()).arg(handle.green())
          .arg(handle.blue()).arg(QString::number(handleOpacity, 'f', 2))
        : handle.name();
    const QString handleStyle = QString("background: %1; border: 1px solid %2; "
                                        "border-radius: 5px; margin: 0 -2px; "
                                        "width: 34px; height: %3px;")
                                .arg(handleColor, border.name()).arg(handleHeight);

    return QString(
        "QSlider::groove:vertical { background: transparent; width: %1px; } "
        "QSlider::sub-page:vertical { %2 } "
        "QSlider::add-page:vertical { %3 } "
        "QSlider::handle:vertical { %4 } "
        "QSlider::handle:vertical:hover { background: %5; border-color: %5; } "
        "QSlider::handle:vertical:disabled { background: %6; border-color: %7; }")
        .arg(trackWidth)
        .arg(inverted ? accentPage : trackPage,
             inverted ? trackPage : accentPage,
             handleStyle, success.name(), disabled.name(), border.name());
}
