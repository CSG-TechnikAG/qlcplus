/*
  Q Light Controller Plus
  multitrackview.cpp

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

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QMessageBox>
#include <QScrollBar>
#include <QSlider>
#include <QDebug>
#include <qmath.h>

#include "multitrackview.h"
#include "theme.h"
#include "track.h"

#define VIEW_DEFAULT_WIDTH  2000
#define VIEW_DEFAULT_HEIGHT 600

#define ZOOM_SLIDER_MIN     0
#define ZOOM_SLIDER_MAX     100
#define ZOOM_SLIDER_DEFAULT 50

/* A QSlider that does not react to the mouse wheel just because the pointer
 * happens to be over it. Zooming is explicit: drag the slider or use
 * Ctrl+wheel over the timeline. */
class ShowZoomSlider final : public QSlider
{
public:
    explicit ShowZoomSlider(QWidget *parent = NULL)
        : QSlider(Qt::Horizontal, parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        event->ignore();
    }
};

static qreal timeScaleForSlider(int value)
{
    /* Value 50 is the historical default (3). Higher values zoom in. */
    return 3.0 * qPow(2.0, (qreal(ZOOM_SLIDER_DEFAULT) - qreal(value)) / 15.0);
}

MultiTrackView::MultiTrackView(QWidget *parent) :
        QGraphicsView(parent)
{
    m_scene = new QGraphicsScene();
    m_scene->setSceneRect(0, 0, VIEW_DEFAULT_WIDTH, VIEW_DEFAULT_HEIGHT);
    setSceneRect(0, 0, VIEW_DEFAULT_WIDTH, VIEW_DEFAULT_HEIGHT);
    setScene(m_scene);

    m_timeSlider = new ShowZoomSlider();
    m_timeSlider->setRange(ZOOM_SLIDER_MIN, ZOOM_SLIDER_MAX);
    m_timeSlider->setValue(ZOOM_SLIDER_DEFAULT);
    m_timeSlider->setSingleStep(1);
    m_timeSlider->setPageStep(5);
    m_timeSlider->setToolTip(tr("Timeline zoom"));
    m_timeSlider->setFixedSize(TRACK_WIDTH - 4, HEADER_HEIGHT);

    connect(m_timeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotTimeScaleChanged(int)));
    m_timeSliderProxy = m_scene->addWidget(m_timeSlider);
    m_timeSliderProxy->setZValue(1000);

    m_header = new ShowHeaderItem(m_scene->width());
    m_header->setTimeScale(timeScaleForSlider(m_timeSlider->value()));
    m_header->setPos(TRACK_WIDTH, 0);
    connect(m_header, SIGNAL(itemClicked(QGraphicsSceneMouseEvent *)),
            this, SLOT(slotHeaderClicked(QGraphicsSceneMouseEvent *)));
    m_scene->addItem(m_header);
    m_snapToGrid = false;

    m_cursor = new ShowCursorItem(m_scene->height());
    m_cursor->setPos(TRACK_WIDTH, 0);
    m_cursor->setZValue(999); // make sure the cursor is always on top of everything else
    m_scene->addItem(m_cursor);
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotViewScrolled(int)));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotViewScrolled(int)));

    m_vdivider = NULL;
    // draw horizontal and vertical lines for tracks
    updateTracksDividers();
}

void MultiTrackView::updateTracksDividers()
{
    if (m_hdividers.count() > 0)
    {
        int hdivCount = m_hdividers.count();
        for (int c = 0; c < hdivCount; c++)
            m_scene->removeItem(m_hdividers.takeLast());
        m_hdividers.clear();
    }
    if (m_vdivider != NULL)
        m_scene->removeItem(m_vdivider);

    int ypos = 35 + TRACK_HEIGHT;
    int hDivNum = 6;
    if (m_tracks.count() > 5)
        hDivNum = m_tracks.count();
    for (int j = 0; j < hDivNum; j++)
    {
        const QColor divider = Theme::instance()->color(Theme::Border);
        QGraphicsItem *item = m_scene->addRect(0, ypos + (j * TRACK_HEIGHT),
                                               m_scene->width(), 1,
                                               QPen(divider), QBrush(divider));
        item->setZValue(-1);
        m_hdividers.append(item);
    }
    const QColor divider = Theme::instance()->color(Theme::Border);
    m_vdivider = m_scene->addRect(TRACK_WIDTH - 3, 0, 3, m_scene->height(),
                        QPen(divider), QBrush(divider));
}

void MultiTrackView::setViewSize(int width, int height)
{
    m_scene->setSceneRect(0, 0, width, height);
    setSceneRect(0, 0, width, height);
    m_header->setWidth(width);
    if (m_snapToGrid == true)
        m_header->setHeight(height);
    else
        m_header->setHeight(HEADER_HEIGHT);
    updateTracksDividers();
    updateFixedItems();
}

void MultiTrackView::updateViewSize()
{
    quint32 gWidth = VIEW_DEFAULT_WIDTH;
    quint32 gHeight = VIEW_DEFAULT_HEIGHT;

    // find leftmost show item
    foreach (ShowItem *item, m_items)
    {
        if (item->x() + item->getWidth() > gWidth)
            gWidth = item->x() + item->getWidth();
    }

    if ((m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT > VIEW_DEFAULT_HEIGHT)
    {
        gHeight = (m_tracks.count() * TRACK_HEIGHT) + HEADER_HEIGHT;
        m_cursor->setHeight(gHeight);
    }

    if (gWidth > VIEW_DEFAULT_WIDTH || gHeight > VIEW_DEFAULT_HEIGHT)
        setViewSize(gWidth + 1000, gHeight);
}

void MultiTrackView::resetView()
{
    for (int t = 0; t < m_tracks.count(); t++)
        m_scene->removeItem(m_tracks.at(t));
    m_tracks.clear();

    for (int i = 0; i < m_items.count(); i++)
        m_scene->removeItem(m_items.at(i));
    m_items.clear();

    rewindCursor();
    updateFixedItems();
    this->horizontalScrollBar()->setSliderPosition(0);
    this->verticalScrollBar()->setSliderPosition(0);
    m_scene->update();
}

void MultiTrackView::addTrack(Track *track)
{
    // check if track already exists
    foreach (TrackItem *item, m_tracks)
    {
        if (item->getTrack()->id() == track->id())
            return;
    }

    TrackItem *trackItem = new TrackItem(track, m_tracks.count());
    trackItem->setName(track->name());
    trackItem->setPos(0, HEADER_HEIGHT + (TRACK_HEIGHT * m_tracks.count()));
    m_scene->addItem(trackItem);
    m_tracks.append(trackItem);
    updateFixedItems();
    activateTrack(track);
    connect(trackItem, SIGNAL(itemClicked(TrackItem*)),
            this, SLOT(slotTrackClicked(TrackItem*)));
    connect(trackItem, SIGNAL(itemDoubleClicked(TrackItem*)),
            this, SLOT(slotTrackDoubleClicked(TrackItem*)));
    connect(trackItem, SIGNAL(itemSoloFlagChanged(TrackItem*,bool)),
            this, SLOT(slotTrackSoloFlagChanged(TrackItem*,bool)));
    connect(trackItem, SIGNAL(itemMuteFlagChanged(TrackItem*,bool)),
            this, SLOT(slotTrackMuteFlagChanged(TrackItem*,bool)));
    connect(trackItem, SIGNAL(itemMoveUpDown(Track*,int)),
            this, SIGNAL(trackMoved(Track*,int)));
    connect(trackItem, SIGNAL(itemRequestDelete(Track*)),
            this, SIGNAL(trackDelete(Track*)));
}

void MultiTrackView::setItemCommonProperties(ShowItem *item, ShowFunction *func, int trackNum)
{
    qDebug() << "[" << func->functionID() << "] Start time:" << func->startTime() << "Duration:" << func->duration();

    item->setTrackIndex(trackNum);

    qreal timeScale = m_header->getTimeScale();

    if (func->startTime() == UINT_MAX)
    {
        item->setStartTime(getTimeFromCursor());
        item->setPos(m_cursor->x(), 36 + (trackNum * TRACK_HEIGHT));
    }
    else
        item->setPos(getPositionFromTime(func->startTime()), 36 + (trackNum * TRACK_HEIGHT));

    item->setTimeScale(timeScale);
    item->setResizeGridStep(resizeGridStep());

    connect(item, SIGNAL(itemDropped(QGraphicsSceneMouseEvent *, ShowItem *)),
            this, SLOT(slotItemMoved(QGraphicsSceneMouseEvent *, ShowItem *)));
    connect(item, SIGNAL(alignToCursor(ShowItem*)),
            this, SLOT(slotAlignToCursor(ShowItem*)));
    connect(item, SIGNAL(itemResized(ShowItem*)),
            this, SIGNAL(showItemResized(ShowItem*)));
    connect(item, SIGNAL(timingsRequested(ShowItem*)),
            this, SIGNAL(showItemTimingsRequested(ShowItem*)));
    connect(item, SIGNAL(duplicateRequested(ShowItem*)),
            this, SIGNAL(showItemDuplicateRequested(ShowItem*)));
    m_scene->addItem(item);
    m_items.append(item);
    int new_scene_width = item->x() + item->getWidth();
    if (new_scene_width > VIEW_DEFAULT_WIDTH && new_scene_width > m_scene->width())
        setViewSize(new_scene_width + 500, VIEW_DEFAULT_HEIGHT);
}

void MultiTrackView::addSequence(Chaser *chaser, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(chaser->id());

    SequenceItem *item = new SequenceItem(chaser, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addAudio(Audio *audio, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(audio->id());

    AudioItem *item = new AudioItem(audio, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addRGBMatrix(RGBMatrix *rgbm, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(rgbm->id());

    RGBMatrixItem *item = new RGBMatrixItem(rgbm, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addEFX(EFX *efx, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(efx->id());

    EFXItem *item = new EFXItem(efx, func);
    setItemCommonProperties(item, func, trackNum);
}

void MultiTrackView::addVideo(Video *video, Track *track, ShowFunction *sf)
{
    if (m_tracks.isEmpty())
        return;

    int trackNum = getTrackIndex(track);

    if (track == NULL)
        track = m_tracks.at(trackNum)->getTrack();

    ShowFunction *func = sf;
    if (func == NULL)
        func = track->createShowFunction(video->id());

    VideoItem *item = new VideoItem(video, func);
    setItemCommonProperties(item, func, trackNum);
}

quint32 MultiTrackView::deleteSelectedItem()
{
    ShowItem *selectedItem = getSelectedItem();
    if (selectedItem != NULL)
    {
        QString msg = tr("Do you want to DELETE item:") + QString("\n\n") + selectedItem->functionName();

        // Ask for user's confirmation
        if (QMessageBox::question(this, tr("Delete Functions"), msg,
                                  QMessageBox::Yes, QMessageBox::No)
                                  == QMessageBox::Yes)
        {
            quint32 fID = selectedItem->functionID();
            m_scene->removeItem(selectedItem);
            m_items.removeOne(selectedItem);
            return fID;
        }
        return Function::invalidId();
    }

    int trackIndex = 0;
    foreach (TrackItem *item, m_tracks)
    {
        if (item->isActive() == true)
        {
            Track *track = item->getTrack();
            quint32 trackID = track->id();
            QList <ShowFunction *> sfList = track->showFunctions();
            QString msg = tr("Do you want to DELETE track:") + QString("\n\n") + track->name();
            if (sfList.count() > 0)
            {
                msg += QString("\n\n") + tr("This operation will also DELETE:") + QString("\n\n");
                foreach (ShowItem *item, m_items)
                {
                    if (item->getTrackIndex() == trackIndex)
                        msg += item->functionName() + QString("\n");
                }
            }

            // Ask for user's confirmation
            if (QMessageBox::question(this, tr("Delete Track"), msg,
                                  QMessageBox::Yes, QMessageBox::No)
                                  == QMessageBox::Yes)
            {
                m_scene->removeItem(item);
                m_tracks.removeOne(item);
                return trackID;
            }
            return Function::invalidId();
        }
        trackIndex++;
    }

    return Function::invalidId();
}

void MultiTrackView::deleteShowItem(Track *track, ShowFunction *sf)
{
    for (int i = 0; i < m_items.count(); i++)
    {
        if (m_items.at(i)->showFunction() == sf)
        {
            m_scene->removeItem(m_items.at(i));
            break;
        }
    }

    track->removeShowFunction(sf);
}

void MultiTrackView::moveCursor(quint32 timePos)
{
    qreal newPos = getPositionFromTime(timePos);
    m_cursor->setPos(newPos, 0);
    m_cursor->setTime(timePos);
}

void MultiTrackView::rewindCursor()
{
    m_cursor->setPos(TRACK_WIDTH, 0);
    m_cursor->setTime(0);
}

void MultiTrackView::activateTrack(Track *track)
{
    foreach (TrackItem *item, m_tracks)
    {
        if (item->getTrack()->id() == track->id())
            item->setActive(true);
        else
            item->setActive(false);
    }
}

ShowItem *MultiTrackView::getSelectedItem() const
{
    foreach (ShowItem *item, m_items)
        if (item->isSelected())
            return item;

    return NULL;
}

quint32 MultiTrackView::getTimeFromCursor() const
{
    quint32 s_time = (double)(m_cursor->x() - TRACK_WIDTH) *
                     (m_header->getTimeScale() * 1000) /
                     (double)(m_header->getHalfSecondWidth() * 2);
    return s_time;
}

quint32 MultiTrackView::getTimeFromPosition(qreal pos) const
{
    return ((double)(pos - TRACK_WIDTH) *
            (double)(m_header->getTimeScale() * 1000) /
            (double)(m_header->getHalfSecondWidth() * 2));
}

qreal MultiTrackView::getPositionFromTime(quint32 time) const
{
    const qreal pixelsPerSecond = qreal(m_header->getHalfSecondWidth() * 2) /
                                  qreal(m_header->getTimeScale());
    return qreal(TRACK_WIDTH) + (qreal(time) * pixelsPerSecond / 1000.0);
}

int MultiTrackView::getTrackIndex(const Track *trk) const
{
    for (int idx = 0; idx < m_tracks.count(); idx++)
    {
        if ((trk == NULL && m_tracks.at(idx)->isActive()) ||
            (trk != NULL && trk == m_tracks.at(idx)->getTrack()))
                return idx;
    }

    return 0;
}

void MultiTrackView::setHeaderType(Show::TimeDivision type)
{
    m_header->setTimeDivisionType(type);
    updateItemsResizeGrid();
}

Show::TimeDivision MultiTrackView::getHeaderType() const
{
    return m_header->getTimeDivisionType();
}

void MultiTrackView::setBPMValue(double value)
{
    m_header->setBPMValue(value);
    updateItemsResizeGrid();
}

quint32 MultiTrackView::getTimeGridStep() const
{
    if (m_header == NULL)
        return 1000;

    const qreal stepPixels = m_header->getTimeDivisionStep();
    const qreal stepMs = (stepPixels * m_header->getTimeScale() * 1000.0) /
                         (m_header->getHalfSecondWidth() * 2.0);
    return qMax<quint32>(1, quint32(qRound(stepMs)));
}

quint32 MultiTrackView::resizeGridStep() const
{
    if (m_snapToGrid == false)
        return 0;

    return getTimeGridStep();
}

void MultiTrackView::updateItemsResizeGrid()
{
    const quint32 step = resizeGridStep();
    foreach (ShowItem *item, m_items)
        item->setResizeGridStep(step);
}

void MultiTrackView::setSnapToGrid(bool enable)
{
    m_snapToGrid = enable;
    if (enable == true)
        m_header->setHeight(m_scene->height());
    else
        m_header->setHeight(HEADER_HEIGHT);
    updateItemsResizeGrid();
}

void MultiTrackView::mouseReleaseEvent(QMouseEvent * e)
{
    if (getSelectedItem() == NULL)
    {
        // Don't handle positions at the left of QLC+ window
        if (mapToScene(e->pos()).x() < 0)
            return;
        quint32 xpos = mapToScene(e->pos()).x();
        if (xpos > TRACK_WIDTH)
        {
            m_cursor->setPos(xpos, 0);
            m_cursor->setTime(getTimeFromCursor());
            emit timeChanged(m_cursor->getTime());
        }
        emit viewClicked(e);
    }

    QGraphicsView::mouseReleaseEvent(e);
    //qDebug() << Q_FUNC_INFO << "View clicked at pos: " << e->pos().x() << e->pos().y();
}

void MultiTrackView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        int zoomValue = m_timeSlider->value();
        if (event->pixelDelta().y() > 0)
            zoomValue++;
        else
            zoomValue--;

        if (zoomValue >= m_timeSlider->minimum() && zoomValue <= m_timeSlider->maximum())
            m_timeSlider->setValue(zoomValue);
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void MultiTrackView::slotHeaderClicked(QGraphicsSceneMouseEvent *event)
{
    m_cursor->setPos(TRACK_WIDTH + event->pos().toPoint().x(), 0);
    m_cursor->setTime(getTimeFromCursor());
    qDebug() << Q_FUNC_INFO << "Cursor moved to time:" << m_cursor->getTime();
    emit timeChanged(m_cursor->getTime());
}

void MultiTrackView::slotTimeScaleChanged(int val)
{
    const qreal timeScale = timeScaleForSlider(val);
    m_header->setTimeScale(timeScale);

    foreach (ShowItem *item, m_items)
    {
        qreal newXpos = getPositionFromTime(item->getStartTime());
        item->setPos(newXpos, item->y());
        item->setTimeScale(timeScale);
    }

    qreal newCursorPos = getPositionFromTime(m_cursor->getTime());
    m_cursor->setPos(newCursorPos, m_cursor->y());
    updateItemsResizeGrid();
    updateViewSize();
}

void MultiTrackView::slotTrackClicked(TrackItem *track)
{
    foreach (TrackItem *item, m_tracks)
    {
        if (item == track)
            item->setActive(true);
        else
            item->setActive(false);
    }
    emit trackClicked(track->getTrack());
}

void MultiTrackView::slotTrackDoubleClicked(TrackItem *track)
{
    emit trackDoubleClicked(track->getTrack());
}

void MultiTrackView::slotTrackSoloFlagChanged(TrackItem* track, bool solo)
{
    foreach (TrackItem *item, m_tracks)
    {
        if (item != track)
            item->setFlags(false, solo);
        Track *trk = item->getTrack();
        if (trk != NULL)
            trk->setMute(item->isMute());
    }
}

void MultiTrackView::slotTrackMuteFlagChanged(TrackItem* item, bool mute)
{
    Track *trk = item->getTrack();
    if (trk != NULL)
        trk->setMute(mute);
}

void MultiTrackView::slotViewScrolled(int)
{
    updateFixedItems();
}

void MultiTrackView::updateFixedItems()
{
    const qreal left = horizontalScrollBar()->value();

    /* The track-name column and the zoom slider are part of the scene for
     * historical reasons, but they must stay anchored to the viewport while
     * the timeline scrolls underneath them. */
    foreach (TrackItem *track, m_tracks)
        track->setX(left);

    if (m_vdivider != NULL)
        m_vdivider->setX(left + TRACK_WIDTH - 3);

    if (m_timeSliderProxy != NULL)
        m_timeSliderProxy->setPos(left, verticalScrollBar()->value());
}

void MultiTrackView::slotItemMoved(QGraphicsSceneMouseEvent *event, ShowItem *item)
{
    qDebug() << Q_FUNC_INFO << "event - <" << event->pos().toPoint().x() << "> - <" << event->pos().toPoint().y() << ">";
    // align to the appropriate track
    bool moved = true;
    quint32 s_time = 0;
    int trackNum = item->getTrackIndex();
    int ypos = HEADER_HEIGHT + 1 + (trackNum * TRACK_HEIGHT);
    int shift = qAbs(item->getDraggingPos().x() - item->x());

    if (item->x() < TRACK_WIDTH)
    {
        item->setPos(TRACK_WIDTH, ypos); // avoid moving an item too early...
    }
    else if (shift < 3) // a drag of less than 3 pixel doesn't move the item
    {
        //qDebug() << "Drag too short (" << shift << "px) not allowed!";
        item->setPos(item->getDraggingPos());
        s_time = item->getStartTime();
        moved = false;
    }
    else if (m_snapToGrid == true)
    {
        const qreal step = m_header->getTimeDivisionStep();
        /* Grid lines are measured from the timeline origin, not from the
         * scene origin (which also contains the track-name column). */
        const qreal offset = qRound((item->x() - TRACK_WIDTH) / step) * step;
        const qreal gridPos = TRACK_WIDTH + offset;
        item->setPos(gridPos, ypos);
        s_time = getTimeFromPosition(gridPos);

        /* Audio clips may start with a pickup. Let them align an internal
         * detected beat to the grid, then move the clip edge accordingly. */
        const quint32 pointerTime = getTimeFromPosition(item->x() + event->pos().x());
        s_time = item->snapStartTime(s_time, getTimeGridStep(), pointerTime);
        item->setPos(getPositionFromTime(s_time), ypos);
    }
    else
    {
        item->setPos(item->x(), ypos);
        s_time = getTimeFromPosition(item->x());
    }

    item->setStartTime(s_time);

    m_scene->update();
    emit showItemMoved(item, getTimeFromPosition(item->x() + event->pos().toPoint().x()), moved);
}

void MultiTrackView::slotAlignToCursor(ShowItem *item)
{
    item->setX(m_cursor->x());
    item->setStartTime(getTimeFromPosition(item->x()));
    m_scene->update();
}

