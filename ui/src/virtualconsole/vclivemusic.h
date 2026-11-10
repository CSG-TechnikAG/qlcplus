/*
  Q Light Controller Plus
  vclivemusic.h

  Live Music virtual console widget. It pulls live SongRequest playback
  data and drives a beat/drop synchronised automatic light show inside
  user-defined fixture/channel boundaries.

  Licensed under the Apache License, Version 2.0
*/

#ifndef VCLIVEMUSIC_H
#define VCLIVEMUSIC_H

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QSharedPointer>

#include "vcwidget.h"
#include "livemusicengine.h"
#include "vclivemusicpreset.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class SongRequestClient;
class GenericDMXSource;
class InputSelectionWidget;
class QLabel;
class QToolButton;
class QPushButton;
class QProgressBar;
class QTimer;
class FlowLayout;
class VCLiveMusicProperties;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCLiveMusic              QStringLiteral("LiveMusic")
#define KXMLQLCVCLiveMusicEnabled       QStringLiteral("Enabled")
#define KXMLQLCVCLiveMusicCurrentPreset QStringLiteral("CurrentPreset")
#define KXMLQLCVCLiveMusicEnableInput   QStringLiteral("EnableInput")

class VCLiveMusic final : public VCWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(VCLiveMusic)

public:
    static const quint8 enableInputSourceId;
    static const quint8 presetInputBaseId;

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCLiveMusic(QWidget *parent, Doc *doc);
    ~VCLiveMusic();

    /*********************************************************************
     * Type
     *********************************************************************/
public:
    enum { widgetType = VCWidget::LiveMusicWidget };

    /*********************************************************************
     * Live state
     *********************************************************************/
public:
    void setLiveEnabled(bool enabled);
    bool liveEnabled() const;
    void toggleLiveEnabled();

    /** @reimp */
    void enableWidgetUI(bool enable) override;

    /*********************************************************************
     * Presets
     *********************************************************************/
public:
    QList<VCLiveMusicPreset *> presets() const;
    void addPreset(const VCLiveMusicPreset &preset);
    void resetPresets();
    void setCurrentPresetIndex(int index);
    int currentPresetIndex() const;
    VCLiveMusicPreset *currentPreset() const;

private:
    void rebuildPresetWidgets();
    void applyCurrentPreset();

    QList<VCLiveMusicPreset *> m_presets;
    int m_currentPresetIndex;
    QHash<QPushButton *, VCLiveMusicPreset *> m_presetButtons;

    /*********************************************************************
     * Clipboard
     *********************************************************************/
public:
    VCWidget *createCopy(VCWidget *parent) const override;
    bool copyFrom(const VCWidget *widget) override;

    /*********************************************************************
     * Properties
     *********************************************************************/
public:
    void editProperties() override;

    /*********************************************************************
     * QLC Mode
     *********************************************************************/
public slots:
    void slotModeChanged(Doc::Mode mode) override;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    void updateFeedback() override;

    /** Set the key combination that turns the live show on/off. */
    void setEnableKeySequence(const QKeySequence &keySequence);
    QKeySequence enableKeySequence() const;

    /** Input source used by the settings dialog for the on/off switch. */
    QSharedPointer<QLCInputSource> enableInputSource() const;

protected slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value) override;
    void slotKeyPressed(const QKeySequence &keySequence) override;

    /*********************************************************************
     * Live data
     *********************************************************************/
private slots:
    void slotTick();
    void slotPlayerChanged(const LiveMusic::Playback &playback);
    void slotSongAnalysisReady(const QString &songKey, const LiveMusic::Song &song);
    void slotEnableClicked(bool checked);
    void slotPresetClicked();

private:
    void startTracking();
    void stopTrackingIfUnused();
    void updateUI(const LiveMusic::FrameInfo &info);
    void clearOutput();

    static int s_widgetCount;

    SongRequestClient *m_client;
    GenericDMXSource *m_source;
    LiveMusic::LiveMusicEngine *m_engine;
    QTimer *m_renderTimer;
    QElapsedTimer m_positionClock;
    LiveMusic::Playback m_playback;
    LiveMusic::Song m_song;
    LiveMusic::FrameInfo m_lastInfo;
    QList<QPair<quint32, quint32> > m_lastFrameKeys;
    bool m_liveEnabled;
    bool m_outputActive;
    qint64 m_lastUIUpdate;
    QKeySequence m_enableKeySequence;

    /*********************************************************************
     * Widgets
     *********************************************************************/
private:
    QToolButton *m_enableButton;
    QLabel *m_songLabel;
    QLabel *m_infoLabel;
    QLabel *m_sceneLabel;
    QLabel *m_timeLabel;
    QProgressBar *m_progressBar;
    QWidget *m_presetContainer;
    FlowLayout *m_presetLayout;

    /*********************************************************************
     * Painting
     *********************************************************************/
protected:
    void paintEvent(QPaintEvent *e) override;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool loadXML(QXmlStreamReader &root) override;
    bool saveXML(QXmlStreamWriter *doc) override;
    void postLoad() override;
};

/** @} */

#endif // VCLIVEMUSIC_H
