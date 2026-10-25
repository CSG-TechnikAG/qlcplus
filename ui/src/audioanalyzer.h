/*
  Q Light Controller Plus
  audioanalyzer.h

  Lightweight, dependency-free audio analysis for Audio functions.  It
  produces a tempo estimate, a compact waveform and beat/trigger positions
  without blocking the UI thread.

  Licensed under the Apache License, Version 2.0
*/

#ifndef AUDIOANALYZER_H
#define AUDIOANALYZER_H

#include <QByteArray>
#include <QString>
#include <QThread>

class Audio;
class Doc;

/** @addtogroup ui UI
 * @{
 */

class AudioAnalyzer final : public QThread
{
    Q_OBJECT

public:
    /** Create an analyser for an Audio function. The caller owns the object,
     *  must connect the signals and then call start(). */
    static AudioAnalyzer *create(Doc *doc, Audio *audio);

    ~AudioAnalyzer();

signals:
    /** Values are serialised as little-endian quint32 values. */
    void analysisReady(quint32 functionId, double bpm, QByteArray waveform,
                       QByteArray beats, QByteArray triggers);
    void analysisFailed(quint32 functionId, const QString &reason);

protected:
    void run() override;

private:
    AudioAnalyzer(Doc *doc, quint32 functionId, const QString &sourceFile);

    Doc *m_doc;
    quint32 m_functionId;
    QString m_sourceFile;
};

/** @} */

#endif // AUDIOANALYZER_H
