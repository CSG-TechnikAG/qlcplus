/*
  Q Light Controller Plus - Unit test
  livemusicengine_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QtTest>
#include <QDir>

#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "livemusicengine.h"
#include "qlcfile.h"
#include "fixture.h"
#include "doc.h"

#include "livemusicengine_test.h"
#include "../common/resource_paths.h"

void LiveMusicEngine_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir) == true);
}

void LiveMusicEngine_Test::cleanupTestCase()
{
    delete m_doc;
}

void LiveMusicEngine_Test::init()
{
    m_doc->clearContents();
}

void LiveMusicEngine_Test::cleanup()
{
}

Fixture *LiveMusicEngine_Test::addFixture(const QString &manufacturer,
                                          const QString &model,
                                          const QString &modeName,
                                          quint32 address)
{
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(manufacturer, model);
    if (def == NULL)
        return NULL;
    QLCFixtureMode *mode = def->mode(modeName);
    if (mode == NULL)
        return NULL;

    Fixture *fixture = new Fixture(m_doc);
    fixture->setFixtureDefinition(def, mode);
    fixture->setAddress(address);
    fixture->setUniverse(0);
    if (m_doc->addFixture(fixture) == false)
    {
        delete fixture;
        return NULL;
    }
    return fixture;
}

LiveMusic::Song LiveMusicEngine_Test::makeSong()
{
    LiveMusic::Song song;
    song.id = "test-song";
    song.title = "Test Anthem";
    song.artist = "Unit Test";
    song.genre = "Dance";
    song.duration = 120.0;
    song.bpm = 120.0;
    song.beatOffset = 0.0;
    song.lowConfidence = false;

    LiveMusic::Part intro;
    intro.start = 0.0;
    intro.end = 8.0;
    intro.type = "intro";
    intro.label = "Intro";
    intro.energyDb = -22.0;
    song.parts.append(intro);

    LiveMusic::Part verse;
    verse.start = 8.0;
    verse.end = 40.0;
    verse.type = "verse";
    verse.label = "Verse 1";
    verse.energyDb = -12.0;
    song.parts.append(verse);

    LiveMusic::Part chorus;
    chorus.start = 40.0;
    chorus.end = 90.0;
    chorus.type = "chorus";
    chorus.label = "Chorus 1";
    chorus.energyDb = -8.0;
    song.parts.append(chorus);

    LiveMusic::Part outro;
    outro.start = 90.0;
    outro.end = 120.0;
    outro.type = "outro";
    outro.label = "Outro";
    outro.energyDb = -20.0;
    song.parts.append(outro);

    LiveMusic::Drop drop;
    drop.time = 10.0;
    drop.confidence = 0.9;
    drop.type = "drop";
    drop.label = "Drop";
    song.drops.append(drop);

    return song;
}

LiveMusic::Playback LiveMusicEngine_Test::makePlayback()
{
    LiveMusic::Playback playback;
    playback.active = true;
    playback.playing = true;
    playback.position = 0.0;
    playback.duration = 120.0;
    playback.id = "test-song";
    playback.title = "Test Anthem";
    playback.artist = "Unit Test";
    playback.genre = "Dance";
    return playback;
}

void LiveMusicEngine_Test::styleDetection()
{
    LiveMusic::Song song;
    song.title = "The HampsterDance Song";
    QCOMPARE(LiveMusic::LiveMusicEngine::detectStyle(song), int(LiveMusic::StyleParty));

    song.title.clear();
    song.genre = "House / Techno";
    QCOMPARE(LiveMusic::LiveMusicEngine::detectStyle(song), int(LiveMusic::StyleEDM));

    song.genre = "Jazz";
    QCOMPARE(LiveMusic::LiveMusicEngine::detectStyle(song), int(LiveMusic::StyleJazz));

    song.genre.clear();
    QCOMPARE(LiveMusic::LiveMusicEngine::detectStyle(song), int(LiveMusic::StyleUnknown));
}

void LiveMusicEngine_Test::channelAllowance()
{
    Fixture *head = addFixture("Lixada", "Mini Gobo Moving Head", "11 Channel", 0);
    QVERIFY(head != NULL);

    LiveMusic::LiveMusicEngine engine(m_doc);
    LiveMusic::FixtureConfig config;
    config.fixtureId = head->id();
    config.allChannels = false;
    config.allowedChannels.insert(0); // Pan
    config.allowedChannels.insert(1); // Pan fine
    engine.setFixtures(QList<LiveMusic::FixtureConfig>() << config);
    engine.setSettings(LiveMusic::ShowSettings());
    engine.setSong(makeSong());
    engine.setPlayback(makePlayback());

    const QMap<QPair<quint32, quint32>, uchar> frame = engine.render(12.34);
    QVERIFY(frame.isEmpty() == false);
    for (QMap<QPair<quint32, quint32>, uchar>::const_iterator it = frame.constBegin();
         it != frame.constEnd(); ++it)
    {
        QCOMPARE(it.key().first, head->id());
        QVERIFY(it.key().second == 0 || it.key().second == 1);
    }

    // An explicitly empty allowance must not emit anything at all.
    config.allowedChannels.clear();
    engine.setFixtures(QList<LiveMusic::FixtureConfig>() << config);
    QVERIFY(engine.render(12.34).isEmpty());
}

void LiveMusicEngine_Test::beatAndDrop()
{
    Fixture *par = addFixture("Showtec", "Compact Par 18 MKII", "4 Channel", 0);
    QVERIFY(par != NULL);

    LiveMusic::LiveMusicEngine engine(m_doc);
    LiveMusic::FixtureConfig config;
    config.fixtureId = par->id();
    config.allChannels = true;

    LiveMusic::ShowSettings settings;
    settings.allowStrobe = true;
    engine.setFixtures(QList<LiveMusic::FixtureConfig>() << config);
    engine.setSettings(settings);
    engine.setSong(makeSong());
    engine.setPlayback(makePlayback());

    LiveMusic::FrameInfo info;
    const QMap<QPair<quint32, quint32>, uchar> dropFrame = engine.render(10.05, &info);
    QVERIFY(dropFrame.isEmpty() == false);
    QCOMPARE(info.section, int(LiveMusic::SectionDrop));
    QVERIFY(info.drop > 0.5);
    QVERIFY(info.bpm == 120.0);
    QVERIFY(info.beatPhase >= 0.0 && info.beatPhase < 1.0);

    // Same beat phase, but long after the drop: the drop pulse must be gone.
    LiveMusic::FrameInfo laterInfo;
    const QMap<QPair<quint32, quint32>, uchar> laterFrame = engine.render(20.05, &laterInfo);
    QVERIFY(laterFrame.isEmpty() == false);
    QCOMPARE(laterInfo.beatPhase, info.beatPhase);
    QVERIFY(laterInfo.drop < info.drop);

    // Movement changes between beat positions.
    QVERIFY(engine.render(12.00).value(QPair<quint32, quint32>(par->id(), 0)) !=
            engine.render(12.20).value(QPair<quint32, quint32>(par->id(), 0)));
}

void LiveMusicEngine_Test::stageRoles()
{
    Fixture *head = addFixture("Lixada", "Mini Gobo Moving Head", "11 Channel", 0);
    Fixture *par = addFixture("Showtec", "Compact Par 18 MKII", "6 Channel", 20);
    Fixture *hazer = addFixture("Antari", "F-5D Fazer", "1 Channel", 40);
    QVERIFY(head != NULL);
    QVERIFY(par != NULL);
    QVERIFY(hazer != NULL);

    LiveMusic::LiveMusicEngine engine(m_doc);
    QList<LiveMusic::FixtureConfig> configs;
    Fixture *fixtureList[3] = { head, par, hazer };
    for (int i = 0; i < 3; i++)
    {
        Fixture *fixture = fixtureList[i];
        LiveMusic::FixtureConfig config;
        config.fixtureId = fixture->id();
        config.allChannels = true;
        configs << config;
    }

    LiveMusic::ShowSettings settings;
    settings.allowStrobe = true;
    settings.allowGobo = true;
    settings.allowPrism = true;
    settings.allowBeam = true;
    settings.allowSmoke = true;
    settings.allowMovement = true;
    settings.energy = 85;

    engine.setFixtures(configs);
    engine.setSettings(settings);
    engine.setSong(makeSong());
    engine.setPlayback(makePlayback());

    const QMap<QPair<quint32, quint32>, uchar> frame = engine.render(10.05);
    QVERIFY(frame.size() > 10);

    // Movement: pan/tilt channels of the moving head are driven.
    QVERIFY(frame.contains(QPair<quint32, quint32>(head->id(), 0)));
    QVERIFY(frame.contains(QPair<quint32, quint32>(head->id(), 2)));
    // Colour: RGB of the par, colour wheel of the head.
    QVERIFY(frame.contains(QPair<quint32, quint32>(par->id(), 1)));
    QVERIFY(frame.contains(QPair<quint32, quint32>(par->id(), 2)));
    QVERIFY(frame.contains(QPair<quint32, quint32>(par->id(), 3)));
    QVERIFY(frame.contains(QPair<quint32, quint32>(head->id(), 4)));
    // Gobo / shutter of the moving head.
    QVERIFY(frame.contains(QPair<quint32, quint32>(head->id(), 5)));
    QVERIFY(frame.contains(QPair<quint32, quint32>(head->id(), 9)));
    // Haze output during the drop.
    QVERIFY(frame.contains(QPair<quint32, quint32>(hazer->id(), 0)));
    QVERIFY(frame.value(QPair<quint32, quint32>(hazer->id(), 0)) > 0);

    const QMap<QString, QList<quint32> > roles =
            engine.defaultRoles(head->id());
    QVERIFY(roles.value("movement").contains(0));
    QVERIFY(roles.value("color").contains(4));
    QVERIFY(roles.value("gobo").contains(5));
    QVERIFY(roles.value("shutter").isEmpty() == false);
}

void LiveMusicEngine_Test::sectionAndScene()
{
    LiveMusic::LiveMusicEngine engine(m_doc);
    engine.setSong(makeSong());
    engine.setPlayback(makePlayback());

    LiveMusic::FrameInfo info = engine.frameInfo(2.0);
    QCOMPARE(info.section, int(LiveMusic::SectionIntro));
    info = engine.frameInfo(42.0);
    QCOMPARE(info.section, int(LiveMusic::SectionChorus));
    info = engine.frameInfo(95.0);
    QCOMPARE(info.section, int(LiveMusic::SectionOutro));

    QVERIFY(LiveMusic::sceneName(info.scene).isEmpty() == false);
    QVERIFY(QString(LiveMusic::styleName(info.style)).isEmpty() == false);
}

QTEST_MAIN(LiveMusicEngine_Test)
