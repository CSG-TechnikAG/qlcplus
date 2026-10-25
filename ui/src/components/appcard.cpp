/*
  Q Light Controller Plus
  appcard.cpp

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

#include <QVBoxLayout>
#include <QFont>
#include <QLabel>

#include "appcard.h"

AppCard::AppCard(QWidget *parent)
    : QFrame(parent)
    , m_layout(NULL)
    , m_title(NULL)
    , m_subtitle(NULL)
{
    setObjectName("appCard");
    setFrameShape(QFrame::NoFrame);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(14, 10, 14, 10);
    m_layout->setSpacing(6);

    m_title = new QLabel(this);
    m_title->setObjectName("appCardTitle");
    QFont titleFont = font();
    titleFont.setPixelSize(14);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    m_title->hide();
    m_layout->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("appCardSubtitle");
    QFont subtitleFont = font();
    subtitleFont.setPixelSize(11);
    m_subtitle->setFont(subtitleFont);
    m_subtitle->setWordWrap(true);
    m_subtitle->hide();
    m_layout->addWidget(m_subtitle);
}

void AppCard::setTitle(const QString &title)
{
    m_title->setText(title);
    m_title->setVisible(title.isEmpty() == false);
}

void AppCard::setSubtitle(const QString &subtitle)
{
    m_subtitle->setText(subtitle);
    m_subtitle->setVisible(subtitle.isEmpty() == false);
}

QVBoxLayout *AppCard::contentLayout() const
{
    return m_layout;
}
