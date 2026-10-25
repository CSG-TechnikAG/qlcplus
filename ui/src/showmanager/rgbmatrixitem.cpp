/*
  Q Light Controller Plus
  rgbmatrixitem.cpp

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

#include <QApplication>
#include <QPainter>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "rgbmatrixitem.h"
#include "trackitem.h"

RGBMatrixItem::RGBMatrixItem(RGBMatrix *rgbm, ShowFunction *func)
    : ShowItem(func)
    , m_matrix(rgbm)
{
    Q_ASSERT(rgbm != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::RGBMatrixType));

    calculateWidth();
    connect(m_matrix, SIGNAL(changed(quint32)), this, SLOT(slotRGBMatrixChanged(quint32)));
}

void RGBMatrixItem::calculateWidth()
{
    int newWidth = 0;
    qint64 matrixDuration = getDuration();

    if (matrixDuration != 0)
        newWidth = qRound((50.0 / double(getTimeScale())) * double(matrixDuration) / 1000.0);
    else
        newWidth = 100;

    if (newWidth < qRound(50.0 / double(m_timeScale)))
        newWidth = qRound(50.0 / double(m_timeScale));
    setWidth(newWidth);
}

void RGBMatrixItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    float xpos = 0;
    float timeUnit = 50.0 / float(getTimeScale());

    ShowItem::paint(painter, option, widget);

    const quint32 step = stepDuration();
    int loopCount = 0;
    if (step > 0)
    {
        if (getDuration() == Function::infiniteSpeed())
            loopCount = 10000 / step;
        else if (getDuration() > 0)
            loopCount = qFloor(getDuration() / step);
    }

    for (int i = 0; i < loopCount; i++)
    {
        xpos += ((timeUnit * float(step)) / 1000);
        // draw loop vertical delimiter
        painter->setPen(QPen(Qt::white, 1));
        painter->drawLine(int(xpos), 1, int(xpos), TRACK_HEIGHT - 5);
    }

    ShowItem::postPaint(painter);
}

void RGBMatrixItem::setTimeScale(qreal val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void RGBMatrixItem::setDuration(quint32 msec, bool stretch)
{
    ShowItem::setDuration(msec, stretch);
    prepareGeometryChange();
    calculateWidth();
    update();
}

quint32 RGBMatrixItem::stepDuration() const
{
    if (m_function == NULL)
        return m_matrix->duration();

    if (m_function->functionSpeed() > 0)
        return m_function->functionSpeed();

    if (m_function->stretch() == true && m_function->duration() > 0)
    {
        const quint32 sourceStep = m_matrix->duration();
        const quint32 sourceTotal = m_matrix->totalDuration();
        if (sourceStep > 0 && sourceStep != Function::infiniteSpeed() &&
            sourceTotal > sourceStep && sourceTotal != Function::infiniteSpeed())
        {
            const quint32 steps = sourceTotal / sourceStep;
            if (steps > 0)
                return qMax<quint32>(1, m_function->duration() / steps);
        }
        return m_function->duration();
    }

    return m_matrix->duration();
}

quint32 RGBMatrixItem::getDuration() const
{
    return m_function->duration() ? m_function->duration() : m_matrix->totalDuration();
}

QString RGBMatrixItem::functionName() const
{
    if (m_matrix)
        return m_matrix->name();
    return QString();
}

RGBMatrix *RGBMatrixItem::getRGBMatrix() const
{
    return m_matrix;
}

void RGBMatrixItem::slotRGBMatrixChanged(quint32)
{
    prepareGeometryChange();
    calculateWidth();
    updateTooltip();
}

void RGBMatrixItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}
