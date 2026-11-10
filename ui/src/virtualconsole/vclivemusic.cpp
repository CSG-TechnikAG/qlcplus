/*
  Q Light Controller Plus
  vclivemusic.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTime>
#include <QToolButton>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

#include "vclivemusic.h"
#include "vclivemusicproperties.h"
#include "songrequestclient.h"
#include "genericdmxsource.h"
#include "flowlayout.h"
#include "virtualconsole.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"

const quint8 VCLiveMusic::enableInputSourceId = 0;
const quint8 VCLiveMusic::presetInputBaseId = 1;
int VCLiveMusic::s_widgetCount = 0;

VCLiveMusic::VCLiveMusic(QWidget *parent, Doc *doc)
    : VCWidget(parent, doc)
    , m_currentPresetIndex(-1)
    , m_client(SongRequestClient::instance())
    , m_source(new GenericDMXSource(doc))
    , m_engine(new LiveMusic::LiveMusicEngine(doc))
    , m_renderTimer(new QTimer(this))
    , m_liveEnabled(false)
    , m_outputActive(false)
    , m_lastUIUpdate(0)
{
    setObjectName(VCLiveMusic::staticMetaObject.className());
    setType(VCWidget::LiveMusicWidget);
    setCaption(tr("Live Music"));
    resize(QSize(380, 180));
    setMinimumSize(QSize(220, 120));

    m_source->setOutputEnabled(false);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(3);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(6);

    m_enableButton = new QToolButton(this);
    m_enableButton->setCheckable(true);
    m_enableButton->setText(tr("LIVE"));
    m_enableButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_enableButton->setIcon(QIcon(":/audio.png"));
    m_enableButton->setMinimumWidth(82);
    m_enableButton->setToolTip(tr("Enable/disable the automatic live light show"));
    topLayout->addWidget(m_enableButton);

    m_songLabel = new QLabel(tr("No song playing"), this);
    m_songLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_songLabel->setMinimumWidth(40);
    QFont songFont = font();
    songFont.setBold(true);
    m_songLabel->setFont(songFont);
    topLayout->addWidget(m_songLabel, 1);
    mainLayout->addLayout(topLayout);

    QHBoxLayout *infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);

    m_infoLabel = new QLabel(tr("BPM --  •  Music"), this);
    m_infoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    infoLayout->addWidget(m_infoLabel, 1);

    m_timeLabel = new QLabel("--:-- / --:--", this);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    infoLayout->addWidget(m_timeLabel);
    mainLayout->addLayout(infoLayout);

    m_sceneLabel = new QLabel(tr("No preset"), this);
    m_sceneLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mainLayout->addWidget(m_sceneLabel);

    m_presetContainer = new QWidget(this);
    m_presetContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_presetLayout = new FlowLayout(m_presetContainer, 2, 2, 2);
    mainLayout->addWidget(m_presetContainer);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(5);
    mainLayout->addWidget(m_progressBar);

    mainLayout->addStretch(1);

    connect(m_enableButton, &QToolButton::clicked,
            this, &VCLiveMusic::slotEnableClicked);
    connect(m_client, &SongRequestClient::playerChanged,
            this, &VCLiveMusic::slotPlayerChanged);
    connect(m_client, &SongRequestClient::songAnalysisReady,
            this, &VCLiveMusic::slotSongAnalysisReady);
    connect(m_client, &SongRequestClient::statusChanged, this, [this]()
    {
        updateUI(m_lastInfo);
    });

    m_renderTimer->setInterval(33); // ~30 fps, smooth movement without flooding DMX
    connect(m_renderTimer, &QTimer::timeout, this, &VCLiveMusic::slotTick);
    m_renderTimer->start();
    m_positionClock.start();

    s_widgetCount++;

    // A brand new widget starts with a useful "whole stage" preset so the
    // show is literally set-and-forget once the user turns it on.
    if (m_doc != NULL && m_doc->fixtures().isEmpty() == false)
    {
        VCLiveMusicPreset preset(presetInputBaseId);
        preset.m_name = tr("Full stage");
        for (Fixture *fixture : m_doc->fixtures())
        {
            LiveMusic::FixtureConfig config;
            config.fixtureId = fixture->id();
            config.allChannels = true;
            preset.m_fixtures.append(config);
        }
        addPreset(preset);
        setCurrentPresetIndex(0);
    }

    enableWidgetUI(false);
}

VCLiveMusic::~VCLiveMusic()
{
    m_renderTimer->stop();
    m_source->unsetAll();
    m_source->setOutputEnabled(false);
    delete m_source;
    delete m_engine;

    resetPresets();
    s_widgetCount--;
    stopTrackingIfUnused();
}

void VCLiveMusic::enableWidgetUI(bool enable)
{
    m_enableButton->setEnabled(enable);
    m_presetContainer->setEnabled(enable);
}

void VCLiveMusic::setLiveEnabled(bool enabled)
{
    if (m_liveEnabled == enabled)
        return;

    m_liveEnabled = enabled;
    m_enableButton->blockSignals(true);
    m_enableButton->setChecked(enabled);
    m_enableButton->blockSignals(false);

    if (enabled)
    {
        if (m_currentPresetIndex < 0 && m_presets.isEmpty() == false)
            setCurrentPresetIndex(0);
        applyCurrentPreset();
        if (mode() == Doc::Operate)
        {
            startTracking();
            m_positionClock.restart();
            m_renderTimer->start();
        }
    }
    else
    {
        clearOutput();
        m_renderTimer->stop();
    }

    updateFeedback();
    update();
}

bool VCLiveMusic::liveEnabled() const
{
    return m_liveEnabled;
}

void VCLiveMusic::toggleLiveEnabled()
{
    setLiveEnabled(!m_liveEnabled);
}

void VCLiveMusic::startTracking()
{
    if (m_client != NULL && m_liveEnabled)
        m_client->startLiveTracking();
}

void VCLiveMusic::stopTrackingIfUnused()
{
    if (s_widgetCount <= 0 && m_client != NULL)
        m_client->stopLiveTracking();
}

/*********************************************************************
 * Presets
 *********************************************************************/

QList<VCLiveMusicPreset *> VCLiveMusic::presets() const
{
    return m_presets;
}

void VCLiveMusic::addPreset(const VCLiveMusicPreset &preset)
{
    VCLiveMusicPreset *copy = new VCLiveMusicPreset(preset);
    m_presets.append(copy);
    std::sort(m_presets.begin(), m_presets.end(), VCLiveMusicPreset::compare);
    rebuildPresetWidgets();

    if (m_currentPresetIndex < 0)
        setCurrentPresetIndex(0);
}

void VCLiveMusic::resetPresets()
{
    for (QPushButton *button : m_presetButtons.keys())
        delete button;
    m_presetButtons.clear();

    for (VCLiveMusicPreset *preset : m_presets)
    {
        if (preset->m_inputSource.isNull() == false)
            setInputSource(QSharedPointer<QLCInputSource>(), preset->m_id);
        delete preset;
    }
    m_presets.clear();
    m_currentPresetIndex = -1;
}

void VCLiveMusic::setCurrentPresetIndex(int index)
{
    if (index < 0 || index >= m_presets.size())
        index = m_presets.isEmpty() ? -1 : 0;

    m_currentPresetIndex = index;
    applyCurrentPreset();
    updateFeedback();
    updateUI(m_lastInfo);
    update();
}

int VCLiveMusic::currentPresetIndex() const
{
    return m_currentPresetIndex;
}

VCLiveMusicPreset *VCLiveMusic::currentPreset() const
{
    if (m_currentPresetIndex < 0 || m_currentPresetIndex >= m_presets.size())
        return NULL;
    return m_presets.at(m_currentPresetIndex);
}

void VCLiveMusic::rebuildPresetWidgets()
{
    for (QPushButton *button : m_presetButtons.keys())
        delete button;
    m_presetButtons.clear();

    if (m_presetContainer->layout() != NULL)
    {
        // FlowLayout owns no widgets; deleting the buttons above is enough.
    }

    for (VCLiveMusicPreset *preset : m_presets)
    {
        QPushButton *button = new QPushButton(m_presetContainer);
        button->setText(fontMetrics().elidedText(preset->m_name, Qt::ElideRight, 96));
        button->setToolTip(preset->m_name);
        button->setCheckable(true);
        button->setAutoDefault(false);
        button->setMinimumWidth(56);
        button->setMaximumWidth(130);
        button->setFocusPolicy(Qt::TabFocus);
        m_presetLayout->addWidget(button);
        m_presetButtons.insert(button, preset);
        connect(button, &QPushButton::clicked,
                this, &VCLiveMusic::slotPresetClicked);

        if (preset->m_inputSource.isNull() == false)
            setInputSource(preset->m_inputSource, preset->m_id);
        else
            setInputSource(QSharedPointer<QLCInputSource>(), preset->m_id);
    }
}

void VCLiveMusic::applyCurrentPreset()
{
    VCLiveMusicPreset *preset = currentPreset();
    if (preset == NULL)
    {
        m_engine->setFixtures(QList<LiveMusic::FixtureConfig>());
        return;
    }

    m_engine->setSettings(preset->m_settings);
    m_engine->setFixtures(preset->m_fixtures);
    m_engine->setSong(m_song);
    m_engine->setPlayback(m_playback);
    m_engine->invalidate();
}

/*********************************************************************
 * Clipboard
 *********************************************************************/

VCWidget *VCLiveMusic::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != NULL);

    VCLiveMusic *copy = new VCLiveMusic(parent, m_doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    return copy;
}

bool VCLiveMusic::copyFrom(const VCWidget *widget)
{
    const VCLiveMusic *other = qobject_cast<const VCLiveMusic *>(widget);
    if (other == NULL)
        return false;

    resetPresets();
    for (VCLiveMusicPreset *preset : other->presets())
        addPreset(*preset);

    setCurrentPresetIndex(other->currentPresetIndex());
    setEnableKeySequence(other->enableKeySequence());
    setLiveEnabled(other->liveEnabled());

    return VCWidget::copyFrom(widget);
}

/*********************************************************************
 * Properties
 *********************************************************************/

void VCLiveMusic::editProperties()
{
    if (m_presets.isEmpty() && m_doc != NULL && m_doc->fixtures().isEmpty() == false)
    {
        VCLiveMusicPreset preset(presetInputBaseId);
        preset.m_name = tr("Full stage");
        for (Fixture *fixture : m_doc->fixtures())
        {
            LiveMusic::FixtureConfig config;
            config.fixtureId = fixture->id();
            config.allChannels = true;
            preset.m_fixtures.append(config);
        }
        addPreset(preset);
    }

    VCLiveMusicProperties properties(this, m_doc);
    if (properties.exec() == QDialog::Accepted)
        m_doc->setModified();
}

void VCLiveMusic::setEnableKeySequence(const QKeySequence &keySequence)
{
    m_enableKeySequence = QKeySequence(keySequence);
}

QKeySequence VCLiveMusic::enableKeySequence() const
{
    return m_enableKeySequence;
}

QSharedPointer<QLCInputSource> VCLiveMusic::enableInputSource() const
{
    return inputSource(enableInputSourceId);
}

/*********************************************************************
 * QLC mode
 *********************************************************************/

void VCLiveMusic::slotModeChanged(Doc::Mode mode)
{
    if (mode == Doc::Operate)
    {
        enableWidgetUI(true);
        if (m_liveEnabled)
        {
            applyCurrentPreset();
            startTracking();
            m_positionClock.restart();
            m_renderTimer->start();
        }
        updateFeedback();
    }
    else
    {
        clearOutput();
        enableWidgetUI(false);
        m_renderTimer->stop();
    }

    VCWidget::slotModeChanged(mode);
}

/*********************************************************************
 * Live data
 *********************************************************************/

void VCLiveMusic::slotPlayerChanged(const LiveMusic::Playback &playback)
{
    m_playback = playback;
    m_positionClock.restart();

    if (m_song.id != playback.id)
    {
        m_song = LiveMusic::Song();
        m_song.id = playback.id;
        m_song.title = playback.title;
        m_song.artist = playback.artist;
        m_song.album = playback.album;
        m_song.genre = playback.genre;
        m_song.duration = playback.duration;
        m_engine->setSong(m_song);
    }

    if (m_liveEnabled)
    {
        m_engine->setPlayback(m_playback);
        if (m_currentPresetIndex < 0 && m_presets.isEmpty() == false)
            setCurrentPresetIndex(0);
    }
}

void VCLiveMusic::slotSongAnalysisReady(const QString &songKey,
                                        const LiveMusic::Song &song)
{
    // Ignore late analysis results for a track that is no longer current.
    if (m_playback.id.isEmpty() == false && songKey != m_playback.id)
        return;

    m_song = song;
    if (m_song.title.isEmpty())
        m_song.title = m_playback.title;
    if (m_song.artist.isEmpty())
        m_song.artist = m_playback.artist;
    if (m_playback.duration <= 0.0)
        m_playback.duration = m_song.duration;
    m_engine->setSong(m_song);
    m_engine->setPlayback(m_playback);
    applyCurrentPreset();
}

void VCLiveMusic::slotTick()
{
    if (m_doc->mode() != Doc::Operate || m_liveEnabled == false
            || isDisabled() || isEnabled() == false)
    {
        if (m_outputActive)
            clearOutput();
        return;
    }

    if (m_playback.playing == false)
    {
        if (m_outputActive)
            clearOutput();
        m_engine->setPlayback(m_playback);
        updateUI(m_engine->frameInfo(m_playback.position));
        return;
    }

    m_source->setOutputEnabled(true);

    double position = m_playback.position + m_positionClock.elapsed() / 1000.0;
    if (m_playback.duration > 0.0)
        position = qMin(position, m_playback.duration);

    m_engine->setPlayback(m_playback);

    LiveMusic::FrameInfo info;
    const QMap<QPair<quint32, quint32>, uchar> frame = m_engine->render(position, &info);
    m_lastInfo = info;

    // Remove channels that were in the previous frame but are no longer used
    // (scene changes, feature switches, preset changes...).
    QList<QPair<quint32, quint32> > frameKeys = frame.keys();
    for (const QPair<quint32, quint32> &key : m_lastFrameKeys)
    {
        if (frame.contains(key) == false)
            m_source->unset(key.first, key.second);
    }

    QMapIterator<QPair<quint32, quint32>, uchar> it(frame);
    while (it.hasNext())
    {
        it.next();
        m_source->set(it.key().first, it.key().second, it.value());
    }
    m_lastFrameKeys = frameKeys;
    m_outputActive = (frame.isEmpty() == false);

    updateUI(info);
}

void VCLiveMusic::clearOutput()
{
    if (m_outputActive)
    {
        m_source->unsetAll();
        m_lastFrameKeys.clear();
        m_outputActive = false;
    }
    m_source->setOutputEnabled(false);
}

void VCLiveMusic::updateUI(const LiveMusic::FrameInfo &info)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastUIUpdate < 90 && m_lastUIUpdate != 0)
        return;
    m_lastUIUpdate = now;

    QString title = m_playback.title;
    if (title.isEmpty())
        title = m_song.title;
    QString artist = m_playback.artist;
    if (artist.isEmpty())
        artist = m_song.artist;

    QString song;
    if (title.isEmpty())
    {
        if (m_client->isConfigured() == false)
            song = tr("SongRequest not configured");
        else if (m_client->status() == SongRequestClient::Disconnected)
            song = tr("SongRequest disconnected");
        else
            song = m_liveEnabled ? tr("Waiting for SongRequest...") : tr("Live music show off");
    }
    else
        song = artist.isEmpty() ? title : QString("%1 — %2").arg(title, artist);
    m_songLabel->setText(m_songLabel->fontMetrics().elidedText(
                             song, Qt::ElideRight, qMax(40, m_songLabel->width() - 4)));
    m_songLabel->setToolTip(song);

    const double bpm = (m_song.bpm > 20.0) ? m_song.bpm : info.bpm;
    QString infoText = tr("%1 BPM  •  %2  •  %3")
            .arg(bpm > 0 ? QString::number(bpm, 'f', 1) : QString("--"))
            .arg(LiveMusic::sectionName(info.section))
            .arg(LiveMusic::styleName(info.style));
    if (info.drop > 0.1)
        infoText += tr("  •  DROP");
    m_infoLabel->setText(infoText);

    VCLiveMusicPreset *preset = currentPreset();
    m_sceneLabel->setText(tr("Preset: %1   Scene: %2")
            .arg(preset != NULL ? preset->m_name : tr("none"))
            .arg(m_playback.playing ? LiveMusic::sceneName(info.scene) : tr("idle")));

    const double duration = m_playback.duration > 0.0 ? m_playback.duration : m_song.duration;
    const double position = qBound(0.0, info.position, duration > 0.0 ? duration : info.position);
    m_timeLabel->setText(QString("%1 / %2")
            .arg(QTime(0, 0).addSecs(int(position)).toString("mm:ss"))
            .arg(duration > 0.0 ? QTime(0, 0).addSecs(int(duration)).toString("mm:ss") : "--:--"));

    if (duration > 0.0)
        m_progressBar->setValue(int(qBound(0.0, position / duration, 1.0) * 1000.0));
    else
        m_progressBar->setValue(0);

    m_enableButton->blockSignals(true);
    m_enableButton->setChecked(m_liveEnabled);
    m_enableButton->blockSignals(false);

    for (QPushButton *button : m_presetButtons.keys())
    {
        VCLiveMusicPreset *buttonPreset = m_presetButtons.value(button);
        button->setChecked(buttonPreset == preset);
    }

    if (m_liveEnabled && m_client->isConfigured() == false)
        m_infoLabel->setToolTip(tr("Configure SongRequest in the Inputs/Outputs toolbar first."));
    else
        m_infoLabel->setToolTip(QString());
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCLiveMusic::updateFeedback()
{
    sendFeedback(m_liveEnabled ? UCHAR_MAX : 0, enableInputSourceId);

    for (VCLiveMusicPreset *preset : m_presets)
    {
        QSharedPointer<QLCInputSource> src = inputSource(preset->m_id);
        if (src.isNull() || src->isValid() == false)
            continue;
        const bool selected = (preset == currentPreset());
        sendFeedback(selected ? src->feedbackValue(QLCInputFeedback::UpperValue)
                              : src->feedbackValue(QLCInputFeedback::LowerValue),
                     src);
    }
}

void VCLiveMusic::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    if (acceptsInput() == false)
        return;

    const quint32 pagedCh = (page() << 16) | channel;

    if (checkInputSource(universe, pagedCh, value, sender(), enableInputSourceId))
    {
        setLiveEnabled(value >= 64);
        return;
    }

    for (int i = 0; i < m_presets.size(); i++)
    {
        VCLiveMusicPreset *preset = m_presets.at(i);
        QSharedPointer<QLCInputSource> src = inputSource(preset->m_id);
        if (src.isNull() || src->isValid() == false)
            continue;
        if (src->universe() == universe
                && src->channel() == pagedCh
                && value >= 64)
        {
            setCurrentPresetIndex(i);
            return;
        }
    }
}

void VCLiveMusic::slotKeyPressed(const QKeySequence &keySequence)
{
    if (acceptsInput() == false)
        return;

    if (m_enableKeySequence.isEmpty() == false && m_enableKeySequence == keySequence)
    {
        toggleLiveEnabled();
        return;
    }

    for (int i = 0; i < m_presets.size(); i++)
    {
        if (m_presets.at(i)->m_keySequence.isEmpty() == false
                && m_presets.at(i)->m_keySequence == keySequence)
        {
            setCurrentPresetIndex(i);
            return;
        }
    }
}

void VCLiveMusic::slotEnableClicked(bool checked)
{
    setLiveEnabled(checked);
}

void VCLiveMusic::slotPresetClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (button == NULL)
        return;
    VCLiveMusicPreset *preset = m_presetButtons.value(button);
    if (preset == NULL)
        return;
    const int index = m_presets.indexOf(preset);
    if (index >= 0)
        setCurrentPresetIndex(index);
}

/*********************************************************************
 * Painting
 *********************************************************************/

void VCLiveMusic::paintEvent(QPaintEvent *e)
{
    VCWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int accentHeight = 4;
    QColor accent = palette().color(QPalette::Highlight);
    if (m_liveEnabled)
    {
        if (m_lastInfo.drop > 0.1)
            accent = QColor(255, 240, 255);
        else
        {
            const double hue = std::fmod(m_lastInfo.position * 30.0
                                         + m_lastInfo.energy * 220.0, 360.0);
            accent = QColor::fromHsv(int(hue), 220, 255);
        }
    }
    else if (m_playback.playing)
    {
        accent = QColor(90, 90, 90);
    }

    painter.fillRect(QRect(0, 0, width(), accentHeight), accent);

    if (m_liveEnabled)
    {
        const double pulse = 1.0 - qBound(0.0, m_lastInfo.beatPhase, 1.0);
        const int alpha = int(40 + pulse * 90.0);
        QColor glow = accent;
        glow.setAlpha(alpha);
        painter.fillRect(QRect(0, accentHeight, width(), height() - accentHeight), glow);
    }
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCLiveMusic::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCLiveMusic)
    {
        qWarning() << Q_FUNC_INFO << "Live Music node not found";
        return false;
    }

    loadXMLCommon(root);

    QList<VCLiveMusicPreset> newPresets;
    bool enabled = false;
    int currentPreset = -1;

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            int x = 0, y = 0, w = 0, h = 0;
            bool visible = true;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCLiveMusicEnabled)
        {
            enabled = (root.readElementText() == KXMLQLCTrue);
        }
        else if (root.name() == KXMLQLCVCLiveMusicCurrentPreset)
        {
            currentPreset = root.readElementText().toInt();
        }
        else if (root.name() == KXMLQLCVCLiveMusicEnableInput)
        {
            QString key = loadXMLSources(root, enableInputSourceId);
            if (key.isEmpty() == false)
                setEnableKeySequence(stripKeySequence(QKeySequence(key)));
        }
        else if (root.name() == KXMLQLCVCLiveMusicPreset)
        {
            VCLiveMusicPreset preset(presetInputBaseId);
            if (preset.loadXML(root))
                newPresets.append(preset);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Live Music tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    resetPresets();
    for (const VCLiveMusicPreset &preset : newPresets)
        addPreset(preset);

    if (m_presets.isEmpty() && m_doc != NULL && m_doc->fixtures().isEmpty() == false)
    {
        VCLiveMusicPreset preset(presetInputBaseId);
        preset.m_name = tr("Full stage");
        for (Fixture *fixture : m_doc->fixtures())
        {
            LiveMusic::FixtureConfig config;
            config.fixtureId = fixture->id();
            config.allChannels = true;
            preset.m_fixtures.append(config);
        }
        addPreset(preset);
        if (currentPreset < 0)
            currentPreset = 0;
    }

    setCurrentPresetIndex(currentPreset);
    setLiveEnabled(enabled);

    return true;
}

bool VCLiveMusic::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCVCLiveMusic);
    saveXMLCommon(doc);
    saveXMLWindowState(doc);
    saveXMLAppearance(doc);

    doc->writeTextElement(KXMLQLCVCLiveMusicEnabled,
                          m_liveEnabled ? KXMLQLCTrue : KXMLQLCFalse);
    if (m_currentPresetIndex >= 0)
        doc->writeTextElement(KXMLQLCVCLiveMusicCurrentPreset,
                              QString::number(m_currentPresetIndex));

    const QSharedPointer<QLCInputSource> enableSrc = inputSource(enableInputSourceId);
    if (m_enableKeySequence.isEmpty() == false
            || (enableSrc.isNull() == false && enableSrc->isValid()))
    {
        doc->writeStartElement(KXMLQLCVCLiveMusicEnableInput);
        if (m_enableKeySequence.isEmpty() == false)
            doc->writeTextElement(KXMLQLCVCWidgetKey, m_enableKeySequence.toString());
        saveXMLInput(doc, enableSrc);
        doc->writeEndElement();
    }

    for (VCLiveMusicPreset *preset : m_presets)
        preset->saveXML(doc);

    doc->writeEndElement();
    return true;
}

void VCLiveMusic::postLoad()
{
    rebuildPresetWidgets();
    if (m_currentPresetIndex < 0 && m_presets.isEmpty() == false)
        setCurrentPresetIndex(0);
    else
        applyCurrentPreset();
}
