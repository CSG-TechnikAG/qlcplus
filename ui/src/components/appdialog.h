/*
  Q Light Controller Plus
  appdialog.h

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

#ifndef APPDIALOG_H
#define APPDIALOG_H

#include <QDialog>
#include <QIcon>

class QLabel;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;

/** @addtogroup ui UI
 * @{
 */

/**
 * Shared dialog shell used by the modern QLC+ UI.
 *
 * Provides a consistent header (icon, title, subtitle), a content area and a
 * right-aligned footer with primary/secondary actions. AppearanceDialog is
 * built on top of it, and new dialogs should use it as well.
 */
class AppDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AppDialog(const QString &title, QWidget *parent = NULL);

    void setSubtitle(const QString &subtitle);
    void setHeaderIcon(const QIcon &icon);
    void setHeaderIcon(const QString &appIconName, int size = 24);

    QVBoxLayout *contentLayout() const;
    QHBoxLayout *footerLayout() const;

    /** Add a footer action before the standard Close button. */
    QPushButton *addFooterButton(const QString &text, bool primary = false,
                                 bool danger = false);
    QPushButton *closeButton() const;

private:
    QLabel *m_icon;
    QLabel *m_title;
    QLabel *m_subtitle;
    QVBoxLayout *m_content;
    QHBoxLayout *m_footer;
    QPushButton *m_close;
};

/** @} */

#endif // APPDIALOG_H
