/*
  Q Light Controller Plus
  appsettingrow.cpp

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

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>

#include "appsettingrow.h"

AppSettingRow::AppSettingRow(const QString &title, const QString &description,
                             QWidget *control, QWidget *parent)
    : QWidget(parent)
    , m_title(NULL)
    , m_description(NULL)
    , m_control(NULL)
{
    setObjectName("appSettingRow");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QVBoxLayout *text = new QVBoxLayout;
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);

    m_title = new QLabel(title, this);
    m_title->setObjectName("appSettingTitle");
    QFont titleFont = m_title->font();
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::Medium);
    m_title->setFont(titleFont);
    text->addWidget(m_title);

    m_description = new QLabel(description, this);
    m_description->setObjectName("appSettingDescription");
    QFont descriptionFont = m_description->font();
    descriptionFont.setPixelSize(11);
    m_description->setFont(descriptionFont);
    m_description->setWordWrap(true);
    m_description->setVisible(description.isEmpty() == false);
    text->addWidget(m_description);

    layout->addLayout(text, 1);
    setControl(control);
}

void AppSettingRow::setTitle(const QString &title)
{
    m_title->setText(title);
}

void AppSettingRow::setDescription(const QString &description)
{
    m_description->setText(description);
    m_description->setVisible(description.isEmpty() == false);
}

void AppSettingRow::setControl(QWidget *control)
{
    if (m_control != NULL)
        layout()->removeWidget(m_control);

    m_control = control;
    if (m_control != NULL)
        static_cast<QHBoxLayout *>(layout())->addWidget(m_control, 0,
                                                        Qt::AlignVCenter);
}
