/*
  Q Light Controller
  consolechannel.cpp

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

#include <QContextMenuEvent>
#include <QIntValidator>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QIntValidator>
#include <QLineEdit>
#include <QTimer>
#include <QDebug>
#include <QLabel>
#include <QMenu>
#include <QList>

#include "qlcchannel.h"
#include "qlccapability.h"

#include "doc.h"
#include "fixture.h"
#include "apputil.h"
#include "appicons.h"
#include "theme.h"
#include "consolechannel.h"

/*****************************************************************************
 * Initialization
 *****************************************************************************/

ConsoleChannel::ConsoleChannel(QWidget* parent, Doc* doc, quint32 fixture, quint32 channel, bool isCheckable)
    : QGroupBox(parent)
    , m_doc(doc)
    , m_fixture(fixture)
    , m_chIndex(channel)
    , m_group(Fixture::invalidId())
    , m_presetButton(NULL)
    , m_cngWidget(NULL)
    , m_spin(NULL)
    , m_valueEdit(NULL)
    , m_slider(NULL)
    , m_label(NULL)
    , m_resetButton(NULL)
    , m_showResetButton(false)
    , m_menu(NULL)
    , m_selected(false)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(channel != QLCChannel::invalid());

    if (isCheckable == true)
        setCheckable(true);
    setFocusPolicy(Qt::NoFocus);
    init();
}

ConsoleChannel::~ConsoleChannel()
{
}

void ConsoleChannel::init()
{
    Fixture* fxi = m_doc->fixture(m_fixture);
    //Q_ASSERT(fxi != NULL);

    new QVBoxLayout(this);
    layout()->setSpacing(0);
    layout()->setContentsMargins(0, 2, 0, 2);

    if (fxi != NULL)
    {
        m_presetButton = new QToolButton(this);
        m_presetButton->setStyle(AppUtil::saneStyle());
        layout()->addWidget(m_presetButton);
        layout()->setAlignment(m_presetButton, Qt::AlignHCenter);
        m_presetButton->setIconSize(QSize(32, 32));
        m_presetButton->setMinimumSize(QSize(32, 32));
        m_presetButton->setMaximumSize(QSize(32, 32));
        m_presetButton->setFocusPolicy(Qt::NoFocus);

        /* Create a menu only if channel has sophisticated contents */
        if (fxi->fixtureDef() != NULL && fxi->fixtureMode() != NULL)
            initMenu();
        else
            m_presetButton->setStyleSheet("QToolButton { border-image: url(:/intensity.png) 0 0 0 0 stretch stretch; }");
    }

    /* Value edit */
    m_spin = new QSpinBox(this);
    m_spin->setRange(0, UCHAR_MAX);
    m_spin->setValue(0);
    m_spin->setMinimumWidth(48);
    m_spin->setMaximumWidth(60);
    m_spin->setMinimumHeight(26);
    m_spin->setFocusPolicy(Qt::StrongFocus);
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setToolTip(tr("Channel value (0-255)"));
    layout()->addWidget(m_spin);
    m_spin->setAlignment(Qt::AlignCenter);
    m_spin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout()->setAlignment(m_spin, Qt::AlignHCenter);

    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setAlignment(Qt::AlignCenter);
    m_valueEdit->setValidator(new QIntValidator(0, 255, m_valueEdit));
    m_valueEdit->setMinimumWidth(48);
    m_valueEdit->setMaximumWidth(60);
    m_valueEdit->setFixedHeight(26);
    m_valueEdit->setText("0");
    m_valueEdit->setToolTip(tr("Channel value (0-255)"));
    layout()->addWidget(m_valueEdit);
    layout()->setAlignment(m_valueEdit, Qt::AlignHCenter);
    m_spin->hide();

    connect(m_valueEdit, &QLineEdit::editingFinished, this, [this]()
    {
        bool ok = false;
        const int value = m_valueEdit->text().toInt(&ok);
        if (ok == false)
        {
            m_valueEdit->setText(QString::number(m_spin->value()));
            return;
        }
        m_spin->setValue(qBound(0, value, 255));
    });

    /* Value slider */
    m_slider = new ClickAndGoSlider(this);
    m_slider->setInvertedAppearance(false);
    m_slider->setRange(0, UCHAR_MAX);
    m_slider->setPageStep(1);
    m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_slider->setFocusPolicy(Qt::NoFocus);
    connect(m_slider, SIGNAL(controlClicked()),
            this, SLOT(slotControlClicked()));

    QString style = cngDefaultStyle(false, 22, 0.78, 24, 18);

    QString userStyle = AppUtil::getStyleSheet("CONSOLE_CHANNEL_COMMON");
    if (!userStyle.isEmpty())
        style = userStyle;

    m_slider->setMinimumWidth(48);
    m_slider->setMaximumWidth(56);
    m_slider->setMinimumHeight(90);
    m_slider->setHandleValueLine(true);
    m_slider->setVisible(false);
    m_slider->setSliderStyleSheet(style);
    layout()->addWidget(m_slider);
    layout()->setAlignment(m_slider, Qt::AlignHCenter);

    /* Channel number label */
    m_label = new QLabel(this);
    m_label->setMinimumWidth(25);
    m_label->setMaximumWidth(80);
    layout()->addWidget(m_label);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(QString::number(m_chIndex + 1));
    m_label->setFocusPolicy(Qt::NoFocus);
    m_label->setWordWrap(true);
    m_label->setAutoFillBackground(false);
    updateEditorStyle();

    /* Set tooltip */
    if (fxi == NULL)
    {
        setToolTip(tr("Intensity"));
    }
    else
    {
        const QLCChannel *ch = fxi->channel(m_chIndex);
        Q_ASSERT(ch != NULL);
        setToolTip(QString("%1").arg(ch->name()));
        setValue(ch->defaultValue(), false);
        m_channel = ch;
    }

    m_spin->hide();
    connect(m_spin, SIGNAL(valueChanged(int)), this, SLOT(slotSpinChanged(int)));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(slotSliderChanged(int)));
    connect(this, SIGNAL(toggled(bool)), this, SLOT(slotChecked(bool)));
}

void ConsoleChannel::showEvent(QShowEvent *)
{
    if (m_styleSheet.isEmpty() == false)
    {
        setChannelStyleSheet(m_styleSheet);
        m_styleSheet = "";
    }

    updateEditorStyle();
    QTimer::singleShot(0, this, [this]() { updateEditorStyle(); });
    m_slider->setVisible(true);
    m_spin->hide();
    if (m_valueEdit != NULL)
    {
        m_valueEdit->setVisible(true);
        m_valueEdit->raise();
    }
    m_label->setVisible(true);
}

/*****************************************************************************
 * Fixture & Channel
 *****************************************************************************/

quint32 ConsoleChannel::fixture() const
{
    return m_fixture;
}

quint32 ConsoleChannel::channelIndex() const
{
    return m_chIndex;
}

const QLCChannel *ConsoleChannel::channel()
{
    return m_channel;
}

/*************************************************************************
 * Group
 *************************************************************************/
void ConsoleChannel::setLabel(QString label)
{
    m_label->setText(label);
}

void ConsoleChannel::setChannelsGroup(quint32 grpid)
{
    m_group = grpid;
    ChannelsGroup *grp = m_doc->channelsGroup(grpid);
    connect(grp, SIGNAL(valueChanged(quint32,uchar)),
            this, SLOT(slotInputValueChanged(quint32,uchar)));
}

void ConsoleChannel::slotInputValueChanged(quint32 channel, uchar value)
{
    Q_UNUSED(channel)
    setValue(value);
}

/*****************************************************************************
 * Value
 *****************************************************************************/

void ConsoleChannel::setValue(uchar value, bool apply)
{
    if (m_slider->value() == value &&
        m_spin->value() == value)
            return;

    if (apply == false)
    {
        m_spin->blockSignals(true);
        m_slider->blockSignals(true);
    }

    m_slider->setValue(int(value));
    m_spin->setValue(int(value));
    if (m_valueEdit != NULL)
        m_valueEdit->setText(QString::number(int(value)));

    if (apply == false)
    {
        m_spin->blockSignals(false);
        m_slider->blockSignals(false);
    }
}

uchar ConsoleChannel::value() const
{
    return uchar(m_slider->value());
}

void ConsoleChannel::slotSpinChanged(int value)
{
    if (value != m_slider->value())
        m_slider->setValue(value);
    if (m_valueEdit != NULL && m_valueEdit->text().toInt() != value)
        m_valueEdit->setText(QString::number(value));

    if (m_group == Fixture::invalidId())
        emit valueChanged(m_fixture, m_chIndex, value);
    else
        emit groupValueChanged(m_group, value);
}

void ConsoleChannel::slotSliderChanged(int value)
{
    if (value != m_spin->value())
        m_spin->setValue(value);
    if (m_valueEdit != NULL && m_valueEdit->text().toInt() != value)
        m_valueEdit->setText(QString::number(value));
}
void ConsoleChannel::slotChecked(bool state)
{
    emit checked(m_fixture, m_chIndex, state);

    // Emit the current value also when turning the channel back on
    if (state == true)
        emit valueChanged(m_fixture, m_chIndex, m_slider->value());
}

/*************************************************************************
 * Look & Feel
 *************************************************************************/

void ConsoleChannel::setChannelStyleSheet(const QString &styleSheet)
{
    if (isVisible())
    {
        QGroupBox::setStyleSheet(styleSheet);
        updateEditorStyle();
        QTimer::singleShot(0, this, [this]() { updateEditorStyle(); });
    }
    else
        m_styleSheet = styleSheet;
}

void ConsoleChannel::updateEditorStyle()
{
    Theme *theme = Theme::instance();
    const QString text = theme->color(Theme::Text).name();
    m_spin->setStyleSheet(QString("QSpinBox { background-color: %1; color: %2; "
                                  "border: 1px solid %3; border-radius: 4px; "
                                  "padding: 2px; }")
                          .arg(theme->color(Theme::SurfaceRaised).name())
                          .arg(text)
                          .arg(theme->color(Theme::BorderStrong).name()));

    if (m_valueEdit != NULL)
    {
        m_valueEdit->setStyleSheet(QString("QLineEdit { background-color: %1; color: %2; "
                                           "border: 1px solid %3; border-radius: 4px; "
                                           "padding: 2px; }")
                                   .arg(theme->color(Theme::SurfaceRaised).name())
                                   .arg(text)
                                   .arg(theme->color(Theme::BorderStrong).name()));
    }

    m_label->setStyleSheet(QString("QLabel { background: transparent; color: %1; }")
                           .arg(theme->color(Theme::TextDim).name()));
}

void ConsoleChannel::showResetButton(bool show)
{
    if (show == true)
    {
        if (m_resetButton == NULL)
        {
            m_resetButton = new QToolButton(this);
            m_resetButton->setStyle(AppUtil::saneStyle());
            layout()->addWidget(m_resetButton);
            layout()->setAlignment(m_resetButton, Qt::AlignHCenter);
            m_resetButton->setIconSize(QSize(32, 32));
            m_resetButton->setMinimumSize(QSize(32, 32));
            m_resetButton->setMaximumSize(QSize(32, 32));
            m_resetButton->setFocusPolicy(Qt::NoFocus);
            m_resetButton->setIcon(AppIcons::icon("x", 16, AppIcons::Destructive));
            m_resetButton->setToolTip(tr("Reset this channel"));
        }
        connect(m_resetButton, SIGNAL(clicked(bool)),
                this, SLOT(slotResetButtonClicked()));
    }
    else
    {
        if (m_resetButton != NULL)
        {
            layout()->removeWidget(m_resetButton);
            delete m_resetButton;
            m_resetButton = NULL;
        }
    }
}

bool ConsoleChannel::hasResetButton() const
{
    return m_resetButton != NULL ? true : false;
}

void ConsoleChannel::slotResetButtonClicked()
{
    emit resetRequest(m_fixture, m_chIndex);
}

/*****************************************************************************
 * Menu
 *****************************************************************************/

void ConsoleChannel::initMenu()
{
    Fixture* fxi = m_doc->fixture(fixture());
    Q_ASSERT(fxi != NULL);

    const QLCChannel* ch = fxi->channel(m_chIndex);
    Q_ASSERT(ch != NULL);

    // Get rid of a possible previous menu
    if (m_menu != NULL)
    {
        delete m_menu;
        m_menu = NULL;
    }

    // Create a popup menu and set the channel name as its title
    m_menu = new QMenu(this);
    m_presetButton->setMenu(m_menu);
    m_presetButton->setPopupMode(QToolButton::InstantPopup);

    QString btnIconStr = ch->getIconNameFromGroup(ch->group());
    if (btnIconStr.startsWith(":"))
        m_presetButton->setStyleSheet("QToolButton { border-image: url(" + btnIconStr + ") 0 0 0 0 stretch stretch; }");
    else
    {
        m_presetButton->setStyleSheet("QToolButton { background: " + btnIconStr + "; }");
        setIntensityButton(ch);
    }

    switch(ch->group())
    {
    case QLCChannel::Colour:
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Preset, ch);
        break;
    case QLCChannel::Effect:
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Preset, ch);
        break;
    case QLCChannel::Gobo:
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Preset, ch);
        break;
    default:
        break;
    }

    if (m_cngWidget != NULL)
    {
        QWidgetAction* action = new QWidgetAction(this);
        action->setDefaultWidget(m_cngWidget);
        m_menu->addAction(action);
        connect(m_cngWidget, SIGNAL(levelChanged(uchar)),
                this, SLOT(slotClickAndGoLevelChanged(uchar)));
        connect(m_cngWidget, SIGNAL(levelAndPresetChanged(uchar,QImage)),
                this, SLOT(slotClickAndGoLevelAndPresetChanged(uchar, QImage)));
    }
    else
    {
        QAction* action = m_menu->addAction(m_presetButton->icon(), ch->name());
        m_menu->setTitle(ch->name());
        action->setEnabled(false);
        m_menu->addSeparator();

        // Initialize the preset menu only for intelligent fixtures
        initCapabilityMenu(ch);
    }
}

void ConsoleChannel::setIntensityButton(const QLCChannel* channel)
{
    QFont fnt = m_presetButton->font();
    fnt.setBold(true);
    m_presetButton->setFont(fnt);

    if (channel->colour() == QLCChannel::Red)
    {
        m_presetButton->setText("R"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Red);
    }
    else if (channel->colour() == QLCChannel::Green)
    {
        m_presetButton->setText("G"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Green);
    }
    else if (channel->colour() == QLCChannel::Blue)
    {
        QPalette pal = m_presetButton->palette();
        pal.setColor(QPalette::ButtonText, Qt::white); // Improve contrast
        m_presetButton->setPalette(pal);
        m_presetButton->setText("B"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Blue);
    }
    else if (channel->colour() == QLCChannel::Cyan)
    {
        m_presetButton->setText("C"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Cyan);
    }
    else if (channel->colour() == QLCChannel::Magenta)
    {
        m_presetButton->setText("M"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Magenta);
    }
    else if (channel->colour() == QLCChannel::Yellow)
    {
        m_presetButton->setText("Y"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Yellow);
    }
    else if (channel->colour() == QLCChannel::Amber)
    {
        m_presetButton->setText("A"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Amber);
    }
    else if (channel->colour() == QLCChannel::White)
    {
        m_presetButton->setText("W"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::White);
    }
    else if (channel->colour() == QLCChannel::UV)
    {
        m_presetButton->setText("UV"); // Don't localize
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::UV);
    }
    else if (channel->colour() == QLCChannel::Lime)
    {
        m_presetButton->setText("L");
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Lime);
    }
    else if (channel->colour() == QLCChannel::Indigo)
    {
        m_presetButton->setText("I");
        m_cngWidget = new ClickAndGoWidget();
        m_cngWidget->setType(ClickAndGoWidget::Indigo);
    }
    else
    {
        // None of the primary colours matched and since this is an
        // intensity channel, it must be controlling a plain dimmer OSLT.
        m_presetButton->setStyleSheet("QToolButton { border-image: url(:/intensity.png) 0 0 0 0 stretch stretch; }");
    }
}

void ConsoleChannel::initCapabilityMenu(const QLCChannel* ch)
{
    QLCCapability* cap;
    QMenu* valueMenu;
    QAction* action;
    QString s;
    QString t;

    QListIterator <QLCCapability*> it(ch->capabilities());
    while (it.hasNext() == true)
    {
        cap = it.next();

        // Set the value range and name as the menu item's name
        s = QString("%1: %2 - %3").arg(cap->name())
            .arg(cap->min()).arg(cap->max());

        if (cap->max() - cap->min() > 0)
        {
            // Create submenu for ranges of more than one value
            valueMenu = new QMenu(m_menu);
            valueMenu->setTitle(s);

            /* Add a color icon */
            if (ch->group() == QLCChannel::Colour)
                valueMenu->setIcon(colorIcon(cap->name()));

            for (int i = cap->min(); i <= cap->max(); i++)
            {
                action = valueMenu->addAction(t.asprintf("%.3d", i));
                action->setData(i);
            }

            m_menu->addMenu(valueMenu);
        }
        else
        {
            // Just one value in this range, put that into the menu
            action = m_menu->addAction(s);
            action->setData(cap->min());

            /* Add a color icon */
            if (ch->group() == QLCChannel::Colour)
                action->setIcon(colorIcon(cap->name()));
        }
    }

    // Connect menu item activation signal to this
    connect(m_menu, SIGNAL(triggered(QAction*)),
            this, SLOT(slotContextMenuTriggered(QAction*)));

    // Set the menu also as the preset button's popup menu
    m_presetButton->setMenu(m_menu);
}

QIcon ConsoleChannel::colorIcon(const QString& name)
{
    /* Return immediately with a rainbow icon -- if appropriate */
    if (name.toLower().contains("rainbow") ||
            name.toLower().contains("cw") == true)
    {
        return AppIcons::icon("scene-color", 32);
    }
    else if (name.toLower().contains("cto") == true)
    {
        QColor color(255, 201, 0);
        QPixmap pm(32, 32);
        pm.fill(color);
        return QIcon(pm);
    }
    else if (name.toLower().contains("ctb") == true)
    {
        QColor color(0, 128, 190);
        QPixmap pm(32, 32);
        pm.fill(color);
        return QIcon(pm);
    }
    else if (name.toLower().contains("uv") == true)
    {
        QColor color(37, 0, 136);
        QPixmap pm(32, 32);
        pm.fill(color);
        return QIcon(pm);
    }

#ifdef Q_WS_X11
    QColor::setAllowX11ColorNames(true);
#endif
    QStringList colorList(QColor::colorNames());
    QString colname;
    QColor color;
    int index;

    colname = name.toLower().remove(QRegularExpression("[0-9]")).remove(' ');
    index = colorList.indexOf(colname);
    if (index != -1)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
        color.setNamedColor(colname);
#else
        color.fromString(colname);
#endif
    }
    else
    {
        QString re("(");
        QListIterator <QString> it(name.toLower().split(" "));
        while (it.hasNext() == true)
        {
            re += it.next();
            if (it.hasNext() == true)
                re += "|";
        }
        re += ")";

        QRegularExpression regex(re, QRegularExpression::CaseInsensitiveOption);
        index = colorList.indexOf(regex);
        if (index != -1)
        {
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
            color.setNamedColor(colorList.at(index));
#else
            color.fromString(colorList.at(index));
#endif
        }
    }

    if (color.isValid() == true)
    {
        QPixmap pm(32, 32);
        pm.fill(color);
        return QIcon(pm);
    }
    else
    {
        return QIcon();
    }
}

void ConsoleChannel::contextMenuEvent(QContextMenuEvent* e)
{
    // Show the preset menu only of it has been created.
    // Generic dimmer fixtures don't have capabilities and so
    // they will not have these menus either.
    if (m_menu != NULL)
    {
        m_menu->exec(e->globalPos());
        e->accept();
    }
}

void ConsoleChannel::slotContextMenuTriggered(QAction* action)
{
    Q_ASSERT(action != NULL);

    // The menuitem's data contains a valid DMX value
    setValue(action->data().toInt());
}

void ConsoleChannel::slotClickAndGoLevelChanged(uchar level)
{
    setValue(level);
}

void ConsoleChannel::slotClickAndGoLevelAndPresetChanged(uchar level, QImage img)
{
    Q_UNUSED(img)
    setValue(level);
}

/*************************************************************************
 * Selection
 *************************************************************************/

bool ConsoleChannel::isSelected() const
{
    return m_selected;
}

void ConsoleChannel::slotControlClicked()
{
    qDebug() << "CONTROL modifier + click";
    if (m_selected == false)
    {
        m_originalStyle = styleSheet();
        int topMargin = isCheckable()?16:1;

        QString common = "QGroupBox::title {top:-15px; left: 12px; subcontrol-origin: border; background-color: transparent; } "
                         "QGroupBox::indicator { width: 18px; height: 18px; } "
                         "QGroupBox::indicator:checked { image: url(:/checkbox_full.png) } "
                         "QGroupBox::indicator:unchecked { image: url(:/checkbox_empty.png) }";
        QString ssSelected = QString("QGroupBox { background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #D9D730, stop: 1 #AFAD27); "
                                 "border: 1px solid gray; border-radius: 4px; margin-top: %1px; margin-right: 1px; } " +
                                 (isCheckable()?common:"")).arg(topMargin);
        setChannelStyleSheet(ssSelected);
        m_selected = true;
    }
    else
    {
        setChannelStyleSheet(m_originalStyle);
        m_selected = false;
    }
}
