/*
  Q Light Controller Plus
  appearancedialog.h

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

#ifndef APPEARANCEDIALOG_H
#define APPEARANCEDIALOG_H

#include "appdialog.h"

class MainShell;
class AppSegmentedControl;
class AppSwitch;
class QButtonGroup;

/** @addtogroup ui UI
 * @{
 */

/**
 * Theme settings dialog built entirely from the M2 component library:
 * AppCard sections, AppSegmentedControl and AppSwitch.
 */
class AppearanceDialog final : public AppDialog
{
    Q_OBJECT

public:
    explicit AppearanceDialog(MainShell *shell, QWidget *parent = NULL);

signals:
    void openStyleSheetFolderRequested();
    void reloadStyleSheetRequested();
    void navigationPositionChanged();

private:
    void applyTheme(int scheme);
    void applyAccent(const QColor &accent);
    void refreshChecks();

private:
    MainShell *m_shell;
    QButtonGroup *m_themeGroup;
    QButtonGroup *m_accentGroup;
    AppSegmentedControl *m_navigation;
    AppSwitch *m_motion;
};

/** @} */

#endif // APPEARANCEDIALOG_H
