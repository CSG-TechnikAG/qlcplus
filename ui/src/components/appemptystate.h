/*
  Q Light Controller Plus
  appemptystate.h

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

#ifndef APPEMPTYSTATE_H
#define APPEMPTYSTATE_H

#include <QWidget>
#include <QIcon>
#include <QString>

class QLabel;
class QPushButton;

/** @addtogroup ui UI
 * @{
 */

/**
 * Centred empty-state block with a soft icon tile, title, description and an
 * optional primary action. Used by data pages when nothing is selected.
 */
class AppEmptyState : public QWidget
{
    Q_OBJECT

public:
    explicit AppEmptyState(QWidget *parent = NULL);

    void setIcon(const QString &appIconName, int size = 44);
    void setIcon(const QIcon &icon);
    void setTitle(const QString &title);
    void setDescription(const QString &description);
    void setActionText(const QString &text);
    void setActionEnabled(bool enabled);

signals:
    void actionTriggered();

private:
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_description;
    QPushButton *m_action;
    QString m_iconName;
    int m_iconSize;
};

/** @} */

#endif // APPEMPTYSTATE_H
