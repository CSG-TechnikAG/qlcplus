/*
  Q Light Controller Plus
  timingstool_test.cpp

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

#include <QtTest>
#include <QComboBox>
#include <QGraphicsSceneMouseEvent>
#include <QGroupBox>
#include <QLabel>
#include <QSignalSpy>
#include <QToolButton>

#include "timingstool_test.h"
#include "timingstool.h"
#include "showitem.h"
#include "showfunction.h"
#include "doc.h"
#include "rgbmatrix.h"
#include "rgbmatrixitem.h"
#include "sequenceitem.h"
#include "chaser.h"
#include "chaserstep.h"
#include "scene.h"
#include "audio.h"
#include "audioitem.h"

class ResizableShowItem final : public ShowItem
{
public:
    explicit ResizableShowItem(ShowFunction *function) : ShowItem(function) {}

    using ShowItem::mousePressEvent;
    using ShowItem::mouseMoveEvent;
    using ShowItem::mouseReleaseEvent;
};

static int findDivision(QComboBox *combo, int milliBeats)
{
    for (int i = 0; i < combo->count(); ++i)
    {
        if (combo->itemData(i).toInt() == milliBeats)
            return i;
    }
    return -1;
}

void TimingsTool_Test::bpmDurationIsApplied()
{
    ShowFunction function(1);
    ShowItem item(&function);
    item.setDuration(1000, false);

    TimingsTool tool(&item);
    tool.setBpmContext(120, 4);
    connect(&tool, &TimingsTool::durationChanged, &item,
            [&item](ShowItem *, int msec, bool stretch) {
        item.setDuration(msec, stretch);
    });

    QComboBox *combo = tool.findChild<QComboBox *>("bpmDivisionCombo");
    QToolButton *apply = tool.findChild<QToolButton *>("bpmApplyButton");
    QVERIFY(combo != NULL);
    QVERIFY(apply != NULL);

    const int barIndex = findDivision(combo, 4000);
    QVERIFY(barIndex >= 0);
    combo->setCurrentIndex(barIndex);
    apply->click();

    QCOMPARE(item.getDuration(), quint32(2000));
    QLabel *label = tool.findChild<QLabel *>("bpmLengthLabel");
    QVERIFY(label != NULL);
    QVERIFY(label->text().contains("1 bar"));
}

void TimingsTool_Test::bpmDurationCanBeExtendedAndShortened()
{
    ShowFunction function(1);
    ShowItem item(&function);
    item.setDuration(1000, false);

    TimingsTool tool(&item);
    tool.setBpmContext(120, 4);
    connect(&tool, &TimingsTool::durationChanged, &item,
            [&item](ShowItem *, int msec, bool stretch) {
        item.setDuration(msec, stretch);
    });

    QComboBox *combo = tool.findChild<QComboBox *>("bpmDivisionCombo");
    QToolButton *apply = tool.findChild<QToolButton *>("bpmApplyButton");
    QToolButton *extend = tool.findChild<QToolButton *>("bpmExtendButton");
    QToolButton *shorten = tool.findChild<QToolButton *>("bpmShortenButton");
    QVERIFY(combo != NULL);
    QVERIFY(apply != NULL);
    QVERIFY(extend != NULL);
    QVERIFY(shorten != NULL);

    const int barIndex = findDivision(combo, 4000);
    QVERIFY(barIndex >= 0);
    combo->setCurrentIndex(barIndex);
    apply->click();
    QCOMPARE(item.getDuration(), quint32(2000));

    extend->click();
    QCOMPARE(item.getDuration(), quint32(4000));

    shorten->click();
    QCOMPARE(item.getDuration(), quint32(2000));
}

void TimingsTool_Test::bpmControlsHiddenWithoutTempo()
{
    ShowFunction function(1);
    ShowItem item(&function);
    item.setDuration(1000, false);

    TimingsTool tool(&item);
    QGroupBox *group = tool.findChild<QGroupBox *>("bpmDurationGroup");
    QVERIFY(group != NULL);
    QVERIFY(group->isHidden());
}

void TimingsTool_Test::rgbMatrixDurationIsPerClip()
{
    Doc doc(this);
    RGBMatrix matrix(&doc);
    matrix.setDuration(250);

    ShowFunction function(1);
    function.setDuration(1500);
    function.setStretch(false);
    RGBMatrixItem item(&matrix, &function);

    TimingsTool tool(&item);
    tool.setBpmContext(120, 4);
    tool.setFunctionContext(&matrix);
    connect(&tool, &TimingsTool::durationChanged, &item,
            [&item](ShowItem *, int msec, bool stretch) {
        item.setDuration(msec, stretch);
    });

    QComboBox *combo = tool.findChild<QComboBox *>("bpmDivisionCombo");
    QToolButton *apply = tool.findChild<QToolButton *>("bpmApplyButton");
    QVERIFY(combo != NULL);
    QVERIFY(apply != NULL);

    QRadioButton *stretchRadio = tool.findChild<QRadioButton *>("stretchDurationRadio");
    QVERIFY(stretchRadio != NULL);
    stretchRadio->setChecked(true);

    const int barIndex = findDivision(combo, 4000);
    QVERIFY(barIndex >= 0);
    combo->setCurrentIndex(barIndex);
    apply->click();

    QCOMPARE(function.duration(), quint32(2000));
    QCOMPARE(function.stretch(), true);
    QCOMPARE(matrix.duration(), quint32(250));
}

void TimingsTool_Test::sequenceDurationIsPerClip()
{
    Doc doc(this);
    Scene scene(&doc);
    scene.setID(1);
    Chaser chaser(&doc);
    chaser.setID(2);
    chaser.setDurationMode(Chaser::PerStep);
    chaser.addStep(ChaserStep(scene.id(), 0, 1000, 0));

    ShowFunction function(1);
    function.setDuration(1000);
    SequenceItem item(&chaser, &function);

    item.setDuration(500, true);

    QCOMPARE(function.duration(), quint32(500));
    QCOMPARE(chaser.totalDuration(), quint32(1000));
    QCOMPARE(item.getDuration(), quint32(500));
}

void TimingsTool_Test::audioSnapUsesBeatPhase()
{
    Doc doc(this);
    Audio audio(&doc);
    audio.setAnalysis(120, QByteArray(1, '\0'), { 300, 800, 1300 }, {});

    ShowFunction function(1);
    function.setDuration(2000);
    AudioItem item(&audio, &function);

    /* With a 500 ms grid, the first detected beat at 300 ms should land on
     * 500 ms, so the clip starts at 200 ms. */
    QCOMPARE(item.snapStartTime(0, 500, 0), quint32(200));
    QCOMPARE(item.snapStartTime(1000, 500, 1000), quint32(1200));

    Audio noBeats(&doc);
    AudioItem noBeatsItem(&noBeats, &function);
    QCOMPARE(noBeatsItem.snapStartTime(0, 500, 0), quint32(0));
}

void TimingsTool_Test::itemRightEdgeResizesDuration()
{
    ShowFunction function(1);
    ResizableShowItem item(&function);
    item.setTimeScale(3);
    item.setWidth(33);
    item.setDuration(2000, false);
    item.setResizeGridStep(500);

    QSignalSpy resizedSpy(&item, &ShowItem::itemResized);

    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setPos(QPointF(33, 10));
    item.mousePressEvent(&press);

    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setPos(QPointF(50, 10));
    item.mouseMoveEvent(&move);

    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setButton(Qt::LeftButton);
    release.setPos(QPointF(50, 10));
    item.mouseReleaseEvent(&release);

    /* 50 px at time scale 3 is 3000 ms, already a multiple of 500 ms */
    QCOMPARE(item.getDuration(), quint32(3000));
    QCOMPARE(resizedSpy.count(), 1);
}

void TimingsTool_Test::rgbMatrixStepCanBeSetToBpm()
{
    Doc doc(this);
    RGBMatrix matrix(&doc);
    matrix.setDuration(250);

    ShowFunction function(1);
    ShowItem item(&function);
    item.setDuration(2000, false);

    TimingsTool tool(&item);
    tool.setBpmContext(120, 4);
    tool.setFunctionContext(&matrix);
    connect(&tool, &TimingsTool::durationChanged, &item,
            [&item](ShowItem *, int msec, bool stretch) {
        item.setDuration(msec, stretch);
    });

    QComboBox *combo = tool.findChild<QComboBox *>("bpmStepDivisionCombo");
    QToolButton *apply = tool.findChild<QToolButton *>("bpmStepApplyButton");
    QVERIFY(combo != NULL);
    QVERIFY(apply != NULL);

    const int beatIndex = findDivision(combo, 1000);
    QVERIFY(beatIndex >= 0);
    combo->setCurrentIndex(beatIndex);
    apply->click();

    /* 1 beat at 120 BPM is 500 ms. The clip keeps four complete passes and
     * the shared source Function is left untouched. */
    QCOMPARE(function.functionSpeed(), quint32(500));
    QCOMPARE(matrix.duration(), quint32(250));
    QCOMPARE(item.getDuration(), quint32(2000));

    const int defaultIndex = findDivision(combo, 0);
    QVERIFY(defaultIndex >= 0);
    combo->setCurrentIndex(defaultIndex);
    apply->click();

    QCOMPARE(function.functionSpeed(), quint32(0));
    QCOMPARE(matrix.duration(), quint32(250));
}

QTEST_MAIN(TimingsTool_Test)
