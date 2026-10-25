/*
  Q Light Controller Plus
  headeritems.cpp

  Copyright (C) Massimo Callegari

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

#include <QPainter>

#include "headeritems.h"
#include "theme.h"

/****************************************************************************
 *
 * Header item
 *
 ****************************************************************************/
ShowHeaderItem::ShowHeaderItem(int width)
    : m_width(width)
    , m_height(HEADER_HEIGHT)
    , m_timeStep(HALF_SECOND_WIDTH)
    , m_timeHit(2)
    , m_timeScale(3.0)
    , m_BPMValue(120)
    , m_type(Show::Time)
{
}

void ShowHeaderItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    emit itemClicked(event);
}

QRectF ShowHeaderItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void ShowHeaderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // draw base background
    Theme *theme = Theme::instance();
    const QColor border = theme->color(Theme::Border);
    const QColor text = theme->color(Theme::Text);
    const QColor dim = theme->color(Theme::TextDim);
    painter->setPen(QPen(border, 1));
    painter->setBrush(QBrush(theme->color(Theme::SurfaceRaised)));
    painter->drawRect(0, 0, m_width, 35);

    if (m_type > Show::Time)
        updateTimeStep();

    // draw vertical timing lines and time labels
    int tmpSec = 0;
    for (int i = 0; i < m_width / m_timeStep; i++)
    {
        float xpos = (float)i * m_timeStep;
        painter->setPen(QPen(dim, 1));
        if (i%m_timeHit == 0)
        {
            painter->drawLine(xpos, 20, xpos, 34);
            if (m_height > HEADER_HEIGHT)
            {
                painter->setPen(QPen(border, 1));
                painter->drawLine(xpos, HEADER_HEIGHT, xpos, m_height);
            }
            painter->setPen(QPen(text, 1));
            const int labelX = qMax(1, int(xpos) - 4);
            if (m_type == Show::Time)
            {
                const qreal seconds = (i / 2.0) * m_timeScale;
                if (seconds < 60.0)
                {
                    const int precision = (m_timeScale < 1.0) ? 1 : 0;
                    painter->drawText(labelX, 15,
                                      QString("%1s").arg(QString::number(seconds, 'f', precision)));
                }
                else
                {
                    const int totalSeconds = qRound(seconds);
                    const int tmpMin = totalSeconds / 60;
                    const int remSeconds = totalSeconds - (tmpMin * 60);
                    painter->drawText(labelX, 15,
                                      QString("%1m%2s").arg(tmpMin).arg(remSeconds));
                }
            }
            else
            {
                tmpSec++;
                painter->drawText(labelX, 15, QString("%1").arg(tmpSec));
            }
        }
        else
        {
            if (m_timeStep > 5)
            {
                painter->drawLine(xpos, 25, xpos, 34);
                if (m_height > HEADER_HEIGHT)
                {
                    painter->setPen(QPen(border, 1));
                    painter->drawLine(xpos, HEADER_HEIGHT, xpos, m_height);
                }
            }
        }
    }

}

void ShowHeaderItem::setTimeScale(qreal val)
{
    m_timeScale = val;
    updateTimeStep();
    update();
}

void ShowHeaderItem::updateTimeStep()
{
    if (m_type > Show::Time && m_BPMValue > 0)
        m_timeStep = ((float)(120 * HALF_SECOND_WIDTH) / (float)m_BPMValue) / (float)m_timeScale;
    else
        m_timeStep = HALF_SECOND_WIDTH;
}

qreal ShowHeaderItem::getTimeScale() const
{
    return m_timeScale;
}

void ShowHeaderItem::setTimeDivisionType(Show::TimeDivision type)
{
    if (type >= Show::Invalid)
        return;

    m_type = type;
    if (m_type == Show::Time)
    {
        m_timeHit = 2;
    }
    else
    {
        if (m_type == Show::BPM_4_4)
            m_timeHit = 4;
        else if (m_type == Show::BPM_3_4)
            m_timeHit = 3;
        else if (m_type == Show::BPM_2_4)
            m_timeHit = 2;
    }
    updateTimeStep();
    update();
}

Show::TimeDivision ShowHeaderItem::getTimeDivisionType() const
{
    return m_type;
}

void ShowHeaderItem::setBPMValue(double value)
{
    if (value > 1)
        m_BPMValue = value;
    updateTimeStep();
    update();
}

int ShowHeaderItem::getHalfSecondWidth() const
{
    return HALF_SECOND_WIDTH;
}

float ShowHeaderItem::getTimeDivisionStep() const
{
    if (m_type > Show::Time && m_timeStep <= 5)
        return m_timeStep * m_timeHit;
    return m_timeStep;
}

void ShowHeaderItem::setWidth(int w)
{
    prepareGeometryChange();
    m_width = w;
}

void ShowHeaderItem::setHeight(int h)
{
    prepareGeometryChange();
    m_height = h;
}


/****************************************************************************
 *
 * Cursor item
 *
 ****************************************************************************/
ShowCursorItem::ShowCursorItem(int h)
    : m_height(h)
    , m_time(0)
{
}

void ShowCursorItem::setHeight(int height)
{
    prepareGeometryChange();
    m_height = height;
}

void ShowCursorItem::setTime(quint32 t)
{
    m_time = t;
}

quint32 ShowCursorItem::getTime() const
{
    return m_time;
}

QRectF ShowCursorItem::boundingRect() const
{
    return QRectF(-5, 0, 10, m_height);
}

void ShowCursorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setBrush(QBrush(Qt::yellow, Qt::SolidPattern));
    painter->setPen(QPen(Qt::yellow, 1));
    QPolygonF CursorHead;
    CursorHead.append(QPointF(-5.0, 22.0));
    CursorHead.append(QPointF(5.0, 22.0));
    CursorHead.append(QPointF(5.0, 25.0));
    CursorHead.append(QPointF(0.0, 35.0));
    CursorHead.append(QPointF(-5.0, 25.0));
    CursorHead.append(QPointF(-5.0, 22.0));
    painter->drawPolygon(CursorHead);
    painter->setPen(Qt::NoPen);
    painter->drawRect(0, 35, 1, m_height - 35);
}
