/*
  Q Light Controller Plus
  mainshell.h

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

#ifndef MAINSHELL_H
#define MAINSHELL_H

#include <QWidget>
#include <QPointer>

class NavigationRail;
class CommandPalette;
class ToastWidget;
class PageTransition;
class QTabWidget;
class QLabel;
class QHBoxLayout;
class QBoxLayout;
class QToolButton;
class AppSegmentedControl;
class QToolBar;

/** @addtogroup ui UI
 * @{
 */

/**
 * New QLC+ 4 application shell.
 *
 * The shell owns the visual chrome only: an app/workspace header, a command
 * bar supplied by App, an animated navigation rail, the page host and the
 * overlay widgets (toasts and command palette). The pages themselves keep
 * living in a QTabWidget whose tab bar is hidden, so all existing page
 * management code in App keeps working unchanged.
 */
class MainShell final : public QWidget
{
    Q_OBJECT

public:
    enum NavigationPosition
    {
        Sidebar = 0,
        BottomBar
    };

    explicit MainShell(QWidget *parent = NULL);
    ~MainShell();

    /** Attach the QTabWidget that stores the application pages. */
    void setPageContainer(QTabWidget *tabs);

    /** Put the existing QToolBar (all workspace actions) in the header. */
    void setCommandBar(QWidget *bar);

    /** Persistent primary actions at the right edge of the header. */
    void setPrimaryActions(QAction *modeAction, QAction *blackoutAction);

    /** Update the Design/Operate segmented control for the current mode. */
    void setModeState(bool operate);

    /** Show the BPM of the currently playing audio function, or hide it. */
    void setAudioBpm(double bpm, const QString &title = QString());

    /** Place the navigation on the left or as a bottom bar. */
    void setNavigationPosition(NavigationPosition position, bool persist = true);
    NavigationPosition navigationPosition() const;

    /** Workspace name shown in the header, without the app name. */
    void setWorkspaceTitle(const QString &title);

    /** Hide/show the top bar and navigation rail (kiosk mode). */
    void setChromeVisible(bool visible);

    /** Switch page with the snapshot transition. */
    void setCurrentIndex(int index, bool animate = true);

    int currentIndex() const;

    /** Non-blocking notification at the bottom of the window. */
    void showToast(const QString &text, bool error = false);

    /** Re-read theme colors and repaint the custom chrome. */
    void refreshTheme();

    /** Re-read the pages after tabs are added/removed. */
    void refreshPages();

    void openCommandPalette();
    void closeCommandPalette();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void detachRequested(int index);
    void navigationPositionChanged(MainShell::NavigationPosition position);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void slotCurrentChanged(int index);
private:
    void rebuildNavigation();
    void updateHeader();
    void applyModeState();
    void startPageTransition(const QPixmap &previous);

private:
    QTabWidget *m_tabs;
    NavigationRail *m_nav;
    QWidget *m_topBar;
    QHBoxLayout *m_headerLayout;
    QBoxLayout *m_bodyLayout;
    QWidget *m_commandBar;
    QLabel *m_appMark;
    QLabel *m_workspaceLabel;
    QLabel *m_pageLabel;
    QToolButton *m_searchButton;
    QLabel *m_bpmLabel;
    AppSegmentedControl *m_modeSwitch;
    QAction *m_modeAction;
    QWidget *m_pageHost;
    QPointer<QWidget> m_transition;
    ToastWidget *m_toast;
    CommandPalette *m_palette;
    bool m_operating;
    NavigationPosition m_navigationPosition;

    Q_DISABLE_COPY(MainShell)
};

/** @} */

#endif // MAINSHELL_H
