/*
  Q Light Controller Plus
  preview3d_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PREVIEW3D_TEST_H
#define PREVIEW3D_TEST_H

#include <QObject>

class Preview3D_Test final : public QObject
{
    Q_OBJECT

private slots:
    void backendNamesRoundTrip();
    void settingsRoundTrip();
    void autoFallsBackToSoftware();
    void explicitGpuSelection();
    void forcedGpuFallback();
    void gpuContextLoss();
    void gpuStaticFrame();
    void gpuEffectsFrame();
    void softwareRasterCounters();
};

#endif
