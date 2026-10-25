/*
  Q Light Controller Plus
  appdialog.cpp

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
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>

#include "appdialog.h"
#include "appicons.h"
#include "theme.h"

static QFrame *dialogSeparator(const QString &objectName, QWidget *parent)
{
    QFrame *line = new QFrame(parent);
    line->setObjectName(objectName);
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    return line;
}

AppDialog::AppDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
    , m_icon(NULL)
    , m_title(NULL)
    , m_subtitle(NULL)
    , m_content(NULL)
    , m_footer(NULL)
    , m_close(NULL)
{
    setObjectName("appDialog");
    setSizeGripEnabled(false);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(12);

    m_icon = new QLabel(this);
    m_icon->setObjectName("appDialogIcon");
    m_icon->setFixedSize(40, 40);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->hide();
    header->addWidget(m_icon, 0, Qt::AlignTop);

    QVBoxLayout *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->setContentsMargins(0, 0, 0, 0);

    m_title = new QLabel(title, this);
    m_title->setObjectName("appDialogTitle");
    QFont titleFont = m_title->font();
    titleFont.setPixelSize(20);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    titles->addWidget(m_title);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("appDialogSubtitle");
    QFont subtitleFont = m_subtitle->font();
    subtitleFont.setPixelSize(12);
    m_subtitle->setFont(subtitleFont);
    m_subtitle->setWordWrap(true);
    m_subtitle->hide();
    titles->addWidget(m_subtitle);

    header->addLayout(titles, 1);
    root->addLayout(header);
    root->addWidget(dialogSeparator("appDialogSeparator", this));

    m_content = new QVBoxLayout;
    m_content->setContentsMargins(0, 0, 0, 0);
    m_content->setSpacing(12);
    root->addLayout(m_content, 1);

    root->addWidget(dialogSeparator("appDialogFooterSeparator", this));

    m_footer = new QHBoxLayout;
    m_footer->setContentsMargins(0, 0, 0, 0);
    m_footer->setSpacing(8);
    m_footer->addStretch(1);
    root->addLayout(m_footer);

    m_close = new QPushButton(tr("Close"), this);
    m_close->setObjectName("appDialogClose");
    m_close->setProperty("uiRole", "primary");
    m_close->setMinimumSize(96, 32);
    m_close->setDefault(true);
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);
    m_footer->addWidget(m_close);
}

void AppDialog::setSubtitle(const QString &subtitle)
{
    m_subtitle->setText(subtitle);
    m_subtitle->setVisible(subtitle.isEmpty() == false);
}

void AppDialog::setHeaderIcon(const QIcon &icon)
{
    if (icon.isNull() == true)
    {
        m_icon->clear();
        m_icon->hide();
        return;
    }

    m_icon->setPixmap(icon.pixmap(24, 24));
    m_icon->show();
}

void AppDialog::setHeaderIcon(const QString &appIconName, int size)
{
    if (AppIcons::hasIcon(appIconName) == false)
    {
        setHeaderIcon(QIcon());
        return;
    }

    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    const QPixmap pixmap = AppIcons::pixmap(appIconName, size,
                                            AppIcons::purposeForName(appIconName),
                                            AppIcons::Normal, dpr);
    QIcon icon(pixmap);
    setHeaderIcon(icon);
}

QVBoxLayout *AppDialog::contentLayout() const
{
    return m_content;
}

QHBoxLayout *AppDialog::footerLayout() const
{
    return m_footer;
}

QPushButton *AppDialog::addFooterButton(const QString &text, bool primary, bool danger)
{
    QPushButton *button = new QPushButton(text, this);
    if (primary == true)
        button->setProperty("uiRole", "primary");
    else if (danger == true)
        button->setProperty("uiRole", "danger");
    button->setMinimumHeight(32);
    m_footer->insertWidget(m_footer->count() - 1, button);
    return button;
}

QPushButton *AppDialog::closeButton() const
{
    return m_close;
}
