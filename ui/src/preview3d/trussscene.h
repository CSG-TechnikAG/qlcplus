/*
  Q Light Controller Plus
  trussscene.h

  The truss model shared by the 3D Preview viewport and its configuration
  panel: a list of truss pieces plus the links between their ports. Moving a
  linked piece rigidly carries the whole connected group with it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TRUSSSSCENE_H
#define TRUSSSSCENE_H

#include <QObject>
#include <QList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "truss.h"

#define KXMLQLCPreview3DTrusses   QStringLiteral("Trusses")
#define KXMLQLCPreview3DTruss     QStringLiteral("Truss")
#define KXMLQLCPreview3DLinks     QStringLiteral("Links")
#define KXMLQLCPreview3DLink      QStringLiteral("Link")

/** A rigid link between two truss ports. */
struct TrussLink
{
    int a;          // truss id
    int aPort;
    int b;          // truss id
    int bPort;
    QQuaternion relRot;   // inverse(M_a) * M_b
    QVector3D relT;
};

class TrussScene final : public QObject
{
    Q_OBJECT

public:
    explicit TrussScene(QObject *parent = NULL);

    const QList<Truss> &trusses() const { return m_trusses; }
    const QList<TrussLink> &links() const { return m_links; }

    int indexOf(int id) const;
    Truss *trussById(int id);
    const Truss *trussById(int id) const;

    /** Add a truss; returns its new id. */
    int addTruss(const Truss &t);
    void removeTruss(int id);
    /** Update a truss's fields; propagates if its transform changed. */
    void replaceTruss(const Truss &t);
    /** Move a truss in the world; linked pieces follow. */
    void translate(int id, const QVector3D &delta);

    /** Drop a piece (and its whole linked group) onto the given floor height,
     *  keeping its rotation. */
    void placeOnFloor(int id, double floorZ);

    /** Try to link two free ports; returns true on success. */
    bool link(int aId, int aPort, int bId, int bPort);
    /** Drop all links of a truss (it stays where it is). */
    void unlinkAll(int id);
    bool isPortLinked(int id, int port) const;
    bool hasLinks(int id) const;

    QList<TrussPort> worldPorts(int id) const;

    /** Port indices of a truss that are actually linked to something. */
    QSet<int> linkedPorts(int id) const;

    /** Remove everything (used on a new/loaded project). */
    void clear();

    /*********************************************************************
     * Load & Save
     *********************************************************************/
    bool saveXML(QXmlStreamWriter *doc) const;
    /** Reset before loading (the caller drives the XML reader). */
    void beginLoad();
    /** Load a single <Trusses> or <Links> element. */
    bool loadXML(QXmlStreamReader &root);
    /** Emit changed() once loading is finished. */
    void endLoad();

signals:
    void changed();

private:
    void propagateFrom(int id);
    bool connected(int a, int b) const;
    bool loadLink(int a, int aPort, int b, int bPort);

    int m_nextId;
    QList<Truss> m_trusses;
    QList<TrussLink> m_links;
};

#endif // TRUSSSSCENE_H
