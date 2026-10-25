/*
  Q Light Controller Plus
  appearancedialog.cpp

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

#include <QApplication>
#include <QButtonGroup>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFont>
#include <QPainter>
#include <QtMath>

#include "appearancedialog.h"
#include "appdialog.h"
#include "appsettingrow.h"
#include "appsegmentedcontrol.h"
#include "appswitch.h"
#include "appcard.h"
#include "appicons.h"
#include "mainshell.h"
#include "theme.h"

static QIcon accentSwatch(const QColor &color)
{
    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    const int side = qRound(18.0 * dpr);
    QPixmap swatch(side, side);
    swatch.setDevicePixelRatio(dpr);
    swatch.fill(Qt::transparent);

    QPainter painter(&swatch);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Theme::contrastingText(color), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(1, 1, 16, 16, 5, 5);
    painter.end();

    return QIcon(swatch);
}

static QIcon solidThemeIcon(const QString &name, int size, const QColor &color)
{
    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    return QIcon(AppIcons::pixmap(name, size, color, dpr));
}

AppearanceDialog::AppearanceDialog(MainShell *shell, QWidget *parent)
    : AppDialog(tr("Appearance"), parent)
    , m_shell(shell)
    , m_themeGroup(new QButtonGroup(this))
    , m_accentGroup(new QButtonGroup(this))
    , m_navigation(NULL)
    , m_motion(NULL)
{
    setWindowTitle(tr("Appearance"));
    setModal(true);
    resize(560, 760);

    setSubtitle(tr("Theme, colors, navigation and motion."));
    setHeaderIcon("appearance", 24);

    m_themeGroup->setExclusive(true);
    m_accentGroup->setExclusive(true);

    QVBoxLayout *root = contentLayout();

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *content = new QWidget(scroll);
    QVBoxLayout *cards = new QVBoxLayout(content);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setSpacing(8);

    /* Theme */
    AppCard *themeCard = new AppCard(content);
    themeCard->setTitle(tr("Theme"));
    themeCard->setSubtitle(tr("Choose the contrast and surface style of the application."));
    QGridLayout *themeGrid = new QGridLayout;
    themeGrid->setSpacing(6);
    const Theme::Scheme schemes[] = { Theme::Dark, Theme::Light,
                                      Theme::OLED, Theme::Classic };
    const char *schemeIcons[] = { "moon", "sun", "circle-dot", "square" };
    for (int i = 0; i < 4; i++)
    {
        QPushButton *button = new QPushButton(AppIcons::icon(schemeIcons[i], 18),
                                              Theme::schemeDisplayName(schemes[i]),
                                              themeCard);
        button->setCheckable(true);
        button->setProperty("qlcSchemeIcon", QString::fromLatin1(schemeIcons[i]));
        button->setMinimumHeight(36);
        button->setIconSize(QSize(18, 18));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_themeGroup->addButton(button, static_cast<int>(schemes[i]));
        themeGrid->addWidget(button, i / 2, i % 2);
        const int scheme = static_cast<int>(schemes[i]);
        connect(button, &QPushButton::clicked, this, [this, scheme]()
        {
            applyTheme(scheme);
        });
    }
    themeCard->contentLayout()->addLayout(themeGrid);
    cards->addWidget(themeCard);

    /* Accent */
    AppCard *accentCard = new AppCard(content);
    accentCard->setTitle(tr("Accent color"));
    accentCard->setSubtitle(tr("Used for selection, primary actions and focus."));
    QHBoxLayout *accentLay = new QHBoxLayout;
    accentLay->setSpacing(6);
    QList<QColor> accents = Theme::accentPresets();
    for (int i = 0; i < accents.count(); i++)
    {
        const QColor color = accents.at(i);
        QToolButton *button = new QToolButton(accentCard);
        button->setCheckable(true);
        button->setFixedSize(30, 30);
        button->setIcon(accentSwatch(color));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(color.name());
        m_accentGroup->addButton(button, i);
        button->setProperty("qlcAccent", color.name());
        accentLay->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, color]()
        {
            applyAccent(color);
        });
    }
    QPushButton *customAccent = new QPushButton(tr("Custom..."), accentCard);
    customAccent->setObjectName("appearanceCustomAccent");
    customAccent->setIcon(AppIcons::icon("plus", 16));
    connect(customAccent, &QPushButton::clicked, this, [this]()
    {
        const QColor color = QColorDialog::getColor(Theme::instance()->accent(), this,
                                                    tr("Select accent color"));
        if (color.isValid() == true)
            applyAccent(color);
    });
    accentLay->addWidget(customAccent);
    accentLay->addStretch(1);
    accentCard->contentLayout()->addLayout(accentLay);
    cards->addWidget(accentCard);

    /* Navigation position */
    AppCard *navCard = new AppCard(content);
    navCard->setTitle(tr("Navigation"));
    navCard->setSubtitle(tr("Move the page navigation to the side or to the bottom edge."));
    m_navigation = new AppSegmentedControl(navCard);
    m_navigation->addSegment(tr("Sidebar"), "panel-left");
    m_navigation->addSegment(tr("Bottom bar"), "panel-bottom");
    m_navigation->setMinimumHeight(36);
    if (m_shell != NULL)
        m_navigation->setCurrentIndex(m_shell->navigationPosition() == MainShell::BottomBar ? 1 : 0,
                                      false);
    connect(m_navigation, &AppSegmentedControl::currentChanged,
            this, [this](int index)
    {
        if (m_shell != NULL)
            m_shell->setNavigationPosition(index == 1 ? MainShell::BottomBar
                                                      : MainShell::Sidebar);
        emit navigationPositionChanged();
    });
    navCard->contentLayout()->addWidget(m_navigation);
    cards->addWidget(navCard);

    /* Motion */
    AppCard *motionCard = new AppCard(content);
    motionCard->setTitle(tr("Motion"));
    motionCard->setSubtitle(tr("Choose how much motion the interface uses."));
    m_motion = new AppSwitch(motionCard);
    m_motion->setChecked(Theme::instance()->animationsEnabled());
    connect(m_motion, &QAbstractButton::toggled, this, [this](bool enabled)
    {
        Theme::instance()->apply(Theme::instance()->scheme(),
                                 Theme::instance()->accent(), enabled);
    });
    AppSettingRow *motionRow = new AppSettingRow(
                tr("Animated effects"),
                tr("Hover fades, ripples, page transitions and navigation motion."),
                m_motion, motionCard);
    motionCard->contentLayout()->addWidget(motionRow);
    cards->addWidget(motionCard);

    /* Custom stylesheet */
    AppCard *cssCard = new AppCard(content);
    cssCard->setTitle(tr("Custom stylesheet"));
    cssCard->setSubtitle(tr("A qlcplusStyle.qss file can override the built-in theme."));
    QHBoxLayout *cssLay = new QHBoxLayout;
    QPushButton *openFolder = new QPushButton(tr("Open folder"), cssCard);
    openFolder->setObjectName("appearanceOpenFolder");
    openFolder->setIcon(AppIcons::icon("folder-open", 16));
    QPushButton *reload = new QPushButton(tr("Reload"), cssCard);
    reload->setObjectName("appearanceReload");
    reload->setIcon(AppIcons::icon("refresh-cw", 16));
    connect(openFolder, &QPushButton::clicked,
            this, &AppearanceDialog::openStyleSheetFolderRequested);
    connect(reload, &QPushButton::clicked,
            this, &AppearanceDialog::reloadStyleSheetRequested);
    cssLay->addWidget(openFolder);
    cssLay->addWidget(reload);
    cssLay->addStretch(1);
    cssCard->contentLayout()->addLayout(cssLay);
    cards->addWidget(cssCard);

    cards->addStretch(1);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    refreshChecks();
}

void AppearanceDialog::applyTheme(int scheme)
{
    Theme::instance()->apply(static_cast<Theme::Scheme>(scheme),
                             Theme::instance()->accent(),
                             Theme::instance()->animationsEnabled());
    refreshChecks();
}

void AppearanceDialog::applyAccent(const QColor &accent)
{
    Theme::instance()->apply(Theme::instance()->scheme(), accent,
                             Theme::instance()->animationsEnabled());
    refreshChecks();
}

void AppearanceDialog::refreshChecks()
{
    Theme *theme = Theme::instance();

    const QColor textColor = theme->color(Theme::Text);
    const QColor activeIconColor = theme->accent();

    foreach (QAbstractButton *button, m_themeGroup->buttons())
    {
        const int scheme = m_themeGroup->id(button);
        const bool checked = (static_cast<int>(theme->scheme()) == scheme);
        button->setIcon(solidThemeIcon(button->property("qlcSchemeIcon").toString(),
                                       18, checked ? activeIconColor : textColor));
        button->setChecked(checked);
    }

    if (QPushButton *custom = findChild<QPushButton *>("appearanceCustomAccent"))
        custom->setIcon(solidThemeIcon("plus", 16, textColor));
    if (QPushButton *openFolder = findChild<QPushButton *>("appearanceOpenFolder"))
        openFolder->setIcon(solidThemeIcon("folder-open", 16, textColor));
    if (QPushButton *reload = findChild<QPushButton *>("appearanceReload"))
        reload->setIcon(solidThemeIcon("refresh-cw", 16, textColor));

    foreach (QAbstractButton *button, m_accentGroup->buttons())
    {
        const QColor color(button->property("qlcAccent").toString());
        button->setChecked(color.isValid() && color.rgb() == theme->accent().rgb());
    }

    if (m_motion != NULL)
    {
        const bool blocked = m_motion->blockSignals(true);
        m_motion->setChecked(theme->animationsEnabled());
        m_motion->blockSignals(blocked);
    }
    if (m_navigation != NULL && m_shell != NULL)
    {
        const bool blocked = m_navigation->blockSignals(true);
        m_navigation->setCurrentIndex(m_shell->navigationPosition() == MainShell::BottomBar ? 1 : 0,
                                      false);
        m_navigation->blockSignals(blocked);
    }
}
