/*
  Q Light Controller Plus
  pathfunction.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtMath>

#include "pathfunction.h"
#include "mastertimer.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

namespace
{

// ---- small DMX / geometry helpers (mirror of the Stage Geometry math) ----

QList<int> angleToDmxCandidates(double angle, double k, double t)
{
    QList<int> out;
    if (k == 0.0)
        return out;

    double nA = (angle - t) / (2.0 * M_PI);
    double nB = (angle - t - k * 255.0) / (2.0 * M_PI);
    int nMin = int(qCeil(qMin(nA, nB)));
    int nMax = int(qFloor(qMax(nA, nB)));
    for (int n = nMin; n <= nMax; n++)
    {
        double value = (angle - t - 2.0 * M_PI * n) / k;
        int dmx = int(qRound(value));
        if (dmx < 0 || dmx > 255)
            continue;
        if (qAbs(k * dmx + t + 2.0 * M_PI * n - angle) <= qAbs(k) / 2.0 + 1e-9
                && !out.contains(dmx))
            out.append(dmx);
    }
    std::sort(out.begin(), out.end());
    return out;
}

QPair<int, int> selectPair(const QList<QPair<int, int> > &pairs, int lastPan, int lastTilt)
{
    if (pairs.isEmpty())
        return QPair<int, int>(-1, -1);
    if (lastPan < 0 || lastTilt < 0)
        return pairs.first();
    QPair<int, int> best = pairs.first();
    int bestCost = qAbs(best.first - lastPan) + qAbs(best.second - lastTilt);
    for (const QPair<int, int> &p : pairs)
    {
        int cost = qAbs(p.first - lastPan) + qAbs(p.second - lastTilt);
        if (cost < bestCost)
        {
            bestCost = cost;
            best = p;
        }
    }
    return best;
}

} // anonymous namespace

PathFunction::PathFunction(Doc* doc)
    : Function(doc, Function::PathType)
    , m_pathZ(0.0)
    , m_mode(Repeat)
    , m_easing(Linear)
    , m_manualPos(0.0)
    , m_inverted(false)
{
    setName(tr("New Path"));

    registerAttribute(tr("Intensity"), Multiply, 0, 1, 1);
    registerAttribute(tr("Position"), LastWins, 0, 1, 0);

    setDuration(5000);
}

PathFunction::~PathFunction()
{
}

QIcon PathFunction::getIcon() const
{
    return QIcon(":/path.png");
}

/*********************************************************************
 * Copying
 *********************************************************************/

Function* PathFunction::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    PathFunction* copy = new PathFunction(doc);
    if (copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    else if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool PathFunction::copyFrom(const Function* function)
{
    const PathFunction* pf = qobject_cast<const PathFunction*>(function);
    if (pf == NULL)
        return false;

    m_path = pf->m_path;
    m_pathZ = pf->m_pathZ;
    m_fixtures = pf->m_fixtures;
    m_mode = pf->m_mode;
    m_easing = pf->m_easing;
    m_manualPos = pf->m_manualPos;
    m_inverted = pf->m_inverted;

    return Function::copyFrom(function);
}

/*********************************************************************
 * Path
 *********************************************************************/

QPolygonF PathFunction::path() const
{
    return m_path;
}

void PathFunction::setPath(const QPolygonF& path)
{
    m_path = path;
    emit changed(id());
}

double PathFunction::pathZ() const
{
    return m_pathZ;
}

void PathFunction::setPathZ(double z)
{
    m_pathZ = z;
    emit changed(id());
}

/*********************************************************************
 * Fixtures
 *********************************************************************/

QList<PathFunction::PathFixture> PathFunction::fixtures() const
{
    return m_fixtures;
}

void PathFunction::setFixtures(const QList<PathFixture>& fixtures)
{
    m_fixtures = fixtures;
    emit changed(id());
}

void PathFunction::clearFixtures()
{
    m_fixtures.clear();
    emit changed(id());
}

QList<quint32> PathFunction::components() const
{
    QList<quint32> ids;
    for (const PathFixture& f : m_fixtures)
        ids << f.id;
    return ids;
}

void PathFunction::slotFixtureRemoved(quint32 fxi_id)
{
    for (int i = m_fixtures.size() - 1; i >= 0; i--)
        if (m_fixtures.at(i).id == fxi_id)
            m_fixtures.removeAt(i);
    Function::slotFixtureRemoved(fxi_id);
}

/*********************************************************************
 * Playback
 *********************************************************************/

PathFunction::PathMode PathFunction::pathMode() const
{
    return m_mode;
}

void PathFunction::setPathMode(PathMode mode)
{
    m_mode = mode;
    emit changed(id());
}

PathFunction::Easing PathFunction::easing() const
{
    return m_easing;
}

void PathFunction::setEasing(Easing easing)
{
    m_easing = easing;
    emit changed(id());
}

double PathFunction::manualPosition() const
{
    return m_manualPos;
}

QPointF PathFunction::currentPoint() const
{
    QMutexLocker locker(&m_pointMutex);
    return m_currentPoint;
}

QPointF PathFunction::fixtureTarget(quint32 id) const
{
    for (const PathFixture& pf : m_fixtures)
        if (pf.id == id)
            return pf.currentTarget;
    return QPointF();
}

void PathFunction::setManualPosition(double pos)
{
    m_manualPos = qBound(0.0, pos, 1.0);
    emit changed(id());
}

bool PathFunction::inverted() const
{
    return m_inverted;
}

void PathFunction::setInverted(bool invert)
{
    m_inverted = invert;
    emit changed(id());
}

/*********************************************************************
 * Running
 *********************************************************************/

double PathFunction::applyEasing(double t) const
{
    t = qBound(0.0, t, 1.0);
    switch (m_easing)
    {
    case EaseIn:    return t * t;
    case EaseOut:   return t * (2.0 - t);
    case EaseInOut: return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
    case Linear:
    default:        return t;
    }
}

double PathFunction::currentParam() const
{
    double t = 0.0;

    if (m_mode == Manual)
    {
        t = qBound(0.0, m_manualPos, 1.0);
    }
    else
    {
        uint dur = duration();
        if (dur == 0)
            dur = 1000;
        double cycle = double(elapsed()) / double(dur);

        if (m_mode == Once)
            t = qMin(cycle, 1.0);
        else if (m_mode == PingPong)
        {
            double ph = std::fmod(cycle, 2.0);
            t = ph <= 1.0 ? ph : 2.0 - ph;
        }
        else // Repeat
            t = cycle - std::floor(cycle);
    }

    if (m_inverted)
        t = 1.0 - t;

    return qBound(0.0, t, 1.0);
}

QPointF PathFunction::pointAt(double t) const
{
    if (m_path.isEmpty())
        return QPointF();
    if (m_path.size() == 1)
        return m_path.first();

    QVector<double> cum;
    cum.append(0.0);
    double total = 0.0;
    for (int i = 1; i < m_path.size(); i++)
    {
        total += QLineF(m_path.at(i - 1), m_path.at(i)).length();
        cum.append(total);
    }
    if (total <= 0.0)
        return m_path.first();

    double target = qBound(0.0, t, 1.0) * total;
    for (int i = 1; i < m_path.size(); i++)
    {
        if (target <= cum.at(i))
        {
            double seg = cum.at(i) - cum.at(i - 1);
            double f = seg > 0.0 ? (target - cum.at(i - 1)) / seg : 0.0;
            return m_path.at(i - 1) + (m_path.at(i) - m_path.at(i - 1)) * f;
        }
    }
    return m_path.last();
}

QSharedPointer<GenericFader> PathFunction::getFader(QList<Universe*> universes, quint32 universeID)
{
    QSharedPointer<GenericFader> fader = m_fadersMap.value(universeID, QSharedPointer<GenericFader>());
    if (fader.isNull())
    {
        fader = universes[universeID]->requestFader(Universe::Auto);
        // In Manual mode the Intensity attribute is reused as the path
        // position, so the lamps must stay at full intensity.
        fader->adjustIntensity(m_mode == Manual ? 1.0 : getAttributeValue(Intensity));
        fader->setName(name());
        fader->setParentFunctionID(id());
        m_fadersMap[universeID] = fader;
    }
    return fader;
}

void PathFunction::writeFixtures(QList<Universe*> universes, double t)
{
    if (m_fixtures.isEmpty() || m_path.isEmpty())
        return;

    QPointF pt = pointAt(t);
    {
        QMutexLocker locker(&m_pointMutex);
        m_currentPoint = pt;
    }

    for (PathFixture& pf : m_fixtures)
    {
        Fixture* fx = doc()->fixture(pf.id);
        if (fx == NULL)
            continue;

        // Per-lamp phase offset, then direction. Wrap the offset into [0,1)
        // first so that inversion can land exactly on the path end (t=1).
        double local = t + pf.offsetDeg / 360.0;
        local -= std::floor(local);
        if (pf.inverted)
            local = 1.0 - local;
        QPointF target = pointAt(local);
        pf.currentTarget = target;

        double dx = target.x() - pf.x;
        double dy = target.y() - pf.y;
        double r = qSqrt(dx * dx + dy * dy);
        double pan = qAtan2(dy, dx);
        double tilt = qAtan2(r, pf.z - m_pathZ);

        QList<QPair<int, int> > pairs;
        const QPair<double, double> branches[2] = {
            QPair<double, double>(pan, tilt),
            QPair<double, double>(pan + M_PI, -tilt)
        };
        for (const QPair<double, double>& b : branches)
        {
            QList<int> pans = angleToDmxCandidates(b.first, pf.panK, pf.panT);
            QList<int> tilts = angleToDmxCandidates(b.second, pf.tiltK, pf.tiltT);
            for (int p : pans)
                for (int tt : tilts)
                    pairs.append(QPair<int, int>(p, tt));
        }

        QPair<int, int> chosen = selectPair(pairs, pf.lastPan, pf.lastTilt);
        if (chosen.first < 0)
            continue;
        pf.lastPan = chosen.first;
        pf.lastTilt = chosen.second;

        quint32 uni = fx->universe();
        if (int(uni) >= universes.count())
            continue;
        QSharedPointer<GenericFader> fader = getFader(universes, uni);

        if (pf.panCh >= 0)
        {
            const uchar v = uchar(chosen.first);
            fader->updateChannel(doc(), universes[uni], pf.id, pf.panCh,
                                 [v](FadeChannel& fc) { fc.setCurrent(v); fc.setTarget(v); });
        }
        if (pf.tiltCh >= 0)
        {
            const uchar v = uchar(chosen.second);
            fader->updateChannel(doc(), universes[uni], pf.id, pf.tiltCh,
                                 [v](FadeChannel& fc) { fc.setCurrent(v); fc.setTarget(v); });
        }
        if (pf.dimCh >= 0)
        {
            fader->updateChannel(doc(), universes[uni], pf.id, pf.dimCh,
                                 [](FadeChannel& fc) { fc.setCurrent(255); fc.setTarget(255); });
        }
    }
}

void PathFunction::preRun(MasterTimer* timer)
{
    Function::preRun(timer);
}

void PathFunction::write(MasterTimer* timer, QList<Universe*> universes)
{
    Q_UNUSED(timer);

    if (isPaused())
        return;

    double t = applyEasing(currentParam());
    writeFixtures(universes, t);

    incrementElapsed();

    if (m_mode == Once && duration() > 0 && elapsed() >= duration())
        stop(FunctionParent::master());
}

void PathFunction::postRun(MasterTimer* timer, QList<Universe*> universes)
{
    dismissAllFaders();
    Function::postRun(timer, universes);
}

int PathFunction::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (attrIndex == Intensity)
    {
        if (m_mode == Manual)
        {
            // In manual timing a slider (playback) drives the path position.
            setManualPosition(fraction);
        }
        else
        {
            QMapIterator<quint32, QSharedPointer<GenericFader> > it(m_fadersMap);
            while (it.hasNext())
            {
                it.next();
                if (!it.value().isNull())
                    it.value()->adjustIntensity(getAttributeValue(Intensity));
            }
        }
    }
    else if (attrIndex == getAttributeIndex(tr("Position")))
    {
        setManualPosition(fraction);
    }

    return attrIndex;
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool PathFunction::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCFunction);
    saveXMLCommon(doc);

    doc->writeAttribute(KXMLQLCPathMode, QString::number(int(m_mode)));
    doc->writeAttribute(KXMLQLCPathEasing, QString::number(int(m_easing)));
    doc->writeAttribute(KXMLQLCPathZ, QString::number(m_pathZ, 'g', 8));
    doc->writeAttribute(KXMLQLCPathManualPos, QString::number(m_manualPos, 'g', 8));
    doc->writeAttribute("Invert", m_inverted ? "true" : "false");

    saveXMLRunOrder(doc);
    saveXMLSpeed(doc);

    doc->writeStartElement(KXMLQLCPathPoints);
    for (const QPointF& p : m_path)
    {
        doc->writeStartElement(KXMLQLCPathPoint);
        doc->writeAttribute("X", QString::number(p.x(), 'g', 8));
        doc->writeAttribute("Y", QString::number(p.y(), 'g', 8));
        doc->writeEndElement();
    }
    doc->writeEndElement();

    for (const PathFixture& f : m_fixtures)
    {
        doc->writeStartElement(KXMLQLCPathFixture);
        doc->writeAttribute(KXMLQLCPathFixtureID, QString::number(f.id));
        doc->writeAttribute("X", QString::number(f.x, 'g', 8));
        doc->writeAttribute("Y", QString::number(f.y, 'g', 8));
        doc->writeAttribute(KXMLQLCPathZ, QString::number(f.z, 'g', 8));
        doc->writeAttribute(KXMLQLCPathPanK, QString::number(f.panK, 'g', 12));
        doc->writeAttribute(KXMLQLCPathPanT, QString::number(f.panT, 'g', 12));
        doc->writeAttribute(KXMLQLCPathTiltK, QString::number(f.tiltK, 'g', 12));
        doc->writeAttribute(KXMLQLCPathTiltT, QString::number(f.tiltT, 'g', 12));
        doc->writeAttribute(KXMLQLCPathPanCh, QString::number(f.panCh));
        doc->writeAttribute(KXMLQLCPathTiltCh, QString::number(f.tiltCh));
        doc->writeAttribute(KXMLQLCPathDimCh, QString::number(f.dimCh));
        doc->writeAttribute("Invert", f.inverted ? "true" : "false");
        doc->writeAttribute("Offset", QString::number(f.offsetDeg, 'g', 8));
        doc->writeEndElement();
    }

    doc->writeEndElement();

    return true;
}

bool PathFunction::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    QXmlStreamAttributes attrs = root.attributes();
    if (attrs.hasAttribute(KXMLQLCPathMode))
        m_mode = PathMode(attrs.value(KXMLQLCPathMode).toString().toInt());
    if (attrs.hasAttribute(KXMLQLCPathEasing))
        m_easing = Easing(attrs.value(KXMLQLCPathEasing).toString().toInt());
    if (attrs.hasAttribute(KXMLQLCPathZ))
        m_pathZ = attrs.value(KXMLQLCPathZ).toString().toDouble();
    if (attrs.hasAttribute(KXMLQLCPathManualPos))
        m_manualPos = attrs.value(KXMLQLCPathManualPos).toString().toDouble();
    if (attrs.hasAttribute("Invert"))
        m_inverted = (attrs.value("Invert").toString() == "true");

    m_path.clear();
    m_fixtures.clear();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCFunctionSpeed)
        {
            loadXMLSpeed(root);
        }
        else if (root.name() == KXMLQLCFunctionRunOrder)
        {
            loadXMLRunOrder(root);
        }
        else if (root.name() == KXMLQLCPathPoints)
        {
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCPathPoint)
                {
                    double x = root.attributes().value("X").toString().toDouble();
                    double y = root.attributes().value("Y").toString().toDouble();
                    m_path.append(QPointF(x, y));
                }
                root.skipCurrentElement();
            }
        }
        else if (root.name() == KXMLQLCPathFixture)
        {
            PathFixture f;
            f.id = root.attributes().value(KXMLQLCPathFixtureID).toString().toUInt();
            f.x = root.attributes().value("X").toString().toDouble();
            f.y = root.attributes().value("Y").toString().toDouble();
            f.z = root.attributes().value(KXMLQLCPathZ).toString().toDouble();
            f.panK = root.attributes().value(KXMLQLCPathPanK).toString().toDouble();
            f.panT = root.attributes().value(KXMLQLCPathPanT).toString().toDouble();
            f.tiltK = root.attributes().value(KXMLQLCPathTiltK).toString().toDouble();
            f.tiltT = root.attributes().value(KXMLQLCPathTiltT).toString().toDouble();
            f.panCh = root.attributes().value(KXMLQLCPathPanCh).toString().toInt();
            f.tiltCh = root.attributes().value(KXMLQLCPathTiltCh).toString().toInt();
            f.dimCh = root.attributes().value(KXMLQLCPathDimCh).toString().toInt();
            f.inverted = (root.attributes().value("Invert").toString() == "true");
            f.offsetDeg = root.attributes().value("Offset").toString().toDouble();
            m_fixtures.append(f);
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Path tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    return true;
}

void PathFunction::postLoad()
{
    Function::postLoad();
}
