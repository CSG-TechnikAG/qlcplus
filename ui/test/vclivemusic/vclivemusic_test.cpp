/*
  Q Light Controller Plus - Test Unit
  vclivemusic_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QtTest>
#include <QBuffer>
#include <QSignalSpy>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define protected public
#define private public
#include "virtualconsole.h"
#include "vclivemusic.h"
#include "vclivemusicpreset.h"
#include "vclivemusic_test.h"
#include "qlcinputsource.h"
#include "vcwidget.h"
#include "doc.h"
#undef private
#undef protected

void VCLiveMusic_Test::initTestCase()
{
    m_doc = NULL;
}

void VCLiveMusic_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
}

void VCLiveMusic_Test::cleanup()
{
    delete VirtualConsole::instance();
    delete m_doc;
}

VCLiveMusicPreset VCLiveMusic_Test::makePreset(quint8 id, const QString &name)
{
    VCLiveMusicPreset preset(id);
    preset.m_name = name;
    preset.m_settings.style = LiveMusic::StyleEDM;
    preset.m_settings.colorMode = LiveMusic::ColorDuotone;
    preset.m_settings.movementStyle = LiveMusic::MoveCircle;
    preset.m_settings.energy = 77;
    preset.m_settings.intensityLimit = 82;
    preset.m_settings.movementRange = 63;
    preset.m_settings.allowStrobe = true;
    preset.m_settings.allowGobo = false;
    preset.m_settings.primaryColor = QColor("#ff0000");
    preset.m_settings.secondaryColor = QColor("#00ff00");
    preset.m_settings.changeBars = 8;

    LiveMusic::FixtureConfig config;
    config.fixtureId = 7;
    config.allChannels = false;
    config.allowedChannels.insert(2);
    config.allowedChannels.insert(4);
    config.intensityLimit = 45;
    config.movementRange = 35;
    config.invertPan = true;
    preset.m_fixtures.append(config);

    preset.m_keySequence = QKeySequence(Qt::Key_F5);
    return preset;
}

void VCLiveMusic_Test::initial()
{
    QWidget parent;
    VCLiveMusic live(&parent, m_doc);

    QCOMPARE(live.type(), int(VCWidget::LiveMusicWidget));
    QCOMPARE(VCWidget::typeToString(VCWidget::LiveMusicWidget), QString("Live Music"));
    QCOMPARE(live.liveEnabled(), false);
    QCOMPARE(live.presets().size(), 0);
    QCOMPARE(live.currentPresetIndex(), -1);
}

void VCLiveMusic_Test::presets()
{
    QWidget parent;
    VCLiveMusic live(&parent, m_doc);

    live.addPreset(makePreset(1, "Warm"));
    VCLiveMusicPreset second = makePreset(2, "Party");
    second.m_settings.style = LiveMusic::StyleParty;
    live.addPreset(second);

    QCOMPARE(live.presets().size(), 2);
    QCOMPARE(live.currentPresetIndex(), 0);
    QCOMPARE(live.currentPreset()->m_name, QString("Warm"));

    live.setCurrentPresetIndex(1);
    QCOMPARE(live.currentPreset()->m_name, QString("Party"));
    QCOMPARE(live.currentPreset()->m_settings.style, int(LiveMusic::StyleParty));
    QCOMPARE(live.currentPreset()->m_fixtures.size(), 1);
    QCOMPARE(live.currentPreset()->m_fixtures.first().allowedChannels.size(), 2);

    live.setLiveEnabled(true);
    QCOMPARE(live.liveEnabled(), true);
    live.setLiveEnabled(false);
    QCOMPARE(live.liveEnabled(), false);
}

void VCLiveMusic_Test::saveLoad()
{
    QWidget parent;
    VCLiveMusic live(&parent, m_doc);
    live.addPreset(makePreset(1, "Warm"));
    live.setCurrentPresetIndex(0);
    live.setLiveEnabled(true);
    live.setEnableKeySequence(QKeySequence(Qt::Key_F9));

    QSharedPointer<QLCInputSource> source(new QLCInputSource(2, 12));
    source->setFeedbackValue(QLCInputFeedback::UpperValue, 200);
    source->setFeedbackValue(QLCInputFeedback::LowerValue, 10);
    live.setInputSource(source, VCLiveMusic::enableInputSourceId);

    QByteArray data;
    QXmlStreamWriter writer(&data);
    QVERIFY(live.saveXML(&writer) == true);
    QVERIFY(data.contains("LiveMusic"));

    QWidget parent2;
    VCLiveMusic loaded(&parent2, m_doc);
    QBuffer buffer(&data);
    QVERIFY(buffer.open(QIODevice::ReadOnly) == true);
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement() == true);
    QCOMPARE(reader.name().toString(), QString("LiveMusic"));
    QVERIFY(loaded.loadXML(reader) == true);

    QCOMPARE(loaded.liveEnabled(), true);
    QCOMPARE(loaded.presets().size(), 1);
    QCOMPARE(loaded.currentPreset()->m_name, QString("Warm"));
    QCOMPARE(loaded.currentPreset()->m_settings.energy, 77);
    QCOMPARE(loaded.currentPreset()->m_settings.intensityLimit, 82);
    QCOMPARE(loaded.currentPreset()->m_settings.movementRange, 63);
    QCOMPARE(loaded.currentPreset()->m_settings.allowGobo, false);
    QCOMPARE(loaded.currentPreset()->m_settings.changeBars, 8);
    QCOMPARE(loaded.currentPreset()->m_fixtures.first().fixtureId, 7u);
    QCOMPARE(loaded.currentPreset()->m_fixtures.first().allowedChannels.size(), 2);
    QCOMPARE(loaded.currentPreset()->m_fixtures.first().invertPan, true);
    QCOMPARE(loaded.enableKeySequence(), QKeySequence(Qt::Key_F9));

    QSharedPointer<QLCInputSource> loadedSource =
            loaded.inputSource(VCLiveMusic::enableInputSourceId);
    QVERIFY(loadedSource.isNull() == false);
    QCOMPARE(loadedSource->universe(), 2u);
    QCOMPARE(loadedSource->channel(), 12u);

    loaded.setLiveEnabled(false);
    loaded.setInputSource(QSharedPointer<QLCInputSource>(),
                          VCLiveMusic::enableInputSourceId);
}

void VCLiveMusic_Test::externalInput()
{
    QWidget parent;
    VCLiveMusic live(&parent, m_doc);
    live.addPreset(makePreset(1, "First"));
    VCLiveMusicPreset second = makePreset(2, "Second");
    live.addPreset(second);

    QSharedPointer<QLCInputSource> enable(new QLCInputSource(0, 4));
    QSharedPointer<QLCInputSource> preset(new QLCInputSource(0, 5));
    live.setInputSource(enable, VCLiveMusic::enableInputSourceId);
    live.setInputSource(preset, 2); // preset ID 2

    m_doc->setMode(Doc::Operate);
    live.setEnabled(true);

    live.slotInputValueChanged(0, 4, 255);
    QCOMPARE(live.liveEnabled(), true);
    live.slotInputValueChanged(0, 4, 0);
    QCOMPARE(live.liveEnabled(), false);

    live.slotInputValueChanged(0, 5, 255);
    QCOMPARE(live.currentPresetIndex(), 1);
    QCOMPARE(live.currentPreset()->m_name, QString("Second"));

    live.setLiveEnabled(false);
    live.setInputSource(QSharedPointer<QLCInputSource>(),
                        VCLiveMusic::enableInputSourceId);
    live.setInputSource(QSharedPointer<QLCInputSource>(), 2);
    m_doc->setMode(Doc::Design);
}

QTEST_MAIN(VCLiveMusic_Test)
