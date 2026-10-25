/*
  Q Light Controller Plus
  audioanalyzer.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QDataStream>
#include <QDebug>
#include <QtEndian>
#include <QVector>
#include <QFileInfo>
#include <QThread>
#include <QtMath>

#include "audioanalyzer.h"
#include "audio.h"
#include "audiodecoder.h"
#include "audioplugincache.h"
#include "doc.h"

static qint32 sampleValue(const uchar *data, int index, int sampleSize)
{
    if (sampleSize == 1)
    {
        return qint32(qint8(data[index]));
    }
    else if (sampleSize == 2)
    {
        return qint32(qFromLittleEndian<qint16>(reinterpret_cast<const void *>(data + index)));
    }
    else if (sampleSize == 3)
    {
        qint32 value = qint32(data[index])
                     | (qint32(data[index + 1]) << 8)
                     | (qint32(data[index + 2]) << 16);
        if (value & 0x800000)
            value |= ~0xFFFFFF;
        return value;
    }
    else if (sampleSize == 4)
    {
        return qint32(qFromLittleEndian<qint32>(reinterpret_cast<const void *>(data + index)));
    }
    return 0;
}

static int samplePeak(int sampleSize)
{
    if (sampleSize == 1)
        return 128;
    if (sampleSize == 2)
        return 32768;
    return 2147483647;
}

AudioAnalyzer::AudioAnalyzer(Doc *doc, quint32 functionId, const QString &sourceFile)
    : QThread(nullptr)
    , m_doc(doc)
    , m_functionId(functionId)
    , m_sourceFile(sourceFile)
{
}

AudioAnalyzer::~AudioAnalyzer()
{
}

AudioAnalyzer *AudioAnalyzer::create(Doc *doc, Audio *audio)
{
    if (doc == NULL || audio == NULL || QFileInfo::exists(audio->getSourceFileName()) == false)
        return NULL;

    return new AudioAnalyzer(doc, audio->id(), audio->getSourceFileName());
}

void AudioAnalyzer::run()
{
    AudioDecoder *decoder = m_doc->audioPluginCache()->getDecoderForFile(m_sourceFile);
    if (decoder == NULL)
    {
        emit analysisFailed(m_functionId, tr("No audio decoder is available for this file."));
        return;
    }

    const AudioParameters ap = decoder->audioParameters();
    const int sampleSize = ap.sampleSize();
    const int channels = qMax(1, ap.channels());
    const int frameBytes = sampleSize * channels;
    const int sampleRate = qMax(1, int(ap.sampleRate()));
    const int blockFrames = qMax(1, sampleRate / 100); // 10 ms analysis frames
    const int blockBytes = qMax(frameBytes, blockFrames * frameBytes);
    const int peakLimit = samplePeak(sampleSize);

    const qint64 totalTime = decoder->totalTime();
    QVector<double> energy;
    QVector<int> peaks;
    energy.reserve(qMax(1, int(totalTime / 10)));
    peaks.reserve(energy.capacity());

    QByteArray pending;
    pending.reserve(blockBytes * 4);
    char raw[65536];

    qint64 readBytes = 0;
    while ((readBytes = decoder->read(raw, sizeof(raw))) > 0)
    {
        pending.append(raw, int(readBytes));
        while (pending.size() >= blockBytes)
        {
            const uchar *block = reinterpret_cast<const uchar *>(pending.constData());
            qint64 sumSquares = 0;
            int maxPeak = 0;
            int values = 0;
            for (int frame = 0; frame < blockFrames; ++frame)
            {
                const int frameOffset = frame * frameBytes;
                for (int ch = 0; ch < channels; ++ch)
                {
                    const qint32 sample = sampleValue(block,
                                                     frameOffset + ch * sampleSize,
                                                     sampleSize);
                    const int absolute = qAbs(sample);
                    maxPeak = qMax(maxPeak, absolute);
                    sumSquares += qint64(sample) * sample;
                    ++values;
                }
            }

            if (values > 0)
            {
                energy.append(qSqrt(double(sumSquares) / double(values)) / double(peakLimit));
                peaks.append(maxPeak);
            }
            pending.remove(0, blockBytes);
        }
    }

    delete decoder;

    if (energy.size() < 8)
    {
        emit analysisFailed(m_functionId, tr("The audio file does not contain enough decoded samples."));
        return;
    }

    // Compact waveform: 50 samples per second, peak based.
    double maxPeak = 0.0;
    for (int p : peaks)
        maxPeak = qMax(maxPeak, double(p));
    if (maxPeak <= 0.0)
        maxPeak = 1.0;

    QByteArray waveform;
    waveform.reserve(energy.size() / 2 + 1);
    for (int i = 0; i < peaks.size(); i += 2)
    {
        const int a = peaks.at(i);
        const int b = (i + 1 < peaks.size()) ? peaks.at(i + 1) : a;
        waveform.append(char(qBound(0, int(qRound(255.0 * qMax(a, b) / maxPeak)), 255)));
    }

    // Onset strength from positive energy changes.
    QVector<double> onset(energy.size(), 0.0);
    double maxOnset = 0.0;
    for (int i = 1; i < energy.size(); ++i)
    {
        onset[i] = qMax(0.0, energy.at(i) - energy.at(i - 1));
        maxOnset = qMax(maxOnset, onset.at(i));
    }

    double bpm = 0.0;
    double beatPeriodFrames = 0.0;
    if (maxOnset > 0.00001)
    {
        for (int i = 0; i < onset.size(); ++i)
            onset[i] /= maxOnset;

        // 10 ms frames: 30..100 frames = 60..200 BPM. Keep the full
        // correlation curve so the peak can be refined to a fraction of a
        // frame instead of being rounded to a whole 10 ms step.
        double bestScore = -1.0;
        int bestLag = 0;
        QVector<double> lagScores(101, 0.0);
        for (int lag = 30; lag <= 100; ++lag)
        {
            double sum = 0.0;
            for (int i = lag; i < onset.size(); ++i)
                sum += onset.at(i) * onset.at(i - lag);
            const double score = sum / double(qMax(1, onset.size() - lag));
            lagScores[lag] = score;
            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        if (bestLag > 0 && bestScore > 0.000001)
        {
            double refinedLag = bestLag;
            if (bestLag > 30 && bestLag < 100)
            {
                const double s0 = lagScores.at(bestLag - 1);
                const double s1 = lagScores.at(bestLag);
                const double s2 = lagScores.at(bestLag + 1);
                const double denom = s0 - 2.0 * s1 + s2;
                if (qAbs(denom) > 1e-12)
                {
                    const double delta = 0.5 * (s0 - s2) / denom;
                    if (qAbs(delta) < 0.5)
                        refinedLag += delta;
                }
            }

            bpm = 6000.0 / refinedLag;
            if (bpm < 70.0)
                bpm *= 2.0;
            else if (bpm > 190.0)
                bpm /= 2.0;
            bpm = qBound(40.0, bpm, 220.0);
            beatPeriodFrames = 6000.0 / bpm;
        }
    }

    QVector<quint32> beats;
    QVector<quint32> triggers;
    if (bpm > 0.0 && beatPeriodFrames > 0.0)
    {
        /* Find the phase using the precise period. Rounding the period to a
         * whole frame here was the source of the increasing drift against
         * the uniformly spaced Show grid. */
        int bestOffset = 0;
        double bestOffsetScore = -1.0;
        const int phaseSteps = qMax(1, int(qCeil(beatPeriodFrames)));
        for (int offset = 0; offset < phaseSteps; ++offset)
        {
            double score = 0.0;
            for (double pos = offset; pos < onset.size(); pos += beatPeriodFrames)
            {
                const int index = qRound(pos);
                if (index >= 0 && index < onset.size())
                    score += onset.at(index);
            }
            if (score > bestOffsetScore)
            {
                bestOffsetScore = score;
                bestOffset = offset;
            }
        }

        const qint64 durationMs = qMax<qint64>(0, totalTime);
        const double beatMs = 60000.0 / bpm;
        const double phaseMs = double(bestOffset) * 10.0;
        for (double t = phaseMs; t < double(durationMs); t += beatMs)
        {
            const quint32 beat = quint32(qRound(t));
            beats.append(beat);
            // A beat is also a good, predictable lighting trigger.
            triggers.append(beat);
        }
    }

    QByteArray beatData;
    QDataStream beatStream(&beatData, QIODevice::WriteOnly);
    for (quint32 beat : beats)
        beatStream << beat;

    QByteArray triggerData;
    QDataStream triggerStream(&triggerData, QIODevice::WriteOnly);
    for (quint32 trigger : triggers)
        triggerStream << trigger;

    emit analysisReady(m_functionId, bpm, waveform, beatData, triggerData);
}
