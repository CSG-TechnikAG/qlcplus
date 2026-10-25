/*
  Q Light Controller Plus
  showitem.cpp

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

#include <QGraphicsSceneEvent>
#include <QGraphicsSceneHoverEvent>
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QMenu>

#include "headeritems.h"
#include "trackitem.h"
#include "showitem.h"
#include "theme.h"
#include "appicons.h"
#include "function.h"

ShowItem::ShowItem(ShowFunction *function, QObject *)
    : m_color(100, 100, 100)
    , m_locked(false)
    , m_pressed(false)
    , m_resizing(false)
    , m_resizeChanged(false)
    , m_resizeStartWidth(0)
    , m_resizeStartDuration(0)
    , m_resizeGridStep(0)
    , m_width(50)
    , m_timeScale(3.0)
    , m_trackIdx(-1)
    , m_function(function)
    , m_alignToCursor(NULL)
    , m_lockAction(NULL)
    , m_timingsAction(NULL)
    , m_duplicateAction(NULL)
{
    Q_ASSERT(function != NULL);

    setCursor(Qt::OpenHandCursor);
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    m_font = qApp->font();
    m_font.setBold(true);
    m_font.setPixelSize(12);

    setLocked(m_function->isLocked());

    m_alignToCursor = new QAction(tr("Align to cursor"), this);
    connect(m_alignToCursor, SIGNAL(triggered()),
            this, SLOT(slotAlignToCursorClicked()));
    m_lockAction = new QAction(tr("Lock item"), this);
    connect(m_lockAction, SIGNAL(triggered()),
            this, SLOT(slotLockItemClicked()));
    m_timingsAction = new QAction(AppIcons::icon("clock", 16),
                                  tr("Start time and duration..."), this);
    connect(m_timingsAction, SIGNAL(triggered()),
            this, SLOT(slotTimingsClicked()));
    m_duplicateAction = new QAction(AppIcons::icon("copy-check", 16),
                                    tr("Duplicate clip"), this);
    connect(m_duplicateAction, SIGNAL(triggered()),
            this, SLOT(slotDuplicateClicked()));
}

void ShowItem::updateTooltip()
{
    if (m_function == NULL)
        return;

    QString details;
    if (m_function->functionSpeed() > 0)
        details += tr("Clip speed: %1\n").arg(Function::speedToString(m_function->functionSpeed()));
    if (m_function->stretch() == true)
        details += tr("Source function stretched to the clip duration\n");

    setToolTip(QString(tr("Name: %1\nStart time: %2\nDuration: %3\n%4%5"))
              .arg(functionName())
              .arg(Function::speedToString(m_function->startTime()))
              .arg(Function::speedToString(getDuration()))
              .arg(details)
              .arg(tr("Click to move this item along the timeline")));
}

QList<QAction *> ShowItem::getDefaultActions() const
{
    QList<QAction *> actions;
    actions.append(m_alignToCursor);

    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(AppIcons::icon("unlock", 16));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(AppIcons::icon("lock", 16));
    }
    actions.append(m_lockAction);
    actions.append(m_timingsAction);
    actions.append(m_duplicateAction);

    return actions;
}

void ShowItem::setTimeScale(qreal val)
{
    prepareGeometryChange();
    m_timeScale = val;
}

qreal ShowItem::getTimeScale() const
{
    return m_timeScale;
}

void ShowItem::setStartTime(quint32 time)
{
    if (m_function == NULL)
        return;

    m_function->setStartTime(time);
    updateTooltip();
}

quint32 ShowItem::getStartTime() const
{
    if (m_function)
        return m_function->startTime();
    return 0;
}

void ShowItem::setDuration(quint32 msec, bool stretch)
{
    if (m_function == NULL)
        return;

    m_function->setDuration(msec);
    m_function->setStretch(stretch);
    if (stretch == true)
        m_function->setFunctionSpeed(0);
    updateTooltip();
}

quint32 ShowItem::getDuration() const
{
    if (m_function)
        return m_function->duration();
    return 0;
}

quint32 ShowItem::snapStartTime(quint32 start, quint32 gridStepMs,
                                quint32 pointerTime) const
{
    Q_UNUSED(gridStepMs)
    Q_UNUSED(pointerTime)
    return start;
}

void ShowItem::setWidth(int w)
{
    m_width = w;
    updateTooltip();
}

int ShowItem::getWidth() const
{
    return m_width;
}

QPointF ShowItem::getDraggingPos() const
{
    return m_pos;
}

bool ShowItem::isDurationAdjustable() const
{
    return true;
}

void ShowItem::setResizeGridStep(quint32 msec)
{
    m_resizeGridStep = msec;
}

bool ShowItem::isNearRightEdge(const QPointF &pos) const
{
    return (isDurationAdjustable() == true &&
            m_locked == false &&
            pos.x() >= qreal(m_width - 7) &&
            pos.x() <= qreal(m_width + 2));
}

void ShowItem::setTrackIndex(int idx)
{
    m_trackIdx = idx;
}

int ShowItem::getTrackIndex() const
{
    return m_trackIdx;
}

void ShowItem::setColor(QColor col)
{
    m_color = col;
    if (m_function)
        m_function->setColor(col);
    update();
}

QColor ShowItem::getColor() const
{
    return m_color;
}

void ShowItem::setLocked(bool locked)
{
    m_locked = locked;
    if (m_function)
        m_function->setLocked(locked);
    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    update();
}

bool ShowItem::isLocked() const
{
    return m_locked;
}

void ShowItem::setFunctionID(quint32 id)
{
    if (m_function != NULL)
        m_function->setFunctionID(id);
}

quint32 ShowItem::functionID() const
{
    if (m_function != NULL)
        return m_function->functionID();

    return Function::invalidId();
}

ShowFunction *ShowItem::showFunction() const
{
    return m_function;
}

QString ShowItem::functionName() const
{
    return QString();
}

void ShowItem::slotAlignToCursorClicked()
{
    emit alignToCursor(this);
}

void ShowItem::slotLockItemClicked()
{
    setLocked(!isLocked());
    //update();
}

void ShowItem::slotTimingsClicked()
{
    emit timingsRequested(this);
}

void ShowItem::slotDuplicateClicked()
{
    emit duplicateRequested(this);
}

void ShowItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isNearRightEdge(event->pos()) == true)
    {
        m_resizing = true;
        m_resizeChanged = false;
        m_resizeStartWidth = m_width;
        m_resizeStartDuration = getDuration();
        m_pressed = false;
        setSelected(true);
        setCursor(Qt::SizeHorCursor);
        event->accept();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
    m_pos = this->pos();
    if (event->button() == Qt::LeftButton)
        m_pressed = true;
    this->setSelected(true);
}

void ShowItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing == true)
    {
        int width = qMax(1, int(event->pos().x()));
        const qint64 deltaWidth = qint64(width) - qint64(m_resizeStartWidth);
        qint64 newDuration = qint64(m_resizeStartDuration) +
                             qRound64((qreal(deltaWidth) * 1000.0 * m_timeScale) / 50.0);
        quint32 duration = qMax<quint32>(1, quint32(qMax<qint64>(1, newDuration)));

        if (m_resizeGridStep > 0)
        {
            const quint32 steps = qMax<quint32>(1, qRound(qreal(duration) / qreal(m_resizeGridStep)));
            duration = steps * m_resizeGridStep;
        }

        if (duration != getDuration())
        {
            const bool stretch = (m_function != NULL && m_function->stretch());
            setDuration(duration, stretch);
            m_resizeChanged = true;
        }

        event->accept();
        return;
    }

    QGraphicsItem::mouseMoveEvent(event);
}

void ShowItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (isNearRightEdge(event->pos()) == true)
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::OpenHandCursor);

    QGraphicsItem::hoverMoveEvent(event);
}

void ShowItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_resizing == false)
        setCursor(Qt::OpenHandCursor);

    QGraphicsItem::hoverLeaveEvent(event);
}

void ShowItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_resizing == true)
    {
        m_resizing = false;
        setCursor(Qt::OpenHandCursor);
        if (m_resizeChanged == true)
            emit itemResized(this);
        m_resizeChanged = false;
        event->accept();
        return;
    }

    QGraphicsItem::mouseReleaseEvent(event);
    qDebug() << Q_FUNC_INFO << "mouse RELEASE event - <" << event->pos().toPoint().x() << "> - <" << event->pos().toPoint().y() << ">";
    setCursor(Qt::OpenHandCursor);
    m_pressed = false;
    emit itemDropped(event, this);
}

void ShowItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_UNUSED(event)

    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    menu.addAction(m_alignToCursor);
    if (isLocked())
    {
        m_lockAction->setText(tr("Unlock item"));
        m_lockAction->setIcon(AppIcons::icon("unlock", 16));
    }
    else
    {
        m_lockAction->setText(tr("Lock item"));
        m_lockAction->setIcon(AppIcons::icon("lock", 16));
    }
    menu.addAction(m_lockAction);
    menu.addAction(m_timingsAction);
    menu.addAction(m_duplicateAction);
    menu.exec(QCursor::pos());
}


QRectF ShowItem::boundingRect() const
{
    return QRectF(0, 0, m_width, TRACK_HEIGHT - 3);
}

void ShowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (this->isSelected() == true)
        painter->setPen(QPen(Theme::instance()->accent(), 3));
    else
        painter->setPen(QPen(Theme::instance()->color(Theme::BorderStrong), 1));

    // draw item background
    painter->setBrush(QBrush(m_color));
    painter->drawRect(0, 0, m_width, TRACK_HEIGHT - 3);

    /* Draw a small handle on the right edge of selected items to make the
     * duration resizing discoverable. */
    if (isSelected() == true && isDurationAdjustable() == true && m_locked == false)
    {
        QColor handle = Theme::contrastingText(m_color);
        handle.setAlpha(190);
        painter->setPen(QPen(handle, 1));
        const int handleX = qMax(2, m_width - 6);
        for (int i = 0; i < 3; ++i)
            painter->drawLine(handleX + (i * 2), 4, handleX + (i * 2), TRACK_HEIGHT - 8);
    }

    painter->setFont(m_font);
}

void ShowItem::postPaint(QPainter *painter)
{
    // draw the function name shadow
    painter->setPen(QPen(QColor(10, 10, 10, 150), 2));
    painter->drawText(QRect(4, 6, m_width - 6, 71), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    // draw the function name
    painter->setPen(QPen(QColor(220, 220, 220, 255), 2));
    painter->drawText(QRect(3, 5, m_width - 5, 72), Qt::AlignLeft | Qt::TextWordWrap, functionName());

    if (m_locked)
        painter->drawPixmap(3, TRACK_HEIGHT >> 1, 24, 24,
                             AppIcons::pixmap("lock", 24,
                                              AppIcons::palette(AppIcons::Action, AppIcons::Normal),
                                              painter->device()->devicePixelRatioF()));

    if (m_pressed)
    {
        quint32 s_time = 0;
        if (x() > TRACK_WIDTH)
            s_time = (double)(x() - TRACK_WIDTH) * (m_timeScale * 500) /
                     (double)(HALF_SECOND_WIDTH);
        painter->drawText(3, TRACK_HEIGHT - 10, Function::speedToString(s_time));
    }
}


