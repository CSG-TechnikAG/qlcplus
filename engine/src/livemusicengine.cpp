/*
  Q Light Controller Plus
  livemusicengine.cpp

  Beat/drop synchronised automatic light show generator driven by
  SongRequest playback data.

  Licensed under the Apache License, Version 2.0
*/

#include <QtMath>

#include <algorithm>
#include <cmath>

#include "livemusicengine.h"
#include "qlccapability.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "fixture.h"
#include "doc.h"

namespace LiveMusic
{

static double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

static double frac(double value)
{
    return value - std::floor(value);
}

static double lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

static double smoothStep(double t)
{
    t = clamp01(t);
    return t * t * (3.0 - 2.0 * t);
}

static quint32 hashString(const QString &text)
{
    quint32 h = 2166136261u;
    const QByteArray data = text.toUtf8();
    for (int i = 0; i < data.size(); i++)
    {
        h ^= quint8(data.at(i));
        h *= 16777619u;
    }
    return h;
}

static quint32 hashMix(quint32 h, quint32 value)
{
    value += 0x9e3779b9u;
    h ^= value;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

QString styleName(int style)
{
    switch (style)
    {
    case StyleParty: return QObject::tr("Party");
    case StyleEDM: return QObject::tr("EDM / Dance");
    case StyleRock: return QObject::tr("Rock");
    case StylePop: return QObject::tr("Pop");
    case StyleHipHop: return QObject::tr("Hip Hop");
    case StyleMetal: return QObject::tr("Metal");
    case StyleJazz: return QObject::tr("Jazz / Blues");
    case StyleReggae: return QObject::tr("Reggae");
    case StyleAcoustic: return QObject::tr("Acoustic / Ballad");
    case StyleAuto: return QObject::tr("Auto");
    case StyleUnknown:
    default: return QObject::tr("Unknown");
    }
}

QString sectionName(int section)
{
    switch (section)
    {
    case SectionIntro: return QObject::tr("Intro");
    case SectionVerse: return QObject::tr("Verse");
    case SectionBuild: return QObject::tr("Build");
    case SectionChorus: return QObject::tr("Chorus");
    case SectionDrop: return QObject::tr("Drop");
    case SectionBreakdown: return QObject::tr("Breakdown");
    case SectionBridge: return QObject::tr("Bridge");
    case SectionOutro: return QObject::tr("Outro");
    case SectionUnknown:
    default: return QObject::tr("Music");
    }
}

QString sceneName(int scene)
{
    switch (scene)
    {
    case ScenePulse: return QObject::tr("Pulse");
    case SceneColorChase: return QObject::tr("Colour Chase");
    case SceneRainbowSweep: return QObject::tr("Rainbow Sweep");
    case SceneDualTone: return QObject::tr("Dual Tone");
    case SceneStrobeDrop: return QObject::tr("Strobe Drop");
    case SceneGoboSpin: return QObject::tr("Gobo Spin");
    case ScenePrismRush: return QObject::tr("Prism Rush");
    case SceneFanWave: return QObject::tr("Fan Wave");
    case SceneFigureEight: return QObject::tr("Figure Eight");
    case SceneSmokeBreak: return QObject::tr("Smoke Break");
    case SceneBuild: return QObject::tr("Build Up");
    case SceneWhiteOut: return QObject::tr("White Out");
    case SceneBallyhoo: return QObject::tr("Ballyhoo");
    default: return QObject::tr("Auto");
    }
}

QString colorModeName(int mode)
{
    switch (mode)
    {
    case ColorRainbow: return QObject::tr("Rainbow");
    case ColorDuotone: return QObject::tr("Duotone");
    case ColorWarm: return QObject::tr("Warm");
    case ColorCool: return QObject::tr("Cool");
    case ColorSingle: return QObject::tr("Single colour");
    case ColorAuto:
    default: return QObject::tr("Auto");
    }
}

QString movementStyleName(int style)
{
    switch (style)
    {
    case MoveSweep: return QObject::tr("Sweep");
    case MoveCircle: return QObject::tr("Circle");
    case MoveFan: return QObject::tr("Fan");
    case MoveFigure8: return QObject::tr("Figure eight");
    case MoveBallyhoo: return QObject::tr("Ballyhoo");
    case MoveStatic: return QObject::tr("Static");
    case MoveAuto:
    default: return QObject::tr("Auto");
    }
}

static QList<QColor> paletteForStyle(int style)
{
    QList<QColor> palette;
    switch (style)
    {
    case StyleParty:
        palette << QColor("#FF2DAF") << QColor("#00E5FF") << QColor("#FFE600")
                << QColor("#00FF66") << QColor("#FF7A00") << QColor("#FFFFFF")
                << QColor("#8A2BE2") << QColor("#FF2020");
        break;
    case StyleEDM:
        palette << QColor("#00BFFF") << QColor("#FF00FF") << QColor("#00FF80")
                << QColor("#FFFFFF") << QColor("#7B2CFF") << QColor("#FF3300");
        break;
    case StyleRock:
        palette << QColor("#FF2000") << QColor("#FF8000") << QColor("#FFFFFF")
                << QColor("#FFD000") << QColor("#FF0040");
        break;
    case StylePop:
        palette << QColor("#FF2DAF") << QColor("#00E5FF") << QColor("#FFE600")
                << QColor("#A65CFF") << QColor("#FFFFFF");
        break;
    case StyleHipHop:
        palette << QColor("#00FF66") << QColor("#9D00FF") << QColor("#FFD000")
                << QColor("#FFFFFF") << QColor("#FF4000");
        break;
    case StyleMetal:
        palette << QColor("#FF0000") << QColor("#FFFFFF") << QColor("#4000FF")
                << QColor("#FF8000") << QColor("#202020");
        break;
    case StyleJazz:
        palette << QColor("#FFB347") << QColor("#3A5FCD") << QColor("#FFE4B5")
                << QColor("#6A5ACD") << QColor("#FF8C69");
        break;
    case StyleReggae:
        palette << QColor("#E60000") << QColor("#FFD700") << QColor("#00A650")
                << QColor("#FFFFFF");
        break;
    case StyleAcoustic:
        palette << QColor("#FFDAB9") << QColor("#FFA07A") << QColor("#FFF8DC")
                << QColor("#87CEEB") << QColor("#DDA0DD");
        break;
    case StyleUnknown:
    default:
        palette << QColor("#FFFFFF") << QColor("#FF3030") << QColor("#30FF60")
                << QColor("#3060FF") << QColor("#FF30E0");
        break;
    }
    return palette;
}

static bool styleAvoidsStrobe(int style)
{
    return style == StyleJazz || style == StyleAcoustic;
}

static bool styleLikesGobo(int style)
{
    return style == StyleRock || style == StyleMetal || style == StyleEDM
            || style == StyleHipHop || style == StylePop || style == StyleParty;
}

static bool styleLikesPrism(int style)
{
    return style == StyleEDM || style == StylePop || style == StyleParty;
}

static int sectionBaseEnergy(int section)
{
    switch (section)
    {
    case SectionIntro: return 30;
    case SectionVerse: return 55;
    case SectionBuild: return 70;
    case SectionChorus: return 88;
    case SectionDrop: return 100;
    case SectionBreakdown: return 30;
    case SectionBridge: return 50;
    case SectionOutro: return 35;
    default: return 60;
    }
}

static int sectionFromName(const QString &name)
{
    const QString n = name.toLower();
    if (n.contains("intro")) return SectionIntro;
    if (n.contains("verse")) return SectionVerse;
    if (n.contains("build") || n.contains("rise")) return SectionBuild;
    if (n.contains("chorus") || n.contains("hook")) return SectionChorus;
    if (n.contains("drop")) return SectionDrop;
    if (n.contains("break") || n.contains("bridge")) return SectionBreakdown;
    if (n.contains("outro") || n.contains("end")) return SectionOutro;
    return SectionUnknown;
}

static double dbToEnergy(double db)
{
    return clamp01((db + 45.0) / 42.0);
}

int LiveMusicEngine::detectStyle(const Song &song)
{
    const QString text = (song.genre + " " + song.artist + " " + song.title).toLower();

    if (text.contains("metal") || text.contains("hardcore") || text.contains("scream"))
        return StyleMetal;
    if (text.contains("reggae") || text.contains("dancehall") || text.contains("ska"))
        return StyleReggae;
    if (text.contains("jazz") || text.contains("blues") || text.contains("swing"))
        return StyleJazz;
    if (text.contains("classical") || text.contains("acoustic") || text.contains("piano")
            || text.contains("ballad") || text.contains("folk"))
        return StyleAcoustic;
    if (text.contains("hip hop") || text.contains("hip-hop") || text.contains("rap")
            || text.contains("trap") || text.contains("drill"))
        return StyleHipHop;
    if (text.contains("rock") || text.contains("punk") || text.contains("grunge"))
        return StyleRock;
    if (text.contains("edm") || text.contains("house") || text.contains("techno")
            || text.contains("trance") || text.contains("dubstep") || text.contains("electro")
            || text.contains("eurodance") || text.contains("rave") || text.contains("club mix")
            || text.contains("2 unlimited"))
        return StyleEDM;
    if (text.contains("hampster") || text.contains("dance") || text.contains("party")
            || text.contains("disco") || text.contains("euro"))
        return StyleParty;
    if (text.contains("pop") || text.contains("charts") || text.contains("radio"))
        return StylePop;

    return StyleUnknown;
}

LiveMusicEngine::LiveMusicEngine(Doc *doc)
    : m_doc(doc)
    , m_dirty(true)
{
}

void LiveMusicEngine::setDoc(Doc *doc)
{
    m_doc = doc;
    m_dirty = true;
}

Doc *LiveMusicEngine::doc() const
{
    return m_doc;
}

void LiveMusicEngine::setFixtures(const QList<FixtureConfig> &fixtures)
{
    m_fixtures = fixtures;
    m_dirty = true;
}

QList<FixtureConfig> LiveMusicEngine::fixtures() const
{
    return m_fixtures;
}

void LiveMusicEngine::setSettings(const ShowSettings &settings)
{
    m_settings = settings;
}

ShowSettings LiveMusicEngine::settings() const
{
    return m_settings;
}

void LiveMusicEngine::setSong(const Song &song)
{
    m_song = song;
    m_dirty = true;
}

Song LiveMusicEngine::song() const
{
    return m_song;
}

void LiveMusicEngine::setPlayback(const Playback &playback)
{
    m_playback = playback;
}

Playback LiveMusicEngine::playback() const
{
    return m_playback;
}

void LiveMusicEngine::invalidate()
{
    m_dirty = true;
}

void LiveMusicEngine::classifyChannel(Profile &profile, quint32 index) const
{
    if (profile.fixture == NULL)
        return;

    const QLCChannel *channel = profile.fixture->channel(index);
    if (channel == NULL)
        return;

    ProfileChannel pc;
    pc.info.index = index;
    pc.info.group = channel->group();
    pc.info.preset = channel->preset();
    pc.info.colour = channel->colour();
    pc.info.controlByte = channel->controlByte();
    pc.info.name = channel->name();
    pc.group = channel->group();
    pc.colour = channel->colour();
    pc.preset = channel->preset();
    profile.channels.append(pc);

    const QString name = channel->name().toLower();
    const bool isFine = channel->controlByte() == QLCChannel::LSB;

    if (channel->group() == QLCChannel::Pan)
    {
        if (isFine)
            profile.panFine.append(index);
        else
            profile.pan.append(index);
        return;
    }
    if (channel->group() == QLCChannel::Tilt)
    {
        if (isFine)
            profile.tiltFine.append(index);
        else
            profile.tilt.append(index);
        return;
    }

    switch (channel->preset())
    {
    case QLCChannel::IntensityRed:
    case QLCChannel::IntensityRedFine:
        profile.red.append(index);
        return;
    case QLCChannel::IntensityGreen:
    case QLCChannel::IntensityGreenFine:
        profile.green.append(index);
        return;
    case QLCChannel::IntensityBlue:
    case QLCChannel::IntensityBlueFine:
        profile.blue.append(index);
        return;
    case QLCChannel::IntensityCyan:
    case QLCChannel::IntensityCyanFine:
        profile.cyan.append(index);
        return;
    case QLCChannel::IntensityMagenta:
    case QLCChannel::IntensityMagentaFine:
        profile.magenta.append(index);
        return;
    case QLCChannel::IntensityYellow:
    case QLCChannel::IntensityYellowFine:
        profile.yellow.append(index);
        return;
    case QLCChannel::IntensityWhite:
    case QLCChannel::IntensityWhiteFine:
        profile.white.append(index);
        return;
    case QLCChannel::IntensityAmber:
    case QLCChannel::IntensityAmberFine:
        profile.amber.append(index);
        return;
    case QLCChannel::IntensityUV:
    case QLCChannel::IntensityUVFine:
        profile.uv.append(index);
        return;
    case QLCChannel::ColorWheel:
    case QLCChannel::ColorWheelFine:
    case QLCChannel::ColorMacro:
        profile.colorWheel.append(index);
        return;
    case QLCChannel::GoboWheel:
    case QLCChannel::GoboWheelFine:
    case QLCChannel::GoboIndex:
    case QLCChannel::GoboIndexFine:
        profile.gobo.append(index);
        return;
    case QLCChannel::PrismRotationSlowFast:
    case QLCChannel::PrismRotationFastSlow:
        profile.prismRotation.append(index);
        return;
    case QLCChannel::ShutterStrobeSlowFast:
    case QLCChannel::ShutterStrobeFastSlow:
        profile.strobe.append(index);
        return;
    case QLCChannel::BeamZoomSmallBig:
    case QLCChannel::BeamZoomBigSmall:
    case QLCChannel::BeamZoomFine:
        profile.beamZoom.append(index);
        return;
    case QLCChannel::BeamFocusNearFar:
    case QLCChannel::BeamFocusFarNear:
    case QLCChannel::BeamFocusFine:
        profile.beamFocus.append(index);
        return;
    case QLCChannel::ShutterIrisMinToMax:
    case QLCChannel::ShutterIrisMaxToMin:
    case QLCChannel::ShutterIrisFine:
        profile.beamFocus.append(index);
        return;
    default:
        break;
    }

    if (channel->group() == QLCChannel::Colour)
    {
        if (channel->colour() != QLCChannel::NoColour)
        {
            switch (channel->colour())
            {
            case QLCChannel::Red: profile.red.append(index); break;
            case QLCChannel::Green: profile.green.append(index); break;
            case QLCChannel::Blue: profile.blue.append(index); break;
            case QLCChannel::Cyan: profile.cyan.append(index); break;
            case QLCChannel::Magenta: profile.magenta.append(index); break;
            case QLCChannel::Yellow: profile.yellow.append(index); break;
            case QLCChannel::White: profile.white.append(index); break;
            case QLCChannel::Amber: profile.amber.append(index); break;
            case QLCChannel::UV: profile.uv.append(index); break;
            default: profile.colorWheel.append(index); break;
            }
        }
        else
            profile.colorWheel.append(index);
        return;
    }

    if (channel->group() == QLCChannel::Gobo)
    {
        profile.gobo.append(index);
        return;
    }
    if (channel->group() == QLCChannel::Prism)
    {
        if (channel->preset() == QLCChannel::PrismRotationSlowFast
                || channel->preset() == QLCChannel::PrismRotationFastSlow)
            profile.prismRotation.append(index);
        else
            profile.prism.append(index);
        return;
    }
    if (channel->group() == QLCChannel::Shutter)
    {
        if (channel->preset() == QLCChannel::ShutterStrobeSlowFast
                || channel->preset() == QLCChannel::ShutterStrobeFastSlow)
            profile.strobe.append(index);
        else
            profile.shutter.append(index);
        return;
    }
    if (channel->group() == QLCChannel::Beam)
    {
        if (channel->preset() == QLCChannel::BeamZoomSmallBig
                || channel->preset() == QLCChannel::BeamZoomBigSmall
                || channel->preset() == QLCChannel::BeamZoomFine)
            profile.beamZoom.append(index);
        else
            profile.beamFocus.append(index);
        return;
    }

    if (channel->group() == QLCChannel::Intensity)
    {
        if (channel->colour() != QLCChannel::NoColour)
        {
            switch (channel->colour())
            {
            case QLCChannel::Red: profile.red.append(index); break;
            case QLCChannel::Green: profile.green.append(index); break;
            case QLCChannel::Blue: profile.blue.append(index); break;
            case QLCChannel::Cyan: profile.cyan.append(index); break;
            case QLCChannel::Magenta: profile.magenta.append(index); break;
            case QLCChannel::Yellow: profile.yellow.append(index); break;
            case QLCChannel::White: profile.white.append(index); break;
            case QLCChannel::Amber: profile.amber.append(index); break;
            case QLCChannel::UV: profile.uv.append(index); break;
            default: break;
            }
            return;
        }

        bool lampChannel = false;
        foreach (QLCCapability *cap, channel->capabilities())
        {
            if (cap->preset() == QLCCapability::LampOn
                    || cap->preset() == QLCCapability::LampOff)
            {
                lampChannel = true;
                break;
            }
        }
        if (lampChannel)
            profile.lamp.append(index);
        else if (isFine)
            profile.dimmerFine.append(index);
        else
            profile.dimmer.append(index);
        return;
    }

    if (profile.isSmoke || profile.isHazer)
    {
        if (channel->group() == QLCChannel::Effect
                || name.contains("smoke") || name.contains("fog")
                || name.contains("haze") || name.contains("fan")
                || name.contains("pump") || name.contains("heat"))
            profile.smoke.append(index);
    }
}

void LiveMusicEngine::rebuildProfiles()
{
    m_profiles.clear();
    if (m_doc == NULL)
        return;

    // Build profiles in fixture-id order so patterns are deterministic.
    QList<FixtureConfig> sorted = m_fixtures;
    std::sort(sorted.begin(), sorted.end(), [](const FixtureConfig &a, const FixtureConfig &b)
    {
        return a.fixtureId < b.fixtureId;
    });

    int count = 0;
    foreach (const FixtureConfig &config, sorted)
    {
        Fixture *fixture = m_doc->fixture(config.fixtureId);
        if (fixture == NULL)
            continue;
        count++;
    }

    int index = 0;
    foreach (const FixtureConfig &config, sorted)
    {
        Fixture *fixture = m_doc->fixture(config.fixtureId);
        if (fixture == NULL)
            continue;

        Profile profile;
        profile.fixture = fixture;
        profile.config = config;
        profile.index = index++;
        profile.count = qMax(1, count);
        profile.isSmoke = fixture->type() == QLCFixtureDef::Smoke;
        profile.isHazer = fixture->type() == QLCFixtureDef::Hazer;

        if (fixture->fixtureMode() != NULL)
        {
            const QLCPhysical physical = fixture->fixtureMode()->physical();
            profile.panMax = physical.focusPanMax() > 0 ? physical.focusPanMax() : 540;
            profile.tiltMax = physical.focusTiltMax() > 0 ? physical.focusTiltMax() : 270;
        }

        for (quint32 ch = 0; ch < fixture->channels(); ch++)
            classifyChannel(profile, ch);

        profile.hasDimmer = (profile.dimmer.isEmpty() == false
                             || profile.dimmerFine.isEmpty() == false);
        m_profiles.append(profile);
    }

    m_dirty = false;
}

bool LiveMusicEngine::channelAllowed(const Profile &profile, quint32 channel) const
{
    if (profile.config.allChannels)
        return true;
    return profile.config.allowedChannels.contains(channel);
}

void LiveMusicEngine::put(QMap<QPair<quint32, quint32>, uchar> &frame,
                          const Profile &profile, quint32 channel, int value) const
{
    if (profile.fixture == NULL || channelAllowed(profile, channel) == false)
        return;
    frame.insert(QPair<quint32, quint32>(profile.fixture->id(), channel),
                 uchar(qBound(0, value, 255)));
}

void LiveMusicEngine::putList(QMap<QPair<quint32, quint32>, uchar> &frame,
                              const Profile &profile, const QList<quint32> &channels,
                              int value) const
{
    foreach (quint32 channel, channels)
        put(frame, profile, channel, value);
}

double LiveMusicEngine::timelineEnergy(const Song &song, double position) const
{
    if (song.timeline.isEmpty())
        return -1.0;

    int lo = 0;
    int hi = song.timeline.size() - 1;
    while (lo < hi)
    {
        const int mid = (lo + hi) / 2;
        if (song.timeline.at(mid).time < position)
            lo = mid + 1;
        else
            hi = mid;
    }
    int i0 = qMax(0, lo - 1);
    int i1 = qMin(song.timeline.size() - 1, lo);
    if (i1 == i0)
        return dbToEnergy(song.timeline.at(i0).rmsDb);

    const TimelinePoint &a = song.timeline.at(i0);
    const TimelinePoint &b = song.timeline.at(i1);
    const double span = qMax(0.001, b.time - a.time);
    const double t = clamp01((position - a.time) / span);
    const double db = lerp(a.rmsDb, b.rmsDb, t);
    const double low = lerp(a.low, b.low, t);
    const double mid = lerp(a.mid, b.mid, t);
    const double high = lerp(a.high, b.high, t);
    const double band = clamp01(low * 0.6 + mid * 0.3 + high * 0.1);
    return clamp01(dbToEnergy(db) * 0.72 + band * 0.28);
}

int LiveMusicEngine::sectionAt(const Song &song, double position) const
{
    // Drops are the strongest structural cue, so let them win over the
    // (sometimes coarse) section labels from the analysis.
    foreach (const Drop &drop, song.drops)
    {
        if (position >= drop.time - 0.2 && position < drop.time + 12.0)
            return SectionDrop;
    }

    foreach (const Part &part, song.parts)
    {
        if (position >= part.start && position < part.end)
        {
            int section = sectionFromName(part.type + " " + part.label);
            if (section != SectionUnknown)
                return section;
        }
    }

    // No useful parts: infer from track progress.
    if (song.duration > 0)
    {
        const double progress = position / song.duration;
        if (progress < 0.08) return SectionIntro;
        if (progress > 0.92) return SectionOutro;
        if (progress < 0.25) return SectionVerse;
        if (progress < 0.72) return SectionChorus;
    }
    return SectionUnknown;
}

double LiveMusicEngine::dropStrength(const Song &song, double position) const
{
    double strength = 0.0;
    foreach (const Drop &drop, song.drops)
    {
        const double dt = position - drop.time;
        if (dt < 0.0 || dt > 4.0)
            continue;
        double s = qMax(0.35, drop.confidence) * std::exp(-dt / 1.35);
        if (dt < 0.12)
            s = qMax(s, 0.85);
        strength = qMax(strength, s);
    }
    return clamp01(strength);
}

double LiveMusicEngine::buildStrength(const Song &song, double position) const
{
    double best = 0.0;
    foreach (const Drop &drop, song.drops)
    {
        const double dt = drop.time - position;
        if (dt <= 0.0 || dt > 8.0)
            continue;
        const double s = 1.0 - smoothStep(dt / 8.0);
        best = qMax(best, s);
    }
    return clamp01(best);
}

static int movementFromScene(int scene)
{
    switch (scene)
    {
    case SceneGoboSpin:
    case ScenePrismRush: return MoveCircle;
    case SceneFanWave: return MoveFan;
    case SceneFigureEight: return MoveFigure8;
    case SceneBallyhoo: return MoveBallyhoo;
    case SceneSmokeBreak:
    case SceneBuild: return MoveSweep;
    case SceneRainbowSweep:
    case SceneColorChase: return MoveSweep;
    default: return MoveFigure8;
    }
}

int LiveMusicEngine::movementForScene(int scene, int style, int section) const
{
    if (m_settings.movementStyle != MoveAuto)
        return m_settings.movementStyle;

    if (section == SectionIntro || section == SectionOutro)
        return MoveSweep;
    if (section == SectionBreakdown)
        return (style == StyleJazz || style == StyleAcoustic) ? MoveStatic : MoveCircle;
    if (section == SectionDrop && (style == StyleEDM || style == StyleHipHop))
        return MoveBallyhoo;

    return movementFromScene(scene);
}

int LiveMusicEngine::sceneAt(const FrameInfo &info) const
{
    QList<int> candidates;
    switch (info.section)
    {
    case SectionIntro:
        candidates << ScenePulse << SceneRainbowSweep << SceneWhiteOut;
        break;
    case SectionVerse:
        candidates << SceneColorChase << SceneDualTone << SceneFanWave << SceneGoboSpin;
        break;
    case SectionBuild:
        candidates << SceneBuild << ScenePrismRush << SceneRainbowSweep << SceneGoboSpin;
        break;
    case SectionChorus:
        candidates << ScenePulse << SceneColorChase << SceneRainbowSweep
                   << SceneGoboSpin << ScenePrismRush << SceneFanWave
                   << SceneFigureEight << SceneBallyhoo;
        break;
    case SectionDrop:
        candidates << SceneStrobeDrop << SceneWhiteOut << ScenePrismRush
                   << SceneBallyhoo << SceneColorChase;
        break;
    case SectionBreakdown:
        candidates << ScenePulse << SceneDualTone << SceneSmokeBreak;
        break;
    case SectionOutro:
        candidates << ScenePulse << SceneDualTone;
        break;
    case SectionUnknown:
    default:
        candidates << ScenePulse << SceneColorChase << SceneRainbowSweep
                   << SceneDualTone << SceneFigureEight;
        break;
    }

    if (styleAvoidsStrobe(info.style))
    {
        candidates.removeAll(SceneStrobeDrop);
        candidates.removeAll(SceneWhiteOut);
    }
    if (styleLikesGobo(info.style) == false)
        candidates.removeAll(SceneGoboSpin);
    if (styleLikesPrism(info.style) == false)
        candidates.removeAll(ScenePrismRush);
    if (m_settings.allowStrobe == false)
        candidates.removeAll(SceneStrobeDrop);
    if (m_settings.allowSmoke == false)
        candidates.removeAll(SceneSmokeBreak);

    if (candidates.isEmpty())
        candidates << ScenePulse;

    int changeBars = qBound(1, m_settings.changeBars, 16);
    int phraseBars = qMax(1, changeBars);
    int sceneSlot = info.bar / phraseBars;
    quint32 h = hashString(m_song.id + m_song.title + m_song.artist);
    h = hashMix(h, quint32(sceneSlot + 1));
    h = hashMix(h, quint32(info.section + 1));
    int selected = candidates.at(int(h % quint32(candidates.size())));

    // Never repeat the exact same look two phrases in a row.
    int previousSlot = sceneSlot - 1;
    if (previousSlot >= 0)
    {
        quint32 ph = hashString(m_song.id + m_song.title + m_song.artist);
        ph = hashMix(ph, quint32(previousSlot + 1));
        ph = hashMix(ph, quint32(info.section + 1));
        int previous = candidates.at(int(ph % quint32(candidates.size())));
        if (previous == selected && candidates.size() > 1)
            selected = candidates.at((candidates.indexOf(selected) + 1) % candidates.size());
    }

    return selected;
}

QColor LiveMusicEngine::colorAt(const FrameInfo &info, int fixtureIndex,
                                int fixtureCount, int colorSlot) const
{
    QList<QColor> palette;

    if (m_settings.primaryColor.isValid() && m_settings.secondaryColor.isValid()
            && (m_settings.colorMode == ColorDuotone || m_settings.colorMode == ColorAuto))
    {
        palette << m_settings.primaryColor << m_settings.secondaryColor;
    }
    else
    {
        palette = paletteForStyle(info.style);
    }

    if (m_settings.colorMode == ColorSingle && m_settings.primaryColor.isValid())
        return m_settings.primaryColor;

    if (m_settings.colorMode == ColorWarm)
    {
        palette = QList<QColor>() << QColor("#FF3000") << QColor("#FF7A00")
                                  << QColor("#FFC000") << QColor("#FFF0D0");
    }
    else if (m_settings.colorMode == ColorCool)
    {
        palette = QList<QColor>() << QColor("#0030FF") << QColor("#00C0FF")
                                  << QColor("#7000FF") << QColor("#E0F0FF");
    }
    else if (m_settings.colorMode == ColorRainbow)
    {
        const double hue = std::fmod(info.position * 42.0
                                     + fixtureIndex * (300.0 / qMax(1, fixtureCount))
                                     + info.beat * 23.0, 360.0);
        return QColor::fromHsv(int(hue), 255, 255);
    }

    if (palette.isEmpty())
        palette << QColor(Qt::white);

    int cycle = colorSlot;
    if (info.section == SectionChorus || info.section == SectionDrop)
        cycle += (info.beat / 2);
    int paletteIndex = int((quint32(cycle) + quint32(info.scene)
                            + quint32(fixtureIndex / 2)) % quint32(palette.size()));
    QColor color = palette.at(paletteIndex);

    // On strong beats blend towards the next palette colour for a richer look.
    if (info.section == SectionDrop && info.beat % 2 == 1 && palette.size() > 1)
        color = palette.at((paletteIndex + 1) % palette.size());

    return color;
}

FrameInfo LiveMusicEngine::frameInfo(double position) const
{
    FrameInfo info;
    info.position = position;
    info.playing = m_playback.playing;

    double bpm = m_song.bpm;
    if (bpm < 20.0)
        bpm = 120.0;
    info.bpm = bpm;

    const double beatDuration = 60.0 / bpm;
    double beatPos = (position - m_song.beatOffset) / beatDuration;
    if (beatPos < 0.0)
        beatPos = 0.0;

    info.beat = int(std::floor(beatPos));
    info.bar = info.beat / 4;
    info.phrase = info.beat / 16;
    info.beatPhase = frac(beatPos);
    info.barPhase = frac(beatPos / 4.0);
    info.section = sectionAt(m_song, position);
    info.style = (m_settings.style == StyleAuto) ? detectStyle(m_song) : m_settings.style;
    info.drop = dropStrength(m_song, position);

    double measured = timelineEnergy(m_song, position);
    if (measured < 0.0)
        measured = sectionBaseEnergy(info.section) / 100.0;
    double energy = qMax(measured, sectionBaseEnergy(info.section) / 100.0 * 0.75);
    energy = clamp01(energy * (0.55 + m_settings.energy / 100.0 * 0.65));
    if (info.drop > 0.0)
        energy = clamp01(energy + info.drop * 0.25);
    info.energy = energy;

    info.scene = sceneAt(info);
    info.sceneName = sceneName(info.scene);
    info.activeFixtures = m_fixtures.size();

    return info;
}

QMap<QPair<quint32, quint32>, uchar> LiveMusicEngine::render(double position,
                                                              FrameInfo *infoPtr)
{
    FrameInfo info = frameInfo(position);
    if (infoPtr != NULL)
        *infoPtr = info;

    QMap<QPair<quint32, quint32>, uchar> frame;
    if (m_playback.playing == false || m_doc == NULL || m_fixtures.isEmpty())
        return frame;

    if (m_dirty)
        rebuildProfiles();

    const bool wantStrobe = m_settings.allowStrobe
            && styleAvoidsStrobe(info.style) == false
            && (info.drop > 0.15 || info.scene == SceneStrobeDrop
                || (info.section == SectionChorus && info.phrase % 8 == 6
                    && info.beat % 4 == 0));
    const double build = buildStrength(m_song, position);
    const int movement = movementForScene(info.scene, info.style, info.section);
    const double beatDuration = 60.0 / qMax(20.0, info.bpm);

    int movementPattern = movement;
    if (movementPattern == MoveAuto)
        movementPattern = MoveSweep;

    const double sectionSpeed = info.section == SectionDrop ? 2.0
            : info.section == SectionBuild ? 1.6
            : info.section == SectionChorus ? 1.0
            : info.section == SectionBreakdown ? 0.4
            : info.section == SectionIntro ? 0.55 : 0.8;
    const double phase = (position - m_song.beatOffset) / beatDuration
                         * 0.55 * sectionSpeed;

    foreach (const Profile &profile, m_profiles)
    {
        if (profile.fixture == NULL)
            continue;

        const int fi = profile.index;
        const int fc = qMax(1, profile.count);
        const double fixturePhase = (movementPattern == MoveFan
                                     || movementPattern == MoveBallyhoo)
                ? (double(fi) / fc) * 2.0 * M_PI : 0.0;

        // Beat envelope. Stronger on bar starts and during drops.
        const double beatPulse = std::pow(1.0 - info.beatPhase, 2.1);
        const double barPulse = (info.beat % 4 == 0) ? 1.0 : 0.35;
        const double pulse = clamp01(beatPulse * (0.7 + barPulse * 0.3)
                                     + info.drop * 0.55 + build * 0.10);

        const int roleColorSlot = (info.beat / 2) + (info.scene / 3);
        const QColor color = colorAt(info, fi, fc, roleColorSlot);

        double fixtureScale = profile.config.intensityLimit / 100.0;
        fixtureScale *= m_settings.intensityLimit / 100.0;
        double dim = clamp01((0.30 + info.energy * 0.60 + pulse * 0.32)
                             * fixtureScale);
        if (info.section == SectionIntro)
            dim *= 0.70;
        else if (info.section == SectionOutro)
            dim *= 0.65;
        if (info.drop > 0.05)
            dim = clamp01(dim + info.drop * 0.25);

        // Strobe fallback for fixtures without a strobe channel: rapid
        // intensity gating on drops. This is still bounded by the dimmer cap.
        if (wantStrobe && profile.strobe.isEmpty())
        {
            const double gate = std::fmod(position * 18.0, 1.0);
            if (gate > 0.52)
                dim *= 0.20;
            else
                dim = qMax(dim, 0.85);
        }

        const int dimValue = int(clamp01(dim) * 255.0);

        // Keep lamps switched on where a dedicated lamp channel exists.
        foreach (quint32 channel, profile.lamp)
        {
            const QLCChannel *ch = profile.fixture->channel(channel);
            if (ch == NULL)
                continue;
            int value = 200;
            foreach (QLCCapability *cap, ch->capabilities())
            {
                if (cap->preset() == QLCCapability::LampOn)
                {
                    value = cap->middle();
                    break;
                }
            }
            put(frame, profile, channel, value);
        }

        // Intensity / master dimmer.
        putList(frame, profile, profile.dimmer, dimValue);
        if (profile.dimmer.isEmpty())
            putList(frame, profile, profile.dimmerFine, dimValue);
        else
            putList(frame, profile, profile.dimmerFine, 0);

        const bool hasColor = m_settings.allowColor;
        const bool hasDimmer = profile.hasDimmer;

        if (hasColor)
        {
            // RGB mixing.
            const double colorScale = hasDimmer ? 1.0 : dim;
            const int r = int(color.red() * colorScale);
            const int g = int(color.green() * colorScale);
            const int b = int(color.blue() * colorScale);
            putList(frame, profile, profile.red, r);
            putList(frame, profile, profile.green, g);
            putList(frame, profile, profile.blue, b);

            // CMY mixing is inverted compared to RGB.
            putList(frame, profile, profile.cyan, 255 - r);
            putList(frame, profile, profile.magenta, 255 - g);
            putList(frame, profile, profile.yellow, 255 - b);

            // White / amber / UV accents.
            const int white = qMin(color.red(), qMin(color.green(), color.blue()));
            putList(frame, profile, profile.white, int(white * colorScale));
            putList(frame, profile, profile.amber,
                    int(qMin(255.0, color.red() * 0.7 + color.green() * 0.35) * colorScale));
            if (info.style == StyleEDM || info.style == StyleParty || info.scene == SceneWhiteOut)
                putList(frame, profile, profile.uv, int(160 * dim * clamp01(info.energy + 0.2)));
            else
                putList(frame, profile, profile.uv, 0);

            // Colour wheels / colour macros. Prefer a capability whose
            // stored colour matches the requested colour, otherwise use a
            // stable value inside the static-colour part of the wheel.
            foreach (quint32 channel, profile.colorWheel)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;

                int bestValue = -1;
                int bestDistance = INT_MAX;
                int staticMin = -1;
                int staticMax = -1;
                QList<QLCCapability *> caps = ch->capabilities();
                foreach (QLCCapability *cap, caps)
                {
                    const QColor capColor = colorFromCapability(ch, cap);
                    if (capColor.isValid())
                    {
                        const int dr = capColor.red() - color.red();
                        const int dg = capColor.green() - color.green();
                        const int db = capColor.blue() - color.blue();
                        const int distance = dr * dr + dg * dg + db * db;
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            bestValue = cap->middle();
                        }
                    }
                    if (staticMin < 0 && cap->max() > cap->min()
                            && cap->preset() != QLCCapability::ColorMacro
                            && cap->preset() != QLCCapability::ColorWheelIndex)
                    {
                        staticMin = cap->min();
                        staticMax = cap->max();
                    }
                }

                if (bestValue < 0)
                {
                    if (staticMin >= 0 && staticMax >= staticMin)
                    {
                        // No per-colour capability data: map the requested
                        // hue onto the wheel's static colour range.
                        int hue = color.hue();
                        if (hue < 0)
                            hue = (info.bar * 37 + fi * 53) % 360;
                        bestValue = staticMin
                                + (hue * (staticMax - staticMin + 1)) / 360;
                    }
                    else
                    {
                        bestValue = int(hashMix(hashString(m_song.id), quint32(info.bar + fi))
                                        % 96u);
                    }
                }
                put(frame, profile, channel, bestValue);
            }
        }
        else
        {
            putList(frame, profile, profile.red, 0);
            putList(frame, profile, profile.green, 0);
            putList(frame, profile, profile.blue, 0);
            putList(frame, profile, profile.cyan, 255);
            putList(frame, profile, profile.magenta, 255);
            putList(frame, profile, profile.yellow, 255);
        }

        // Coordinated stage movement.
        if (m_settings.allowMovement)
        {
            const double range = (profile.config.movementRange / 100.0)
                    * (m_settings.movementRange / 100.0);
            const double p = phase + fixturePhase;
            double pan = 0.5;
            double tilt = 0.5;

            switch (movementPattern)
            {
            case MoveCircle:
                pan = 0.5 + 0.42 * std::cos(p + fi * 0.35);
                tilt = 0.5 + 0.36 * std::sin(p + fi * 0.35);
                break;
            case MoveFan:
                pan = 0.5 + (fi - (fc - 1) / 2.0) * (0.62 / qMax(1, fc - 1))
                        + 0.10 * std::sin(p);
                tilt = 0.48 + 0.26 * std::sin(p * 0.9);
                break;
            case MoveFigure8:
                pan = 0.5 + 0.42 * std::sin(p + fi * 0.22);
                tilt = 0.5 + 0.34 * std::sin(2.0 * p + fi * 0.32);
                break;
            case MoveBallyhoo:
                pan = 0.5 + 0.42 * std::sin(p + (fi % 2) * M_PI);
                tilt = 0.5 + 0.34 * std::cos(p + (fi % 2) * M_PI);
                break;
            case MoveStatic:
                pan = 0.5 + 0.06 * std::sin(p * 0.35);
                tilt = 0.55 + 0.04 * std::cos(p * 0.3);
                break;
            case MoveSweep:
            default:
                pan = 0.5 + 0.42 * std::sin(p + fi * 0.28);
                tilt = 0.52 + 0.18 * std::sin(p * 0.55 + fi * 0.2);
                break;
            }

            pan = 0.5 + (pan - 0.5) * range;
            tilt = 0.5 + (tilt - 0.5) * range;
            if (profile.config.invertPan)
                pan = 1.0 - pan;
            if (profile.config.invertTilt)
                tilt = 1.0 - tilt;

            const int pan16 = int(clamp01(pan) * 65535.0);
            const int tilt16 = int(clamp01(tilt) * 65535.0);
            foreach (quint32 channel, profile.pan)
                put(frame, profile, channel, pan16 >> 8);
            foreach (quint32 channel, profile.panFine)
                put(frame, profile, channel, pan16 & 0xFF);
            foreach (quint32 channel, profile.tilt)
                put(frame, profile, channel, tilt16 >> 8);
            foreach (quint32 channel, profile.tiltFine)
                put(frame, profile, channel, tilt16 & 0xFF);
        }

        // Shutter / strobe.
        const bool strobeThisFixture = wantStrobe
                && (info.drop > 0.0 || info.scene == SceneStrobeDrop
                    || (fi % 3 == info.bar % 3));
        foreach (quint32 channel, profile.shutter)
        {
            const QLCChannel *ch = profile.fixture->channel(channel);
            if (ch == NULL)
                continue;
            int openValue = 0;
            foreach (QLCCapability *cap, ch->capabilities())
            {
                if (cap->preset() == QLCCapability::ShutterOpen)
                {
                    openValue = cap->middle();
                    break;
                }
            }
            put(frame, profile, channel, openValue);
        }

        if (strobeThisFixture)
        {
            foreach (quint32 channel, profile.strobe)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;
                int value = 128;
                foreach (QLCCapability *cap, ch->capabilities())
                {
                    if (cap->preset() == QLCCapability::StrobeSlowToFast
                            || cap->preset() == QLCCapability::StrobeFrequency)
                    {
                        const double strength = clamp01(0.35 + info.drop * 0.65);
                        value = int(lerp(cap->min(), cap->max(), strength));
                        break;
                    }
                    if (cap->preset() == QLCCapability::StrobeFastToSlow)
                    {
                        const double strength = clamp01(0.35 + info.drop * 0.65);
                        value = int(lerp(cap->max(), cap->min(), strength));
                        break;
                    }
                    if (cap->preset() == QLCCapability::StrobeRandom)
                    {
                        value = cap->middle();
                        break;
                    }
                }
                put(frame, profile, channel, value);
            }
        }

        // Gobos: choose a static gobo, changing with the bar/phrase.
        const bool wantGobo = m_settings.allowGobo
                && (styleLikesGobo(info.style) || info.scene == SceneGoboSpin)
                && (info.scene == SceneGoboSpin
                    || info.section == SectionChorus
                    || info.section == SectionDrop
                    || (info.section == SectionVerse && info.phrase % 2 == 1));
        if (wantGobo)
        {
            int goboSlot = info.bar / qMax(1, m_settings.changeBars) + fi;
            foreach (quint32 channel, profile.gobo)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;

                QList<int> staticValues;
                foreach (QLCCapability *cap, ch->capabilities())
                {
                    const QString capName = cap->name().toLower();
                    if (capName.contains("open") || capName.contains("no gobo")
                            || capName.contains("rotation") || capName.contains("shake")
                            || capName.contains("jitter") || capName.contains("macro"))
                        continue;
                    if (cap->preset() == QLCCapability::GoboShakeMacro)
                        continue;
                    staticValues << cap->middle();
                }
                if (staticValues.isEmpty() == false)
                {
                    int value = staticValues.at(int(quint32(goboSlot) % quint32(staticValues.size())));
                    put(frame, profile, channel, value);
                }
                else
                {
                    // Fall back to a value in the static gobo part of the wheel.
                    put(frame, profile, channel, int(hashMix(hashString(m_song.id),
                                                              quint32(goboSlot + fi)) % 64u));
                }
            }
        }
        else
        {
            // Leave colour/gobo wheels alone when not requested; they hold
            // their last position and avoid pointless DMX churn.
        }

        // Prism: use it as an accent, not a permanent effect.
        const bool wantPrism = m_settings.allowPrism
                && (styleLikesPrism(info.style) || info.scene == ScenePrismRush)
                && (info.scene == ScenePrismRush
                    || info.section == SectionDrop
                    || (info.section == SectionChorus && info.phrase % 2 == 0));
        foreach (quint32 channel, profile.prism)
        {
            const QLCChannel *ch = profile.fixture->channel(channel);
            if (ch == NULL)
                continue;
            int value = wantPrism ? 200 : 0;
            foreach (QLCCapability *cap, ch->capabilities())
            {
                if (wantPrism && cap->preset() == QLCCapability::PrismEffectOn)
                {
                    value = cap->middle();
                    break;
                }
                if (!wantPrism && cap->preset() == QLCCapability::PrismEffectOff)
                {
                    value = cap->middle();
                    break;
                }
            }
            put(frame, profile, channel, value);
        }
        if (wantPrism)
        {
            foreach (quint32 channel, profile.prismRotation)
            {
                const int speed = int(clamp01(0.25 + info.energy * 0.55
                                              + info.drop * 0.2) * 255.0);
                put(frame, profile, channel, speed);
            }
        }

        // Beam / zoom accents. Builds narrow the beam, drops open it up.
        if (m_settings.allowBeam)
        {
            const double zoomTarget = info.section == SectionBuild
                    ? 0.15 + 0.35 * (1.0 - build)
                    : info.section == SectionDrop ? 0.85
                    : info.section == SectionIntro ? 0.35
                    : 0.55 + 0.2 * std::sin(position * 0.4 + fi);
            foreach (quint32 channel, profile.beamZoom)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;
                int value = int(clamp01(zoomTarget) * 255.0);
                foreach (QLCCapability *cap, ch->capabilities())
                {
                    if (cap->preset() == QLCCapability::BigToSmall)
                    {
                        value = int(lerp(cap->max(), cap->min(), zoomTarget));
                        break;
                    }
                    if (cap->preset() == QLCCapability::SmallToBig)
                    {
                        value = int(lerp(cap->min(), cap->max(), zoomTarget));
                        break;
                    }
                }
                put(frame, profile, channel, value);
            }
            foreach (quint32 channel, profile.beamFocus)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;
                int value = int(clamp01(0.5 + 0.25 * std::sin(position * 0.3 + fi)) * 255.0);
                foreach (QLCCapability *cap, ch->capabilities())
                {
                    if (cap->preset() == QLCCapability::NearToFar
                            || cap->preset() == QLCCapability::SmallToBig)
                    {
                        value = int(lerp(cap->min(), cap->max(), 0.5));
                        break;
                    }
                }
                put(frame, profile, channel, value);
            }
        }

        // Smoke / haze. Smoke machines get punchy bursts on drops and
        // phrase changes; hazers get a low continuous bed during choruses.
        if (m_settings.allowSmoke && (profile.isSmoke || profile.isHazer))
        {
            double smoke = 0.0;
            if (profile.isHazer)
            {
                if (info.section == SectionChorus || info.section == SectionDrop
                        || info.section == SectionBuild)
                    smoke = 0.18 + info.energy * 0.20;
                if (info.drop > 0.1)
                    smoke = qMax(smoke, 0.55);
            }
            else
            {
                if (info.drop > 0.02)
                    smoke = qMax(smoke, info.drop * 0.85);
                if (info.section == SectionChorus && info.phrase % 4 == 0
                        && info.beat % 16 == 0)
                    smoke = qMax(smoke, 0.55);
                if (info.scene == SceneSmokeBreak)
                    smoke = qMax(smoke, 0.45);
            }
            const int smokeValue = int(clamp01(smoke) * 230.0);
            foreach (quint32 channel, profile.smoke)
            {
                const QLCChannel *ch = profile.fixture->channel(channel);
                if (ch == NULL)
                    continue;
                int value = smokeValue;
                // Fan channels should run whenever smoke is produced.
                if (ch->name().toLower().contains("fan"))
                    value = smoke > 0.05 ? qMax(120, smokeValue) : 0;
                put(frame, profile, channel, value);
            }
        }
    }

    info.outputChannels = frame.size();
    if (infoPtr != NULL)
        *infoPtr = info;

    return frame;
}

QList<ChannelInfo> LiveMusicEngine::channelInfos(quint32 fixtureId) const
{
    QList<ChannelInfo> infos;
    if (m_doc == NULL)
        return infos;
    Fixture *fixture = m_doc->fixture(fixtureId);
    if (fixture == NULL)
        return infos;

    for (quint32 i = 0; i < fixture->channels(); i++)
    {
        const QLCChannel *channel = fixture->channel(i);
        if (channel == NULL)
            continue;
        ChannelInfo info;
        info.index = i;
        info.group = channel->group();
        info.preset = channel->preset();
        info.colour = channel->colour();
        info.controlByte = channel->controlByte();
        info.name = channel->name();
        infos.append(info);
    }
    return infos;
}

QMap<QString, QList<quint32> > LiveMusicEngine::defaultRoles(quint32 fixtureId) const
{
    QMap<QString, QList<quint32> > roles;
    if (m_doc == NULL)
        return roles;
    Fixture *fixture = m_doc->fixture(fixtureId);
    if (fixture == NULL)
        return roles;

    Profile profile;
    profile.fixture = fixture;
    profile.isSmoke = fixture->type() == QLCFixtureDef::Smoke;
    profile.isHazer = fixture->type() == QLCFixtureDef::Hazer;
    for (quint32 i = 0; i < fixture->channels(); i++)
        classifyChannel(profile, i);

    roles.insert("intensity", profile.dimmer + profile.dimmerFine + profile.lamp);
    roles.insert("color", profile.red + profile.green + profile.blue
                 + profile.cyan + profile.magenta + profile.yellow
                 + profile.white + profile.amber + profile.uv
                 + profile.colorWheel);
    roles.insert("movement", profile.pan + profile.panFine + profile.tilt + profile.tiltFine);
    roles.insert("gobo", profile.gobo);
    roles.insert("prism", profile.prism + profile.prismRotation);
    roles.insert("shutter", profile.shutter + profile.strobe);
    roles.insert("beam", profile.beamZoom + profile.beamFocus);
    roles.insert("smoke", profile.smoke);
    return roles;
}

QColor LiveMusicEngine::colorFromCapability(const QLCChannel *channel,
                                            const QLCCapability *capability)
{
    if (capability == NULL)
        return QColor();

    const QVariantList resources = capability->resources();
    if (resources.isEmpty() == false)
    {
        const QString str = resources.first().toString().trimmed();
        QColor color(str);
        if (color.isValid())
            return color;
        if (str.startsWith("#") == false)
        {
            color = QColor("#" + str);
            if (color.isValid())
                return color;
        }
    }

    if (channel != NULL && channel->colour() != QLCChannel::NoColour)
    {
        switch (channel->colour())
        {
        case QLCChannel::Red: return QColor(Qt::red);
        case QLCChannel::Green: return QColor(Qt::green);
        case QLCChannel::Blue: return QColor(Qt::blue);
        case QLCChannel::Cyan: return QColor(Qt::cyan);
        case QLCChannel::Magenta: return QColor(Qt::magenta);
        case QLCChannel::Yellow: return QColor(Qt::yellow);
        case QLCChannel::White: return QColor(Qt::white);
        case QLCChannel::Amber: return QColor(255, 160, 0);
        case QLCChannel::UV: return QColor(120, 0, 255);
        default: break;
        }
    }

    const QString name = capability->name().toLower();
    if (name.contains("white") || name.contains("open")) return QColor(Qt::white);
    if (name.contains("red")) return QColor(Qt::red);
    if (name.contains("green")) return QColor(Qt::green);
    if (name.contains("blue")) return QColor(Qt::blue);
    if (name.contains("cyan")) return QColor(Qt::cyan);
    if (name.contains("magenta")) return QColor(Qt::magenta);
    if (name.contains("yellow")) return QColor(Qt::yellow);
    if (name.contains("orange")) return QColor(255, 140, 0);
    if (name.contains("pink")) return QColor(255, 105, 180);
    if (name.contains("purple") || name.contains("violet")) return QColor(150, 0, 255);
    if (name.contains("amber")) return QColor(255, 160, 0);

    return QColor();
}

} // namespace LiveMusic
