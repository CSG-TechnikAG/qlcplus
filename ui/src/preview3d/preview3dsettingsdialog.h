/*
  Q Light Controller Plus
  preview3dsettingsdialog.h

  M5 settings, fallback diagnostics and capture actions for the 3D Preview.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PREVIEW3DSETTINGSDIALOG_H
#define PREVIEW3DSETTINGSDIALOG_H

#include <QDialog>

#include "preview3drenderbackend.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPlainTextEdit;

class Preview3DSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    Preview3DSettingsDialog(const Preview3DRenderSettings &settings,
                            bool gpuAvailable, const QString &diagnostics,
                            QWidget *parent = NULL);

    /** Settings selected by the user. */
    Preview3DRenderSettings settings() const;

signals:
    /** Ask Preview3D to capture the current preview image. */
    void capturePreviewRequested();

private slots:
    void slotCopyDiagnostics();
    void slotSaveDiagnostics();
    void slotResetToSoftware();

private:
    Preview3DRenderSettings m_initial;
    QComboBox *m_rendererCombo;
    QComboBox *m_qualityCombo;
    QDoubleSpinBox *m_scaleSpin;
    QCheckBox *m_shadowsCheck;
    QCheckBox *m_hazeCheck;
    QCheckBox *m_hintsCheck;
    QCheckBox *m_statusCheck;
    QCheckBox *m_fpsCheck;
    QPlainTextEdit *m_diagnostics;
};

#endif // PREVIEW3DSETTINGSDIALOG_H
