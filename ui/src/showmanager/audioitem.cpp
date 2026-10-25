/*
  Q Light Controller Plus
  audioitem.cpp

  Copyright (C) Massimo Callegari

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

#include <QApplication>
#include <QPainter>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "audioitem.h"
#include "theme.h"
#include "trackitem.h"
#include "audiodecoder.h"
#include "audioplugincache.h"

AudioItem::AudioItem(Audio *aud, ShowFunction *func)
    : ShowItem(func)
    , m_audio(aud)
    , m_previewLeftAction(NULL)
    , m_previewRightAction(NULL)
    , m_previewStereoAction(NULL)
    , m_preview(NULL)
{
    Q_ASSERT(aud != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::AudioType));

    if (func->duration() == 0)
        func->setDuration(aud->totalDuration());

    calculateWidth();
    connect(m_audio, SIGNAL(changed(quint32)),
            this, SLOT(slotAudioChanged(quint32)));
    connect(m_audio, &Audio::analysisChanged, this, [this]() { update(); });

    /* Preview actions */
    m_previewLeftAction = new QAction(tr("Preview Left Channel"), this);
    m_previewLeftAction->setCheckable(true);
    connect(m_previewLeftAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewLeft()));
    m_previewRightAction = new QAction(tr("Preview Right Channel"), this);
    m_previewRightAction->setCheckable(true);
    connect(m_previewRightAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewRight()));
    m_previewStereoAction = new QAction(tr("Preview Stereo Channels"), this);
    m_previewStereoAction->setCheckable(true);
    connect(m_previewStereoAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAudioPreviewStereo()));
}

void AudioItem::calculateWidth()
{
    int newWidth = 0;
    qint64 audio_duration = m_audio->totalDuration();

    if (audio_duration != 0)
        newWidth = qRound((50.0 / double(getTimeScale())) * double(audio_duration) / 1000.0);
    else
        newWidth = 100;

    if (newWidth < qRound(50.0 / double(m_timeScale)))
        newWidth = qRound(50.0 / double(m_timeScale));
    setWidth(newWidth);
}

void AudioItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    float timeScale = 50/(float)m_timeScale;

    ShowItem::paint(painter, option, widget);

    if (m_audio->hasAnalysis() == true)
    {
        paintAnalysisWaveform(painter);
    }
    else if (m_preview != NULL)
    {
        // show preview here
        painter->drawPixmap(0, 0, m_preview->scaled(m_width, TRACK_HEIGHT - 4));
    }

    if (m_audio->fadeInSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeInSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(1, TRACK_HEIGHT - 4, fadeXpos, 2);
    }

    if (m_audio->fadeOutSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeOutSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(m_width - fadeXpos, 2, m_width - 1, TRACK_HEIGHT - 4);
    }

    ShowItem::postPaint(painter);
}

void AudioItem::paintAnalysisWaveform(QPainter *painter)
{
    const QByteArray waveform = m_audio->waveform();
    const quint32 duration = m_audio->totalDuration();
    if (waveform.isEmpty() || duration == 0 || m_width <= 4)
        return;

    /* Keep the waveform and its beat markers edge-to-edge with the clip,
     * so they line up exactly with the timeline grid above. */
    const QRectF plot(0, 3, m_width, TRACK_HEIGHT - 8);
    const qreal center = plot.center().y();
    const qreal halfHeight = plot.height() / 2.0;
    QColor itemColor = getColor();
    if (itemColor.isValid() == false)
        itemColor = ShowFunction::defaultColor(Function::AudioType);
    /* The item background is painted with the function colour. Using the
     * same colour for the waveform made it invisible; use a high-contrast
     * foreground while keeping the item's configured colour. */
    QColor waveColor = Theme::contrastingText(itemColor);
    waveColor.setAlpha(220);

    painter->save();
    painter->setClipRect(plot);
    painter->setPen(QPen(waveColor, 1.0));
    for (int x = 0; x < int(plot.width()); ++x)
    {
        const int index = qBound(0, int((qint64(x) * waveform.size()) / qMax(1, int(plot.width()))),
                                 waveform.size() - 1);
        const qreal amplitude = (uchar(waveform.at(index)) / 255.0) * halfHeight;
        painter->drawLine(QPointF(plot.left() + x, center - amplitude),
                          QPointF(plot.left() + x, center + amplitude));
    }

    const QColor beatColor = Theme::instance()->accent();
    const QVector<quint32> beats = m_audio->beatPositions();
    for (int i = 0; i < beats.size(); ++i)
    {
        if (beats.at(i) > duration)
            break;
        const qreal x = plot.left() + plot.width() * (qreal(beats.at(i)) / duration);
        QPen pen(beatColor, (i % 4 == 0) ? 2.0 : 1.0);
        pen.setColor(QColor(beatColor.red(), beatColor.green(), beatColor.blue(),
                            (i % 4 == 0) ? 210 : 120));
        painter->setPen(pen);
        painter->drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }

    const QVector<quint32> triggers = m_audio->triggerPoints();
    if (triggers.isEmpty() == false)
    {
        const QColor triggerColor = Theme::instance()->color(Theme::Warning);
        painter->setPen(QPen(triggerColor, 2.0));
        for (quint32 trigger : triggers)
        {
            if (trigger > duration)
                break;
            const qreal x = plot.left() + plot.width() * (qreal(trigger) / duration);
            painter->drawLine(QPointF(x, plot.top()), QPointF(x, plot.top() + 4));
        }
    }

    painter->restore();
}

void AudioItem::setTimeScale(qreal val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void AudioItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(msec)
    Q_UNUSED(stretch)
    // nothing to do
}

quint32 AudioItem::snapStartTime(quint32 start, quint32 gridStepMs,
                                 quint32 pointerTime) const
{
    if (m_audio == NULL || gridStepMs == 0)
        return start;

    const QVector<quint32> beats = m_audio->beatPositions();
    if (beats.isEmpty() == true)
        return start;

    const quint32 duration = m_audio->totalDuration();
    const quint32 localPointer = pointerTime > start ? pointerTime - start : 0;

    int nearestBeat = -1;
    quint32 nearestDistance = UINT_MAX;
    for (int i = 0; i < beats.size(); ++i)
    {
        if (duration > 0 && beats.at(i) > duration)
            break;

        const quint32 distance = beats.at(i) > localPointer
                               ? beats.at(i) - localPointer
                               : localPointer - beats.at(i);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestBeat = i;
        }
    }

    if (nearestBeat < 0)
        return start;

    /* Align the chosen internal beat, rather than the clip edge, to the Show
     * grid. This keeps pickup intros in phase with the musical timeline. */
    const quint32 beat = beats.at(nearestBeat);
    qint64 target = qRound64((qreal(start) + qreal(beat)) / qreal(gridStepMs))
                    * qint64(gridStepMs);
    qint64 candidate = target - qint64(beat);
    if (candidate < 0)
        candidate += gridStepMs;

    return candidate >= 0 ? quint32(candidate) : start;
}

QString AudioItem::functionName() const
{
    if (m_audio)
        return m_audio->name();
    return QString();
}

Audio *AudioItem::getAudio() const
{
    return m_audio;
}

void AudioItem::updateWaveformPreview()
{
    PreviewThread *waveformThread = new PreviewThread;
    waveformThread->setAudioItem(this);
    connect(waveformThread, SIGNAL(finished()), waveformThread, SLOT(deleteLater()));
    waveformThread->start();
}

void AudioItem::slotAudioChanged(quint32)
{
    updateWaveformPreview();
    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_audio->totalDuration());
}

void AudioItem::slotAudioPreviewLeft()
{
    m_previewRightAction->setChecked(false);
    m_previewStereoAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::slotAudioPreviewRight()
{
    m_previewLeftAction->setChecked(false);
    m_previewStereoAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::slotAudioPreviewStereo()
{
    m_previewLeftAction->setChecked(false);
    m_previewRightAction->setChecked(false);
    updateWaveformPreview();
}

void AudioItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    if (m_audio->getAudioDecoder() != NULL)
    {
        AudioDecoder *ad = m_audio->getAudioDecoder();
        AudioParameters ap = ad->audioParameters();

        if (ap.channels() == 1)
            m_previewLeftAction->setText(tr("Preview Mono"));
        menu.addAction(m_previewLeftAction);
        if (ap.channels() == 2)
        {
            m_previewLeftAction->setText(tr("Preview Left Channel"));
            menu.addAction(m_previewRightAction);
            menu.addAction(m_previewStereoAction);
        }
        menu.addSeparator();
    }

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}

void PreviewThread::setAudioItem(AudioItem *item)
{
    m_item = item;
}

qint32 PreviewThread::getSample(const unsigned char *data, quint32 idx, int sampleSize) const
{
    qint32 value = 0;
    if (sampleSize == 1)
    {
        value = (qint32)data[idx];
    }
    else if (sampleSize == 2)
    {
        qint16 *array = (qint16 *)data;
        value = array[idx / 2];
    }
    else if (sampleSize == 3 || sampleSize == 4)
    {
        qint32 *array = (qint32 *)data;
        value = array[idx / 4] >> 16;
    }
    //qDebug() << "sampleValue:" << value;
    return value;
}

void PreviewThread::run()
{
    bool left = m_item->m_previewLeftAction->isChecked() || m_item->m_previewStereoAction->isChecked();
    bool right = m_item->m_previewRightAction->isChecked() || m_item->m_previewStereoAction->isChecked();

    if ((left || right) && m_item->m_audio->getAudioDecoder() != NULL)
    {
        AudioDecoder *ad = m_item->m_audio->doc()->audioPluginCache()->getDecoderForFile(m_item->m_audio->getSourceFileName());
        AudioParameters ap = ad->audioParameters();
        ad->seek(0);
        // 1- find out how many samples have to be represented on a single pixel on a 1:1 time scale
        int sampleSize = ap.sampleSize();
        int channels = ap.channels();
        int oneSecondSamples = ap.sampleRate() * channels;
        int onePixelSamples = oneSecondSamples / 50;

        qint32 maxValue = 0;
        // 24 and 32 bit samples would produce a RMS too high, so let's
        // work on 16bit values
        if (sampleSize > 2)
            sampleSize = 2;

        if (left && right)
            maxValue = 0x7F << (8 * (sampleSize - 1));
        else
            maxValue = 0x3F << (8 * (sampleSize - 1));

        quint32 onePixelReadLen = onePixelSamples * sampleSize;

        // 2- decode the whole file and fill a QPixmap with a sample block RMS value for each pixel
        qint64 dataRead = 1;
        unsigned char audioData[onePixelReadLen * 4];
        quint32 audioDataOffset = 0;
        QPixmap *preview = new QPixmap((50 * m_item->m_audio->totalDuration()) / 1000, 76);
        preview->fill(Qt::transparent);
        QPainter p(preview);
        int xpos = 0;

        qDebug() << "Audio duration:" << m_item->m_audio->totalDuration() <<
                    ", channels:" << channels << ", pixmap width:" << preview->width() <<
                    ", maxValue:" << maxValue << ", samples:" << sampleSize;
        qDebug() << "Samples per second:" << oneSecondSamples << ", for one pixel:" << onePixelSamples <<
                    ", onePixelReadLen:" << onePixelReadLen;

        delete m_item->m_preview;
        m_item->m_preview = NULL;
        m_item->update();

        while (dataRead)
        {
            quint32 tmpExceedData = 0;
            if (audioDataOffset < onePixelReadLen)
            {
                dataRead = ad->read((char *)audioData + audioDataOffset, onePixelReadLen * 2);
                if (dataRead > 0)
                {
                    if ((quint32)dataRead + audioDataOffset >= onePixelReadLen)
                    {
                        tmpExceedData = (dataRead + audioDataOffset) - onePixelReadLen;
                        dataRead = onePixelReadLen;
                    }
                    else
                    {
                        qDebug() << "Not enough data. Requested:" << onePixelReadLen << "got:" << dataRead;
                        audioDataOffset = dataRead;
                        continue;
                    }
                }
            }
            else
            {
                dataRead = onePixelReadLen;
                tmpExceedData = audioDataOffset - onePixelReadLen;
            }

            if (dataRead == onePixelReadLen)
            {
                quint32 i = 0;
                // calculate the RMS value (peak) for this data block
                qint64 rmsLeft = 0;
                qint64 rmsRight = 0;
                bool done = false;
                while (!done)
                {
                    if (left)
                    {
                        qint32 sampleVal = getSample(audioData, i, sampleSize);
                        rmsLeft += (sampleVal * sampleVal);
                    }
                    i += sampleSize;

                    if (channels == 2)
                    {
                        if (right)
                        {
                            qint32 sampleVal = getSample(audioData, i, sampleSize);
                            rmsRight += (sampleVal * sampleVal);
                        }
                        i += sampleSize;
                    }

                    if (i >= dataRead)
                    {
                        //qDebug() << "i:" << i << "xpos:" << xpos;
                        done = true;
                    }
                }

                if (left)
                    rmsLeft = sqrt(rmsLeft / onePixelSamples);
                if (right)
                    rmsRight = sqrt(rmsRight / onePixelSamples);
                //qDebug() << "sample" << i << "RMS right:" << rmsRight << ", RMS left:" << rmsLeft;

                // 3- Draw the actual waveform
                unsigned short lineHeightLeft = 0, lineHeightRight = 0;

                if (left)
                    lineHeightLeft = (76 * rmsLeft) / maxValue;
                if (right)
                    lineHeightRight = (76 * rmsRight) / maxValue;

                if (left && right)
                {
                    if (lineHeightLeft > 1)
                        p.drawLine(xpos, 19 - (lineHeightLeft / 2), xpos, 19 + (lineHeightLeft / 2));
                    else
                        p.drawLine(xpos, 19, xpos + 1, 19);

                    if (lineHeightRight > 1)
                        p.drawLine(xpos, 51 - (lineHeightRight / 2), xpos, 51 + (lineHeightRight / 2));
                    else
                        p.drawLine(xpos, 51, xpos + 1, 51);
                }
                else
                {
                    unsigned short lineHeight = left ? lineHeightLeft : lineHeightRight;

                    if (lineHeight > 1)
                        p.drawLine(xpos, 38 - (lineHeight / 2), xpos, 38 + (lineHeight / 2));
                    else
                        p.drawLine(xpos, 38, xpos + 1, 38);
                    //qDebug() << "Data read: " << dataRead << ", rms: " << rmsRight << ", line height: " << lineHeight << ", xpos = " << xpos;
                }
                xpos++;

                if (tmpExceedData > 0)
                {
                    //qDebug() << "Exceed data found: " << tmpExceedData;
                    memmove(audioData, audioData + onePixelReadLen, tmpExceedData);
                    audioDataOffset = tmpExceedData;
                }
                else
                    audioDataOffset = 0;
            }
        }
        //qDebug() << "Iterations done: " << xpos;
        delete ad;
        m_item->m_preview = preview;
    }
    else // no preview selected. Delete pixmap
    {
        delete m_item->m_preview;
        m_item->m_preview = NULL;
    }

    m_item->update();
}
