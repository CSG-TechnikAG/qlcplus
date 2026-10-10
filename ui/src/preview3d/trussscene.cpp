/*
  Q Light Controller Plus
  trussscene.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtMath>
#include <QQueue>
#include <QSet>

#include "trussscene.h"

TrussScene::TrussScene(QObject *parent)
    : QObject(parent)
    , m_nextId(1)
{
}

int TrussScene::indexOf(int id) const
{
    for (int i = 0; i < m_trusses.size(); ++i)
        if (m_trusses.at(i).id == id)
            return i;
    return -1;
}

Truss *TrussScene::trussById(int id)
{
    const int i = indexOf(id);
    return (i >= 0) ? &m_trusses[i] : NULL;
}

const Truss *TrussScene::trussById(int id) const
{
    const int i = indexOf(id);
    return (i >= 0) ? &m_trusses.at(i) : NULL;
}

int TrussScene::addTruss(const Truss &t)
{
    Truss copy = t;
    copy.id = m_nextId++;
    m_trusses << copy;
    emit changed();
    return copy.id;
}

void TrussScene::removeTruss(int id)
{
    const int i = indexOf(id);
    if (i < 0)
        return;

    m_trusses.removeAt(i);
    for (int k = m_links.size() - 1; k >= 0; --k)
        if (m_links.at(k).a == id || m_links.at(k).b == id)
            m_links.removeAt(k);

    emit changed();
}

void TrussScene::replaceTruss(const Truss &t)
{
    const int i = indexOf(t.id);
    if (i < 0)
        return;

    const bool moved = (m_trusses.at(i).pos != t.pos)
                    || (m_trusses.at(i).rot != t.rot);
    m_trusses[i] = t;
    if (moved)
        propagateFrom(t.id);
    emit changed();
}

void TrussScene::translate(int id, const QVector3D &delta)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    m_trusses[i].pos += delta;
    propagateFrom(id);
    emit changed();
}

void TrussScene::placeOnFloor(int id, double floorZ)
{
    if (trussById(id) == NULL)
        return;

    // Collect the whole connected group.
    QSet<int> group;
    group.insert(id);
    QQueue<int> queue;
    queue.enqueue(id);
    while (!queue.isEmpty())
    {
        const int cur = queue.dequeue();
        for (const TrussLink &l : m_links)
        {
            int other = -1;
            if (l.a == cur) other = l.b;
            else if (l.b == cur) other = l.a;
            else continue;
            if (!group.contains(other))
            {
                group.insert(other);
                queue.enqueue(other);
            }
        }
    }

    double minZ = 1e18;
    for (int gid : group)
    {
        const Truss *t = trussById(gid);
        if (t == NULL)
            continue;
        for (const TrussSegment &s : t->segments())
            minZ = qMin(minZ, qMin(s.a.z(), s.b.z()));
    }

    if (minZ > 1e17 || qAbs(minZ - floorZ) < 1e-6)
        return;

    translate(id, QVector3D(0.0, 0.0, floorZ - minZ));
}

void TrussScene::propagateFrom(int id)
{
    const Truss *root = trussById(id);
    if (root == NULL)
        return;

    QSet<int> visited;
    visited.insert(id);
    QQueue<int> queue;
    queue.enqueue(id);

    while (!queue.isEmpty())
    {
        const int cur = queue.dequeue();
        const TrussXform mc = trussById(cur)->xform();

        for (const TrussLink &l : m_links)
        {
            int other = -1;
            bool curIsA = false;
            if (l.a == cur) { other = l.b; curIsA = true; }
            else if (l.b == cur) { other = l.a; curIsA = false; }
            else continue;

            if (visited.contains(other))
                continue;

            TrussXform rel;
            rel.rot = l.relRot;
            rel.t = l.relT;

            const TrussXform mo = curIsA ? mc.compose(rel) : mc.compose(rel.inverse());
            Truss *o = trussById(other);
            if (o != NULL)
                o->setXform(mo);

            visited.insert(other);
            queue.enqueue(other);
        }
    }
}

bool TrussScene::connected(int a, int b) const
{
    QSet<int> visited;
    visited.insert(a);
    QQueue<int> queue;
    queue.enqueue(a);

    while (!queue.isEmpty())
    {
        const int cur = queue.dequeue();
        if (cur == b)
            return true;
        for (const TrussLink &l : m_links)
        {
            int other = -1;
            if (l.a == cur) other = l.b;
            else if (l.b == cur) other = l.a;
            else continue;
            if (!visited.contains(other))
            {
                visited.insert(other);
                queue.enqueue(other);
            }
        }
    }
    return false;
}

bool TrussScene::isPortLinked(int id, int port) const
{
    for (const TrussLink &l : m_links)
        if ((l.a == id && l.aPort == port) || (l.b == id && l.bPort == port))
            return true;
    return false;
}

bool TrussScene::hasLinks(int id) const
{
    for (const TrussLink &l : m_links)
        if (l.a == id || l.b == id)
            return true;
    return false;
}

void TrussScene::unlinkAll(int id)
{
    bool any = false;
    for (int k = m_links.size() - 1; k >= 0; --k)
        if (m_links.at(k).a == id || m_links.at(k).b == id)
        {
            m_links.removeAt(k);
            any = true;
        }
    if (any)
        emit changed();
}

bool TrussScene::link(int aId, int aPort, int bId, int bPort)
{
    if (aId == bId)
        return false;

    Truss *a = trussById(aId);
    Truss *b = trussById(bId);
    if (a == NULL || b == NULL)
        return false;

    if (isPortLinked(aId, aPort) || isPortLinked(bId, bPort))
        return false;
    if (connected(aId, bId))
        return false;

    const QList<TrussPort> ap = a->localPorts();
    const QList<TrussPort> bp = b->localPorts();
    if (aPort < 0 || aPort >= ap.size() || bPort < 0 || bPort >= bp.size())
        return false;

    const TrussXform ma = a->xform();
    const QVector3D pWorld = ma.apply(ap.at(aPort).localPos);
    const QVector3D dWorld = ma.rot.rotatedVector(ap.at(aPort).localDir);

    // Orient B so its port faces back at A's port (shortest rotation), then
    // place it. Works for horizontal and vertical attachments alike.
    const QVector3D qDir = bp.at(bPort).localDir.normalized();
    const QVector3D target = (-dWorld).normalized();

    TrussXform mb;
    mb.rot = QQuaternion::rotationTo(qDir, target);
    const QVector3D qPos = bp.at(bPort).localPos;
    mb.t = pWorld - mb.rot.rotatedVector(qPos);
    b->setXform(mb);

    TrussLink l;
    l.a = aId; l.aPort = aPort;
    l.b = bId; l.bPort = bPort;
    const TrussXform rel = ma.inverse().compose(mb);
    l.relRot = rel.rot;
    l.relT = rel.t;
    m_links << l;

    // B may already carry its own linked pieces: carry them along too.
    propagateFrom(bId);

    emit changed();
    return true;
}

QList<TrussPort> TrussScene::worldPorts(int id) const
{
    QList<TrussPort> out;
    const Truss *t = trussById(id);
    if (t == NULL)
        return out;

    const TrussXform x = t->xform();
    for (const TrussPort &p : t->localPorts())
        out << TrussPort{ x.apply(p.localPos), x.rot.rotatedVector(p.localDir) };
    return out;
}

QSet<int> TrussScene::linkedPorts(int id) const
{
    QSet<int> s;
    for (const TrussLink &l : m_links)
    {
        if (l.a == id)
            s.insert(l.aPort);
        else if (l.b == id)
            s.insert(l.bPort);
    }
    return s;
}

void TrussScene::clear()
{
    m_trusses.clear();
    m_links.clear();
    m_nextId = 1;
    emit changed();
}

bool TrussScene::loadLink(int aId, int aPort, int bId, int bPort)
{
    Truss *a = trussById(aId);
    Truss *b = trussById(bId);
    if (a == NULL || b == NULL)
        return false;

    TrussLink l;
    l.a = aId; l.aPort = aPort;
    l.b = bId; l.bPort = bPort;
    const TrussXform rel = a->xform().inverse().compose(b->xform());
    l.relRot = rel.rot;
    l.relT = rel.t;
    m_links << l;
    return true;
}

bool TrussScene::saveXML(QXmlStreamWriter *doc) const
{
    doc->writeStartElement(KXMLQLCPreview3DTrusses);
    for (const Truss &t : m_trusses)
    {
        doc->writeStartElement(KXMLQLCPreview3DTruss);
        doc->writeAttribute("ID", QString::number(t.id));
        doc->writeAttribute("Type", Truss::typeId(t.type));
        doc->writeAttribute("Profile", Truss::typeId(t.profile));
        doc->writeAttribute("Length", QString::number(t.length, 'g', 8));
        doc->writeAttribute("Width", QString::number(t.width, 'g', 8));
        doc->writeAttribute("Height", QString::number(t.height, 'g', 8));
        doc->writeAttribute("Bay", QString::number(t.bayLength, 'g', 8));
        doc->writeAttribute("X", QString::number(t.pos.x(), 'g', 8));
        doc->writeAttribute("Y", QString::number(t.pos.y(), 'g', 8));
        doc->writeAttribute("Z", QString::number(t.pos.z(), 'g', 8));
        doc->writeAttribute("QW", QString::number(t.rot.scalar(), 'g', 8));
        doc->writeAttribute("QX", QString::number(t.rot.x(), 'g', 8));
        doc->writeAttribute("QY", QString::number(t.rot.y(), 'g', 8));
        doc->writeAttribute("QZ", QString::number(t.rot.z(), 'g', 8));
        doc->writeAttribute("Visible", t.visible ? "true" : "false");
        doc->writeAttribute("Color", t.color.name());
        doc->writeEndElement();
    }
    doc->writeEndElement(); // Trusses

    doc->writeStartElement(KXMLQLCPreview3DLinks);
    for (const TrussLink &l : m_links)
    {
        doc->writeStartElement(KXMLQLCPreview3DLink);
        doc->writeAttribute("A", QString::number(l.a));
        doc->writeAttribute("APort", QString::number(l.aPort));
        doc->writeAttribute("B", QString::number(l.b));
        doc->writeAttribute("BPort", QString::number(l.bPort));
        doc->writeEndElement();
    }
    doc->writeEndElement(); // Links
    return true;
}

void TrussScene::beginLoad()
{
    m_trusses.clear();
    m_links.clear();
    m_nextId = 1;
}

void TrussScene::endLoad()
{
    emit changed();
}

bool TrussScene::loadXML(QXmlStreamReader &root)
{
    if (root.name() == KXMLQLCPreview3DTrusses)
    {
        while (root.readNextStartElement())
        {
            if (root.name() == KXMLQLCPreview3DTruss)
            {
                const QXmlStreamAttributes a = root.attributes();
                Truss t;
                t.id = a.value("ID").toInt();
                t.type = Truss::typeFromId(a.value("Type").toString());
                t.profile = a.hasAttribute("Profile")
                          ? Truss::typeFromId(a.value("Profile").toString())
                          : Truss::Box;
                t.length = a.hasAttribute("Length") ? a.value("Length").toDouble() : 4.0;
                t.width = a.hasAttribute("Width") ? a.value("Width").toDouble() : 0.3;
                t.height = a.hasAttribute("Height") ? a.value("Height").toDouble() : 0.3;
                t.bayLength = a.hasAttribute("Bay") ? a.value("Bay").toDouble() : 0.5;
                t.pos = QVector3D(a.value("X").toDouble(),
                                  a.value("Y").toDouble(),
                                  a.value("Z").toDouble());

                const double qw = a.hasAttribute("QW") ? a.value("QW").toDouble() : 1.0;
                t.rot = QQuaternion(qw,
                                    a.value("QX").toDouble(),
                                    a.value("QY").toDouble(),
                                    a.value("QZ").toDouble());
                if (t.rot.lengthSquared() < 1e-6)
                    t.rot = QQuaternion();
                else
                    t.rot.normalize();

                t.visible = (a.value("Visible") != "false");
                if (a.hasAttribute("Color"))
                    t.color = QColor(a.value("Color").toString());

                if (t.id <= 0)
                    t.id = m_nextId;
                m_nextId = qMax(m_nextId, t.id + 1);
                m_trusses << t;
            }
            root.skipCurrentElement();
        }
    }
    else if (root.name() == KXMLQLCPreview3DLinks)
    {
        while (root.readNextStartElement())
        {
            if (root.name() == KXMLQLCPreview3DLink)
            {
                const QXmlStreamAttributes a = root.attributes();
                loadLink(a.value("A").toInt(), a.value("APort").toInt(),
                         a.value("B").toInt(), a.value("BPort").toInt());
            }
            root.skipCurrentElement();
        }
    }
    else
    {
        root.skipCurrentElement();
    }

    return true;
}
