/*
  Q Light Controller Plus
  timingstool.h

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

#ifndef TIMINGSTOOL_H
#define TIMINGSTOOL_H

#include <QRadioButton>
#include <QGroupBox>
#include <QWidget>

class QComboBox;
class QLabel;
class QToolButton;
class SpeedDial;
class ShowItem;
class Function;

class TimingsTool final : public QWidget
{
    Q_OBJECT
public:
    explicit TimingsTool(ShowItem *item, QWidget *parent = 0);
    ~TimingsTool();

    void showDurationControls(bool show);
    void showDurationOptions(bool show);

    /**
     * Enable the musical duration editor for a Show tempo.
     *
     * @param bpm The Show tempo in beats per minute
     * @param beatsPerBar The number of beats in a bar (2, 3 or 4)
     */
    void setBpmContext(double bpm, int beatsPerBar);

    /**
     * Enable the BPM step/loop speed editor for tempo-based functions.
     *
     * @param function The Function represented by the Show item
     */
    void setFunctionContext(Function *function);

signals:
    void startTimeChanged(ShowItem *item, int msec);
    void durationChanged(ShowItem *item, int msec, bool stretch);

protected slots:
    void slotStartTimeChanged(int msec);
    void slotDurationChanged(int msec);
    void slotBpmDivisionChanged(int index);
    void slotBpmApply();
    void slotBpmShorten();
    void slotBpmExtend();
    void slotStepDivisionChanged(int index);
    void slotStepApply();

private:
    void populateBpmDivisions();
    void updateBpmControls(bool initial);
    void updateStepControls(bool initial);
    void applyBpmDuration(quint32 msec);
    int defaultBpmDivision(quint32 duration) const;
    quint32 divisionDuration(int divisionIndex) const;
    quint32 divisionDuration(QComboBox *combo, int divisionIndex) const;
    quint32 stepDuration() const;
    QString musicalLength(quint32 msec) const;

private:
    SpeedDial* m_startDial;
    SpeedDial* m_durationDial;
    ShowItem *m_item;

    QGroupBox* m_durationOptions;
    QRadioButton *m_stretchOriginalRadio;
    QRadioButton *m_expandLoopRadio;

    QGroupBox *m_bpmGroup;
    QComboBox *m_bpmDivisionCombo;
    QToolButton *m_bpmApplyButton;
    QToolButton *m_bpmShortenButton;
    QToolButton *m_bpmExtendButton;
    QLabel *m_bpmLengthLabel;

    QGroupBox *m_stepGroup;
    QComboBox *m_stepDivisionCombo;
    QToolButton *m_stepApplyButton;
    QLabel *m_stepLengthLabel;
    Function *m_function;
    bool m_stepAvailable;

    double m_bpm;
    int m_beatsPerBar;
    bool m_bpmAvailable;
    bool m_updatingBpm;
};

#endif // TIMINGSTOOL_H
