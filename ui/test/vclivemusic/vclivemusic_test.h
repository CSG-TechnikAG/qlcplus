/*
  Q Light Controller Plus - Test Unit
  vclivemusic_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef VCLIVEMUSIC_TEST_H
#define VCLIVEMUSIC_TEST_H

#include <QObject>

#include "vclivemusicpreset.h"

class Doc;

class VCLiveMusic_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase() { }
    void init();
    void cleanup();

    void initial();
    void presets();
    void saveLoad();
    void externalInput();

private:
    VCLiveMusicPreset makePreset(quint8 id, const QString &name);

    Doc *m_doc;
};

#endif // VCLIVEMUSIC_TEST_H
