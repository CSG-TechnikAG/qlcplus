/*
  Q Light Controller Plus
  appemptystate.cpp

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
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>

#include "appemptystate.h"
#include "appicons.h"
#include "theme.h"

AppEmptyState::AppEmptyState(QWidget *parent)
    : QWidget(parent)
    , m_icon(NULL)
    , m_title(NULL)
    , m_description(NULL)
    , m_action(NULL)
    , m_iconSize(44)
{
    setObjectName("appEmptyState");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignCenter);

    m_icon = new QLabel(this);
    m_icon->setObjectName("appEmptyIcon");
    m_icon->setFixedSize(72, 72);
    m_icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_icon, 0, Qt::AlignHCenter);

    layout->addSpacing(4);

    m_title = new QLabel(this);
    m_title->setObjectName("appEmptyTitle");
    QFont titleFont = m_title->font();
    titleFont.setPixelSize(16);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);

    m_description = new QLabel(this);
    m_description->setObjectName("appEmptyDescription");
    QFont descriptionFont = m_description->font();
    descriptionFont.setPixelSize(12);
    m_description->setFont(descriptionFont);
    m_description->setAlignment(Qt::AlignCenter);
    m_description->setWordWrap(true);
    m_description->setMaximumWidth(360);
    layout->addWidget(m_description, 0, Qt::AlignHCenter);

    layout->addSpacing(8);

    m_action = new QPushButton(this);
    m_action->setObjectName("appEmptyAction");
    m_action->setProperty("uiRole", "primary");
    m_action->setMinimumSize(132, 34);
    m_action->hide();
    layout->addWidget(m_action, 0, Qt::AlignHCenter);

    connect(m_action, &QPushButton::clicked,
            this, &AppEmptyState::actionTriggered);
    connect(Theme::instance(), &Theme::changed, this, [this]()
    {
        if (m_iconName.isEmpty() == false)
            setIcon(m_iconName, m_iconSize);
    });
}

void AppEmptyState::setIcon(const QString &appIconName, int size)
{
    m_iconName = appIconName;
    m_iconSize = size;

    if (appIconName.isEmpty() == true)
    {
        m_icon->clear();
        m_icon->hide();
        return;
    }

    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    const QPixmap pixmap = AppIcons::pixmap(appIconName, size,
                                            AppIcons::purposeForName(appIconName),
                                            AppIcons::Normal, dpr);
    m_icon->setPixmap(pixmap);
    m_icon->show();
}

void AppEmptyState::setIcon(const QIcon &icon)
{
    m_iconName.clear();
    if (icon.isNull() == true)
    {
        m_icon->clear();
        m_icon->hide();
        return;
    }

    m_icon->setPixmap(icon.pixmap(m_iconSize, m_iconSize));
    m_icon->show();
}

void AppEmptyState::setTitle(const QString &title)
{
    m_title->setText(title);
    m_title->setVisible(title.isEmpty() == false);
}

void AppEmptyState::setDescription(const QString &description)
{
    m_description->setText(description);
    m_description->setVisible(description.isEmpty() == false);
}

void AppEmptyState::setActionText(const QString &text)
{
    m_action->setText(text);
    m_action->setVisible(text.isEmpty() == false);
}

void AppEmptyState::setActionEnabled(bool enabled)
{
    m_action->setEnabled(enabled);
}
