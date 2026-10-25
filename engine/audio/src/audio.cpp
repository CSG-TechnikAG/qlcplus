/*
  Q Light Controller Plus
  audio.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDataStream>

#include "audiodecoder.h"
#include "audiorenderer.h"
#include "audioplugincache.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
 #if defined(__APPLE__) || defined(Q_OS_MAC)
   #include "audiorenderer_portaudio.h"
 #elif defined(WIN32) || defined(Q_OS_WIN)
   #include "audiorenderer_waveout.h"
 #else
   #include "audiorenderer_alsa.h"
 #endif
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
 #include "audiorenderer_qt5.h"
#else
 #include "audiorenderer_qt6.h"
#endif

#include "audio.h"
#include "doc.h"

#define KXMLQLCAudioSource QStringLiteral("Source")
#define KXMLQLCAudioDevice QStringLiteral("Device")
#define KXMLQLCAudioVolume QStringLiteral("Volume")
#define KXMLQLCAudioArtist QStringLiteral("Artist")
#define KXMLQLCAudioAlbum QStringLiteral("Album")
#define KXMLQLCAudioGenre QStringLiteral("Genre")
#define KXMLQLCAudioSongRequestId QStringLiteral("SongRequestID")
#define KXMLQLCAudioAnalysis QStringLiteral("Analysis")
#define KXMLQLCAudioBpm QStringLiteral("BPM")
#define KXMLQLCAudioWaveform QStringLiteral("Waveform")
#define KXMLQLCAudioBeats QStringLiteral("Beats")
#define KXMLQLCAudioTriggers QStringLiteral("Triggers")

/*****************************************************************************
 * Initialization
 *****************************************************************************/

Audio::Audio(Doc* doc)
  : Function(doc, Function::AudioType)
  , m_doc(doc)
  , m_decoder(NULL)
  , m_audio_out(NULL)
  , m_audioDevice(QString())
  , m_sourceFileName("")
  , m_audioDuration(0)
  , m_volume(1.0)
{
    setName(tr("New Audio"));
    setRunOrder(Audio::SingleShot);

    // Listen to member Function removals
    connect(doc, SIGNAL(functionRemoved(quint32)),
            this, SLOT(slotFunctionRemoved(quint32)));
}

Audio::~Audio()
{
    if (m_audio_out != NULL)
    {
        m_audio_out->stop();
        delete m_audio_out;
    }
    if (m_decoder != NULL)
        delete m_decoder;
}

QIcon Audio::getIcon() const
{
    return QIcon(":/audio.png");
}

/*****************************************************************************
 * Copying
 *****************************************************************************/

Function* Audio::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    Function* copy = new Audio(doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool Audio::copyFrom(const Function* function)
{
    const Audio* aud = qobject_cast<const Audio*> (function);
    if (aud == NULL)
        return false;

    setSourceFileName(aud->m_sourceFileName);
    m_audioDuration = aud->m_audioDuration;
    m_artist = aud->m_artist;
    m_album = aud->m_album;
    m_genre = aud->m_genre;
    m_songRequestId = aud->m_songRequestId;
    m_bpm = aud->m_bpm;
    m_waveform = aud->m_waveform;
    m_beats = aud->m_beats;
    m_triggers = aud->m_triggers;

    return Function::copyFrom(function);
}

QStringList Audio::getCapabilities() const
{
    return m_doc->audioPluginCache()->getSupportedFormats();
}

/*********************************************************************
 * Properties
 *********************************************************************/
quint32 Audio::totalDuration()
{
    return (quint32)m_audioDuration;
}

void Audio::setTotalDuration(quint32 msec)
{
    qDebug() << "Audio set total duration:" << msec;
    m_audioDuration = msec;

    emit totalDurationChanged();
}

bool Audio::setSourceFileName(QString filename)
{
    if (m_sourceFileName.isEmpty() == false)
    {
        // unload previous source
        if (m_decoder != NULL)
        {
            delete m_decoder;
            m_decoder = NULL;
        }
    }

    const bool sourceChanged = (m_sourceFileName != filename);
    if (sourceChanged)
        clearAnalysis();

    m_sourceFileName = filename;

    if (QFile(m_sourceFileName).exists())
    {
        setName(QFileInfo(m_sourceFileName).fileName());
    }
    else
    {
        doc()->appendToErrorLog(tr("Audio file <b>%1</b> not found").arg(m_sourceFileName));
        setName(tr("File not found"));
        //m_audioDuration = 0;
        emit changed(id());
        return true;
    }
    emit sourceFilenameChanged();

    m_decoder = m_doc->audioPluginCache()->getDecoderForFile(m_sourceFileName);

    if (m_decoder == NULL)
        return false;

    setDuration(m_decoder->totalTime());
    setTotalDuration(m_decoder->totalTime());

    emit changed(id());

    return true;
}

QString Audio::getSourceFileName() const
{
    return m_sourceFileName;
}

AudioDecoder *Audio::getAudioDecoder() const
{
    return m_decoder;
}

void Audio::setAudioDevice(QString dev)
{
    m_audioDevice = dev;
}

qreal Audio::volume() const
{
    return m_volume;
}

void Audio::setVolume(qreal volume)
{
    m_volume = volume;
}

QString Audio::audioDevice() const
{
    return m_audioDevice;
}

QString Audio::artist() const
{
    return m_artist;
}

void Audio::setArtist(const QString &artist)
{
    if (m_artist == artist)
        return;
    m_artist = artist;
    emit changed(id());
}

QString Audio::album() const
{
    return m_album;
}

void Audio::setAlbum(const QString &album)
{
    if (m_album == album)
        return;
    m_album = album;
    emit changed(id());
}

QString Audio::genre() const
{
    return m_genre;
}

void Audio::setGenre(const QString &genre)
{
    if (m_genre == genre)
        return;
    m_genre = genre;
    emit changed(id());
}

QString Audio::songRequestId() const
{
    return m_songRequestId;
}

void Audio::setSongRequestId(const QString &requestId)
{
    if (m_songRequestId == requestId)
        return;
    m_songRequestId = requestId;
    emit changed(this->id());
}

double Audio::bpm() const
{
    return m_bpm;
}

QByteArray Audio::waveform() const
{
    return m_waveform;
}

QVector<quint32> Audio::beatPositions() const
{
    return m_beats;
}

QVector<quint32> Audio::triggerPoints() const
{
    return m_triggers;
}

bool Audio::hasAnalysis() const
{
    return m_waveform.isEmpty() == false;
}

void Audio::setAnalysis(double bpm, const QByteArray &waveform,
                        const QVector<quint32> &beats,
                        const QVector<quint32> &triggers)
{
    m_bpm = qBound(0.0, bpm, 400.0);
    m_waveform = waveform;
    m_beats = beats;
    m_triggers = triggers;
    emit analysisChanged();
    emit changed(id());
}

void Audio::clearAnalysis()
{
    if (m_bpm == 0 && m_waveform.isEmpty() && m_beats.isEmpty() && m_triggers.isEmpty())
        return;
    m_bpm = 0.0;
    m_waveform.clear();
    m_beats.clear();
    m_triggers.clear();
    emit analysisChanged();
}

int Audio::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (m_audio_out != NULL && attrIndex == Intensity)
        m_audio_out->adjustIntensity(m_volume * getAttributeValue(Function::Intensity));

    return attrIndex;
}

void Audio::slotEndOfStream()
{
    if (!stopped())
        stop(FunctionParent::master());

    if (m_audio_out != NULL)
    {
        m_audio_out->stop();
        m_audio_out->deleteLater();
        m_audio_out = NULL;
    }
    if (m_decoder != NULL)
        m_decoder->seek(0);
}

void Audio::slotFunctionRemoved(quint32 fid)
{
    Q_UNUSED(fid)
}

/*********************************************************************
 * Save & Load
 *********************************************************************/

bool Audio::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* Function tag */
    doc->writeStartElement(KXMLQLCFunction);

    /* Common attributes */
    saveXMLCommon(doc);

    /* Speed */
    saveXMLSpeed(doc);

    /* Playback mode */
    saveXMLRunOrder(doc);

    doc->writeStartElement(KXMLQLCAudioSource);

    if (m_audioDevice.isEmpty() == false)
        doc->writeAttribute(KXMLQLCAudioDevice, m_audioDevice);

    if (m_volume != 1.0)
        doc->writeAttribute(KXMLQLCAudioVolume, QString::number(m_volume));
    if (m_artist.isEmpty() == false)
        doc->writeAttribute(KXMLQLCAudioArtist, m_artist);
    if (m_album.isEmpty() == false)
        doc->writeAttribute(KXMLQLCAudioAlbum, m_album);
    if (m_genre.isEmpty() == false)
        doc->writeAttribute(KXMLQLCAudioGenre, m_genre);
    if (m_songRequestId.isEmpty() == false)
        doc->writeAttribute(KXMLQLCAudioSongRequestId, m_songRequestId);

    doc->writeCharacters(m_doc->normalizeComponentPath(m_sourceFileName));

    doc->writeEndElement();

    if (m_bpm > 0 || m_waveform.isEmpty() == false)
    {
        doc->writeStartElement(KXMLQLCAudioAnalysis);
        doc->writeAttribute(KXMLQLCAudioBpm, QString::number(m_bpm, 'g', 10));
        doc->writeTextElement(KXMLQLCAudioWaveform, QString::fromLatin1(m_waveform.toBase64()));
        QByteArray beats;
        QDataStream bStream(&beats, QIODevice::WriteOnly);
        for (quint32 v : m_beats)
            bStream << v;
        QByteArray triggers;
        QDataStream tStream(&triggers, QIODevice::WriteOnly);
        for (quint32 v : m_triggers)
            tStream << v;
        doc->writeTextElement(KXMLQLCAudioBeats, QString::fromLatin1(beats.toBase64()));
        doc->writeTextElement(KXMLQLCAudioTriggers, QString::fromLatin1(triggers.toBase64()));
        doc->writeEndElement();
    }

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

bool Audio::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::AudioType))
    {
        qWarning() << Q_FUNC_INFO << root.attributes().value(KXMLQLCFunctionType).toString()
                   << "is not Audio";
        return false;
    }

    QString fname = name();

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCAudioSource)
        {
            QXmlStreamAttributes attrs = root.attributes();

            if (attrs.hasAttribute(KXMLQLCAudioDevice))
                setAudioDevice(attrs.value(KXMLQLCAudioDevice).toString());
            if (attrs.hasAttribute(KXMLQLCAudioVolume))
                setVolume(attrs.value(KXMLQLCAudioVolume).toString().toDouble());
            m_artist = attrs.value(KXMLQLCAudioArtist).toString();
            m_album = attrs.value(KXMLQLCAudioAlbum).toString();
            m_genre = attrs.value(KXMLQLCAudioGenre).toString();
            m_songRequestId = attrs.value(KXMLQLCAudioSongRequestId).toString();

            setSourceFileName(m_doc->denormalizeComponentPath(root.readElementText()));
        }
        else if (root.name() == KXMLQLCFunctionSpeed)
        {
            loadXMLSpeed(root);
        }
        else if (root.name() == KXMLQLCFunctionRunOrder)
        {
            loadXMLRunOrder(root);
        }
        else if (root.name() == KXMLQLCAudioAnalysis)
        {
            const double bpm = root.attributes().value(KXMLQLCAudioBpm).toDouble();
            QByteArray waveform;
            QByteArray beatsData;
            QByteArray triggersData;
            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCAudioWaveform)
                    waveform = QByteArray::fromBase64(root.readElementText().toLatin1());
                else if (root.name() == KXMLQLCAudioBeats)
                    beatsData = QByteArray::fromBase64(root.readElementText().toLatin1());
                else if (root.name() == KXMLQLCAudioTriggers)
                    triggersData = QByteArray::fromBase64(root.readElementText().toLatin1());
                else
                    root.skipCurrentElement();
            }
            QVector<quint32> beats;
            QDataStream bStream(&beatsData, QIODevice::ReadOnly);
            while (bStream.atEnd() == false)
            {
                quint32 v;
                bStream >> v;
                beats.append(v);
            }
            QVector<quint32> triggers;
            QDataStream tStream(&triggersData, QIODevice::ReadOnly);
            while (tStream.atEnd() == false)
            {
                quint32 v;
                tStream >> v;
                triggers.append(v);
            }
            m_bpm = qBound(0.0, bpm, 400.0);
            m_waveform = waveform;
            m_beats = beats;
            m_triggers = triggers;
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Audio tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    setName(fname);

    return true;
}

void Audio::postLoad()
{
}

/*********************************************************************
 * Running
 *********************************************************************/
void Audio::preRun(MasterTimer* timer)
{
    if (m_decoder != NULL)
    {
        uint fadeIn = overrideFadeInSpeed() == defaultSpeed() ? fadeInSpeed() : overrideFadeInSpeed();

        if (m_audio_out != NULL && m_audio_out->isRunning())
        {
            delete m_audio_out;
            m_audio_out = NULL;
        }

        m_decoder->seek(elapsed());
        AudioParameters ap = m_decoder->audioParameters();
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
 #if defined(__APPLE__) || defined(Q_OS_MAC)
        //m_audio_out = new AudioRendererCoreAudio();
        m_audio_out = new AudioRendererPortAudio(m_audioDevice);
 #elif defined(WIN32) || defined(Q_OS_WIN)
        m_audio_out = new AudioRendererWaveOut(m_audioDevice);
 #else
        m_audio_out = new AudioRendererAlsa(m_audioDevice);
 #endif
        m_audio_out->moveToThread(QCoreApplication::instance()->thread());
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_audio_out = new AudioRendererQt5(m_audioDevice, doc());
#else
        m_audio_out = new AudioRendererQt6(m_audioDevice, doc());
#endif
        m_audio_out->setDecoder(m_decoder);
        m_audio_out->initialize(ap.sampleRate(), ap.channels(), ap.format());
        m_audio_out->adjustIntensity(m_volume * getAttributeValue(Intensity));
        m_audio_out->setFadeIn(elapsed() ? 0 : fadeIn);
        m_audio_out->setLooped(runOrder() == Audio::Loop);
        m_audio_out->setUserStop(false);
        m_audio_out->start();
    }

    Function::preRun(timer);
}

void Audio::setPause(bool enable)
{
    if (isRunning())
    {
        if (m_audio_out != NULL)
        {
            if (enable)
                m_audio_out->suspend();
            else
                m_audio_out->resume();
        }

        Function::setPause(enable);
    }
}

void Audio::write(MasterTimer* timer, QList<Universe *> universes)
{
    Q_UNUSED(timer)
    Q_UNUSED(universes)

    if (isPaused())
        return;

    incrementElapsed();

    if (m_audio_out && !m_audio_out->isLooped())
    {
        uint fadeout = overrideFadeOutSpeed() == defaultSpeed() ? fadeOutSpeed() : overrideFadeOutSpeed();

        if (fadeout)
        {
            if (m_audio_out != NULL && totalDuration() - elapsed() <= fadeOutSpeed())
                m_audio_out->setFadeOut(fadeOutSpeed());
        }
        if (m_audio_out->isEos())
            slotEndOfStream();
    }
}

void Audio::postRun(MasterTimer* timer, QList<Universe*> universes)
{
    // Check whether a fade out is needed "outside" of the natural playback
    // This is the case of a Chaser step
    uint fadeout = overrideFadeOutSpeed() == defaultSpeed() ? fadeOutSpeed() : overrideFadeOutSpeed();

    if (fadeout == 0)
    {
        slotEndOfStream();
    }
    else
    {
        if (m_audio_out != NULL)
            m_audio_out->setFadeOut(fadeout);
    }

    Function::postRun(timer, universes);
}
