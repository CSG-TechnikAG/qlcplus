/*
  Q Light Controller Plus
  monitorgraphicsview.cpp

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

#include "monitorproperties.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "qlcfixturemode.h"
#include "doc.h"

#include <QGraphicsRectItem>
#include <QtMath>

MonitorGraphicsView::MonitorGraphicsView(Doc *doc, QWidget *parent)
    : QGraphicsView(parent)
    , m_doc(doc)
    , m_unitValue(1000)
    , m_gridEnabled(true)
    , m_bgItem(NULL)
    , m_stageW(0)
    , m_stageH(0)
    , m_stageOffX(0)
    , m_stageOffY(0)
    , m_stageItem(NULL)
{
    m_scene = new QGraphicsScene();
    m_scene->setSceneRect(this->rect());
    setScene(m_scene);

    m_gridSize = QSizeF(5, 5);
    m_cellPixels = 50;

    updateGrid();
}

MonitorGraphicsView::~MonitorGraphicsView()
{
    clearFixtures();
}

void MonitorGraphicsView::setGridSize(QSizeF size)
{
    m_gridSize = size;
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
}

void MonitorGraphicsView::setStage(double sx, double sy, double offX, double offY)
{
    if (qFuzzyCompare(m_stageW, sx) && qFuzzyCompare(m_stageH, sy) &&
        qFuzzyCompare(m_stageOffX, offX) && qFuzzyCompare(m_stageOffY, offY))
        return;
    m_stageW = sx;
    m_stageH = sy;
    m_stageOffX = offX;
    m_stageOffY = offY;
    updateGrid();
}

void MonitorGraphicsView::setGridMetrics(float value)
{
    m_unitValue = value;
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
}

quint32 MonitorGraphicsView::selectedFixtureID() const
{
    MonitorFixtureItem *item = getSelectedItem();
    if (item != NULL)
        return item->fixtureID();
    else
        return Fixture::invalidId();
}

QList<quint32> MonitorGraphicsView::fixturesID() const
{
    return m_fixtures.keys();
}

void MonitorGraphicsView::setFixtureGelColor(quint32 id, QColor col)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setGelColor(col);
}

void MonitorGraphicsView::setFixtureRotation(quint32 id, ushort degrees)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item != NULL)
        item->setRotation(degrees);
}

void MonitorGraphicsView::showFixturesLabels(bool visible)
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        item->showLabel(visible);
}

QColor MonitorGraphicsView::fixtureGelColor(quint32 id)
{
    MonitorFixtureItem *item = m_fixtures[id];
    if (item == NULL)
        return QColor();

    return item->getColor();
}

QPointF MonitorGraphicsView::realPositionToPixels(qreal xpos, qreal ypos)
{
    qreal realX = m_xOffset + ((xpos * m_cellPixels) / m_unitValue);
    qreal realY = m_yOffset + ((ypos * m_cellPixels) / m_unitValue);

    return QPointF(realX, realY);
}

void MonitorGraphicsView::updateFixture(quint32 id)
{
    Fixture *fxi = m_doc->fixture(id);
    if (fxi == NULL || m_fixtures.contains(id) == false)
        return;

    const QLCFixtureMode *mode = fxi->fixtureMode();
    int width = 0;
    int height = 0;

    if (mode != 0)
    {
        width = mode->physical().width();
        height = mode->physical().height();
    }

    if (width == 0) width = 300;
    if (height == 0) height = 300;


    MonitorFixtureItem *item = m_fixtures[id];
    item->setSize(QSize(qRound((width * m_cellPixels) / m_unitValue),
                        qRound((height * m_cellPixels) / m_unitValue)));

    item->setPos(realPositionToPixels(item->realPosition().x(), item->realPosition().y()));
}

void MonitorGraphicsView::setBackgroundImage(QString filename)
{
    m_backgroundImage = filename;
    if (m_bgItem != NULL)
    {
        m_scene->removeItem(m_bgItem);
        delete m_bgItem;
        m_bgItem = NULL;
    }
    if (filename.isEmpty() == false)
    {
        m_bgPixmap = QPixmap(m_backgroundImage);
        m_bgItem = new QGraphicsPixmapItem(m_bgPixmap);
        m_bgItem->setZValue(0); // make sure it goes on the bacground
        m_scene->addItem(m_bgItem);
    }
    updateGrid();
}

MonitorFixtureItem *MonitorGraphicsView::getSelectedItem() const
{
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        MonitorFixtureItem *item = it.value();
        if (item->isSelected() == true)
            return item;
    }
    return NULL;
}

void MonitorGraphicsView::addFixture(quint32 id, QPointF pos)
{
    if (id == Fixture::invalidId() || m_fixtures.contains(id) == true)
        return;

    if (m_doc->fixture(id) == NULL)
        return;

    MonitorFixtureItem *item = new MonitorFixtureItem(m_doc, id);
    item->setZValue(2);
    item->setRealPosition(pos);
    m_fixtures[id] = item;
    m_scene->addItem(item);
    updateFixture(id);
    connect(item, SIGNAL(itemDropped(MonitorFixtureItem*)),
            this, SLOT(slotFixtureMoved(MonitorFixtureItem*)));
}

void MonitorGraphicsView::setFixturePosition(quint32 id, QPointF pos)
{
    MonitorFixtureItem *item = m_fixtures.value(id, NULL);
    if (item == NULL)
    {
        addFixture(id, pos);
        return;
    }
    item->setRealPosition(pos);
    updateFixture(id);
}

bool MonitorGraphicsView::removeFixture(quint32 id)
{
    MonitorFixtureItem *item = NULL;

    if (id == Fixture::invalidId())
    {
        item = getSelectedItem();
        if (item != NULL)
            id = item->fixtureID();
    }
    else
        item = m_fixtures[id];

    if (item == NULL)
        return false;

    m_scene->removeItem(item);
    m_fixtures.take(id);
    m_doc->monitorProperties()->removeFixture(id);
    delete item;

    return true;
}

void MonitorGraphicsView::clearFixtures()
{
    foreach (MonitorFixtureItem *item, m_fixtures)
        delete item;
    m_fixtures.clear();
}

void MonitorGraphicsView::updateGrid()
{
    int itemsCount = m_gridItems.count();
    for (int i = 0; i < itemsCount; i++)
        m_scene->removeItem((QGraphicsItem *)m_gridItems.takeLast());

    qreal gridW = m_gridSize.width();
    qreal gridH = m_gridSize.height();
    if (gridW <= 0.0) gridW = 1.0;
    if (gridH <= 0.0) gridH = 1.0;

    m_xOffset = 0;
    m_yOffset = 0;

    if (m_gridEnabled == true)
    {
        qreal xInc = qreal(this->width()) / gridW;
        qreal yInc = qreal(this->height()) / gridH;
        if (yInc < xInc)
        {
            m_cellPixels = yInc;
            m_xOffset = (this->width() - (m_cellPixels * gridW)) / 2.0;
        }
        else if (xInc < yInc)
        {
            m_cellPixels = xInc;
            m_yOffset = (this->height() - (m_cellPixels * gridH)) / 2.0;
        }
        else
        {
            m_cellPixels = xInc;
        }

        qreal gridPixelW = m_cellPixels * gridW;
        qreal gridPixelH = m_cellPixels * gridH;

        // Vertical lines: one per whole unit, plus a partial right border.
        int nx = int(qFloor(gridW));
        for (int i = 0; i <= nx; i++)
        {
            qreal xPos = m_xOffset + i * m_cellPixels;
            QGraphicsLineItem *item = m_scene->addLine(xPos, m_yOffset, xPos, m_yOffset + gridPixelH,
                                                       QPen(QColor(40, 40, 40, 255)));
            item->setZValue(1);
            m_gridItems.append(item);
        }
        if (qAbs(gridW - nx) > 1e-6)
        {
            qreal xPos = m_xOffset + gridPixelW;
            QGraphicsLineItem *item = m_scene->addLine(xPos, m_yOffset, xPos, m_yOffset + gridPixelH,
                                                       QPen(QColor(40, 40, 40, 255)));
            item->setZValue(1);
            m_gridItems.append(item);
        }

        // Horizontal lines
        int ny = int(qFloor(gridH));
        for (int i = 0; i <= ny; i++)
        {
            qreal yPos = m_yOffset + i * m_cellPixels;
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, yPos, m_xOffset + gridPixelW, yPos,
                                                       QPen(QColor(40, 40, 40, 255)));
            item->setZValue(1);
            m_gridItems.append(item);
        }
        if (qAbs(gridH - ny) > 1e-6)
        {
            qreal yPos = m_yOffset + gridPixelH;
            QGraphicsLineItem *item = m_scene->addLine(m_xOffset, yPos, m_xOffset + gridPixelW, yPos,
                                                       QPen(QColor(40, 40, 40, 255)));
            item->setZValue(1);
            m_gridItems.append(item);
        }

        if (m_bgItem != NULL)
        {
            m_bgItem->setX(m_xOffset);
            m_bgItem->setY(m_yOffset);
            m_bgItem->setPixmap(m_bgPixmap.scaled(int(gridPixelW), int(gridPixelH)));
        }
    }

    // Stage Geometry stage overlay (metres), drawn behind the grid.
    if (m_stageItem != NULL)
    {
        m_scene->removeItem(m_stageItem);
        delete m_stageItem;
        m_stageItem = NULL;
    }
    if (m_stageW > 0.0 && m_stageH > 0.0 && m_cellPixels > 0.0)
    {
        // Room depth in mm (the grid is the room). The stage offset is in
        // metres; the Stage Geometry world uses y-up, the Monitor y-down, so
        // the stage is mirrored vertically to keep both views consistent.
        double roomDmm = m_gridSize.height() * m_unitValue;
        QPointF tl = realPositionToPixels(m_stageOffX * 1000.0,
                                          roomDmm - m_stageOffY * 1000.0 - m_stageH * 1000.0);
        QPointF br = realPositionToPixels((m_stageOffX + m_stageW) * 1000.0,
                                          roomDmm - m_stageOffY * 1000.0);
        m_stageItem = m_scene->addRect(QRectF(tl, br),
                                       QPen(QColor(91, 141, 239, 220), 2),
                                       QBrush(QColor(91, 141, 239, 40)));
        m_stageItem->setZValue(0.5);
    }
}

void MonitorGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateGrid();
    QHashIterator <quint32, MonitorFixtureItem*> it(m_fixtures);
    while (it.hasNext() == true)
    {
        it.next();
        updateFixture(it.key());
    }
}

void MonitorGraphicsView::mouseReleaseEvent(QMouseEvent *e)
{
    emit viewClicked(e);

    QGraphicsView::mouseReleaseEvent(e);
}

void MonitorGraphicsView::slotFixtureMoved(MonitorFixtureItem *item)
{
    quint32 fid = m_fixtures.key(item);

    // Convert the pixel position of the fixture into
    // position in millimeters
    QPointF mmPos;
    mmPos.setX(((item->x() - m_xOffset) * m_unitValue) / m_cellPixels);
    mmPos.setY(((item->y() - m_yOffset) * m_unitValue) / m_cellPixels);

    // update the fixture item's real position
    item->setRealPosition(mmPos);

    emit fixtureMoved(fid, mmPos);
}
