/*
  Q Light Controller Plus
  appcard.h

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

#ifndef APPCARD_H
#define APPCARD_H

#include <QFrame>

class QLabel;
class QVBoxLayout;

/** @addtogroup ui UI
 * @{
 */

/** Rounded surface container used by the modern settings/dialog layouts. */
class AppCard final : public QFrame
{
public:
    explicit AppCard(QWidget *parent = NULL);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);

    QVBoxLayout *contentLayout() const;

private:
    QVBoxLayout *m_layout;
    QLabel *m_title;
    QLabel *m_subtitle;
};

/** @} */

#endif // APPCARD_H
