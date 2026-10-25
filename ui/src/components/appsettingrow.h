/*
  Q Light Controller Plus
  appsettingrow.h

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

#ifndef APPSETTINGROW_H
#define APPSETTINGROW_H

#include <QWidget>

class QLabel;

/** @addtogroup ui UI
 * @{
 */

/** Title/description text on the left, a control (switch, selector) on the right. */
class AppSettingRow : public QWidget
{
public:
    explicit AppSettingRow(const QString &title, const QString &description,
                           QWidget *control = NULL, QWidget *parent = NULL);

    void setTitle(const QString &title);
    void setDescription(const QString &description);
    void setControl(QWidget *control);

private:
    QLabel *m_title;
    QLabel *m_description;
    QWidget *m_control;
};

/** @} */

#endif // APPSETTINGROW_H
