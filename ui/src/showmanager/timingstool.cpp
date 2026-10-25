/*
  Q Light Controller Plus
  timingstool.cpp

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

#include <QBoxLayout>
#include <QComboBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStringList>
#include <QToolButton>
#include <qmath.h>

#include "timingstool.h"
#include "speeddial.h"
#include "showitem.h"
#include "showfunction.h"
#include "apputil.h"
#include "function.h"
#include "rgbmatrix.h"
#include "efx.h"

#define WINDOW_FLAGS Qt::WindowFlags( \
    (Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::Window | \
     Qt::WindowStaysOnTopHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint))

#define SETTINGS_GEOMETRY "timingstool/geometry"

TimingsTool::TimingsTool(ShowItem *item, QWidget *parent)
    : QWidget(parent)
    , m_startDial(NULL)
    , m_durationDial(NULL)
    , m_item(item)
    , m_durationOptions(NULL)
    , m_stretchOriginalRadio(NULL)
    , m_expandLoopRadio(NULL)
    , m_bpmGroup(NULL)
    , m_bpmDivisionCombo(NULL)
    , m_bpmApplyButton(NULL)
    , m_bpmShortenButton(NULL)
    , m_bpmExtendButton(NULL)
    , m_bpmLengthLabel(NULL)
    , m_stepGroup(NULL)
    , m_stepDivisionCombo(NULL)
    , m_stepApplyButton(NULL)
    , m_stepLengthLabel(NULL)
    , m_function(NULL)
    , m_stepAvailable(false)
    , m_bpm(0)
    , m_beatsPerBar(4)
    , m_bpmAvailable(false)
    , m_updatingBpm(false)
{
    Q_ASSERT(item != NULL);

    setWindowFlags(WINDOW_FLAGS);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(m_item->functionName());

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *content = new QWidget(scrollArea);
    QBoxLayout* lay = new QBoxLayout(QBoxLayout::TopToBottom, content);
    lay->setContentsMargins(6, 6, 6, 6);
    scrollArea->setWidget(content);

    QBoxLayout *outerLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    /* Create dials */
    m_startDial = new SpeedDial(this);
    m_startDial->setTitle(tr("Start Time"));
    ushort dialMask = m_startDial->visibilityMask();
    dialMask = (dialMask & ~SpeedDial::Infinite);
    dialMask = (dialMask & ~SpeedDial::Tap);
    m_startDial->setVisibilityMask(dialMask);
    m_startDial->setValue(m_item->getStartTime());
    lay->addWidget(m_startDial);
    connect(m_startDial, SIGNAL(valueChanged(int)),
            this, SLOT(slotStartTimeChanged(int)));

    m_durationDial = new SpeedDial(this);
    m_durationDial->setTitle(tr("Duration"));
    m_durationDial->setVisibilityMask(dialMask);
    m_durationDial->setValue(m_item->getDuration());
    lay->addWidget(m_durationDial);
    connect(m_durationDial, SIGNAL(valueChanged(int)),
            this, SLOT(slotDurationChanged(int)));

    /* Musical duration, active when the Show uses a BPM grid */
    m_bpmGroup = new QGroupBox(tr("BPM duration"), this);
    m_bpmGroup->setObjectName("bpmDurationGroup");
    QGridLayout *bpmLayout = new QGridLayout(m_bpmGroup);
    bpmLayout->setContentsMargins(8, 8, 8, 8);
    bpmLayout->setHorizontalSpacing(6);
    bpmLayout->setVerticalSpacing(6);

    m_bpmLengthLabel = new QLabel(m_bpmGroup);
    m_bpmLengthLabel->setObjectName("bpmLengthLabel");
    m_bpmLengthLabel->setWordWrap(true);
    bpmLayout->addWidget(m_bpmLengthLabel, 0, 0, 1, 3);

    QLabel *divisionLabel = new QLabel(tr("Division:"), m_bpmGroup);
    bpmLayout->addWidget(divisionLabel, 1, 0);

    m_bpmDivisionCombo = new QComboBox(m_bpmGroup);
    m_bpmDivisionCombo->setObjectName("bpmDivisionCombo");
    m_bpmDivisionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    bpmLayout->addWidget(m_bpmDivisionCombo, 1, 1);

    QWidget *stepButtons = new QWidget(m_bpmGroup);
    QHBoxLayout *stepLayout = new QHBoxLayout(stepButtons);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLayout->setSpacing(2);

    m_bpmApplyButton = new QToolButton(stepButtons);
    m_bpmApplyButton->setObjectName("bpmApplyButton");
    m_bpmApplyButton->setText(tr("Apply"));
    m_bpmApplyButton->setToolTip(tr("Set the item duration to the selected BPM division"));
    stepLayout->addWidget(m_bpmApplyButton);

    m_bpmShortenButton = new QToolButton(stepButtons);
    m_bpmShortenButton->setObjectName("bpmShortenButton");
    m_bpmShortenButton->setText(QString::fromUtf8("\xE2\x88\x92"));
    m_bpmShortenButton->setToolTip(tr("Shorten to the previous BPM division"));
    stepLayout->addWidget(m_bpmShortenButton);

    m_bpmExtendButton = new QToolButton(stepButtons);
    m_bpmExtendButton->setObjectName("bpmExtendButton");
    m_bpmExtendButton->setText("+");
    m_bpmExtendButton->setToolTip(tr("Extend to the next BPM division"));
    stepLayout->addWidget(m_bpmExtendButton);

    bpmLayout->addWidget(stepButtons, 1, 2);

    QLabel *bpmHint = new QLabel(tr("Apply a division to size the item, then use \xE2\x88\x92/+ "
                                    "to shorten or extend it."), m_bpmGroup);
    bpmHint->setWordWrap(true);
    bpmLayout->addWidget(bpmHint, 2, 0, 1, 3);

    m_bpmGroup->hide();
    lay->addWidget(m_bpmGroup);

    connect(m_bpmDivisionCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotBpmDivisionChanged(int)));
    connect(m_bpmApplyButton, SIGNAL(clicked()),
            this, SLOT(slotBpmApply()));
    connect(m_bpmShortenButton, SIGNAL(clicked()),
            this, SLOT(slotBpmShorten()));
    connect(m_bpmExtendButton, SIGNAL(clicked()),
            this, SLOT(slotBpmExtend()));

    /* BPM step/loop speed, for RGB Matrices and EFX */
    m_stepGroup = new QGroupBox(tr("BPM function speed"), this);
    m_stepGroup->setObjectName("bpmStepGroup");
    QGridLayout *stepGroupLayout = new QGridLayout(m_stepGroup);
    stepGroupLayout->setContentsMargins(8, 8, 8, 8);
    stepGroupLayout->setHorizontalSpacing(6);
    stepGroupLayout->setVerticalSpacing(6);

    m_stepLengthLabel = new QLabel(m_stepGroup);
    m_stepLengthLabel->setObjectName("bpmStepLengthLabel");
    m_stepLengthLabel->setWordWrap(true);
    stepGroupLayout->addWidget(m_stepLengthLabel, 0, 0, 1, 2);

    QLabel *stepDivisionLabel = new QLabel(tr("Division:"), m_stepGroup);
    stepGroupLayout->addWidget(stepDivisionLabel, 1, 0);

    m_stepDivisionCombo = new QComboBox(m_stepGroup);
    m_stepDivisionCombo->setObjectName("bpmStepDivisionCombo");
    m_stepDivisionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    stepGroupLayout->addWidget(m_stepDivisionCombo, 1, 1);

    m_stepApplyButton = new QToolButton(m_stepGroup);
    m_stepApplyButton->setObjectName("bpmStepApplyButton");
    m_stepApplyButton->setText(tr("Apply"));
    m_stepApplyButton->setToolTip(tr("Apply the selected BPM division to the function steps"));
    stepGroupLayout->addWidget(m_stepApplyButton, 1, 2);

    QLabel *stepHint = new QLabel(tr("Sets the RGB Matrix step or EFX loop length and "
                                     "extends the item to complete passes."), m_stepGroup);
    stepHint->setWordWrap(true);
    stepGroupLayout->addWidget(stepHint, 2, 0, 1, 3);

    m_stepGroup->hide();
    lay->addWidget(m_stepGroup);

    connect(m_stepDivisionCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotStepDivisionChanged(int)));
    connect(m_stepApplyButton, SIGNAL(clicked()),
            this, SLOT(slotStepApply()));

    m_durationOptions = new QGroupBox(tr("Duration options"));

    m_stretchOriginalRadio = new QRadioButton(tr("Stretch the original function duration"));
    m_stretchOriginalRadio->setObjectName("stretchDurationRadio");
    m_expandLoopRadio = new QRadioButton(tr("Loop function until duration is reached"));
    m_expandLoopRadio->setObjectName("loopDurationRadio");
    if (m_item->showFunction() != NULL && m_item->showFunction()->stretch() == true)
        m_stretchOriginalRadio->setChecked(true);
    else
        m_expandLoopRadio->setChecked(true);

    m_durationOptions->setLayout(new QVBoxLayout());
    m_durationOptions->layout()->addWidget(m_stretchOriginalRadio);
    m_durationOptions->layout()->addWidget(m_expandLoopRadio);
    m_durationOptions->hide();
    lay->addWidget(m_durationOptions);

    lay->addStretch();

    /* Position */
    QSettings settings;
    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid() == true)
        this->restoreGeometry(var.toByteArray());

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen != NULL)
    {
        const QSize available = screen->availableGeometry().size();
        const int maxWidth = qMax(320, available.width() - 40);
        const int maxHeight = qMax(320, available.height() - 80);
        QSize target;
        if (var.isValid() == true)
            target = size();
        else
            target = QSize(420, 760);
        target.setWidth(qMax(360, qMin(target.width(), maxWidth)));
        target.setHeight(qMax(420, qMin(target.height(), maxHeight)));
        resize(target);
    }

    AppUtil::ensureWidgetIsVisible(this);
}

TimingsTool::~TimingsTool()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

void TimingsTool::showDurationControls(bool show)
{
    if (show == true)
    {
        m_durationDial->show();
        if (m_bpmAvailable == true)
            m_bpmGroup->show();
        if (m_bpmAvailable == true && m_stepAvailable == true)
            m_stepGroup->show();
    }
    else
    {
        m_durationDial->hide();
        m_bpmGroup->hide();
        m_stepGroup->hide();
        m_durationOptions->hide();
    }
}

void TimingsTool::showDurationOptions(bool show)
{
    if (show == true)
        m_durationOptions->show();
    else
        m_durationOptions->hide();
}

void TimingsTool::setBpmContext(double bpm, int beatsPerBar)
{
    m_bpm = bpm;
    m_beatsPerBar = beatsPerBar > 0 ? beatsPerBar : 4;
    m_bpmAvailable = bpm > 0;

    if (m_bpmAvailable == false)
    {
        m_bpmGroup->hide();
        m_stepGroup->hide();
        return;
    }

    populateBpmDivisions();
    m_bpmGroup->show();
    updateBpmControls(true);
    updateStepControls(true);
}

void TimingsTool::setFunctionContext(Function *function)
{
    m_function = function;
    m_stepAvailable = (function != NULL &&
                       function->tempoType() == Function::Time &&
                       (function->type() == Function::RGBMatrixType ||
                        function->type() == Function::EFXType));

    if (m_bpmAvailable == true && m_stepAvailable == true)
    {
        m_stepGroup->show();
        updateStepControls(true);
    }
    else
    {
        m_stepGroup->hide();
    }
}

void TimingsTool::populateBpmDivisions()
{
    const int barBeats = m_beatsPerBar;
    struct Division
    {
        qreal beats;
        QString label;
    };

    QList<Division> divisions;
    divisions << Division{0.25, tr("1/4 beat")};
    divisions << Division{0.5, tr("1/2 beat")};
    divisions << Division{1.0, tr("1 beat")};
    divisions << Division{2.0, tr("2 beats")};
    divisions << Division{qreal(barBeats), tr("1 bar")};
    divisions << Division{qreal(barBeats * 2), tr("2 bars")};
    divisions << Division{qreal(barBeats * 4), tr("4 bars")};
    divisions << Division{qreal(barBeats * 8), tr("8 bars")};
    divisions << Division{qreal(barBeats * 16), tr("16 bars")};
    divisions << Division{qreal(barBeats * 32), tr("32 bars")};

    m_updatingBpm = true;
    m_bpmDivisionCombo->clear();
    m_stepDivisionCombo->clear();
    foreach (const Division &division, divisions)
    {
        const QVariant data(qRound(division.beats * 1000.0));
        m_bpmDivisionCombo->addItem(division.label, data);
        m_stepDivisionCombo->addItem(division.label, data);
    }
    /* Allow clearing a per-clip speed override and returning to the
     * source Function speed. */
    m_stepDivisionCombo->addItem(tr("Function default"), QVariant(0));
    m_updatingBpm = false;
}

quint32 TimingsTool::divisionDuration(int divisionIndex) const
{
    return divisionDuration(m_bpmDivisionCombo, divisionIndex);
}

quint32 TimingsTool::divisionDuration(QComboBox *combo, int divisionIndex) const
{
    if (combo == NULL || divisionIndex < 0 || divisionIndex >= combo->count() || m_bpm <= 0)
        return 0;

    const int milliBeats = combo->itemData(divisionIndex).toInt();
    if (milliBeats == 0)
        return 0;

    const qreal beats = qreal(milliBeats) / 1000.0;
    const qreal beatTime = 60000.0 / m_bpm;
    return qMax(1, qRound(beats * beatTime));
}

quint32 TimingsTool::stepDuration() const
{
    if (m_function == NULL)
        return 0;

    ShowFunction *clip = m_item != NULL ? m_item->showFunction() : NULL;
    if (clip != NULL && clip->functionSpeed() > 0)
        return clip->functionSpeed();

    if (m_function->type() == Function::RGBMatrixType)
    {
        RGBMatrix *matrix = qobject_cast<RGBMatrix *>(m_function);
        if (matrix == NULL)
            return 0;

        if (clip != NULL && clip->stretch() == true && clip->duration() > 0)
        {
            const quint32 sourceStep = matrix->duration();
            const quint32 sourceTotal = matrix->totalDuration();
            if (sourceStep > 0 && sourceStep != Function::infiniteSpeed() &&
                sourceTotal > sourceStep && sourceTotal != Function::infiniteSpeed())
            {
                const quint32 steps = sourceTotal / sourceStep;
                if (steps > 0)
                    return qMax<quint32>(1, clip->duration() / steps);
            }
            return clip->duration();
        }
        return matrix->duration();
    }
    if (m_function->type() == Function::EFXType)
    {
        if (clip != NULL && clip->stretch() == true && clip->duration() > 0)
            return clip->duration();

        EFX *efx = qobject_cast<EFX *>(m_function);
        return efx != NULL ? efx->duration() : 0;
    }

    return 0;
}

void TimingsTool::updateStepControls(bool initial)
{
    if (m_bpmAvailable == false || m_stepAvailable == false ||
        m_function == NULL || m_stepDivisionCombo->count() == 0)
    {
        m_stepGroup->hide();
        return;
    }

    ShowFunction *clip = m_item->showFunction();
    const bool hasOverride = (clip != NULL && clip->functionSpeed() > 0);
    const quint32 current = stepDuration();
    if (current == 0)
    {
        m_stepLengthLabel->setText(tr("Step: automatic (function default)"));
    }
    else
    {
        QString suffix;
        if (hasOverride == true)
            suffix = tr(" - clip override");
        else if (clip != NULL && clip->stretch() == true)
            suffix = tr(" - stretched");
        else
            suffix = tr(" - function default");

        m_stepLengthLabel->setText(tr("Step: %1 (%2)%3")
                                   .arg(musicalLength(current))
                                   .arg(Function::speedToString(current))
                                   .arg(suffix));
    }

    if (initial == true)
    {
        int exact = -1;
        int closest = 0;
        int defaultIndex = -1;
        quint32 closestDistance = UINT_MAX;
        for (int i = 0; i < m_stepDivisionCombo->count(); ++i)
        {
            const int data = m_stepDivisionCombo->itemData(i).toInt();
            if (data == 0)
            {
                defaultIndex = i;
                continue;
            }

            const quint32 divisionMs = divisionDuration(m_stepDivisionCombo, i);
            const quint32 distance = current > divisionMs ? current - divisionMs
                                                          : divisionMs - current;
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closest = i;
            }
            if (distance <= 2)
                exact = i;
        }

        int selected = defaultIndex;
        if (hasOverride == true)
            selected = exact >= 0 ? exact : closest;
        if (selected < 0)
            selected = 0;

        m_updatingBpm = true;
        m_stepDivisionCombo->setCurrentIndex(selected);
        m_updatingBpm = false;
    }

    const quint32 step = divisionDuration(m_stepDivisionCombo,
                                          m_stepDivisionCombo->currentIndex());
    m_stepApplyButton->setEnabled(step > 0 || hasOverride == true);
}

QString TimingsTool::musicalLength(quint32 msec) const
{
    if (msec == 0)
        return tr("Automatic");
    if (m_bpm <= 0)
        return Function::speedToString(msec);

    const qreal beatTime = 60000.0 / m_bpm;
    qreal totalBeats = qreal(msec) / beatTime;
    /* Round away millisecond-level rounding introduced by functions that
     * distribute their duration over multiple steps. */
    const int nearestBeat = qRound(totalBeats);
    if (qAbs(totalBeats - qreal(nearestBeat)) < 0.02)
        totalBeats = qreal(nearestBeat);

    const int bars = int(totalBeats / qreal(m_beatsPerBar));
    qreal remainingBeats = totalBeats - (bars * m_beatsPerBar);

    QStringList parts;
    if (bars > 0)
        parts << (bars == 1 ? tr("1 bar") : tr("%1 bars").arg(bars));

    if (remainingBeats > 0.001)
    {
        const int roundedBeats = qRound(remainingBeats);
        if (qAbs(remainingBeats - qreal(roundedBeats)) < 0.001)
        {
            parts << (roundedBeats == 1 ? tr("1 beat")
                                        : tr("%1 beats").arg(roundedBeats));
        }
        else
        {
            QString value = QString::number(remainingBeats, 'f', 2);
            while (value.endsWith('0') == true)
                value.chop(1);
            if (value.endsWith('.') == true)
                value.chop(1);
            parts << tr("%1 beats").arg(value);
        }
    }

    if (parts.isEmpty() == true)
        parts << tr("0 beats");

    return parts.join(" + ");
}

void TimingsTool::updateBpmControls(bool initial)
{
    if (m_bpmAvailable == false || m_item == NULL)
        return;

    const quint32 current = m_item->getDuration();
    if (current == 0)
    {
        m_bpmLengthLabel->setText(tr("Length: automatic (function duration)"));
    }
    else
    {
        m_bpmLengthLabel->setText(tr("Length: %1 (%2)")
                                  .arg(musicalLength(current))
                                  .arg(Function::speedToString(current)));
    }

    if (initial == true && m_bpmDivisionCombo->count() > 0)
    {
        const int selected = defaultBpmDivision(current);
        m_updatingBpm = true;
        m_bpmDivisionCombo->setCurrentIndex(selected);
        m_updatingBpm = false;
    }

    const quint32 step = divisionDuration(m_bpmDivisionCombo->currentIndex());
    m_bpmApplyButton->setEnabled(step > 0);
    m_bpmShortenButton->setEnabled(step > 0 && current > step);
    m_bpmExtendButton->setEnabled(step > 0);
}

int TimingsTool::defaultBpmDivision(quint32 duration) const
{
    int beatIndex = -1;
    int barIndex = -1;
    for (int i = 0; i < m_bpmDivisionCombo->count(); ++i)
    {
        const int milliBeats = m_bpmDivisionCombo->itemData(i).toInt();
        if (milliBeats == 1000)
            beatIndex = i;
        else if (milliBeats == m_beatsPerBar * 1000)
            barIndex = i;
    }

    if (duration == 0)
        return barIndex >= 0 ? barIndex : 0;
    if (barIndex >= 0 && duration + 5 >= divisionDuration(barIndex))
        return barIndex;
    if (beatIndex >= 0 && duration + 5 >= divisionDuration(beatIndex))
        return beatIndex;

    /* Never start with a bar-sized step for a short item. */
    return 1;
}

void TimingsTool::applyBpmDuration(quint32 msec)
{
    if (m_item == NULL)
        return;

    msec = qMax<quint32>(1, msec);
    const bool stretch = m_stretchOriginalRadio->isChecked();

    m_updatingBpm = true;
    m_durationDial->blockSignals(true);
    m_durationDial->setValue(int(msec));
    m_durationDial->blockSignals(false);
    emit durationChanged(m_item, msec, stretch);
    m_updatingBpm = false;

    updateBpmControls(false);
}

void TimingsTool::slotStartTimeChanged(int msec)
{
    emit startTimeChanged(m_item, msec);
}

void TimingsTool::slotDurationChanged(int msec)
{
    if (m_updatingBpm == true)
        return;

    if (m_stretchOriginalRadio->isChecked())
        emit durationChanged(m_item, msec, true);
    else
        emit durationChanged(m_item, msec, false);

    updateBpmControls(false);
}

void TimingsTool::slotBpmDivisionChanged(int index)
{
    Q_UNUSED(index)
    if (m_updatingBpm == true || m_bpmAvailable == false)
        return;

    updateBpmControls(false);
}

void TimingsTool::slotBpmApply()
{
    if (m_bpmAvailable == false || m_item == NULL)
        return;

    const quint32 msec = divisionDuration(m_bpmDivisionCombo->currentIndex());
    if (msec > 0)
        applyBpmDuration(msec);
}

void TimingsTool::slotBpmShorten()
{
    if (m_bpmAvailable == false || m_item == NULL)
        return;

    const quint32 step = divisionDuration(m_bpmDivisionCombo->currentIndex());
    const quint32 current = m_item->getDuration();
    if (step == 0 || current <= step)
        return;

    const quint32 nearest = qRound(qreal(current) / qreal(step));
    const bool onBoundary = qAbs(qint64(current) - qint64(nearest * step)) <= 5;
    quint32 next;
    if (onBoundary == true)
        next = (nearest > 1) ? (nearest - 1) * step : step;
    else
        next = qMax<quint32>(step, (current / step) * step);

    applyBpmDuration(next);
}

void TimingsTool::slotBpmExtend()
{
    if (m_bpmAvailable == false || m_item == NULL)
        return;

    const quint32 step = divisionDuration(m_bpmDivisionCombo->currentIndex());
    if (step == 0)
        return;

    const quint32 current = m_item->getDuration();
    quint32 next;
    if (current == 0)
    {
        next = step;
    }
    else
    {
        const quint32 nearest = qRound(qreal(current) / qreal(step));
        const bool onBoundary = qAbs(qint64(current) - qint64(nearest * step)) <= 5;
        if (onBoundary == true)
            next = (nearest + 1) * step;
        else
            next = ((current / step) + 1) * step;
    }

    applyBpmDuration(next);
}

void TimingsTool::slotStepDivisionChanged(int index)
{
    Q_UNUSED(index)
    if (m_updatingBpm == true || m_bpmAvailable == false)
        return;

    updateStepControls(false);
}

void TimingsTool::slotStepApply()
{
    if (m_bpmAvailable == false || m_stepAvailable == false ||
        m_function == NULL || m_item == NULL)
    {
        return;
    }

    const quint32 divisionMs = divisionDuration(m_stepDivisionCombo,
                                                m_stepDivisionCombo->currentIndex());

    ShowFunction *clip = m_item->showFunction();
    if (clip == NULL)
        return;

    if (divisionMs == 0)
    {
        const bool keepStretch = clip->stretch();
        if (clip->functionSpeed() != 0)
        {
            clip->setFunctionSpeed(0);
            emit durationChanged(m_item, m_item->getDuration(), keepStretch);
            updateBpmControls(false);
            updateStepControls(false);
        }
        return;
    }

    const quint32 oldItemDuration = m_item->getDuration();
    quint32 functionTotal = 0;

    if (m_function->type() == Function::RGBMatrixType)
    {
        RGBMatrix *matrix = qobject_cast<RGBMatrix *>(m_function);
        if (matrix == NULL)
            return;

        int steps = 0;
        if (matrix->duration() > 0 && matrix->duration() != Function::infiniteSpeed())
            steps = matrix->totalDuration() / matrix->duration();
        functionTotal = steps > 0 ? steps * divisionMs : divisionMs;
    }
    else if (m_function->type() == Function::EFXType)
    {
        functionTotal = divisionMs;
    }
    else
    {
        return;
    }

    quint32 itemDuration = functionTotal;
    if (oldItemDuration > 0 && functionTotal > 0)
    {
        const quint32 passes = qMax<quint32>(1, (oldItemDuration + functionTotal - 1) / functionTotal);
        itemDuration = passes * functionTotal;
    }

    /* Keep the speed override on the clip, so the source Function stays
     * untouched and can be reused by other clips with different timings. */
    clip->setFunctionSpeed(divisionMs);
    clip->setStretch(false);
    m_expandLoopRadio->setChecked(true);
    emit durationChanged(m_item, itemDuration, false);
    updateBpmControls(false);
    updateStepControls(false);
}
