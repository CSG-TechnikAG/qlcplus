/*
  Q Light Controller Plus
  audio.h

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

#ifndef AUDIO_H
#define AUDIO_H

#include <QColor>
#include <QByteArray>
#include <QVector>

#include "audiorenderer.h"
#include "audiodecoder.h"
#include "function.h"

class QXmlStreamReader;

/** @addtogroup engine_functions Functions
 * @{
 */

class Audio final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Audio)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Audio(Doc* doc);
    virtual ~Audio();

    /** @reimp */
    QIcon getIcon() const override;

private:
    Doc *m_doc;
    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

public slots:
    /** Catches Doc::functionRemoved() so that destroyed members can be
        removed immediately. */
    void slotFunctionRemoved(quint32 function);

    /*********************************************************************
     * Capabilities
     *********************************************************************/
public:
    QStringList getCapabilities() const;

    /*********************************************************************
     * Properties
     *********************************************************************/
public:
    /**
     * Returns the duration of the source audio file loaded
     *
     * @return Duration in milliseconds of the source audio file
     */
    quint32 totalDuration() override;

    /**
     * Set the playback duration of the audio file
     *
     * @param The playback total duration in milliseconds
     */
    void setTotalDuration(quint32 msec) override;

    /**
     * Set the source file name used by this Audio object
     */
    bool setSourceFileName(QString filename);

    /**
     * Retrieve the source file name used by this Audio object
     */
    QString getSourceFileName() const;

    /**
     * Retrieve the currently associated audio decoder
     */
    AudioDecoder* getAudioDecoder() const;

    /**
     * Set a specific audio device for rendering. If empty
     * the QLC+ global device will be used
     */
    void setAudioDevice(QString dev);

    /** Get/Set the audio function startup volume */
    qreal volume() const;
    void setVolume(qreal volume);

    /**
     * Retrieve the audio device set for this function
     */
    QString audioDevice() const;

    /** SongRequest / library metadata */
    QString artist() const;
    void setArtist(const QString &artist);
    QString album() const;
    void setAlbum(const QString &album);
    QString genre() const;
    void setGenre(const QString &genre);
    QString songRequestId() const;
    void setSongRequestId(const QString &id);

    /** Detected tempo and waveform analysis (waveform is 0..255 at 50 Hz) */
    double bpm() const;
    QByteArray waveform() const;
    QVector<quint32> beatPositions() const;
    QVector<quint32> triggerPoints() const;
    bool hasAnalysis() const;
    void setAnalysis(double bpm, const QByteArray &waveform,
                     const QVector<quint32> &beats,
                     const QVector<quint32> &triggers);
    void clearAnalysis();

    int adjustAttribute(qreal fraction, int attributeId) override;

signals:
    void sourceFilenameChanged();
    void analysisChanged();

protected slots:
    void slotEndOfStream();

private:
    /** Instance of an AudioDecoder to perform actual audio decoding */
    AudioDecoder *m_decoder;
    /** output interface to render audio data got from m_decoder */
    AudioRenderer *m_audio_out;
    /** Audio device to use for rendering */
    QString m_audioDevice;
    /** Name of the source audio file */
    QString m_sourceFileName;
    /** Duration of the media object */
    qint64 m_audioDuration;
    /** Startup volume of the audio file */
    qreal m_volume;
    /** Imported metadata */
    QString m_artist;
    QString m_album;
    QString m_genre;
    QString m_songRequestId;
    /** Audio analysis */
    double m_bpm;
    QByteArray m_waveform;
    QVector<quint32> m_beats;
    QVector<quint32> m_triggers;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
public:
    /** Save function's contents to an XML document */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /** Load function's contents from an XML document */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    void postLoad() override;

    /*********************************************************************
     * Running
     *********************************************************************/
public:
    /** @reimpl */
    void preRun(MasterTimer*) override;

    /** @reimpl */
    void setPause(bool enable) override;

    /** @reimpl */
    void write(MasterTimer* timer, QList<Universe*> universes) override;

    /** @reimpl */
    void postRun(MasterTimer* timer, QList<Universe *> universes) override;
};

/** @} */

#endif
