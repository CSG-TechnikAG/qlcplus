/*
  Q Light Controller Plus
  stagegeometryengine.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "stagegeometryengine.h"

#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#include <QtMath>

namespace
{

struct Direction
{
    const char *name;
    double angle;
};

const Direction PAN_DIRS[] = {
    { "rechts", 0.0 },
    { "vorne", M_PI / 2.0 },
    { "links", M_PI },
    { "hinten", -M_PI / 2.0 }
};

const Direction TILT_DIRS[] = {
    { "unten", 0.0 },
    { "vorne", M_PI / 2.0 },
    { "oben", M_PI },
    { "hinten", 3.0 * M_PI / 2.0 }
};

const int PAN_DIR_COUNT = int(sizeof(PAN_DIRS) / sizeof(PAN_DIRS[0]));
const int TILT_DIR_COUNT = int(sizeof(TILT_DIRS) / sizeof(TILT_DIRS[0]));

double directionAngle(const Direction *dirs, int count, const QString &name)
{
    for (int i = 0; i < count; i++)
        if (name == QLatin1String(dirs[i].name))
            return dirs[i].angle;
    return 0.0;
}

QStringList directionNames(const Direction *dirs, int count)
{
    QStringList names;
    for (int i = 0; i < count; i++)
        names << QString::fromLatin1(dirs[i].name);
    return names;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// StageGeometryFixture
// ---------------------------------------------------------------------------

StageGeometryFixture::StageGeometryFixture(quint32 id)
    : id(id)
    , name(QString())
    , universe(0)
    , baseAddress(0)
    , panChannel(-1)
    , tiltChannel(-1)
    , dimmerChannel(-1)
    , calibrated(false)
    , fixture()
    , hasFront(false)
    , frontPan(0)
    , frontTilt(0)
    , lastPan(-1)
    , lastTilt(-1)
{
    for (const QString &n : StageGeometryEngine::panDirectionNames())
        panRefs[n] = QList<int>();
    for (const QString &n : StageGeometryEngine::tiltDirectionNames())
        tiltRefs[n] = QList<int>();
}

StageGeom::Measurement StageGeometryFixture::measurement() const
{
    StageGeom::Measurement m;

    for (auto it = pointDmx.constBegin(); it != pointDmx.constEnd(); ++it)
        m.points.insert(it.key(), it.value());

    for (auto it = panRefs.constBegin(); it != panRefs.constEnd(); ++it)
        if (!it.value().isEmpty())
            m.panReferences.append(QPair<double, QList<int> >(
                StageGeometryEngine::panDirectionAngle(it.key()), it.value()));

    for (auto it = tiltRefs.constBegin(); it != tiltRefs.constEnd(); ++it)
        if (!it.value().isEmpty())
            m.tiltReferences.append(QPair<double, QList<int> >(
                StageGeometryEngine::tiltDirectionAngle(it.key()), it.value()));

    return m;
}

void StageGeometryFixture::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLQLCStageGeometryFixture);
    doc->writeAttribute("QLCFixture", QString::number(id));

    if (calibrated)
    {
        doc->writeAttribute("Calibrated", "true");
        doc->writeAttribute("X", QString::number(fixture.x(), 'f', 6));
        doc->writeAttribute("Y", QString::number(fixture.y(), 'f', 6));
        doc->writeAttribute("Z", QString::number(fixture.z(), 'f', 6));
        doc->writeAttribute("PanK", QString::number(fixture.pan().k(), 'g', 12));
        doc->writeAttribute("PanT", QString::number(fixture.pan().t(), 'g', 12));
        doc->writeAttribute("TiltK", QString::number(fixture.tilt().k(), 'g', 12));
        doc->writeAttribute("TiltT", QString::number(fixture.tilt().t(), 'g', 12));
    }

    if (hasFront)
    {
        doc->writeStartElement("Front");
        doc->writeAttribute("Pan", QString::number(frontPan));
        doc->writeAttribute("Tilt", QString::number(frontTilt));
        doc->writeEndElement();
    }

    doc->writeStartElement("Points");
    for (auto it = pointDmx.constBegin(); it != pointDmx.constEnd(); ++it)
    {
        doc->writeStartElement("Point");
        doc->writeAttribute("X", QString::number(it.key().x(), 'g', 8));
        doc->writeAttribute("Y", QString::number(it.key().y(), 'g', 8));
        doc->writeAttribute("Pan", QString::number(it.value().first));
        doc->writeAttribute("Tilt", QString::number(it.value().second));
        doc->writeEndElement();
    }
    doc->writeEndElement(); // Points

    for (int axis = 0; axis < 2; axis++)
    {
        const QMap<QString, QList<int> > &store = (axis == 0) ? panRefs : tiltRefs;
        doc->writeStartElement(axis == 0 ? "PanRefs" : "TiltRefs");
        for (auto it = store.constBegin(); it != store.constEnd(); ++it)
        {
            doc->writeStartElement("Ref");
            doc->writeAttribute("Name", it.key());
            for (int v : it.value())
                doc->writeTextElement("Value", QString::number(v));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }

    doc->writeStartElement("Confirmed");
    for (const QString &c : confirmed)
        doc->writeTextElement("Item", c);
    doc->writeEndElement();

    doc->writeEndElement(); // Fixture
}

void StageGeometryFixture::loadXML(QXmlStreamReader &root)
{
    if (root.attributes().value("Calibrated") == "true")
    {
        double x = root.attributes().value("X").toDouble();
        double y = root.attributes().value("Y").toDouble();
        double z = root.attributes().value("Z").toDouble();
        StageGeom::AxisCalibration pan(root.attributes().value("PanK").toDouble(),
                                       root.attributes().value("PanT").toDouble());
        StageGeom::AxisCalibration tilt(root.attributes().value("TiltK").toDouble(),
                                        root.attributes().value("TiltT").toDouble());
        fixture = StageGeom::Fixture(x, y, z, pan, tilt);
        calibrated = true;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == "Front")
        {
            hasFront = true;
            frontPan = root.attributes().value("Pan").toInt();
            frontTilt = root.attributes().value("Tilt").toInt();
            root.skipCurrentElement();
        }
        else if (root.name() == "Points")
        {
            while (root.readNextStartElement())
            {
                if (root.name() == "Point")
                {
                    QPointF p(root.attributes().value("X").toDouble(),
                              root.attributes().value("Y").toDouble());
                    pointDmx[p] = QPair<int, int>(root.attributes().value("Pan").toInt(),
                                                  root.attributes().value("Tilt").toInt());
                }
                root.skipCurrentElement();
            }
        }
        else if (root.name() == "PanRefs" || root.name() == "TiltRefs")
        {
            bool isPan = (root.name() == "PanRefs");
            while (root.readNextStartElement())
            {
                if (root.name() == "Ref")
                {
                    QString name = root.attributes().value("Name").toString();
                    QList<int> values;
                    while (root.readNextStartElement())
                    {
                        if (root.name() == "Value")
                            values << root.readElementText().toInt();
                        else
                            root.skipCurrentElement();
                    }
                    if (isPan)
                        panRefs[name] = values;
                    else
                        tiltRefs[name] = values;
                }
                else
                    root.skipCurrentElement();
            }
        }
        else if (root.name() == "Confirmed")
        {
            while (root.readNextStartElement())
            {
                if (root.name() == "Item")
                    confirmed.insert(root.readElementText());
                else
                    root.skipCurrentElement();
            }
        }
        else
            root.skipCurrentElement();
    }
}

// ---------------------------------------------------------------------------
// StageGeometryEngine
// ---------------------------------------------------------------------------

StageGeometryEngine::StageGeometryEngine(Doc *doc)
    : m_doc(doc)
    , m_clearing(false)
    , m_sx(4.0)
    , m_sy(3.0)
    , m_stageOffX(0.5)
    , m_stageOffY(1.5)
    , m_stageHeight(0.5)
    , m_roomHeight(4.0)
    , m_live(false)
    , m_target(0, 0, 0)
    , m_liveUniverse(0)
    , m_liveXCh(1)
    , m_liveYCh(2)
    , m_liveZCh(3)
    , m_liveAll(true)
{
    if (m_doc != NULL)
    {
        connect(m_doc, &Doc::fixtureAdded, this, &StageGeometryEngine::slotFixturesChanged);
        connect(m_doc, &Doc::fixtureRemoved, this, &StageGeometryEngine::slotFixturesChanged);
        connect(m_doc, &Doc::fixtureChanged, this, &StageGeometryEngine::slotFixturesChanged);
        // While Doc is being cleared the fixture list is momentarily stale
        // (fixtures are deleted before fixtureRemoved() is emitted), so don't
        // touch it until the dust settles.
        connect(m_doc, &Doc::clearing, this, [this]() { m_clearing = true; });
        connect(m_doc, &Doc::cleared, this, [this]() { m_clearing = false; rebuildFixtures(); });
        connect(m_doc->inputOutputMap(), &InputOutputMap::inputValueChanged,
                this, &StageGeometryEngine::slotInputValueChanged);
    }
    rebuildFixtures();
}

StageGeometryEngine::~StageGeometryEngine()
{
    qDeleteAll(m_fixtures);
    m_fixtures.clear();
}

double StageGeometryEngine::stageWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_sx;
}

double StageGeometryEngine::stageDepth() const
{
    QMutexLocker locker(&m_mutex);
    return m_sy;
}

double StageGeometryEngine::roomHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_roomHeight;
}

double StageGeometryEngine::stageOffsetX() const
{
    QMutexLocker locker(&m_mutex);
    return m_stageOffX;
}

double StageGeometryEngine::stageOffsetY() const
{
    QMutexLocker locker(&m_mutex);
    return m_stageOffY;
}

double StageGeometryEngine::stageHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_stageHeight;
}

StageGeom::Stage StageGeometryEngine::stage() const
{
    QMutexLocker locker(&m_mutex);
    return StageGeom::Stage(m_sx, m_sy);
}

StageGeom::Room StageGeometryEngine::room() const
{
    QMutexLocker locker(&m_mutex);
    return computeRoom();
}

StageGeom::Room StageGeometryEngine::computeRoom() const
{
    // Room x/y extent comes from the Fixture Monitor 2D grid; the stage is
    // placed inside it by the offset; the height is the room height.
    double w = 5.0;
    double d = 5.0;
    if (m_doc != NULL)
    {
        MonitorProperties *mp = m_doc->monitorProperties();
        if (mp != NULL)
        {
            QVector3D gs = mp->gridSize();
            double unit = (mp->gridUnits() == MonitorProperties::Feet) ? 0.3048 : 1.0;
            if (gs.x() > 0.0)
                w = gs.x() * unit;
            if (gs.z() > 0.0)
                d = gs.z() * unit;
        }
    }
    // The stage origin (0,0,0) is at (offX, offY) inside the room and
    // stageHeight above the room floor, so the room is shifted accordingly.
    return StageGeom::Room(-m_stageOffX, w - m_stageOffX,
                           -m_stageOffY, d - m_stageOffY,
                           -m_stageHeight, m_roomHeight);
}

void StageGeometryEngine::setStage(double sx, double sy)
{
    if (sx <= 0.0 || sy <= 0.0)
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (qFuzzyCompare(m_sx, sx) && qFuzzyCompare(m_sy, sy))
            return;
        m_sx = sx;
        m_sy = sy;
    }
    emit geometryChanged();
}

void StageGeometryEngine::setStageOffset(double ox, double oy)
{
    {
        QMutexLocker locker(&m_mutex);
        if (qFuzzyCompare(m_stageOffX, ox) && qFuzzyCompare(m_stageOffY, oy))
            return;
        m_stageOffX = ox;
        m_stageOffY = oy;
    }
    emit geometryChanged();
}

void StageGeometryEngine::setStageHeight(double h)
{
    {
        QMutexLocker locker(&m_mutex);
        if (qFuzzyCompare(m_stageHeight, h))
            return;
        m_stageHeight = h;
    }
    emit geometryChanged();
}

void StageGeometryEngine::setRoomHeight(double z1)
{
    if (z1 <= 0.0)
        return;
    {
        QMutexLocker locker(&m_mutex);
        if (qFuzzyCompare(m_roomHeight, z1))
            return;
        m_roomHeight = z1;
    }
    emit geometryChanged();
}

int StageGeometryEngine::fixtureCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_order.size();
}

StageGeometryFixture *StageGeometryEngine::fixtureAt(int index) const
{
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_order.size())
        return NULL;
    return m_fixtures.value(m_order.at(index), NULL);
}

StageGeometryFixture *StageGeometryEngine::fixtureById(quint32 id) const
{
    QMutexLocker locker(&m_mutex);
    return m_fixtures.value(id, NULL);
}

QPair<int, int> StageGeometryEngine::choosePair(StageGeometryFixture *fixture,
                                                double x, double y, double z) const
{
    if (fixture == NULL || !fixture->calibrated)
        return QPair<int, int>(-1, -1);

    QList<QPair<int, int> > pairs = fixture->fixture.aim(x, y, z);
    QPair<int, int> chosen = StageGeom::selectPair(
        pairs, QPair<int, int>(fixture->lastPan, fixture->lastTilt));
    if (chosen.first >= 0)
    {
        fixture->lastPan = chosen.first;
        fixture->lastTilt = chosen.second;
    }
    return chosen;
}

bool StageGeometryEngine::isMovingHead(Fixture *fixture)
{
    if (fixture == NULL)
        return false;
    return findChannel(fixture, QLCChannel::PositionPan) >= 0 &&
           findChannel(fixture, QLCChannel::PositionTilt) >= 0;
}

int StageGeometryEngine::findChannel(Fixture *fixture, int preset, int finePreset)
{
    if (fixture == NULL)
        return -1;
    int fine = -1;
    for (quint32 c = 0; c < fixture->channels(); c++)
    {
        const QLCChannel *ch = fixture->channel(c);
        if (ch == NULL)
            continue;
        if (int(ch->preset()) == preset)
            return int(c);
        if (fine < 0 && finePreset != 0 && int(ch->preset()) == finePreset)
            fine = int(c);
    }
    return fine;
}

void StageGeometryEngine::slotFixturesChanged()
{
    if (m_clearing)
        return;
    rebuildFixtures();
}

void StageGeometryEngine::rebuildFixtures()
{
    if (m_doc == NULL)
        return;

    QList<quint32> order;
    QSet<quint32> seen;

    for (Fixture *fx : m_doc->fixtures())
    {
        if (!isMovingHead(fx))
            continue;
        quint32 id = fx->id();
        StageGeometryFixture *f = m_fixtures.value(id, NULL);
        if (f == NULL)
        {
            f = new StageGeometryFixture(id);
            m_fixtures[id] = f;
        }
        updateFixtureInfo(f, fx);
        order << id;
        seen << id;
    }

    for (auto it = m_fixtures.begin(); it != m_fixtures.end(); )
    {
        if (!seen.contains(it.key()))
        {
            delete it.value();
            it = m_fixtures.erase(it);
        }
        else
            ++it;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_order = order;
    }
    emit fixturesChanged();
}

void StageGeometryEngine::updateFixtureInfo(StageGeometryFixture *f, Fixture *fx)
{
    f->name = fx->name();
    f->universe = int(fx->universe());
    f->baseAddress = int(fx->address());
    f->panChannel = findChannel(fx, QLCChannel::PositionPan);
    f->tiltChannel = findChannel(fx, QLCChannel::PositionTilt);
    f->dimmerChannel = findChannel(fx, QLCChannel::IntensityMasterDimmer,
                                   QLCChannel::IntensityDimmer);
}

void StageGeometryEngine::setLive(bool running)
{
    QMutexLocker locker(&m_mutex);
    m_live = running;
}

bool StageGeometryEngine::isLive() const
{
    QMutexLocker locker(&m_mutex);
    return m_live;
}

void StageGeometryEngine::setLiveConfig(int universe, int xCh, int yCh, int zCh, bool all)
{
    QMutexLocker locker(&m_mutex);
    m_liveUniverse = universe;
    m_liveXCh = xCh;
    m_liveYCh = yCh;
    m_liveZCh = zCh;
    m_liveAll = all;
}

int StageGeometryEngine::liveUniverse() const { QMutexLocker l(&m_mutex); return m_liveUniverse; }
int StageGeometryEngine::liveXChannel() const { QMutexLocker l(&m_mutex); return m_liveXCh; }
int StageGeometryEngine::liveYChannel() const { QMutexLocker l(&m_mutex); return m_liveYCh; }
int StageGeometryEngine::liveZChannel() const { QMutexLocker l(&m_mutex); return m_liveZCh; }
bool StageGeometryEngine::liveAllFixtures() const { QMutexLocker l(&m_mutex); return m_liveAll; }

void StageGeometryEngine::setTarget(const QVector3D &target)
{
    QMutexLocker locker(&m_mutex);
    m_target = target;
}

QVector3D StageGeometryEngine::target() const
{
    QMutexLocker locker(&m_mutex);
    return m_target;
}

bool StageGeometryEngine::liveTargetFromInput(QVector3D &target) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_input.contains(m_liveUniverse))
        return false;
    const QByteArray data = m_input.value(m_liveUniverse);
    int xi = m_liveXCh - 1, yi = m_liveYCh - 1, zi = m_liveZCh - 1;
    if (xi < 0 || yi < 0 || zi < 0 || xi >= data.size() || yi >= data.size() || zi >= data.size())
        return false;

    auto lerp = [](int v, double lo, double hi) {
        return lo + (double(v) / 255.0) * (hi - lo);
    };
    StageGeom::Room room = computeRoom();
    target = QVector3D(lerp(uchar(data.at(xi)), room.minX(), room.maxX()),
                       lerp(uchar(data.at(yi)), room.minY(), room.maxY()),
                       lerp(uchar(data.at(zi)), 0.0, room.maxZ()));
    return true;
}

int StageGeometryEngine::inputValue(int universe, int channel) const
{
    QMutexLocker locker(&m_mutex);
    if (channel < 0 || !m_input.contains(universe))
        return -1;
    const QByteArray data = m_input.value(universe);
    if (channel >= data.size())
        return -1;
    return uchar(data.at(channel));
}

void StageGeometryEngine::slotInputValueChanged(quint32 universe, quint32 channel, uchar value,
                                                const QString &key)
{
    Q_UNUSED(key)
    QMutexLocker locker(&m_mutex);
    QByteArray &data = m_input[universe];
    if (data.size() < 512)
        data.resize(512);
    if (channel < 512)
        data[channel] = char(value);
}

QStringList StageGeometryEngine::panDirectionNames()
{
    return directionNames(PAN_DIRS, PAN_DIR_COUNT);
}

double StageGeometryEngine::panDirectionAngle(const QString &name)
{
    return directionAngle(PAN_DIRS, PAN_DIR_COUNT, name);
}

QStringList StageGeometryEngine::tiltDirectionNames()
{
    return directionNames(TILT_DIRS, TILT_DIR_COUNT);
}

double StageGeometryEngine::tiltDirectionAngle(const QString &name)
{
    return directionAngle(TILT_DIRS, TILT_DIR_COUNT, name);
}

bool StageGeometryEngine::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCStageGeometry)
    {
        root.skipCurrentElement();
        return false;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCStageGeometryStage)
        {
            double sx = root.attributes().value("SX").toDouble();
            double sy = root.attributes().value("SY").toDouble();
            double ox = root.attributes().value("OX").toDouble();
            double oy = root.attributes().value("OY").toDouble();
            double sh = root.attributes().value("SH").toDouble();
            QMutexLocker locker(&m_mutex);
            if (sx > 0 && sy > 0)
            {
                m_sx = sx;
                m_sy = sy;
            }
            m_stageOffX = ox;
            m_stageOffY = oy;
            if (root.attributes().hasAttribute("SH"))
                m_stageHeight = sh;
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCStageGeometryRoom)
        {
            // x/y come from the Fixture Monitor; only the height is persisted.
            double z1 = root.attributes().value("Z1").toDouble();
            if (z1 > 0)
            {
                QMutexLocker locker(&m_mutex);
                m_roomHeight = z1;
            }
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCStageGeometryLive)
        {
            m_liveUniverse = root.attributes().value("Universe").toInt();
            m_liveXCh = root.attributes().value("XCh").toInt();
            m_liveYCh = root.attributes().value("YCh").toInt();
            m_liveZCh = root.attributes().value("ZCh").toInt();
            m_liveAll = (root.attributes().value("All") != "false");
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCStageGeometryFixtures)
        {
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCStageGeometryFixture)
                {
                    quint32 id = root.attributes().value("QLCFixture").toUInt();
                    StageGeometryFixture *f = m_fixtures.value(id, NULL);
                    if (f != NULL)
                        f->loadXML(root);
                    else
                        root.skipCurrentElement();
                }
                else
                    root.skipCurrentElement();
            }
        }
        else
            root.skipCurrentElement();
    }

    emit geometryChanged();
    emit fixturesChanged();
    return true;
}

bool StageGeometryEngine::saveXML(QXmlStreamWriter *doc) const
{
    QMutexLocker locker(&m_mutex);

    doc->writeStartElement(KXMLQLCStageGeometry);

    doc->writeStartElement(KXMLQLCStageGeometryStage);
    doc->writeAttribute("SX", QString::number(m_sx, 'g', 8));
    doc->writeAttribute("SY", QString::number(m_sy, 'g', 8));
    doc->writeAttribute("OX", QString::number(m_stageOffX, 'g', 8));
    doc->writeAttribute("OY", QString::number(m_stageOffY, 'g', 8));
    doc->writeAttribute("SH", QString::number(m_stageHeight, 'g', 8));
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCStageGeometryRoom);
    doc->writeAttribute("Z1", QString::number(m_roomHeight, 'g', 8));
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCStageGeometryLive);
    doc->writeAttribute("Universe", QString::number(m_liveUniverse));
    doc->writeAttribute("XCh", QString::number(m_liveXCh));
    doc->writeAttribute("YCh", QString::number(m_liveYCh));
    doc->writeAttribute("ZCh", QString::number(m_liveZCh));
    doc->writeAttribute("All", m_liveAll ? "true" : "false");
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCStageGeometryFixtures);
    for (quint32 id : m_order)
    {
        StageGeometryFixture *f = m_fixtures.value(id, NULL);
        if (f != NULL)
            f->saveXML(doc);
    }
    doc->writeEndElement();

    doc->writeEndElement();

    return true;
}
