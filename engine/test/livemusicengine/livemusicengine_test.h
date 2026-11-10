/*
  Q Light Controller Plus - Unit test
  livemusicengine_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef LIVEMUSICENGINE_TEST_H
#define LIVEMUSICENGINE_TEST_H

#include <QObject>

#include "livemusicengine.h"

class Doc;
class Fixture;

class LiveMusicEngine_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void styleDetection();
    void channelAllowance();
    void beatAndDrop();
    void stageRoles();
    void sectionAndScene();

private:
    Fixture *addFixture(const QString &manufacturer, const QString &model,
                        const QString &modeName, quint32 address);
    LiveMusic::Song makeSong();
    LiveMusic::Playback makePlayback();

    Doc *m_doc;
};

#endif // LIVEMUSICENGINE_TEST_H
