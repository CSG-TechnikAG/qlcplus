/*
  Q Light Controller
  grandmasterslider.cpp

  Copyright (c) Heikki Junnila

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <cmath>

#include "grandmasterslider.h"
#include "clickandgoslider.h"
#include "qlcinputchannel.h"
#include "virtualconsole.h"
#include "vcproperties.h"
#include "inputpatch.h"
#include "apputil.h"
#include "theme.h"

GrandMasterSlider::GrandMasterSlider(QWidget* parent, InputOutputMap *ioMap)
    : QFrame(parent)
    , m_ioMap(ioMap)
{
    Q_ASSERT(ioMap != NULL);

    Theme *theme = Theme::instance();
    QString sStyle = AppUtil::getStyleSheet("GRANDMASTER");
    if (sStyle.isEmpty())
    {
        sStyle = QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 6px; }")
                 .arg(theme->color(Theme::SurfaceRaised).name())
                 .arg(theme->color(Theme::Border).name());
    }
    setStyleSheet(sStyle);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);

    setMinimumSize(QSize(40, 100));
    setMaximumSize(QSize(40, USHRT_MAX));

    new QVBoxLayout(this);
    layout()->setContentsMargins(2, 2, 2, 2);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignHCenter);
    m_valueLabel->setStyleSheet("QFrame { background-color: transparent; border: 0px; border-radius: 0px; }");
    layout()->addWidget(m_valueLabel);

    m_slider = new ClickAndGoSlider(this);
    m_slider->setRange(0, UCHAR_MAX);
    const QColor handleBase = theme->color(Theme::Text);
    const QString handleColor = QString("rgba(%1, %2, %3, 0.78)")
                                .arg(handleBase.red()).arg(handleBase.green()).arg(handleBase.blue());
    m_slider->setStyleSheet(QString(
        "QSlider::groove:vertical { background: transparent; width: 24px; } "
        "QSlider::sub-page:vertical { background: %1; border: 1px solid %2; margin: 0 1px; border-radius: 3px; } "
        "QSlider::add-page:vertical { background: %3; border: none; margin: 0 3px; border-radius: 2px; } "
        "QSlider::handle:vertical { background: %4; border: 1px solid %2; border-radius: 4px; margin: 0 -1px; height: 20px; } "
        "QSlider::handle:vertical:hover { background: %3; border-color: %3; } "
        "QSlider::handle:vertical:disabled { background: %5; border-color: %2; }")
        .arg(theme->color(Theme::SurfaceHover).name())
        .arg(theme->color(Theme::BorderStrong).name())
        .arg(theme->color(Theme::Danger).name())
        .arg(handleColor)
        .arg(theme->color(Theme::TextDisabled).name()));
    if (ClickAndGoSlider *s = qobject_cast<ClickAndGoSlider *>(m_slider))
        s->setHandleValueLine(true);
    m_slider->setMinimumSize(QSize(30, 50));
    m_slider->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    layout()->addWidget(m_slider);
    layout()->setAlignment(m_slider, Qt::AlignHCenter);
    m_slider->setValue(255);
    connect(m_slider, SIGNAL(valueChanged(int)),
            this, SLOT(slotValueChanged(int)));

    m_nameLabel = new QLabel(this);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setAlignment(Qt::AlignHCenter);
    m_nameLabel->setText(tr("GM"));
    layout()->addWidget(m_nameLabel);

    /* Listen to GM value changes */
    connect(m_ioMap, SIGNAL(grandMasterValueChanged(uchar)),
            this, SLOT(slotGrandMasterValueChanged(uchar)));
    connect(m_ioMap, SIGNAL(grandMasterValueModeChanged(GrandMaster::ValueMode)),
            this, SLOT(slotGrandMasterValueModeChanged(GrandMaster::ValueMode)));

    /* External input connection */
    connect(m_ioMap, SIGNAL(inputValueChanged(quint32, quint32, uchar)),
            this, SLOT(slotInputValueChanged(quint32, quint32, uchar)));
    connect(Theme::instance(), &Theme::changed, this, &GrandMasterSlider::refreshTheme);

    updateTooltip();
    updateDisplayValue();
}

GrandMasterSlider::~GrandMasterSlider()
{
}

void GrandMasterSlider::refreshTheme()
{
    Theme *theme = Theme::instance();
    QString sStyle = AppUtil::getStyleSheet("GRANDMASTER");
    if (sStyle.isEmpty())
    {
        sStyle = QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 6px; }")
                 .arg(theme->color(Theme::SurfaceRaised).name())
                 .arg(theme->color(Theme::Border).name());
    }
    setStyleSheet(sStyle);

    if (m_slider != NULL)
    {
        const QColor track = theme->color(Theme::SurfaceHover);
        const QColor border = theme->color(Theme::BorderStrong);
        const QColor fill = theme->color(Theme::Danger);
        const QColor handle = theme->color(Theme::Text);
        const QColor disabled = theme->color(Theme::TextDisabled);
        const QString handleColor = QString("rgba(%1, %2, %3, 0.78)")
                                    .arg(handle.red()).arg(handle.green()).arg(handle.blue());
        const bool inverted = m_slider->invertedAppearance();
        const QString trackPage = QString("background: %1; border: 1px solid %2; margin: 0 1px; border-radius: 3px;")
                                  .arg(track.name(), border.name());
        const QString fillPage = QString("background: %1; border: none; margin: 0 3px; border-radius: 2px;")
                                 .arg(fill.name());

        m_slider->setStyleSheet(QString(
            "QSlider::groove:vertical { background: transparent; width: 24px; } "
            "QSlider::sub-page:vertical { %1 } "
            "QSlider::add-page:vertical { %2 } "
            "QSlider::handle:vertical { background: %3; border: 1px solid %4; border-radius: 4px; margin: 0 -1px; height: 20px; } "
            "QSlider::handle:vertical:hover { background: %5; border-color: %5; } "
            "QSlider::handle:vertical:disabled { background: %6; border-color: %4; }")
            .arg(inverted ? fillPage : trackPage,
                 inverted ? trackPage : fillPage,
                 handleColor, border.name(), fill.name(), disabled.name()));
    }
}

bool GrandMasterSlider::invertedAppearance() const
{
    Q_ASSERT(m_slider != NULL);
    return m_slider->invertedAppearance();
}

void GrandMasterSlider::setInvertedAppearance(bool invert)
{
    Q_ASSERT(m_slider != NULL);
    m_slider->setInvertedAppearance(invert);
    refreshTheme();
    sendFeedback();
}

void GrandMasterSlider::slotValueChanged(int value)
{
    // Update display value
    updateDisplayValue();

    // Avoid double calls triggered by slotGrandMasterValueChanged
    int curval = m_ioMap->grandMasterValue();
    if (value != curval)
    {
        // Write new grand master value to universes
        m_ioMap->setGrandMasterValue(value);
    }
}

void GrandMasterSlider::updateTooltip()
{
    QString tooltip;

    switch (m_ioMap->grandMasterValueMode())
    {
        case GrandMaster::Limit:
            tooltip += tr("Grand Master <B>limits</B> the maximum value of");
            break;
        case GrandMaster::Reduce:
            tooltip += tr("Grand Master <B>reduces</B> the current value of");
            break;
    }

    tooltip += QString(" ");

    switch (m_ioMap->grandMasterChannelMode())
    {
        case GrandMaster::Intensity:
            tooltip += tr("intensity channels");
            break;
        case GrandMaster::AllChannels:
            tooltip += tr("all channels");
            break;
    }

    setToolTip(tooltip);
}

void GrandMasterSlider::updateDisplayValue()
{
    int value = m_slider->value();
    QString str;
    if (m_ioMap->grandMasterValueMode() == GrandMaster::Limit)
    {
        str = QString("%1").arg(value, 3, 10, QChar('0'));
    }
    else
    {
        int p = floor(((double(value) / double(UCHAR_MAX)) * double(100)) + 0.5);
        str = QString("%1%").arg(p, 2, 10, QChar('0'));
    }
    m_valueLabel->setText(str);
    sendFeedback();
}

void GrandMasterSlider::slotGrandMasterValueChanged(uchar value)
{
    m_slider->blockSignals(true);
    m_slider->setValue(value);
    m_slider->blockSignals(false);

    updateDisplayValue();
}

void GrandMasterSlider::slotGrandMasterValueModeChanged(GrandMaster::ValueMode mode)
{
    Q_UNUSED(mode);
    updateTooltip();
    updateDisplayValue();
}

void GrandMasterSlider::sendFeedback()
{
    quint32 universe = VirtualConsole::instance()->properties().grandMasterInputUniverse();
    quint32 channel = VirtualConsole::instance()->properties().grandMasterInputChannel();
    QString chName;

    if (universe == InputOutputMap::invalidUniverse() || channel == QLCChannel::invalid())
        return;

    InputPatch* pat = m_ioMap->inputPatch(universe);
    if (pat != NULL)
    {
        QLCInputProfile* profile = pat->profile();
        if (profile != NULL)
        {
            QLCInputChannel* ich = profile->channel(channel);
            if (ich != NULL)
                chName = ich->name();
        }
    }
    if (m_slider->invertedAppearance())
        m_ioMap->sendFeedBack(universe, channel, UCHAR_MAX - m_slider->value(), chName);
    else
        m_ioMap->sendFeedBack(universe, channel, m_slider->value(), chName);
}

/*****************************************************************************
 * External input
 *****************************************************************************/

void GrandMasterSlider::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    if (universe == VirtualConsole::instance()->properties().grandMasterInputUniverse() &&
        channel == VirtualConsole::instance()->properties().grandMasterInputChannel())
    {
        m_slider->setValue(value);
    }
}

