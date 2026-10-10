/*
  Q Light Controller Plus
  fixturescene.h

  The fixture model shared by the 3D Preview viewport and its fixture panel.
  Horizontal placement (x/y) and yaw come from the Fixture Monitor, so the two
  views stay in sync; the vertical placement (height) and tilt are 3D-only and
  are persisted with the 3D Preview.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTURESCENE_H
#define FIXTURESCENE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QHash>
#include <QVector3D>
#include <QColor>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class Doc;
class Fixture;
class QTimer;

#define KXMLQLCPreview3DFixtures QStringLiteral("Fixtures")
#define KXMLQLCPreview3DFixture  QStringLiteral("Fixture")

/** Coarse visual category, used by the 3D renderer to pick a fixture model. */
enum class FixtureModel
{
    Dimmer,     ///< multi-channel dimmer pack / rack chassis
    Lamp,       ///< single-channel conventional (theatre) lantern
    Par,        ///< par can (cylinder + lens + yoke)
    Bar,        ///< LED bar (housing + lens grid, one beam per column)
    Derby,      ///< spinning effect / derby (dome + radiating beams)
    MovingHead, ///< pan/tilt head (base + yoke + drum)
    Smoke       ///< fog / haze machine (chassis + nozzle + fluid tank)
};

/** One fixture placed in the 3D Preview. */
struct FixtureInstance
{
    quint32 id = 0;
    QString name;
    QVector3D pos;                      // world metres (x, y, z)
    double yaw = 0.0;                   // degrees, from the Fixture Monitor
    double pitch = 0.0;                 // degrees, 3D only (0 = horizontal, +90 = up)
    bool visible = true;
    QColor color = QColor(210, 205, 190);
    QVector3D size = QVector3D(0.25, 0.25, 0.25);   // metres (x, y, z)
    double beamDeg = 25.0;              // full beam angle
    FixtureModel model = FixtureModel::Par;
    int ledCols = 1;                    // LED bar / matrix columns (layout width)
    int ledRows = 1;                    // LED bar / matrix rows (layout height)

    // Live (operate-mode) values. When `live` is false the renderer falls back
    // to the static colour / orientation above.
    bool live = false;
    double liveIntensity = 1.0;         // 0..1, from dimmer / master
    QColor liveColor = QColor(255, 255, 255);
    double livePan = 0.0;               // degrees, from the pan channel
    double liveTilt = 0.0;              // degrees, from the tilt channel
    QList<QColor> liveHeads;            // per-head colour (LED bars / matrices)
    QList<double> liveHeadIntensity;    // per-head intensity (LED bars / matrices)
};

class FixtureScene final : public QObject
{
    Q_OBJECT

public:
    FixtureScene(Doc *doc, QObject *parent = NULL);

    /** Rebuild the list from the document's fixtures (keeps 3D extras). */
    void rebuild();

    /** Discard the 3D-only extras and rebuild from the document. */
    void clear();
    /** Discard the 3D-only extras before a project load. */
    void beginLoad();

    const QList<FixtureInstance> &fixtures() const { return m_fixtures; }
    FixtureInstance *instance(quint32 id);
    int indexOf(quint32 id) const;

    /** Move in the world (metres). x/y are written back to the Fixture Monitor. */
    void setPosition(quint32 id, const QVector3D &pos);
    /** Set yaw (deg, synced to the Monitor) and pitch (deg, 3D only). */
    void setOrientation(quint32 id, double yaw, double pitch);
    void setVisible(quint32 id, bool visible);

    /** True when a fixture should get a 3D model (dimmers and LEDs). */
    static bool isRenderable(Fixture *fx);

    /** True while operate-mode live values are being tracked. */
    bool isLive() const { return m_live; }
    /** Enable/disable live value tracking (called on mode changes). */
    void setLive(bool on);

    /*********************************************************************
     * Load & Save (3D-only extras)
     *********************************************************************/
    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

signals:
    /** Emitted when the placement/model changes (a user edit). */
    void changed();
    /** Emitted for operate-mode live value changes (never a user edit). */
    void liveChanged();

private slots:
    void slotLiveTick();
    void slotStrobeTick();

private:
    void readFromMonitor(FixtureInstance &fi);
    void writePosition(quint32 id, const QVector3D &pos);
    void writeRotation(quint32 id, double yaw);
    void updateModel(FixtureInstance &fi, Fixture *fx);
    void buildLiveCache(quint32 id, Fixture *fx);
    void updateLive();

    Doc *m_doc;
    bool m_writing = false;
    bool m_clearing = false;
    QList<FixtureInstance> m_fixtures;

    struct Extras
    {
        double height = 0.0;
        double pitch = 0.0;
        bool visible = true;
    };
    QMap<quint32, Extras> m_extras;     // survives rebuild()

    // Live value tracking (operate mode)
    struct LiveCache
    {
        int dimmer = -1;
        int master = -1;
        int pan = -1;
        int tilt = -1;
        double panMax = 360.0;
        double tiltMax = 270.0;
        QVector<quint32> rgb;
        QVector<quint32> cmy;
        QVector<quint32> shutter;
    };
    QHash<quint32, LiveCache> m_liveCache;
    QList<Fixture *> m_liveFixtures;    // fixtures whose valuesChanged we watch
    QTimer *m_liveTimer;                // coalesces rapid updates
    QTimer *m_strobeTimer;              // drives the strobe phase
    bool m_live;
    bool m_strobePhase;
};

#endif // FIXTURESCENE_H
