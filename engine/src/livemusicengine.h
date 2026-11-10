/*
  Q Light Controller Plus
  livemusicengine.h

  Beat/drop synchronised automatic light show generator driven by
  SongRequest playback data.

  Licensed under the Apache License, Version 2.0
*/

#ifndef LIVEMUSICENGINE_H
#define LIVEMUSICENGINE_H

#include <QColor>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

#include "qlcchannel.h"

class QLCCapability;
class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

namespace LiveMusic
{

enum Style
{
    StyleAuto = 0,
    StyleParty,
    StyleEDM,
    StyleRock,
    StylePop,
    StyleHipHop,
    StyleMetal,
    StyleJazz,
    StyleReggae,
    StyleAcoustic,
    StyleUnknown
};

enum ColorMode
{
    ColorAuto = 0,
    ColorRainbow,
    ColorDuotone,
    ColorWarm,
    ColorCool,
    ColorSingle
};

enum MovementStyle
{
    MoveAuto = 0,
    MoveSweep,
    MoveCircle,
    MoveFan,
    MoveFigure8,
    MoveBallyhoo,
    MoveStatic
};

enum Section
{
    SectionUnknown = 0,
    SectionIntro,
    SectionVerse,
    SectionBuild,
    SectionChorus,
    SectionDrop,
    SectionBreakdown,
    SectionBridge,
    SectionOutro
};

enum Scene
{
    ScenePulse = 0,
    SceneColorChase,
    SceneRainbowSweep,
    SceneDualTone,
    SceneStrobeDrop,
    SceneGoboSpin,
    ScenePrismRush,
    SceneFanWave,
    SceneFigureEight,
    SceneSmokeBreak,
    SceneBuild,
    SceneWhiteOut,
    SceneBallyhoo,
    SceneCount
};

struct Drop
{
    Drop() : time(0), confidence(0) { }
    double time;
    double confidence;
    QString type;
    QString label;
};

struct Part
{
    Part() : start(0), end(0), energyDb(-60) { }
    double start;
    double end;
    QString type;
    QString label;
    double energyDb;
};

struct TimelinePoint
{
    TimelinePoint() : time(0), rmsDb(-60), low(0), mid(0), high(0), novelty(0) { }
    double time;
    double rmsDb;
    double low;
    double mid;
    double high;
    double novelty;
};

struct Song
{
    Song() : duration(0), bpm(0), beatOffset(0), lowConfidence(true),
             loudness(-14), dynamicRange(10) { }

    QString id;
    QString title;
    QString artist;
    QString album;
    QString genre;
    double duration;
    double bpm;
    double beatOffset;
    bool lowConfidence;
    double loudness;
    double dynamicRange;
    QList<Drop> drops;
    QList<Part> parts;
    QVector<TimelinePoint> timeline;

    bool hasAnalysis() const { return bpm > 20.0; }
};

struct Playback
{
    Playback() : playing(false), active(false), position(0), duration(0) { }
    bool playing;
    bool active;
    double position;
    double duration;
    QString id;
    QString title;
    QString artist;
    QString album;
    QString genre;
};

struct FixtureConfig
{
    FixtureConfig() : fixtureId(0), allChannels(true), intensityLimit(100),
                      movementRange(100), invertPan(false), invertTilt(false) { }

    quint32 fixtureId;
    /** When true every supported show channel of the fixture may be driven.
     *  When false only the channels listed in allowedChannels are touched. */
    bool allChannels;
    QSet<quint32> allowedChannels;
    int intensityLimit;   //!< 0..100
    int movementRange;    //!< 0..100
    bool invertPan;
    bool invertTilt;
};

struct ShowSettings
{
    ShowSettings() : style(StyleAuto), colorMode(ColorAuto), movementStyle(MoveAuto),
                     energy(65), intensityLimit(100), movementRange(100),
                     allowColor(true), allowMovement(true), allowStrobe(true),
                     allowGobo(true), allowPrism(true), allowBeam(true),
                     allowSmoke(true), changeBars(4) { }

    int style;
    int colorMode;
    int movementStyle;
    int energy;           //!< 0..100
    int intensityLimit;   //!< 0..100
    int movementRange;    //!< 0..100
    bool allowColor;
    bool allowMovement;
    bool allowStrobe;
    bool allowGobo;
    bool allowPrism;
    bool allowBeam;
    bool allowSmoke;
    int changeBars;       //!< bars between scene changes (2, 4, 8, 16)
    QColor primaryColor;
    QColor secondaryColor;
};

struct ChannelInfo
{
    ChannelInfo() : index(0), group(QLCChannel::NoGroup), preset(QLCChannel::Custom),
                    colour(QLCChannel::NoColour), controlByte(QLCChannel::MSB) { }
    quint32 index;
    QLCChannel::Group group;
    QLCChannel::Preset preset;
    QLCChannel::PrimaryColour colour;
    QLCChannel::ControlByte controlByte;
    QString name;
};

struct FrameInfo
{
    FrameInfo() : position(0), bpm(0), beat(0), bar(0), phrase(0),
                  beatPhase(0), barPhase(0), energy(0), drop(0),
                  section(SectionUnknown), style(StyleUnknown), scene(ScenePulse),
                  playing(false), activeFixtures(0), outputChannels(0) { }

    double position;
    double bpm;
    int beat;
    int bar;
    int phrase;
    double beatPhase;
    double barPhase;
    double energy;
    double drop;
    int section;
    int style;
    int scene;
    QString sceneName;
    bool playing;
    int activeFixtures;
    int outputChannels;
};

QString styleName(int style);
QString sectionName(int section);
QString sceneName(int scene);
QString colorModeName(int mode);
QString movementStyleName(int style);

/** Convert a SongRequest analysis object (already parsed into a Song) into
 *  a deterministic, beat/drop aware DMX frame. The engine never touches
 *  channels that are not part of a fixture's configured allowance. */
class LiveMusicEngine
{
public:
    explicit LiveMusicEngine(Doc *doc = NULL);

    void setDoc(Doc *doc);
    Doc *doc() const;

    void setFixtures(const QList<FixtureConfig> &fixtures);
    QList<FixtureConfig> fixtures() const;
    void setSettings(const ShowSettings &settings);
    ShowSettings settings() const;

    void setSong(const Song &song);
    Song song() const;
    void setPlayback(const Playback &playback);
    Playback playback() const;

    /** Drop cached fixture profiles (e.g. after the workspace changed). */
    void invalidate();

    /** Render a complete DMX frame for the given song position in seconds.
     *  info (if not NULL) receives the current beat/section/scene state. */
    QMap<QPair<quint32, quint32>, uchar> render(double position,
                                                FrameInfo *info = NULL);

    /** Calculate display information without generating a frame. */
    FrameInfo frameInfo(double position) const;

    /** Return every channel of a fixture with its QLC+ classification.
     *  Used by the widget's settings dialog to build the channel tree. */
    QList<ChannelInfo> channelInfos(quint32 fixtureId) const;

    /** Return the channels the engine considers usable for a full show,
     *  grouped by role name ("intensity", "color", "pan", ...). */
    QMap<QString, QList<quint32> > defaultRoles(quint32 fixtureId) const;

    static int detectStyle(const Song &song);
    static QColor colorFromCapability(const QLCChannel *channel,
                                      const QLCCapability *capability);

private:
    struct ProfileChannel
    {
        ChannelInfo info;
        QLCChannel::Group group;
        QLCChannel::PrimaryColour colour;
        QLCChannel::Preset preset;
    };

    struct Profile
    {
        Profile() : fixture(NULL), isSmoke(false), isHazer(false),
                    panMax(540), tiltMax(270), index(0), count(1),
                    hasDimmer(false) { }

        Fixture *fixture;
        FixtureConfig config;
        QList<ProfileChannel> channels;
        QList<quint32> dimmer;
        QList<quint32> dimmerFine;
        QList<quint32> lamp;
        QList<quint32> red;
        QList<quint32> green;
        QList<quint32> blue;
        QList<quint32> cyan;
        QList<quint32> magenta;
        QList<quint32> yellow;
        QList<quint32> white;
        QList<quint32> amber;
        QList<quint32> uv;
        QList<quint32> pan;
        QList<quint32> panFine;
        QList<quint32> tilt;
        QList<quint32> tiltFine;
        QList<quint32> colorWheel;
        QList<quint32> gobo;
        QList<quint32> prism;
        QList<quint32> prismRotation;
        QList<quint32> shutter;
        QList<quint32> strobe;
        QList<quint32> beamZoom;
        QList<quint32> beamFocus;
        QList<quint32> smoke;
        bool isSmoke;
        bool isHazer;
        int panMax;
        int tiltMax;
        int index;
        int count;
        bool hasDimmer;
    };

    void rebuildProfiles();
    void classifyChannel(Profile &profile, quint32 index) const;
    bool channelAllowed(const Profile &profile, quint32 channel) const;
    void put(QMap<QPair<quint32, quint32>, uchar> &frame, const Profile &profile,
             quint32 channel, int value) const;
    void putList(QMap<QPair<quint32, quint32>, uchar> &frame, const Profile &profile,
                 const QList<quint32> &channels, int value) const;

    double timelineEnergy(const Song &song, double position) const;
    int sectionAt(const Song &song, double position) const;
    double dropStrength(const Song &song, double position) const;
    double buildStrength(const Song &song, double position) const;
    int sceneAt(const FrameInfo &info) const;
    int movementForScene(int scene, int style, int section) const;
    QColor colorAt(const FrameInfo &info, int fixtureIndex, int fixtureCount,
                   int colorSlot) const;

    Doc *m_doc;
    QList<FixtureConfig> m_fixtures;
    ShowSettings m_settings;
    Song m_song;
    Playback m_playback;
    QList<Profile> m_profiles;
    bool m_dirty;
};

} // namespace LiveMusic

Q_DECLARE_METATYPE(LiveMusic::Song)
Q_DECLARE_METATYPE(LiveMusic::Playback)

/** @} */

#endif // LIVEMUSICENGINE_H
