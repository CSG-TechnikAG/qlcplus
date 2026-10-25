/*
  Q Light Controller Plus
  preview3dsettingsdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTextStream>
#include <QVBoxLayout>

#include "preview3dsettingsdialog.h"

Preview3DSettingsDialog::Preview3DSettingsDialog(const Preview3DRenderSettings &settings,
                                                 bool gpuAvailable,
                                                 const QString &diagnostics,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_initial(settings)
    , m_rendererCombo(NULL)
    , m_qualityCombo(NULL)
    , m_scaleSpin(NULL)
    , m_shadowsCheck(NULL)
    , m_hazeCheck(NULL)
    , m_hintsCheck(NULL)
    , m_statusCheck(NULL)
    , m_fpsCheck(NULL)
    , m_diagnostics(NULL)
{
    setWindowTitle(tr("3D Preview Settings"));
    setModal(true);
    setMinimumSize(720, 660);
    resize(780, 700);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QGroupBox *renderBox = new QGroupBox(tr("Renderer and quality"), this);
    QFormLayout *renderForm = new QFormLayout(renderBox);

    m_rendererCombo = new QComboBox(renderBox);
    m_rendererCombo->addItem(tr("Auto"), int(Preview3DRenderer::Auto));
    m_rendererCombo->addItem(tr("OpenGL"), int(Preview3DRenderer::Gpu));
    m_rendererCombo->addItem(tr("Software"), int(Preview3DRenderer::Software));
    if (gpuAvailable == false)
    {
        const int index = m_rendererCombo->findData(int(Preview3DRenderer::Gpu));
        QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_rendererCombo->model());
        if (model != NULL && index >= 0)
        {
            model->item(index)->setText(tr("OpenGL (unavailable)"));
            model->item(index)->setEnabled(false);
        }
    }
    int rendererIndex = m_rendererCombo->findData(int(settings.renderer));
    if (rendererIndex < 0)
        rendererIndex = m_rendererCombo->findData(int(Preview3DRenderer::Auto));
    m_rendererCombo->setCurrentIndex(rendererIndex);
    renderForm->addRow(tr("Renderer"), m_rendererCombo);

    m_qualityCombo = new QComboBox(renderBox);
    m_qualityCombo->addItem(tr("Low"), int(Preview3DQuality::Low));
    m_qualityCombo->addItem(tr("Medium"), int(Preview3DQuality::Medium));
    m_qualityCombo->addItem(tr("High"), int(Preview3DQuality::High));
    m_qualityCombo->addItem(tr("Ultra"), int(Preview3DQuality::Ultra));
    const int qualityIndex = m_qualityCombo->findData(int(settings.quality));
    m_qualityCombo->setCurrentIndex(qualityIndex >= 0 ? qualityIndex : 2);
    renderForm->addRow(tr("Quality"), m_qualityCombo);

    m_scaleSpin = new QDoubleSpinBox(renderBox);
    m_scaleSpin->setRange(50.0, 200.0);
    m_scaleSpin->setDecimals(0);
    m_scaleSpin->setSingleStep(5.0);
    m_scaleSpin->setSuffix(QStringLiteral(" %"));
    m_scaleSpin->setValue(qBound(50.0, settings.resolutionScale * 100.0, 200.0));
    m_scaleSpin->setToolTip(tr("Internal GPU render resolution. Adaptive quality may lower it further."));
    renderForm->addRow(tr("Resolution"), m_scaleSpin);

    m_shadowsCheck = new QCheckBox(tr("Shadow map"), renderBox);
    m_shadowsCheck->setChecked(settings.shadows);
    renderForm->addRow(QString(), m_shadowsCheck);

    m_hazeCheck = new QCheckBox(tr("Distance haze"), renderBox);
    m_hazeCheck->setChecked(settings.haze);
    renderForm->addRow(QString(), m_hazeCheck);

    layout->addWidget(renderBox);

    QGroupBox *overlayBox = new QGroupBox(tr("View overlays"), this);
    QVBoxLayout *overlayLayout = new QVBoxLayout(overlayBox);
    m_hintsCheck = new QCheckBox(tr("Interaction hints"), overlayBox);
    m_hintsCheck->setChecked(settings.showHints);
    m_statusCheck = new QCheckBox(tr("Renderer and quality status"), overlayBox);
    m_statusCheck->setChecked(settings.showRenderStatus);
    m_fpsCheck = new QCheckBox(tr("FPS counter"), overlayBox);
    m_fpsCheck->setChecked(settings.showFps);
    overlayLayout->addWidget(m_hintsCheck);
    overlayLayout->addWidget(m_statusCheck);
    overlayLayout->addWidget(m_fpsCheck);
    layout->addWidget(overlayBox);

    if (gpuAvailable == false)
    {
        QLabel *warning = new QLabel(
                    tr("OpenGL 3.3 core is not available. The software renderer is the guaranteed "
                       "fallback and will be used if the GPU cannot start."), this);
        warning->setWordWrap(true);
        warning->setStyleSheet(QStringLiteral("color: #e0b060;"));
        layout->addWidget(warning);
    }

    QGroupBox *diagnosticsBox = new QGroupBox(tr("Active backend and diagnostics"), this);
    QVBoxLayout *diagnosticsLayout = new QVBoxLayout(diagnosticsBox);

    QString activeLine;
    QString fallbackLine;
    for (const QString &line : diagnostics.split(QLatin1Char('\n')))
    {
        if (line.startsWith(tr("Active renderer:")))
            activeLine = line;
        else if (line.startsWith(tr("Fallback reason:")))
            fallbackLine = line;
    }
    QLabel *summary = new QLabel(diagnosticsBox);
    summary->setTextFormat(Qt::PlainText);
    summary->setText(activeLine + QLatin1Char('\n') + fallbackLine);
    summary->setWordWrap(true);
    summary->setStyleSheet(QStringLiteral("font-weight: 600;"));
    diagnosticsLayout->addWidget(summary);

    m_diagnostics = new QPlainTextEdit(diagnosticsBox);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setPlainText(diagnostics);
    m_diagnostics->setMinimumHeight(130);
    m_diagnostics->setMaximumHeight(190);
    m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
    diagnosticsLayout->addWidget(m_diagnostics);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *copyButton = new QPushButton(tr("Copy diagnostics"), diagnosticsBox);
    QPushButton *saveButton = new QPushButton(tr("Save diagnostics..."), diagnosticsBox);
    QPushButton *captureButton = new QPushButton(tr("Capture preview..."), diagnosticsBox);
    actionLayout->addWidget(copyButton);
    actionLayout->addWidget(saveButton);
    actionLayout->addWidget(captureButton);
    actionLayout->addStretch();
    diagnosticsLayout->addLayout(actionLayout);
    layout->addWidget(diagnosticsBox, 1);

    QDialogButtonBox *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    buttons->button(QDialogButtonBox::Reset)->setText(tr("Reset to software"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &Preview3DSettingsDialog::slotResetToSoftware);
    layout->addWidget(buttons);

    connect(copyButton, &QPushButton::clicked,
            this, &Preview3DSettingsDialog::slotCopyDiagnostics);
    connect(saveButton, &QPushButton::clicked,
            this, &Preview3DSettingsDialog::slotSaveDiagnostics);
    connect(captureButton, &QPushButton::clicked,
            this, &Preview3DSettingsDialog::capturePreviewRequested);
}

Preview3DRenderSettings Preview3DSettingsDialog::settings() const
{
    Preview3DRenderSettings out = m_initial;
    out.renderer = Preview3DRenderer(m_rendererCombo->currentData().toInt());
    out.quality = Preview3DQuality(m_qualityCombo->currentData().toInt());
    out.resolutionScale = qBound(0.5, m_scaleSpin->value() / 100.0, 2.0);
    out.shadows = m_shadowsCheck->isChecked();
    out.haze = m_hazeCheck->isChecked();
    out.showHints = m_hintsCheck->isChecked();
    out.showRenderStatus = m_statusCheck->isChecked();
    out.showFps = m_fpsCheck->isChecked();
    return out;
}

void Preview3DSettingsDialog::slotCopyDiagnostics()
{
    QApplication::clipboard()->setText(m_diagnostics->toPlainText());
}

void Preview3DSettingsDialog::slotSaveDiagnostics()
{
    const QString fileName = QFileDialog::getSaveFileName(
                this, tr("Save 3D Preview diagnostics"),
                QDir::homePath() + QStringLiteral("/qlcplus-preview3d-diagnostics.txt"),
                tr("Text files (*.txt);;All files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text) == false)
    {
        QMessageBox::warning(this, tr("3D Preview diagnostics"),
                             tr("Cannot write diagnostics to %1").arg(fileName));
        return;
    }
    QTextStream stream(&file);
    stream << m_diagnostics->toPlainText();
    file.close();
}

void Preview3DSettingsDialog::slotResetToSoftware()
{
    const int index = m_rendererCombo->findData(int(Preview3DRenderer::Software));
    if (index >= 0)
        m_rendererCombo->setCurrentIndex(index);
    accept();
}
