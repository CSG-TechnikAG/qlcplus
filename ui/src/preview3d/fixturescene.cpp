/*
  Q Light Controller Plus
  fixturescene.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtMath>
#include <QSize>
#include <QTimer>

#include "fixturescene.h"
#include "doc.h"
#include "fixture.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcphysical.h"
#include "stagegeometry.h"

FixtureScene::FixtureScene(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_liveTimer(NULL)
    , m_strobeTimer(NULL)
    , m_live(false)
    , m_strobePhase(true)
{
    m_liveTimer = new QTimer(this);
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(30);   // coalesce bursts to ~30 fps
    connect(m_liveTimer, &QTimer::timeout, this, &FixtureScene::slotLiveTick);

    m_strobeTimer = new QTimer(this);
    m_strobeTimer->setInterval(45); // ~11 Hz strobe
    connect(m_strobeTimer, &QTimer::timeout, this, &FixtureScene::slotStrobeTick);

    if (m_doc != NULL)
    {
        connect(m_doc, &Doc::fixtureAdded, this, &FixtureScene::rebuild);
        connect(m_doc, &Doc::fixtureRemoved, this, &FixtureScene::rebuild);
        connect(m_doc, &Doc::fixtureChanged, this, &FixtureScene::rebuild);
        // While Doc is cleared the fixture list is momentarily stale (fixtures
        // are deleted before fixtureRemoved() is emitted).
        connect(m_doc, &Doc::clearing, this, [this]() {
            m_clearing = true;
            // Drop references to fixtures that are about to be deleted.
            m_liveFixtures.clear();
            m_liveCache.clear();
            m_liveTimer->stop();
            m_strobeTimer->stop();
        });
        connect(m_doc, &Doc::cleared, this, [this]() { m_clearing = false; rebuild(); });
        // Live preview is only meaningful while running.
        connect(m_doc, &Doc::modeChanged, this,
                [this](Doc::Mode) { setLive(m_doc->mode() == Doc::Operate); });
        if (m_doc->monitorProperties() != NULL)
            connect(m_doc->monitorProperties(), &MonitorProperties::fixtureItemChanged,
                    this, [this](quint32) { if (!m_writing && !m_clearing) rebuild(); });

        m_live = (m_doc->mode() == Doc::Operate);
    }
    rebuild();
}

bool FixtureScene::isRenderable(Fixture *fx)
{
    if (fx == NULL)
        return false;

    switch (fx->type())
    {
    case QLCFixtureDef::Dimmer:
    case QLCFixtureDef::ColorChanger:
    case QLCFixtureDef::LEDBarBeams:
    case QLCFixtureDef::LEDBarPixels:
    case QLCFixtureDef::Strobe:
    case QLCFixtureDef::Flower:
    case QLCFixtureDef::MovingHead:
    case QLCFixtureDef::Smoke:
    case QLCFixtureDef::Hazer:
        return true;
    case QLCFixtureDef::Effect:
        // Effects are a broad bucket; only render the derby-like ones.
        return fx->name().contains("derby", Qt::CaseInsensitive)
            || (fx->fixtureDef() != NULL
                && fx->fixtureDef()->model().contains("derby", Qt::CaseInsensitive));
    default:
        return false;
    }
}

int FixtureScene::indexOf(quint32 id) const
{
    for (int i = 0; i < m_fixtures.size(); ++i)
        if (m_fixtures.at(i).id == id)
            return i;
    return -1;
}

FixtureInstance *FixtureScene::instance(quint32 id)
{
    const int i = indexOf(id);
    return (i >= 0) ? &m_fixtures[i] : NULL;
}

void FixtureScene::updateModel(FixtureInstance &fi, Fixture *fx)
{
    switch (fx->type())
    {
    case QLCFixtureDef::Dimmer:
        // A single dimmer channel drives a conventional lantern; a multi-channel
        // device is a dimmer pack.
        fi.model = (fx->channels() <= 1) ? FixtureModel::Lamp : FixtureModel::Dimmer;
        break;
    case QLCFixtureDef::LEDBarBeams:
    case QLCFixtureDef::LEDBarPixels:
        fi.model = FixtureModel::Bar;
        break;
    case QLCFixtureDef::Flower:
    case QLCFixtureDef::Effect:
        fi.model = FixtureModel::Derby;
        break;
    case QLCFixtureDef::MovingHead:
        fi.model = FixtureModel::MovingHead;
        break;
    case QLCFixtureDef::Smoke:
    case QLCFixtureDef::Hazer:
        fi.model = FixtureModel::Smoke;
        break;
    default:
        fi.model = FixtureModel::Par;
        break;
    }

    QLCPhysical ph;
    // The mode's physical is authoritative for an instance (it carries the LED
    // <Layout>); fall back to the definition's global physical.
    if (fx->fixtureMode() != NULL)
        ph = fx->fixtureMode()->physical();
    if (ph.isEmpty() && fx->fixtureDef() != NULL)
        ph = fx->fixtureDef()->physical();

    // Generic dimmers carry synthetic physical data (300 mm per channel), so
    // ignore their dimensions and fall back to the type defaults below.
    const bool genericDimmer = (fx->fixtureDef() != NULL
        && fx->fixtureDef()->manufacturer() == KXMLFixtureGeneric
        && fx->fixtureDef()->model() == KXMLFixtureGeneric);
    if (genericDimmer)
        ph = QLCPhysical();

    if (!ph.isEmpty() && ph.width() > 0 && ph.height() > 0 && ph.depth() > 0)
    {
        fi.size = QVector3D(qMax(0.05, ph.width() / 1000.0),
                            qMax(0.05, ph.depth() / 1000.0),
                            qMax(0.05, ph.height() / 1000.0));
    }
    else
    {
        // Generic fixtures have no physical data: sensible defaults per type.
        switch (fi.model)
        {
        case FixtureModel::Dimmer:
            fi.size = QVector3D(0.42, 0.20, 0.09);   // flat rack pack
            break;
        case FixtureModel::Lamp:
            fi.size = QVector3D(0.30, 0.34, 0.30);   // theatre lantern
            break;
        case FixtureModel::Bar:
            fi.size = QVector3D(1.0, 0.12, 0.12);
            break;
        case FixtureModel::Derby:
            fi.size = QVector3D(0.30, 0.30, 0.20);
            break;
        case FixtureModel::MovingHead:
            fi.size = QVector3D(0.30, 0.30, 0.45);
            break;
        case FixtureModel::Smoke:
            fi.size = QVector3D(0.45, 0.30, 0.30);   // fog machine chassis
            break;
        default:
            fi.size = QVector3D(0.28, 0.30, 0.28);   // par can
            break;
        }
    }

    // LED bar / matrix layout: columns x rows from the physical <Layout>.
    fi.ledCols = 1;
    fi.ledRows = 1;
    if (fi.model == FixtureModel::Bar)
    {
        const QSize layout = ph.layoutSize();
        fi.ledCols = qMax(1, layout.width());
        fi.ledRows = qMax(1, layout.height());
        if (fi.ledCols <= 1 && fi.ledRows <= 1)
        {
            fi.ledCols = 8;     // generic bar with no layout information
            fi.ledRows = 1;
        }
    }

    double lens = 0.0;
    if (ph.lensDegreesMax() > 0.0)
        lens = ph.lensDegreesMax();
    else if (ph.lensDegreesMin() > 0.0)
        lens = ph.lensDegreesMin();
    fi.beamDeg = (lens > 1.0) ? lens : 25.0;
}

void FixtureScene::readFromMonitor(FixtureInstance &fi)
{
    MonitorProperties *mp = (m_doc != NULL) ? m_doc->monitorProperties() : NULL;
    if (mp == NULL)
        return;

    const QVector3D mm = mp->fixturePosition(fi.id, 0, 0);
    const QVector3D rot = mp->fixtureRotation(fi.id, 0, 0);

    StageGeom::Room r(0, 1, 0, 1, 0, 4);
    if (StageGeometry::instance() != NULL)
        r = StageGeometry::instance()->room();

    fi.pos.setX(mm.x() / 1000.0 + r.minX());
    fi.pos.setY(r.maxY() - mm.y() / 1000.0);
    // The Monitor's 2D view is y-down, so its yaw runs the other way.
    fi.yaw = -rot.y();

    const QColor gel = mp->fixtureGelColor(fi.id, 0, 0);
    if (gel.isValid() && gel.lightness() > 8)
        fi.color = gel;
}

void FixtureScene::writePosition(quint32 id, const QVector3D &pos)
{
    MonitorProperties *mp = (m_doc != NULL) ? m_doc->monitorProperties() : NULL;
    if (mp == NULL)
        return;
    m_writing = true;

    StageGeom::Room r(0, 1, 0, 1, 0, 4);
    if (StageGeometry::instance() != NULL)
        r = StageGeometry::instance()->room();

    const double mx = (pos.x() - r.minX()) * 1000.0;
    const double my = (r.maxY() - pos.y()) * 1000.0;
    mp->setFixturePosition(id, 0, 0, QVector3D(mx, my, 0));
    m_writing = false;
}

void FixtureScene::writeRotation(quint32 id, double yaw)
{
    MonitorProperties *mp = (m_doc != NULL) ? m_doc->monitorProperties() : NULL;
    if (mp == NULL)
        return;
    m_writing = true;
    mp->setFixtureRotation(id, 0, 0, QVector3D(0, -yaw, 0));
    m_writing = false;
}

void FixtureScene::rebuild()
{
    if (m_clearing)
        return;

    QList<FixtureInstance> out;
    if (m_doc != NULL)
    {
        for (Fixture *fx : m_doc->fixtures())
        {
            if (!isRenderable(fx))
                continue;

            FixtureInstance fi;
            fi.id = fx->id();
            fi.name = fx->name();
            fi.color = (fx->type() == QLCFixtureDef::Dimmer)
                     ? QColor(235, 225, 195) : QColor(190, 215, 255);
            updateModel(fi, fx);
            readFromMonitor(fi);

            const Extras ex = m_extras.value(fi.id, Extras());
            fi.pos.setZ(ex.height);
            fi.pitch = ex.pitch;
            // "Visible" mirrors what is placed in the Fixture Monitor's 2D view.
            MonitorProperties *mp = (m_doc != NULL) ? m_doc->monitorProperties() : NULL;
            fi.visible = (mp != NULL) ? mp->containsFixture(fi.id) : true;

            out << fi;
        }
    }
    m_fixtures = out;

    // Re-arm live value tracking for the new fixture set.
    for (Fixture *fx : m_liveFixtures)
        disconnect(fx, nullptr, this, nullptr);
    m_liveFixtures.clear();
    m_liveCache.clear();
    if (m_doc != NULL)
    {
        for (Fixture *fx : m_doc->fixtures())
        {
            if (!isRenderable(fx))
                continue;
            connect(fx, &Fixture::valuesChanged, this,
                    [this]() { if (m_live && !m_liveTimer->isActive()) m_liveTimer->start(); });
            m_liveFixtures << fx;
            buildLiveCache(fx->id(), fx);
        }
    }

    if (m_live)
        updateLive();

    emit changed();
}

void FixtureScene::clear()
{
    m_extras.clear();
    rebuild();
}

void FixtureScene::beginLoad()
{
    // A project load replaces the whole 3D state; drop any extras left over
    // from a previously open project so missing entries fall back to defaults.
    m_extras.clear();
}

void FixtureScene::buildLiveCache(quint32 id, Fixture *fx)
{
    LiveCache lc;
    if (fx == NULL)
        return;

    if (fx->heads() > 0)
    {
        const QLCFixtureHead head = fx->head(0);

        const quint32 dim = head.channelNumber(QLCChannel::Intensity, QLCChannel::MSB);
        if (dim != QLCChannel::invalid())
            lc.dimmer = int(dim);

        const quint32 pan = head.channelNumber(QLCChannel::Pan, QLCChannel::MSB);
        if (pan != QLCChannel::invalid())
        {
            lc.pan = int(pan);
            QLCFixtureMode *mode = fx->fixtureMode();
            if (mode != NULL && mode->physical().focusPanMax() != 0)
                lc.panMax = mode->physical().focusPanMax();
        }

        const quint32 tilt = head.channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
        if (tilt != QLCChannel::invalid())
        {
            lc.tilt = int(tilt);
            QLCFixtureMode *mode = fx->fixtureMode();
            if (mode != NULL && mode->physical().focusTiltMax() != 0)
                lc.tiltMax = mode->physical().focusTiltMax();
        }

        lc.rgb = head.rgbChannels();
        lc.cmy = head.cmyChannels();
        lc.shutter = head.shutterChannels();
    }

    const quint32 master = fx->masterIntensityChannel();
    if (master != QLCChannel::invalid())
        lc.master = int(master);

    m_liveCache[id] = lc;
}

static bool fixtureIsStrobing(Fixture *fx, const QByteArray &values,
                              const QVector<quint32> &shutters)
{
    QLCFixtureMode *mode = (fx != NULL) ? fx->fixtureMode() : NULL;
    for (quint32 s : shutters)
    {
        if (int(s) >= values.size())
            continue;
        const uchar val = uchar(values.at(s));
        QLCChannel *ch = (mode != NULL) ? mode->channel(s) : NULL;
        if (ch == NULL)
            continue;

        if (ch->preset() == QLCChannel::ShutterStrobeSlowFast ||
            ch->preset() == QLCChannel::ShutterStrobeFastSlow)
        {
            if (val > 0)
                return true;
            continue;
        }

        QLCCapability *cap = ch->searchCapability(val);
        if (cap != NULL)
        {
            switch (cap->preset())
            {
            case QLCCapability::StrobeSlowToFast:
            case QLCCapability::StrobeFastToSlow:
            case QLCCapability::StrobeRandom:
            case QLCCapability::StrobeRandomSlowToFast:
            case QLCCapability::StrobeRandomFastToSlow:
            case QLCCapability::StrobeFrequency:
            case QLCCapability::StrobeFreqRange:
                return true;
            default:
                break;
            }
        }
    }
    return false;
}

void FixtureScene::updateLive()
{
    bool anyStrobe = false;

    for (FixtureInstance &fi : m_fixtures)
    {
        Fixture *fx = (m_doc != NULL) ? m_doc->fixture(fi.id) : NULL;
        if (!m_live || fx == NULL)
        {
            fi.live = false;
            fi.liveHeads.clear();
            fi.liveHeadIntensity.clear();
            continue;
        }

        const LiveCache lc = m_liveCache.value(fi.id);
        const QByteArray v = fx->channelValues();

        // Colour from the first head (per-head for bars below).
        QColor col(255, 255, 255);
        bool hasCol = false;
        if (lc.rgb.size() >= 3 && int(lc.rgb.at(2)) < v.size())
        {
            col = QColor(uchar(v.at(lc.rgb.at(0))), uchar(v.at(lc.rgb.at(1))),
                         uchar(v.at(lc.rgb.at(2))));
            hasCol = true;
        }
        else if (lc.cmy.size() >= 3 && int(lc.cmy.at(2)) < v.size())
        {
            col = QColor::fromCmyk(uchar(v.at(lc.cmy.at(0))), uchar(v.at(lc.cmy.at(1))),
                                   uchar(v.at(lc.cmy.at(2))), 0);
            hasCol = true;
        }

        // Intensity from dimmer * master.
        double inten = 1.0;
        if (lc.master >= 0 && lc.master < v.size())
            inten *= uchar(v.at(lc.master)) / 255.0;
        if (lc.dimmer >= 0 && lc.dimmer < v.size())
            inten *= uchar(v.at(lc.dimmer)) / 255.0;

        // Pan / tilt (absolute, centred).
        double pan = 0.0, tilt = 0.0;
        if (lc.pan >= 0 && lc.pan < v.size())
            pan = double(uchar(v.at(lc.pan))) * lc.panMax / 255.0 - lc.panMax * 0.5;
        if (lc.tilt >= 0 && lc.tilt < v.size())
            tilt = double(uchar(v.at(lc.tilt))) * lc.tiltMax / 255.0 - lc.tiltMax * 0.5;

        const bool strobing = fixtureIsStrobing(fx, v, lc.shutter);
        if (strobing)
        {
            anyStrobe = true;
            if (!m_strobePhase)
                inten = 0.0;
        }

        fi.live = true;
        fi.liveColor = hasCol ? col : fi.color;
        fi.liveIntensity = inten;
        fi.livePan = pan;
        fi.liveTilt = tilt;

        // Per-head colour/intensity (LED bars and matrices).
        fi.liveHeads.clear();
        fi.liveHeadIntensity.clear();
        if (fx->heads() > 1)
        {
            for (int h = 0; h < fx->heads(); ++h)
            {
                const QLCFixtureHead head = fx->head(h);
                const QVector<quint32> rgb = head.rgbChannels();
                QColor hc = col;
                if (rgb.size() >= 3 && int(rgb.at(2)) < v.size())
                    hc = QColor(uchar(v.at(rgb.at(0))), uchar(v.at(rgb.at(1))),
                                uchar(v.at(rgb.at(2))));
                double hi = inten;
                const quint32 hdim = head.channelNumber(QLCChannel::Intensity, QLCChannel::MSB);
                if (hdim != QLCChannel::invalid() && int(hdim) < v.size())
                    hi = uchar(v.at(hdim)) / 255.0;
                fi.liveHeads << hc;
                fi.liveHeadIntensity << hi;
            }
        }
    }

    if (anyStrobe && !m_strobeTimer->isActive())
    {
        m_strobePhase = true;
        m_strobeTimer->start();
    }
    else if (!anyStrobe && m_strobeTimer->isActive())
    {
        m_strobeTimer->stop();
    }

    emit liveChanged();
}

void FixtureScene::setLive(bool on)
{
    if (m_live == on)
        return;
    m_live = on;
    m_strobePhase = true;
    if (!on)
    {
        m_strobeTimer->stop();
        m_liveTimer->stop();
    }
    updateLive();
}

void FixtureScene::slotLiveTick()
{
    if (m_live)
        updateLive();
}

void FixtureScene::slotStrobeTick()
{
    m_strobePhase = !m_strobePhase;
    if (m_live)
        updateLive();
}

void FixtureScene::setPosition(quint32 id, const QVector3D &pos)
{
    FixtureInstance *fi = instance(id);
    if (fi == NULL)
        return;

    const quint32 fid = fi->id;
    fi->pos = pos;
    m_extras[id].height = pos.z();
    writePosition(fid, pos);
    emit changed();
}

void FixtureScene::setOrientation(quint32 id, double yaw, double pitch)
{
    FixtureInstance *fi = instance(id);
    if (fi == NULL)
        return;

    const quint32 fid = fi->id;
    fi->yaw = yaw;
    fi->pitch = pitch;
    m_extras[id].pitch = pitch;
    writeRotation(fid, yaw);
    emit changed();
}

void FixtureScene::setVisible(quint32 id, bool visible)
{
    MonitorProperties *mp = (m_doc != NULL) ? m_doc->monitorProperties() : NULL;
    if (mp == NULL)
        return;

    FixtureInstance *fi = instance(id);
    if (fi == NULL)
        return;

    fi->visible = visible;

    if (visible)
    {
        // Placing it makes it appear in the Fixture Monitor's 2D view too.
        StageGeom::Room r(0, 1, 0, 1, 0, 4);
        if (StageGeometry::instance() != NULL)
            r = StageGeometry::instance()->room();
        const QVector3D p = fi->pos;
        writePosition(id, p);
        mp->setFixtureFlags(id, 0, 0, 0);
    }
    else
    {
        // Removing it from the Monitor hides it here as well.
        mp->removeFixture(id);
    }
}

bool FixtureScene::saveXML(QXmlStreamWriter *doc) const
{
    if (m_extras.isEmpty())
        return true;

    doc->writeStartElement(KXMLQLCPreview3DFixtures);
    for (auto it = m_extras.constBegin(); it != m_extras.constEnd(); ++it)
    {
        doc->writeStartElement(KXMLQLCPreview3DFixture);
        doc->writeAttribute("ID", QString::number(it.key()));
        doc->writeAttribute("Height", QString::number(it.value().height, 'g', 8));
        doc->writeAttribute("Pitch", QString::number(it.value().pitch, 'g', 8));
        doc->writeAttribute("Visible", it.value().visible ? "true" : "false");
        doc->writeEndElement();
    }
    doc->writeEndElement(); // Fixtures
    return true;
}

bool FixtureScene::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCPreview3DFixtures)
    {
        root.skipCurrentElement();
        return false;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCPreview3DFixture)
        {
            const QXmlStreamAttributes a = root.attributes();
            const quint32 id = a.value("ID").toUInt();
            Extras ex;
            ex.height = a.hasAttribute("Height") ? a.value("Height").toDouble() : 0.0;
            ex.pitch = a.hasAttribute("Pitch") ? a.value("Pitch").toDouble() : 0.0;
            ex.visible = (a.value("Visible") != "false");
            // id 0 is a valid fixture id, so test for the attribute.
            if (a.hasAttribute("ID"))
                m_extras[id] = ex;
        }
        root.skipCurrentElement();
    }

    rebuild();
    return true;
}
