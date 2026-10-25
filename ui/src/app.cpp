/*
  Q Light Controller Plus
  app.cpp

  Copyright (c) Heikki Junnila,
                Christopher Staite
                Massimo Callegari

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

#include <QToolButton>
#include <QTimer>
#include <QDesktopServices>
#include <QActionGroup>
#include <QColorDialog>
#include <QtWidgets>
#include <QUrl>
#include <unistd.h>
#include <QtCore>

#if defined(WIN32) || defined(Q_OS_WIN)
  #include <windows.h>
#endif

#include "functionliveeditdialog.h"
#include "inputoutputmanager.h"
#include "functionselection.h"
#include "functionmanager.h"
#include "inputoutputmap.h"
#include "virtualconsole.h"
#include "fixturemanager.h"
#include "dmxdumpfactory.h"
#include "showmanager.h"
#include "mastertimer.h"
#include "addresstool.h"
#include "simpledesk.h"
#include "stagegeometry.h"
#include "preview3d.h"
#include "aboutbox.h"
#include "monitor.h"
#include "vcframe.h"
#include "app.h"
#include "doc.h"

#include "qlcfixturedefcache.h"
#include "audioplugincache.h"
#include "rgbscriptscache.h"
#include "videoprovider.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "apputil.h"
#include "appicons.h"
#include "mainshell.h"
#include "appearancedialog.h"
#include "songrequestclient.h"
#include "songrequestdialog.h"
#include "audio.h"
#include "uieffects.h"
#include "theme.h"

#if defined(WIN32) || defined(Q_OS_WIN)
#   include "hotplugmonitor.h"
#endif

#if defined(__APPLE__) || defined(Q_OS_MAC)
extern void qt_set_sequence_auto_mnemonic(bool b);
#endif

#if defined(WIN32) || defined(Q_OS_WIN)
// Defined in Windows 11 headers but not in earlier versions.
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

typedef BOOL (WINAPI *SetProcessInformationType)(
    HANDLE hProcess,
    PROCESS_INFORMATION_CLASS ProcessInformationClass,
    LPVOID ProcessInformation,
    DWORD ProcessInformationSize
);
#endif

//#define DEBUG_SPEED

#ifdef DEBUG_SPEED
 #include <QTime>
 QTime speedTime;
#endif

#define SETTINGS_GEOMETRY      QStringLiteral("workspace/geometry")
#define SETTINGS_WORKINGPATH   QStringLiteral("workspace/workingpath")
#define SETTINGS_RECENTFILE    QStringLiteral("workspace/recent")
#define KXMLQLCWorkspaceWindow QStringLiteral("CurrentWindow")

#define MAX_RECENT_FILES    10

#define KModeTextOperate QObject::tr("Operate")
#define KModeTextDesign QObject::tr("Design")
#define KUniverseCount 4

/*****************************************************************************
 * Initialization
 *****************************************************************************/

App::App()
    : QMainWindow()
    , m_shell(NULL)
    , m_tab(NULL)
    , m_overscan(false)
    , m_noGui(false)
    , m_progressDialog(NULL)
    , m_doc(NULL)

    , m_fileNewAction(NULL)
    , m_fileOpenAction(NULL)
    , m_fileSaveAction(NULL)
    , m_fileSaveAsAction(NULL)

    , m_modeToggleAction(NULL)
    , m_controlMonitorAction(NULL)
    , m_addressToolAction(NULL)
    , m_controlFullScreenAction(NULL)
    , m_controlBlackoutAction(NULL)
    , m_controlPanicAction(NULL)
    , m_dumpDmxAction(NULL)
    , m_liveEditAction(NULL)
    , m_liveEditVirtualConsoleAction(NULL)
    , m_songRequestAction(NULL)
    , m_audioBpmTimer(NULL)

    , m_helpIndexAction(NULL)
    , m_helpAboutAction(NULL)
    , m_quitAction(NULL)
    , m_fileOpenMenu(NULL)
    , m_fadeAndStopMenu(NULL)

    , m_appearanceMenu(NULL)
    , m_appearanceAction(NULL)
    , m_themeGroup(NULL)
    , m_accentGroup(NULL)
    , m_navGroup(NULL)
    , m_customAccentAction(NULL)
    , m_animationsAction(NULL)

    , m_toolbar(NULL)

    , m_dumpProperties(NULL)
    , m_videoProvider(NULL)
{
    QCoreApplication::setOrganizationName("qlcplus");
    QCoreApplication::setOrganizationDomain("qlcplus.org");
    QCoreApplication::setApplicationName(APPNAME);

    /* Apply the persisted appearance before any widget is created. This
     * also makes the palette available to the custom painted widgets. */
    Theme::instance()->applySaved();

    /* Lightweight hover/ripple animations, disabled by the Classic theme
     * or by the user preference. */
    UiEffects::instance();

    connect(Theme::instance(), SIGNAL(changed()), this, SLOT(slotThemeChanged()));
}

App::~App()
{
    QSettings settings;

    // Don't save kiosk-mode window geometry because that will screw things up
    if (m_doc->isKiosk() == false && QLCFile::hasWindowManager())
        settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
    else
        settings.setValue(SETTINGS_GEOMETRY, QVariant());

    if (Monitor::instance() != NULL)
        delete Monitor::instance();

    if (FixtureManager::instance() != NULL)
        delete FixtureManager::instance();

    if (FunctionManager::instance() != NULL)
        delete FunctionManager::instance();

    if (ShowManager::instance() != NULL)
        delete ShowManager::instance();

    if (InputOutputManager::instance() != NULL)
        delete InputOutputManager::instance();

    if (VirtualConsole::instance() != NULL)
        delete VirtualConsole::instance();

    if (SimpleDesk::instance() != NULL)
        delete SimpleDesk::instance();

    if (StageGeometry::instance() != NULL)
        delete StageGeometry::instance();

    if (m_dumpProperties != NULL)
        delete m_dumpProperties;

    if (m_videoProvider != NULL)
        delete m_videoProvider;

    if (m_doc != NULL)
        delete m_doc;

    m_doc = NULL;
}

void App::startup()
{
#if defined(__APPLE__) || defined(Q_OS_MAC)
    createProgressDialog();
#endif

    init();
    slotModeDesign();
    slotDocModified(false);

#if defined(__APPLE__) || defined(Q_OS_MAC)
    destroyProgressDialog();
#endif

    // Activate FixtureManager
    setActiveWindow(FixtureManager::staticMetaObject.className());
}

void App::enableOverscan()
{
    m_overscan = true;
}

void App::disableGUI()
{
    m_noGui = true;
}

void App::init()
{
    QSettings settings;

    setWindowIcon(QIcon(":/qlcplus.png"));

    m_shell = new MainShell(this);
    setCentralWidget(m_shell);

    m_tab = new QTabWidget(m_shell);
    m_tab->setTabPosition(QTabWidget::South);
    m_shell->setPageContainer(m_tab);

#if defined(__APPLE__) || defined(Q_OS_MAC)
    m_tab->setElideMode(Qt::TextElideMode::ElideNone);
    qt_set_sequence_auto_mnemonic(true);
#endif

    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid() == true)
    {
        this->restoreGeometry(var.toByteArray());
    }
    else
    {
        /* Application geometry and window state */
        QSize size = settings.value("/workspace/size").toSize();
        if (size.isValid() == true)
        {
            resize(size);
        }
        else
        {
            if (QLCFile::hasWindowManager() == false)
            {
                QScreen *screen = QGuiApplication::screens().first();
                QRect geometry = screen->geometry();
                if (m_noGui == true)
                {
                    setGeometry(geometry.width(), geometry.height(), 1, 1);
                }
                else
                {
                    int w = geometry.width();
                    int h = geometry.height();
                    if (m_overscan == true)
                    {
                        // if overscan is requested, introduce a 5% margin
                        w = (float)geometry.width() * 0.95;
                        h = (float)geometry.height() * 0.95;
                    }
                    setGeometry((geometry.width() - w) / 2, (geometry.height() - h) / 2, w, h);
                }
            }
            else
                resize(1280, 800);
        }

        QVariant state = settings.value("/workspace/state", Qt::WindowNoState);
        if (state.isValid() == true)
            setWindowState(Qt::WindowState(state.toInt()));
    }

    QVariant dir = settings.value(SETTINGS_WORKINGPATH);
    if (dir.isValid() == true)
        m_workingDirectory = QDir(dir.toString());

    // The engine object
    initDoc();
    // Main view actions
    initActions();
    // Main tool bar
    initToolBar();

    m_dumpProperties = new DmxDumpFactoryProperties(KUniverseCount);

    // Create primary views.
    m_tab->setIconSize(QSize(20, 20));

    QWidget* w = new FixtureManager(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "fixtures");
    m_tab->addTab(w, AppIcons::icon("fixtures", 20), tr("Fixtures"));

    w = new FunctionManager(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "functions");
    m_tab->addTab(w, AppIcons::icon("functions", 20), tr("Functions"));

    w = new ShowManager(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "shows");
    m_tab->addTab(w, AppIcons::icon("shows", 20), tr("Shows"));

    w = new VirtualConsole(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "virtual-console");
    m_tab->addTab(w, AppIcons::icon("virtual-console", 20), tr("Virtual Console"));

    w = new SimpleDesk(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "simple-desk");
    m_tab->addTab(w, AppIcons::icon("simple-desk", 20), tr("Simple Desk"));

    w = new StageGeometry(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "stage-geometry");
    m_tab->addTab(w, AppIcons::icon("stage-geometry", 20), tr("Stage Geometry"));

    w = new Preview3D(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "preview-3d");
    m_tab->addTab(w, AppIcons::icon("preview-3d", 20), tr("3D Preview"));

    w = new InputOutputManager(m_tab, m_doc);
    w->setProperty("qlcNavIcon", "inputs-outputs");
    m_tab->addTab(w, AppIcons::icon("inputs-outputs", 20), tr("Inputs/Outputs"));

    connect(m_shell, SIGNAL(detachRequested(int)), this, SLOT(slotDetachContext(int)));
    m_shell->refreshPages();

    // Listen to blackout changes and toggle m_controlBlackoutAction
    connect(m_doc->inputOutputMap(), SIGNAL(blackoutChanged(bool)), this, SLOT(slotBlackoutChanged(bool)));

    // Listen to DMX value changes and update each Fixture values array
    connect(m_doc->inputOutputMap(), SIGNAL(universeWritten(quint32, const QByteArray&)),
            this, SLOT(slotUniverseWritten(quint32, const QByteArray&)));

    // Enable/Disable panic button
    connect(m_doc->masterTimer(), SIGNAL(functionListChanged()), this, SLOT(slotRunningFunctionsChanged()));
    slotRunningFunctionsChanged();

    // Start up in non-modified state
    m_doc->resetModified();

#if defined(WIN32) || defined(Q_OS_WIN)
    HotPlugMonitor::setWinId(winId());

    // When on Windows 11, disable system timer resolution throttling when
    // app is minimised, occluded, etc.
    disableTimerResolutionThrottling();
#endif

    this->setStyleSheet(AppUtil::getStyleSheet("MAIN"));

    m_videoProvider = new VideoProvider(m_doc, this);
}

void App::setActiveWindow(const QString& name)
{
    if (name.isEmpty() == true)
        return;

    for (int i = 0; i < m_tab->count(); i++)
    {
        QWidget* widget = m_tab->widget(i);
        if (widget != NULL && widget->metaObject()->className() == name)
        {
            if (m_shell != NULL)
                m_shell->setCurrentIndex(i);
            else
                m_tab->setCurrentIndex(i);
            break;
        }
    }
}

#if defined(WIN32) || defined(Q_OS_WIN)
bool App::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    //qDebug() << Q_FUNC_INFO << eventType;
    return HotPlugMonitor::parseWinEvent(message, result);
}

void App::disableTimerResolutionThrottling()
{
    // On Windows 11, we want it to always honour system timer resolution requests,
    // because otherwise by default when an application is minimised, or otherwise
    // non-visible or non-audible to the end-user, Windows may ignore timer
    // resolution requests and not give a higher resolution than the default system
    // timer resolution (typically 15.625 ms).

    // Note: we must resolve the SetProcessInformation API function at run-time
    // because it does not exist prior to Windows 8. On supported Windows versions
    // earlier than 11, the call to SetProcessInformation will just fail, which we
    // can ignore.

    HMODULE hKernel32 = LoadLibrary(L"kernel32.dll");
    Q_ASSERT(hKernel32 != NULL); // Shouldn't ever fail because kernel32 already loaded into every process

    // Extra void* cast to avoid -Wcast-function-type warning.
    SetProcessInformationType pfnSetProcessInformation = (SetProcessInformationType)(void *)GetProcAddress(hKernel32, "SetProcessInformation");

    if (pfnSetProcessInformation != NULL)
    {
        PROCESS_POWER_THROTTLING_STATE pwrState = {
                .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                .ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
                .StateMask = 0 // Disables timer resolution throttling
        };

        if (!pfnSetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pwrState, sizeof(pwrState)))
        {
            qWarning() << Q_FUNC_INFO << "SetProcessInformation() failed with error" << GetLastError() << "(ignore if Windows version < 11)";
        }
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "SetProcessInformation() API does not exist on this version of Windows";
    }
}
#endif

void App::closeEvent(QCloseEvent* e)
{
    if (m_doc->mode() == Doc::Operate && m_doc->isKiosk() == false)
    {
        QMessageBox box(QMessageBox::Warning,
                        tr("Cannot exit in Operate mode"),
                        tr("The application is in Operate mode. "
                           "Switch to Design mode to close it."),
                        QMessageBox::NoButton, this);
        QPushButton *switchButton = box.addButton(tr("Switch to Design Mode"),
                                                  QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(switchButton);
        box.exec();

        if (box.clickedButton() != switchButton)
        {
            e->ignore();
            return;
        }

        slotModeDesign();
        if (m_doc->mode() != Doc::Design)
        {
            e->ignore();
            return;
        }
        /* Design mode reached: fall through to the normal save/close flow. */
    }

    if (m_doc->isKiosk() == false)
    {
        if (saveModifiedDoc(tr("Close"), tr("Do you wish to save the current workspace " \
                                            "before closing the application?")) == true)
        {
            e->accept();
        }
        else
        {
            e->ignore();
        }
    }
    else
    {
        if (m_doc->isKiosk() == true)
        {
            int result = QMessageBox::warning(this, tr("Close the application?"),
                                              tr("Do you wish to close the application?"),
                                              QMessageBox::Yes, QMessageBox::No);
            if (result == QMessageBox::No)
            {
                e->ignore();
                return;
            }
        }

        e->accept();
    }
}

/*****************************************************************************
 * Progress dialog
 *****************************************************************************/

void App::createProgressDialog()
{
    m_progressDialog = new QProgressDialog;
    m_progressDialog->setCancelButton(NULL);
    m_progressDialog->show();
    m_progressDialog->raise();
    m_progressDialog->setRange(0, 10);
    slotSetProgressText(QString());
    QApplication::processEvents();
}

void App::destroyProgressDialog()
{
    delete m_progressDialog;
    m_progressDialog = NULL;
}

void App::slotSetProgressText(const QString& text)
{
    if (m_progressDialog == NULL)
        return;

    static int progress = 0;
    m_progressDialog->setValue(progress++);
    m_progressDialog->setLabelText(QString("<B>%1</B><BR/>%2")
                                   .arg(tr("Starting Q Light Controller ++"))
                                   .arg(text));
    QApplication::processEvents();
}

/*****************************************************************************
 * Doc
 *****************************************************************************/

void App::clearDocument()
{
    m_doc->masterTimer()->stop();
    VirtualConsole::instance()->resetContents();
    ShowManager::instance()->clearContents();
    m_doc->clearContents();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
    SimpleDesk::instance()->clearContents();
    if (StageGeometry::instance() != NULL)
        StageGeometry::instance()->clearContents();
    if (Preview3D::instance() != NULL)
        Preview3D::instance()->clearContents();
    m_doc->inputOutputMap()->resetUniverses();
    setFileName(QString());
    m_doc->resetModified();
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
}

Doc *App::doc()
{
    return m_doc;
}

void App::initDoc()
{
    Q_ASSERT(m_doc == NULL);
    m_doc = new Doc(this);

    connect(m_doc, SIGNAL(modified(bool)), this, SLOT(slotDocModified(bool)));
    connect(m_doc, SIGNAL(needAutosave()), this, SLOT(slotDocAutosave()));
    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged(Doc::Mode)));
#ifdef DEBUG_SPEED
    speedTime.start();
#endif
    /* Load user fixtures first so that they override system fixtures */
    m_doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    m_doc->fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());

    /* Load channel modifiers templates */
    m_doc->modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
    m_doc->modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());

    /* Load RGB scripts */
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::systemScriptsDirectory());
    m_doc->rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());

    /* Load plugins */
    connect(m_doc->ioPluginCache(), SIGNAL(pluginLoaded(const QString&)),
            this, SLOT(slotSetProgressText(const QString&)));
    m_doc->ioPluginCache()->load(IOPluginCache::systemPluginDirectory());

    /* Load audio decoder plugins
     * This doesn't use a AudioPluginCache::systemPluginDirectory() cause
     * otherwise the qlcconfig.h creation should have been moved into the
     * audio folder, which doesn't make much sense */
    m_doc->audioPluginCache()->load(QLCFile::systemDirectory(AUDIOPLUGINDIR, KExtPlugin));

    /* Restore outputmap settings */
    Q_ASSERT(m_doc->inputOutputMap() != NULL);

    /* Load input plugins & profiles */
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::userProfileDirectory());
    m_doc->inputOutputMap()->loadProfiles(InputOutputMap::systemProfileDirectory());
    m_doc->inputOutputMap()->loadDefaults();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Doc initialization took" << speedTime.elapsed() << "ms";
#endif

    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
}

void App::slotDocModified(bool state)
{
    QString caption(APPNAME);

    if (fileName().isEmpty() == false)
        caption += QString(" - ") + QDir::toNativeSeparators(fileName());
    else
        caption += tr(" - New Workspace");

    if (state == true)
        caption += QString(" *");

    setWindowTitle(caption);

    if (m_shell != NULL)
    {
        QString shellTitle = fileName().isEmpty()
                           ? tr("New Workspace")
                           : QFileInfo(fileName()).fileName();
        if (state == true)
            shellTitle += QString(" *");
        m_shell->setWorkspaceTitle(shellTitle);
    }
}

void App::slotDocAutosave()
{
    saveXML(autoSaveFileName(), true);
}

void App::slotUniverseWritten(quint32 idx, const QByteArray &ua)
{
    foreach (Fixture *fixture, m_doc->fixtures())
    {
        if (fixture->universe() != idx)
            continue;

        fixture->setChannelValues(ua);
    }
}

/*****************************************************************************
 * Main application Mode
 *****************************************************************************/

void App::enableKioskMode()
{
    // Turn on operate mode
    m_doc->setKiosk(true);
    m_doc->setMode(Doc::Operate);

    // No need for these
    m_tab->removeTab(m_tab->indexOf(FixtureManager::instance()));
    m_tab->removeTab(m_tab->indexOf(FunctionManager::instance()));
    m_tab->removeTab(m_tab->indexOf(ShowManager::instance()));
    m_tab->removeTab(m_tab->indexOf(SimpleDesk::instance()));
    if (StageGeometry::instance() != NULL)
        m_tab->removeTab(m_tab->indexOf(StageGeometry::instance()));
    m_tab->removeTab(m_tab->indexOf(InputOutputManager::instance()));

    // The shell chrome takes space; kiosk shows only the console page
    if (m_shell != NULL)
    {
        m_shell->refreshPages();
        m_shell->setChromeVisible(false);
    }
}

void App::createKioskCloseButton(const QRect& rect)
{
    QPushButton* btn = new QPushButton(VirtualConsole::instance()->contents());
    btn->setIcon(QIcon(":/exit.png"));
    btn->setToolTip(tr("Exit"));
    btn->setGeometry(rect);
    connect(btn, SIGNAL(clicked()), this, SLOT(close()));
    btn->show();
}

void App::slotModeOperate()
{
    m_doc->setMode(Doc::Operate);
}

void App::slotModeDesign()
{
    if (m_doc->masterTimer()->runningFunctions() > 0)
    {
        int result = QMessageBox::warning(
                         this,
                         tr("Switch to Design Mode"),
                         tr("There are still running functions.\n"
                            "Really stop them and switch back to "
                            "Design mode?"),
                         QMessageBox::Yes,
                         QMessageBox::No);

        if (result == QMessageBox::No)
            return;
        else
            m_doc->masterTimer()->stopAllFunctions();
    }

    m_liveEditVirtualConsoleAction->setChecked(false);
    m_doc->setMode(Doc::Design);
}

void App::slotModeToggle()
{
    if (m_doc->mode() == Doc::Design)
        slotModeOperate();
    else
        slotModeDesign();
}

void App::slotModeChanged(Doc::Mode mode)
{
    if (mode == Doc::Operate)
    {
        /* Disable editing features */
        m_fileNewAction->setEnabled(false);
        m_fileOpenAction->setEnabled(false);
        m_liveEditAction->setEnabled(true);
        m_liveEditVirtualConsoleAction->setEnabled(true);

        updateModeIcon();
    }
    else if (mode == Doc::Design)
    {
        /* Enable editing features */
        m_fileNewAction->setEnabled(true);
        m_fileOpenAction->setEnabled(true);
        m_liveEditAction->setEnabled(false);
        m_liveEditVirtualConsoleAction->setEnabled(false);

        updateModeIcon();
    }
}

/*****************************************************************************
 * Actions and toolbar
 *****************************************************************************/

void App::initActions()
{
    /* File actions */
    m_fileNewAction = new QAction(tr("&New"), this);
    setActionIcon(m_fileNewAction, "file-plus");
    m_fileNewAction->setShortcut(QKeySequence("CTRL+N"));
    connect(m_fileNewAction, SIGNAL(triggered(bool)), this, SLOT(slotFileNew()));

    m_fileOpenAction = new QAction(tr("&Open"), this);
    setActionIcon(m_fileOpenAction, "folder-open");
    m_fileOpenAction->setShortcut(QKeySequence("CTRL+O"));
    connect(m_fileOpenAction, SIGNAL(triggered(bool)), this, SLOT(slotFileOpen()));

    m_fileSaveAction = new QAction(tr("&Save"), this);
    setActionIcon(m_fileSaveAction, "save");
    m_fileSaveAction->setShortcut(QKeySequence("CTRL+S"));
    connect(m_fileSaveAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSave()));

    m_fileSaveAsAction = new QAction(tr("Save &As..."), this);
    setActionIcon(m_fileSaveAsAction, "save-all");
    connect(m_fileSaveAsAction, SIGNAL(triggered(bool)), this, SLOT(slotFileSaveAs()));

    /* Control actions */
    m_modeToggleAction = new QAction(tr("&Operate"), this);
    m_modeToggleAction->setToolTip(tr("Switch to operate mode"));
    m_modeToggleAction->setShortcut(QKeySequence("CTRL+F12"));
    connect(m_modeToggleAction, SIGNAL(triggered(bool)), this, SLOT(slotModeToggle()));

    m_controlMonitorAction = new QAction(tr("&Monitor"), this);
    setActionIcon(m_controlMonitorAction, "monitor");
    m_controlMonitorAction->setShortcut(QKeySequence("CTRL+M"));
    connect(m_controlMonitorAction, SIGNAL(triggered(bool)), this, SLOT(slotControlMonitor()));

    m_addressToolAction = new QAction(tr("Address Tool"), this);
    setActionIcon(m_addressToolAction, "address-tool");
    connect(m_addressToolAction, SIGNAL(triggered()), this, SLOT(slotAddressTool()));

    m_controlBlackoutAction = new QAction(tr("Toggle &Blackout"), this);
    setActionIcon(m_controlBlackoutAction, "blackout");
    m_controlBlackoutAction->setCheckable(true);
    connect(m_controlBlackoutAction, SIGNAL(triggered(bool)), this, SLOT(slotControlBlackout()));
    m_controlBlackoutAction->setChecked(m_doc->inputOutputMap()->blackout());

    m_liveEditAction = new QAction(tr("Live edit a function"), this);
    setActionIcon(m_liveEditAction, "pencil-line");
    connect(m_liveEditAction, SIGNAL(triggered()), this, SLOT(slotFunctionLiveEdit()));
    m_liveEditAction->setEnabled(false);

    m_liveEditVirtualConsoleAction = new QAction(tr("Toggle Virtual Console Live edit"), this);
    setActionIcon(m_liveEditVirtualConsoleAction, "virtual-console");
    connect(m_liveEditVirtualConsoleAction, SIGNAL(triggered()), this, SLOT(slotLiveEditVirtualConsole()));
    m_liveEditVirtualConsoleAction->setCheckable(true);
    m_liveEditVirtualConsoleAction->setEnabled(false);

    m_dumpDmxAction = new QAction(tr("Dump DMX values to a function"), this);
    setActionIcon(m_dumpDmxAction, "dump-dmx");
    m_dumpDmxAction->setShortcut(QKeySequence("CTRL+D"));
    connect(m_dumpDmxAction, SIGNAL(triggered()), this, SLOT(slotDumpDmxIntoFunction()));

    m_controlPanicAction = new QAction(tr("Stop ALL functions!"), this);
    setActionIcon(m_controlPanicAction, "octagon-x");
    m_controlPanicAction->setShortcut(QKeySequence("CTRL+SHIFT+ESC"));
    connect(m_controlPanicAction, SIGNAL(triggered(bool)), this, SLOT(slotControlPanic()));

    m_fadeAndStopMenu = new QMenu();
    QAction *fade1 = new QAction(tr("Fade 1 second and stop"), this);
    fade1->setData(QVariant(1000));
    connect(fade1, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade1);

    QAction *fade5 = new QAction(tr("Fade 5 seconds and stop"), this);
    fade5->setData(QVariant(5000));
    connect(fade5, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade5);

    QAction *fade10 = new QAction(tr("Fade 10 second and stop"), this);
    fade10->setData(QVariant(10000));
    connect(fade10, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade10);

    QAction *fade30 = new QAction(tr("Fade 30 second and stop"), this);
    fade30->setData(QVariant(30000));
    connect(fade30, SIGNAL(triggered()), this, SLOT(slotFadeAndStopAll()));
    m_fadeAndStopMenu->addAction(fade30);

    m_controlPanicAction->setMenu(m_fadeAndStopMenu);

    m_controlFullScreenAction = new QAction(tr("Toggle Full Screen"), this);
    setActionIcon(m_controlFullScreenAction, "maximize");
    m_controlFullScreenAction->setCheckable(true);
    m_controlFullScreenAction->setShortcut(QKeySequence("CTRL+F11"));
    connect(m_controlFullScreenAction, SIGNAL(triggered(bool)), this, SLOT(slotControlFullScreen()));

    /* Help actions */
    m_helpIndexAction = new QAction(tr("&Index"), this);
    setActionIcon(m_helpIndexAction, "circle-help");
    m_helpIndexAction->setShortcut(QKeySequence("SHIFT+F1"));
    connect(m_helpIndexAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpIndex()));

    m_helpAboutAction = new QAction(tr("&About QLC+"), this);
    setActionIcon(m_helpAboutAction, "info");
    connect(m_helpAboutAction, SIGNAL(triggered(bool)), this, SLOT(slotHelpAbout()));

    initAppearanceMenu();

    if (QLCFile::hasWindowManager() == false)
    {
        m_quitAction = new QAction(tr("Quit QLC+"), this);
        setActionIcon(m_quitAction, "log-out");
        m_quitAction->setShortcut(QKeySequence("CTRL+ALT+Backspace"));
        connect(m_quitAction, SIGNAL(triggered(bool)), this, SLOT(close()));
    }
}

static QIcon accentSwatch(const QColor &color)
{
    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;
    const int side = qRound(16.0 * dpr);

    QPixmap swatch(side, side);
    swatch.setDevicePixelRatio(dpr);
    swatch.fill(Qt::transparent);

    QPainter painter(&swatch);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Theme::contrastingText(color), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(1, 1, 14, 14, 4, 4);
    painter.end();

    return QIcon(swatch);
}

void App::initAppearanceMenu()
{
    m_appearanceMenu = new QMenu(tr("Appearance"), this);

    QAction *settingsAction = m_appearanceMenu->addAction(
                AppIcons::icon("appearance", 16), tr("Appearance settings..."));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(slotAppearanceSettings()));

    m_appearanceMenu->addSeparator();

    /* Theme schemes */
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);

    const Theme::Scheme schemes[] = { Theme::Dark, Theme::Light,
                                      Theme::OLED, Theme::Classic };
    for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); i++)
    {
        QAction *action = new QAction(Theme::schemeDisplayName(schemes[i]), this);
        action->setCheckable(true);
        action->setData(static_cast<int>(schemes[i]));
        m_themeGroup->addAction(action);
        m_appearanceMenu->addAction(action);
    }

    connect(m_themeGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(slotThemeActionTriggered(QAction*)));

    /* Navigation position */
    QMenu *navMenu = m_appearanceMenu->addMenu(tr("Navigation"));
    m_navGroup = new QActionGroup(this);
    m_navGroup->setExclusive(true);

    QAction *sidebarAction = new QAction(tr("Sidebar"), this);
    sidebarAction->setCheckable(true);
    sidebarAction->setData(QStringLiteral("sidebar"));
    m_navGroup->addAction(sidebarAction);
    navMenu->addAction(sidebarAction);

    QAction *bottomAction = new QAction(tr("Bottom bar"), this);
    bottomAction->setCheckable(true);
    bottomAction->setData(QStringLiteral("bottom"));
    m_navGroup->addAction(bottomAction);
    navMenu->addAction(bottomAction);

    connect(m_navGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(slotNavigationPositionTriggered(QAction*)));

    m_appearanceMenu->addSeparator();

    /* Accent colors */
    QMenu *accentMenu = m_appearanceMenu->addMenu(tr("Accent color"));
    m_accentGroup = new QActionGroup(this);
    m_accentGroup->setExclusive(true);

    QList<QColor> presets = Theme::accentPresets();
    QStringList presetNames;
    presetNames << tr("QLC+ Blue") << tr("Cyan") << tr("Green")
                << tr("Amber") << tr("Red") << tr("Magenta") << tr("Purple");

    for (int i = 0; i < presets.count(); i++)
    {
        const QColor color = presets.at(i);
        QAction *action = new QAction(i < presetNames.count() ? presetNames.at(i)
                                                              : color.name(), this);
        action->setCheckable(true);
        action->setData(color.name());
        m_accentGroup->addAction(action);
        accentMenu->addAction(action);
    }

    accentMenu->addSeparator();
    m_customAccentAction = accentMenu->addAction(tr("Custom..."));
    connect(m_customAccentAction, SIGNAL(triggered()), this, SLOT(slotCustomAccent()));

    connect(m_accentGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(slotAccentActionTriggered(QAction*)));

    m_appearanceMenu->addSeparator();

    m_animationsAction = new QAction(tr("Animated effects"), this);
    m_animationsAction->setCheckable(true);
    m_animationsAction->setToolTip(tr("Hover fades and click ripples"));
    connect(m_animationsAction, SIGNAL(toggled(bool)),
            this, SLOT(slotAnimationsToggled(bool)));
    m_appearanceMenu->addAction(m_animationsAction);

    m_appearanceMenu->addSeparator();

    QAction *openFolder = m_appearanceMenu->addAction(AppIcons::icon("folder-open", 16),
                                                      tr("Open custom stylesheet folder"));
    connect(openFolder, SIGNAL(triggered()), this, SLOT(slotOpenStyleSheetFolder()));

    QAction *reload = m_appearanceMenu->addAction(AppIcons::icon("refresh-cw", 16),
                                                  tr("Reload custom stylesheet"));
    connect(reload, SIGNAL(triggered()), this, SLOT(slotReloadStyleSheet()));

    m_appearanceAction = new QAction(tr("Appearance"), this);
    setActionIcon(m_appearanceAction, "appearance");
    m_appearanceAction->setToolTip(tr("Theme, accent color and animations"));
    m_appearanceAction->setMenu(m_appearanceMenu);

    updateAppearanceChecks();
}

void App::updateAppearanceChecks()
{
    Theme *theme = Theme::instance();

    if (m_themeGroup != NULL)
    {
        foreach (QAction *action, m_themeGroup->actions())
        {
            if (action->data().toInt() == static_cast<int>(theme->scheme()))
                action->setChecked(true);
        }
    }

    if (m_accentGroup != NULL)
    {
        bool presetFound = false;

        foreach (QAction *action, m_accentGroup->actions())
        {
            const QColor preset(action->data().toString());
            const bool matches = preset.isValid()
                                 && preset.rgb() == theme->accent().rgb();
            action->setChecked(matches);
            if (matches == true)
                presetFound = true;
        }

        if (m_customAccentAction != NULL)
        {
            if (presetFound == true)
            {
                m_customAccentAction->setIcon(QIcon());
                m_customAccentAction->setText(tr("Custom..."));
            }
            else
            {
                m_customAccentAction->setIcon(accentSwatch(theme->accent()));
                m_customAccentAction->setText(tr("Custom (%1)").arg(theme->accent().name()));
            }
        }
    }

    if (m_animationsAction != NULL)
    {
        m_animationsAction->blockSignals(true);
        m_animationsAction->setChecked(theme->animationsEnabled());
        m_animationsAction->blockSignals(false);
    }

    if (m_navGroup != NULL && m_shell != NULL)
    {
        const QString current = (m_shell->navigationPosition() == MainShell::BottomBar)
                              ? QStringLiteral("bottom") : QStringLiteral("sidebar");
        foreach (QAction *action, m_navGroup->actions())
        {
            action->setChecked(action->data().toString() == current);
        }
    }
}

void App::slotThemeActionTriggered(QAction *action)
{
    if (action == NULL)
        return;

    Theme::instance()->apply(static_cast<Theme::Scheme>(action->data().toInt()),
                             Theme::instance()->accent(),
                             Theme::instance()->animationsEnabled());
}

void App::slotAccentActionTriggered(QAction *action)
{
    if (action == NULL)
        return;

    const QColor accent(action->data().toString());
    if (accent.isValid() == false)
        return;

    Theme::instance()->apply(Theme::instance()->scheme(), accent,
                             Theme::instance()->animationsEnabled());
}

void App::slotCustomAccent()
{
    QColor accent = QColorDialog::getColor(Theme::instance()->accent(), this,
                                           tr("Select accent color"));
    if (accent.isValid() == false || accent == Theme::instance()->accent())
        return;

    Theme::instance()->apply(Theme::instance()->scheme(), accent,
                             Theme::instance()->animationsEnabled());
}

void App::slotAnimationsToggled(bool enabled)
{
    Theme::instance()->apply(Theme::instance()->scheme(),
                             Theme::instance()->accent(), enabled);
}

void App::slotOpenStyleSheetFolder()
{
    const QString dir = AppUtil::userStylesheetDirectory();
    if (dir.isEmpty() == true)
        return;

    if (QDir().mkpath(dir) == false)
    {
        QMessageBox::warning(this, tr("Appearance"),
                             tr("Unable to create the folder: %1").arg(dir));
        return;
    }

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(dir)) == false)
    {
        QMessageBox::information(this, tr("Appearance"),
                                 tr("The custom stylesheet folder is:\n%1").arg(dir));
    }
}

void App::slotReloadStyleSheet()
{
    AppUtil::reloadStyleSheets();
    setStyleSheet(AppUtil::getStyleSheet("MAIN"));

    if (m_shell != NULL)
        m_shell->showToast(tr("Custom stylesheet reloaded"));
}

void App::slotAppearanceSettings()
{
    AppearanceDialog dialog(m_shell, this);
    connect(&dialog, SIGNAL(openStyleSheetFolderRequested()),
            this, SLOT(slotOpenStyleSheetFolder()));
    connect(&dialog, SIGNAL(reloadStyleSheetRequested()),
            this, SLOT(slotReloadStyleSheet()));
    dialog.exec();
    updateAppearanceChecks();
}

void App::slotNavigationPositionTriggered(QAction *action)
{
    if (action == NULL || m_shell == NULL)
        return;

    m_shell->setNavigationPosition(action->data().toString() == "bottom"
                                   ? MainShell::BottomBar : MainShell::Sidebar);
    updateAppearanceChecks();
}

void App::slotThemeChanged()
{
    /* The built-in theme is applied at QApplication level, but the user
     * supplied qlcplusStyle.qss is still applied to the main window so it
     * can override individual components. Re-apply it after every change. */
    AppUtil::reloadStyleSheets();
    setStyleSheet(AppUtil::getStyleSheet("MAIN"));
    updateActionIcons();
    updateAppearanceChecks();
    if (m_shell != NULL)
        m_shell->refreshTheme();
}

void App::setActionIcon(QAction *action, const QString &iconName, int size)
{
    if (action == NULL)
        return;

    action->setIcon(AppIcons::icon(iconName, size));
    m_actionIcons.insert(action, iconName);
}

void App::updateSongRequestStatus()
{
    if (m_songRequestAction == NULL)
        return;

    SongRequestClient *client = SongRequestClient::instance();
    m_songRequestAction->setVisible(client->isConfigured());
    if (client->isConfigured() == false)
        return;

    Theme *theme = Theme::instance();
    QColor color = theme->color(Theme::TextDim);
    switch (client->status())
    {
    case SongRequestClient::Connected:
        color = theme->color(Theme::Success);
        break;
    case SongRequestClient::Connecting:
        color = theme->color(Theme::Warning);
        break;
    case SongRequestClient::Disconnected:
        color = theme->color(Theme::Danger);
        break;
    case SongRequestClient::Disabled:
    default:
        break;
    }

    m_songRequestAction->setIcon(QIcon(AppIcons::pixmap("songrequest", 24, color,
                                                        qApp->devicePixelRatio())));
    QString tooltip = client->statusText();
    if (client->lastError().isEmpty() == false)
        tooltip += QString(" — %1").arg(client->lastError());
    m_songRequestAction->setToolTip(tooltip);
}

void App::openSongRequestDialog()
{
    SongRequestDialog dialog(m_doc, this);
    dialog.exec();
}

void App::updateAudioBpm()
{
    if (m_shell == NULL || m_doc == NULL)
        return;

    double bpm = 0.0;
    QString title;
    for (Function *function : m_doc->functions())
    {
        if (function->type() != Function::AudioType || function->isRunning() == false)
            continue;
        Audio *audio = qobject_cast<Audio *>(function);
        if (audio != NULL && audio->bpm() > 0)
        {
            bpm = audio->bpm();
            title = audio->name();
            break;
        }
    }
    m_shell->setAudioBpm(bpm, title);
}

void App::updateModeIcon()
{
    if (m_modeToggleAction == NULL || m_doc == NULL)
        return;

    const bool operating = (m_doc->mode() == Doc::Operate);
    const QString iconName = operating ? QStringLiteral("play")
                                       : QStringLiteral("pencil-ruler");

    m_modeToggleAction->setText(operating ? tr("Operate") : tr("Design"));
    m_modeToggleAction->setToolTip(operating ? tr("Switch to design mode")
                                             : tr("Switch to operate mode"));

    Theme *theme = Theme::instance();
    const QColor fill = operating ? theme->color(Theme::Success) : theme->accent();
    const QColor color = (theme->scheme() == Theme::Classic)
                       ? theme->color(Theme::Text)
                       : Theme::contrastingText(fill);
    const qreal dpr = qApp != NULL ? qApp->devicePixelRatio() : 1.0;

    m_modeToggleAction->setIcon(QIcon(AppIcons::pixmap(iconName, 16, color, dpr)));

    if (m_shell != NULL)
        m_shell->setModeState(operating);
}

void App::updateBlackoutIcon()
{
    if (m_controlBlackoutAction == NULL)
        return;

    if (m_controlBlackoutAction->isChecked() == true)
    {
        AppIcons::Palette palette(QColor("#FFFFFF"), QColor("#FFFFFF"),
                                  QColor("#FFFFFF"), QColor("#FFFFFF"));
        m_controlBlackoutAction->setIcon(AppIcons::icon("blackout", 20, palette));
    }
    else
    {
        m_controlBlackoutAction->setIcon(AppIcons::icon("blackout", 20,
                                                        AppIcons::Blackout));
    }
}

void App::updateActionIcons()
{
    AppIcons::clearCache();

    QHash<QAction *, QString>::const_iterator it = m_actionIcons.constBegin();
    while (it != m_actionIcons.constEnd())
    {
        if (it.key() != NULL)
        {
            if (it.key() == m_controlPanicAction && m_controlPanicAction->isEnabled())
                it.key()->setIcon(AppIcons::icon(it.value(), 20, AppIcons::Destructive));
            else
                it.key()->setIcon(AppIcons::icon(it.value(), 20));
        }
        ++it;
    }

    updateModeIcon();
    updateBlackoutIcon();

    if (m_tab == NULL)
        return;

    for (int i = 0; i < m_tab->count(); i++)
    {
        QWidget *page = m_tab->widget(i);
        if (page == NULL)
            continue;

        const QString iconName = page->property("qlcNavIcon").toString();
        if (iconName.isEmpty() == false)
            m_tab->setTabIcon(i, AppIcons::icon(iconName, 20));
    }
}

void App::initToolBar()
{
    m_toolbar = new QToolBar(tr("Workspace"), m_shell);
    m_toolbar->setObjectName("commandBar");
    m_toolbar->setFloatable(false);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_toolbar->addAction(m_fileNewAction);
    m_toolbar->addAction(m_fileOpenAction);
    m_toolbar->addAction(m_fileSaveAction);
    m_toolbar->addAction(m_fileSaveAsAction);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_controlMonitorAction);
    m_toolbar->addAction(m_addressToolAction);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_controlFullScreenAction);
    m_toolbar->addAction(m_appearanceAction);
    m_toolbar->addAction(m_helpIndexAction);
    m_toolbar->addAction(m_helpAboutAction);
    if (QLCFile::hasWindowManager() == false)
        m_toolbar->addAction(m_quitAction);

    /* Create an empty widget between help items to flush them to the right */
    QWidget* widget = new QWidget(m_toolbar);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(widget);
    m_toolbar->addAction(m_dumpDmxAction);
    m_toolbar->addAction(m_liveEditAction);
    m_toolbar->addAction(m_liveEditVirtualConsoleAction);

    m_songRequestAction = new QAction(tr("SongRequest"), this);
    connect(m_songRequestAction, &QAction::triggered,
            this, &App::openSongRequestDialog);
    m_toolbar->addAction(m_songRequestAction);

    SongRequestClient *songRequest = SongRequestClient::instance();
    connect(songRequest, &SongRequestClient::statusChanged,
            this, &App::updateSongRequestStatus);
    songRequest->loadSettings();
    updateSongRequestStatus();

    m_toolbar->addSeparator();
    m_toolbar->addAction(m_controlPanicAction);

    QToolButton* btn = qobject_cast<QToolButton*> (m_toolbar->widgetForAction(m_fileOpenAction));
    Q_ASSERT(btn != NULL);
    btn->setPopupMode(QToolButton::DelayedPopup);
    updateFileOpenMenu("");
    updateMenuButtonPopup(m_fileOpenAction);

    btn = qobject_cast<QToolButton*> (m_toolbar->widgetForAction(m_controlPanicAction));
    Q_ASSERT(btn != NULL);
    btn->setPopupMode(QToolButton::DelayedPopup);
    updateMenuButtonPopup(m_controlPanicAction);

    btn = qobject_cast<QToolButton*> (m_toolbar->widgetForAction(m_appearanceAction));
    Q_ASSERT(btn != NULL);
    btn->setPopupMode(QToolButton::InstantPopup);
    updateMenuButtonPopup(m_appearanceAction);

    m_shell->setCommandBar(m_toolbar);
    m_shell->setPrimaryActions(m_modeToggleAction, m_controlBlackoutAction);
    updateModeIcon();
    updateBlackoutIcon();

    m_audioBpmTimer = new QTimer(this);
    m_audioBpmTimer->setInterval(500);
    connect(m_audioBpmTimer, &QTimer::timeout, this, &App::updateAudioBpm);
    m_audioBpmTimer->start();
}

/*****************************************************************************
 * File action slots
 *****************************************************************************/

bool App::handleFileError(QFile::FileError error)
{
    QString msg;

    switch (error)
    {
        case QFile::NoError:
            return true;
        break;
        case QFile::ReadError:
            msg = tr("Unable to read from file");
        break;
        case QFile::WriteError:
            msg = tr("Unable to write to file");
        break;
        case QFile::FatalError:
            msg = tr("A fatal error occurred");
        break;
        case QFile::ResourceError:
            msg = tr("Unable to access resource");
        break;
        case QFile::OpenError:
            msg = tr("Unable to open file for reading or writing");
        break;
        case QFile::AbortError:
            msg = tr("Operation was aborted");
        break;
        case QFile::TimeOutError:
            msg = tr("Operation timed out");
        break;
        default:
        case QFile::UnspecifiedError:
            msg = tr("An unspecified error has occurred. Nice.");
        break;
    }

    QMessageBox::warning(this, tr("File error"), msg);

    return false;
}

bool App::saveModifiedDoc(const QString & title, const QString & message)
{
    // if it's not modified, there's nothing to save
    if (m_doc->isModified() == false)
        return true;

    int result = QMessageBox::warning(this, title,
                                          message,
                                          QMessageBox::Yes |
                                          QMessageBox::No |
                                          QMessageBox::Cancel);
    if (result == QMessageBox::Yes)
    {
        slotFileSave();
        // we check whether m_doc is not modified anymore, rather than
        // result of slotFileSave() since the latter returns NoError
        // in cases like when the user pressed cancel in the save dialog
        if (m_doc->isModified() == false)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (result == QMessageBox::No)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void App::updateMenuButtonPopup(QAction *action)
{
    if (m_toolbar == NULL || action == NULL)
        return;

    QToolButton *btn = qobject_cast<QToolButton *>(m_toolbar->widgetForAction(action));
    if (btn == NULL)
        return;

    const bool hasMenu = (action->menu() != NULL);
    if (btn->property("uiHasMenu").toBool() != hasMenu)
    {
        btn->setProperty("uiHasMenu", hasMenu);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
    }
}

void App::updateFileOpenMenu(QString addRecent)
{
    QSettings settings;
    QStringList menuRecentList;

    if (m_fileOpenMenu == NULL)
    {
        m_fileOpenMenu = new QMenu(this);
        connect(m_fileOpenMenu, SIGNAL(triggered(QAction*)),
                this, SLOT(slotRecentFileClicked(QAction*)));
    }

    foreach (QAction* a, m_fileOpenMenu->actions())
    {
        menuRecentList.append(a->text());
        m_fileOpenMenu->removeAction(a);
    }

    if (addRecent.isEmpty() == false)
    {
        menuRecentList.removeAll(addRecent); // in case the string is already present, remove it...
        menuRecentList.prepend(addRecent); // and add it to the top
        for (int i = 0; i < menuRecentList.count(); i++)
        {
            settings.setValue(QString("%1%2").arg(SETTINGS_RECENTFILE).arg(i), menuRecentList.at(i));
            m_fileOpenMenu->addAction(menuRecentList.at(i));
        }
    }
    else
    {
        for (int i = 0; i < MAX_RECENT_FILES; i++)
        {
            QVariant recent = settings.value(QString("%1%2").arg(SETTINGS_RECENTFILE).arg(i));
            if (recent.isValid() == true)
            {
                menuRecentList.append(recent.toString());
                m_fileOpenMenu->addAction(menuRecentList.at(i));
            }
        }
    }

    // Set the recent files menu to the file open action
    if (menuRecentList.isEmpty() == false)
        m_fileOpenAction->setMenu(m_fileOpenMenu);

    updateMenuButtonPopup(m_fileOpenAction);
}

bool App::slotFileNew()
{
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("New Workspace"), msg) == false)
    {
        return false;
    }

    clearDocument();
    return true;
}

QFile::FileError App::slotFileOpen()
{
    QString fn;

    /* Check that the user is aware of losing previous changes */
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("Open Workspace"), msg) == false)
    {
        /* Second thoughts... Cancel loading. */
        return QFile::NoError;
    }

    /* Create a file open dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Open Workspace"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.selectFile(fileName());
    if (m_workingDirectory.exists() == true)
        dialog.setDirectory(m_workingDirectory);

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("Workspaces (*%1)").arg(KExtWorkspace);
#if defined(WIN32) || defined(Q_OS_WIN)
    filters << tr("All Files (*.*)");
#else
    filters << tr("All Files (*)");
#endif
    dialog.setNameFilters(filters);

    /* Append useful URLs to the dialog */
    QList <QUrl> sidebar;
    sidebar.append(QUrl::fromLocalFile(QDir::homePath()));
    sidebar.append(QUrl::fromLocalFile(QDir::rootPath()));
    dialog.setSidebarUrls(sidebar);

    /* Get file name */
    if (dialog.exec() != QDialog::Accepted)
        return QFile::NoError;
    QSettings settings;
    m_workingDirectory = dialog.directory();
    settings.setValue(SETTINGS_WORKINGPATH, m_workingDirectory.absolutePath());

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return QFile::NoError;

    /* Clear existing document data */
    clearDocument();

#ifdef DEBUG_SPEED
    speedTime.restart();
#endif

    /* Load the file */
    QFile::FileError error = loadXML(fn);
    if (handleFileError(error) == true)
        m_doc->resetModified();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Project loaded in" << speedTime.elapsed() << "ms.";
#endif

    /* Update these in any case, since they are at least emptied now as
       a result of calling clearDocument() a few lines ago. */
    //if (FunctionManager::instance() != NULL)
    //    FunctionManager::instance()->updateTree();
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();

    updateFileOpenMenu(fn);

    return error;
}

QFile::FileError App::slotFileSave()
{
    QFile::FileError error;
    QString asfName = autoSaveFileName();

    /* Attempt to save with the existing name. Fall back to Save As. */
    if (fileName().isEmpty() == true)
        error = slotFileSaveAs();
    else
        error = saveXML(fileName());

    if (handleFileError(error))
    {
        QFile asFile(asfName);
        if (asFile.exists())
            asFile.remove();

        if (m_shell != NULL && error == QFile::NoError)
            m_shell->showToast(tr("Workspace saved"));
    }
    return error;
}

QFile::FileError App::slotFileSaveAs()
{
    QString fn;
    QString asfName = autoSaveFileName();

    /* Create a file save dialog */
    QFileDialog dialog(this);
    dialog.setWindowTitle(tr("Save Workspace As"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile(fileName());

    /* Append file filters to the dialog */
    QStringList filters;
    filters << tr("Workspaces (*%1)").arg(KExtWorkspace);
#if defined(WIN32) || defined(Q_OS_WIN)
    filters << tr("All Files (*.*)");
#else
    filters << tr("All Files (*)");
#endif
    dialog.setNameFilters(filters);

    /* Append useful URLs to the dialog */
    QList <QUrl> sidebar;
    sidebar.append(QUrl::fromLocalFile(QDir::homePath()));
    sidebar.append(QUrl::fromLocalFile(QDir::rootPath()));
    dialog.setSidebarUrls(sidebar);

    /* Get file name */
    if (dialog.exec() != QDialog::Accepted)
        return QFile::NoError;

    fn = dialog.selectedFiles().first();
    if (fn.isEmpty() == true)
        return QFile::NoError;

    /* Always use the workspace suffix */
    if (fn.right(4) != KExtWorkspace)
        fn += KExtWorkspace;

    /* Set the workspace path before saving the new XML. In this way local files
       can be loaded even if the workspace file will be moved */
    m_doc->setWorkspacePath(QFileInfo(fn).absolutePath());

    /* Save the document and set workspace name */
    QFile::FileError error = saveXML(fn);

    if (handleFileError(error))
    {
        /* remove autosave file if present */
        QFile asFile(asfName);
        if (asFile.exists())
            asFile.remove();
    }

    updateFileOpenMenu(fn);
    return error;
}

/*****************************************************************************
 * Control action slots
 *****************************************************************************/

void App::slotControlMonitor()
{
    Monitor::createAndShow(this, m_doc);
}

void App::slotAddressTool()
{
    AddressTool at(this);
    at.exec();
}

void App::slotControlBlackout()
{
    m_doc->inputOutputMap()->setBlackout(!m_doc->inputOutputMap()->blackout());
}

void App::slotBlackoutChanged(bool state)
{
    m_controlBlackoutAction->setChecked(state);
    updateBlackoutIcon();
}

void App::slotControlPanic()
{
    m_doc->masterTimer()->stopAllFunctions();
}

void App::slotFadeAndStopAll()
{
    QAction *action = (QAction *)sender();
    int timeout = action->data().toInt();

    m_doc->masterTimer()->fadeAndStopAll(timeout);
}

void App::slotRunningFunctionsChanged()
{
    const bool running = m_doc->masterTimer()->runningFunctions() > 0;
    m_controlPanicAction->setEnabled(running);
    m_controlPanicAction->setIcon(running
        ? AppIcons::icon("octagon-x", 20, AppIcons::Destructive)
        : AppIcons::icon("octagon-x", 20));
}

void App::slotDumpDmxIntoFunction()
{
    DmxDumpFactory ddf(m_doc, m_dumpProperties, this);
    if (ddf.exec() != QDialog::Accepted)
        return;
}

void App::slotFunctionLiveEdit()
{
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType | Function::EFXType | Function::RGBMatrixType);
    fs.disableFilters(Function::ShowType | Function::ScriptType | Function::CollectionType | Function::AudioType);

    if (fs.exec() == QDialog::Accepted)
    {
        if (fs.selection().count() > 0)
        {
            FunctionLiveEditDialog fle(m_doc, fs.selection().first(), this);
            fle.exec();
        }
    }
}

void App::slotLiveEditVirtualConsole()
{
    VirtualConsole::instance()->toggleLiveEdit();
}

void App::slotDetachContext(int index)
{
    /* Get the widget that has been double-clicked */
    QWidget *context = m_tab->widget(index);
    context->setProperty("tabIndex", index);
    context->setProperty("tabIcon", QVariant::fromValue(m_tab->tabIcon(index)));
    context->setProperty("tabLabel", m_tab->tabText(index));

    qDebug() << "Detaching context" << context;

    DetachedContext *detachedWindow = new DetachedContext(this);
    detachedWindow->setCentralWidget(context);
    detachedWindow->resize(800, 600);
    detachedWindow->show();
    context->show();

    connect(detachedWindow, SIGNAL(closing()),
            this, SLOT(slotReattachContext()));

    if (m_shell != NULL)
        m_shell->refreshPages();
}

void App::slotReattachContext()
{
    DetachedContext *window = qobject_cast<DetachedContext *>(sender());

    QWidget *context = window->centralWidget();
    int tabIndex = context->property("tabIndex").toInt();
    QIcon tabIcon = context->property("tabIcon").value<QIcon>();
    QString tabLabel = context->property("tabLabel").toString();

    qDebug() << "Reattaching context" << tabIndex << tabLabel << context;

    context->setParent(m_tab);
    m_tab->insertTab(tabIndex, context, tabIcon, tabLabel);

    if (m_shell != NULL)
        m_shell->refreshPages();
}

void App::slotControlFullScreen()
{
    static int wstate = windowState();

    if (windowState() & Qt::WindowFullScreen)
    {
        if (wstate & Qt::WindowMaximized)
            showMaximized();
        else
            showNormal();
        wstate = windowState();
    }
    else
    {
        wstate = windowState();
        showFullScreen();

        // In case slotControlFullScreen() is called programmatically (from main.cpp)
        if (m_controlFullScreenAction->isChecked() == false)
            m_controlFullScreenAction->setChecked(true);
    }
}

void App::slotControlFullScreen(bool usingGeometry)
{
    if (usingGeometry == true)
    {
        QScreen *screen = QGuiApplication::screens().first();
        setGeometry(screen->geometry());
    }
    else
    {
        slotControlFullScreen();
    }
}

/*****************************************************************************
 * Help action slots
 *****************************************************************************/

void App::slotHelpIndex()
{
    QDesktopServices::openUrl(QUrl("https://docs.qlcplus.org/"));
}

void App::slotHelpAbout()
{
    AboutBox ab(this);
    ab.exec();
}

void App::slotRecentFileClicked(QAction *recent)
{
    if (recent == NULL)
        return;

    QString recentAbsPath = recent->text();
    QFile testFile(recentAbsPath);
    if (testFile.exists() == false)
    {
        QMessageBox::critical(this, tr("Error"),
                              tr("File not found!\nThe selected file has been moved or deleted."),
                              QMessageBox::Close);
        return;
    }

    /* Check that the user is aware of losing previous changes */
    QString msg(tr("Do you wish to save the current workspace?\n" \
                   "Changes will be lost if you don't save them."));
    if (saveModifiedDoc(tr("Open Workspace"), msg) == false)
    {
        /* Second thoughts... Cancel loading. */
        return;
    }

    m_workingDirectory = QFileInfo(recentAbsPath).absoluteDir();
    QSettings settings;
    settings.setValue(SETTINGS_WORKINGPATH, m_workingDirectory.absolutePath());

    /* Clear existing document data */
    clearDocument();

#ifdef DEBUG_SPEED
    speedTime.restart();
#endif

    /* Load the file */
    QFile::FileError error = loadXML(recentAbsPath);
    if (handleFileError(error) == true)
        m_doc->resetModified();

#ifdef DEBUG_SPEED
    qDebug() << "[App] Project loaded in" << speedTime.elapsed() << "ms.";
#endif

    /* Update these in any case, since they are at least emptied now as
       a result of calling clearDocument() a few lines ago. */
    //if (FunctionManager::instance() != NULL)
    //    FunctionManager::instance()->updateTree();
    if (FixtureManager::instance() != NULL)
        FixtureManager::instance()->updateView();
    if (InputOutputManager::instance() != NULL)
        InputOutputManager::instance()->updateList();
    if (Monitor::instance() != NULL)
        Monitor::instance()->updateView();
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

void App::setFileName(const QString& fileName)
{
    m_fileName = fileName;
}

QString App::fileName() const
{
    return m_fileName;
}

QString App::autoSaveFileName() const
{
    QString fName = m_fileName;

    if (fName.isEmpty())
        fName = "NewProject.autosave.qxw";
    else
    {
        fName.remove(".qxw");
        fName.append(".autosave.qxw");
    }

    return fName;
}

QFile::FileError App::loadXML(const QString& fileName)
{
    QFile::FileError retval = QFile::NoError;

    if (fileName.isEmpty() == true)
        return QFile::OpenError;

    QXmlStreamReader *doc = QLCFile::getXMLReader(fileName);
    if (doc == NULL || doc->device() == NULL || doc->hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from" << fileName;
        return QFile::ReadError;
    }

    while (!doc->atEnd())
    {
        if (doc->readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (doc->hasError())
    {
        QLCFile::releaseXMLReader(doc);
        return QFile::ResourceError;
    }

    /* Set the workspace path before loading the new XML. In this way local files
       can be loaded even if the workspace file has been moved */
    m_doc->setWorkspacePath(QFileInfo(fileName).absolutePath());

    if (doc->dtdName() == KXMLQLCWorkspace)
    {
        if (loadXML(*doc) == false)
        {
            retval = QFile::ReadError;
        }
        else
        {
            setFileName(fileName);
            m_doc->resetModified();
            retval = QFile::NoError;
        }
    }
    else
    {
        retval = QFile::ReadError;
        qWarning() << Q_FUNC_INFO << fileName
                   << "is not a workspace file";
    }

    QLCFile::releaseXMLReader(doc);

    return retval;
}

bool App::loadXML(QXmlStreamReader& doc, bool goToConsole, bool fromMemory)
{
    if (doc.readNextStartElement() == false)
        return false;

    if (doc.name() != KXMLQLCWorkspace)
    {
        qWarning() << Q_FUNC_INFO << "Workspace node not found";
        return false;
    }

    QString activeWindowName = doc.attributes().value(KXMLQLCWorkspaceWindow).toString();

    while (doc.readNextStartElement())
    {
        if (doc.name() == KXMLQLCEngine)
        {
            m_doc->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCVirtualConsole)
        {
            VirtualConsole::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCSimpleDesk)
        {
            SimpleDesk::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCStageGeometry)
        {
            if (StageGeometry::instance() != NULL)
                StageGeometry::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLQLCPreview3D)
        {
            if (Preview3D::instance() != NULL)
                Preview3D::instance()->loadXML(doc);
        }
        else if (doc.name() == KXMLFixture)
        {
            /* Legacy support code, nowadays in Doc */
            Fixture::loader(doc, m_doc);
        }
        else if (doc.name() == KXMLQLCFunction)
        {
            /* Legacy support code, nowadays in Doc */
            Function::loader(doc, m_doc);
        }
        else if (doc.name() == KXMLQLCCreator)
        {
            /* Ignore creator information */
            doc.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Workspace tag:" << doc.name();
            doc.skipCurrentElement();
        }
    }

    if (goToConsole == true)
        // Force the active window to be Virtual Console
        setActiveWindow(VirtualConsole::staticMetaObject.className());
    else
        // Set the active window to what was saved in the workspace file
        setActiveWindow(activeWindowName);

    // Perform post-load operations
    VirtualConsole::instance()->postLoad();

    if (m_doc->errorLog().isEmpty() == false &&
        fromMemory == false)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Warning"),
                        tr("Some errors occurred while loading the project:") + "<br><br>" + m_doc->errorLog(),
                        QMessageBox::Ok);
        msg.setTextFormat(Qt::RichText);
        QSpacerItem* horizontalSpacer = new QSpacerItem(800, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = (QGridLayout*)msg.layout();
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msg.exec();
    }

    m_doc->inputOutputMap()->startUniverses();

    return true;
}

QFile::FileError App::saveXML(const QString& fileName, bool autosave)
{
    QString tempFileName(fileName);
    tempFileName += ".temp";
    QFile file(tempFileName);
    if (file.open(QIODevice::WriteOnly) == false)
        return file.error();

    QXmlStreamWriter doc(&file);
    doc.setAutoFormatting(true);
    doc.setAutoFormattingIndent(1);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    doc.setCodec("UTF-8");
#endif

    doc.writeStartDocument();
    doc.writeDTD(QString("<!DOCTYPE %1>").arg(KXMLQLCWorkspace));

    doc.writeStartElement(KXMLQLCWorkspace);
    doc.writeAttribute("xmlns", QString("%1%2").arg(KXMLQLCplusNamespace).arg(KXMLQLCWorkspace));
    /* Currently active window */
    QWidget* widget = m_tab->currentWidget();
    if (widget != NULL)
        doc.writeAttribute(KXMLQLCWorkspaceWindow, QString(widget->metaObject()->className()));

    doc.writeStartElement(KXMLQLCCreator);
    doc.writeTextElement(KXMLQLCCreatorName, APPNAME);
    doc.writeTextElement(KXMLQLCCreatorVersion, APPVERSION);
    doc.writeTextElement(KXMLQLCCreatorAuthor, QLCFile::currentUserName());
    doc.writeEndElement(); // close KXMLQLCCreator

    /* Write engine components to the XML document */
    m_doc->saveXML(&doc);

    /* Write virtual console to the XML document */
    VirtualConsole::instance()->saveXML(&doc);

    /* Write Simple Desk to the XML document */
    SimpleDesk::instance()->saveXML(&doc);

    /* Write Stage Geometry to the XML document */
    if (StageGeometry::instance() != NULL)
        StageGeometry::instance()->saveXML(&doc);

    /* Write the 3D Preview (trusses, links, camera) to the XML document */
    if (Preview3D::instance() != NULL)
        Preview3D::instance()->saveXML(&doc);

    doc.writeEndElement(); // close KXMLQLCWorkspace

    /* End the document and close all the open elements */
    doc.writeEndDocument();
    file.close();
#ifdef Q_OS_UNIX
    sync();
#endif

    // Save to actual requested file name
    QFile currFile(fileName);
    if (currFile.exists() && !currFile.remove())
    {
        qWarning() << "Could not erase" << fileName;
        return currFile.error();
    }
    if (!file.rename(fileName))
    {
        qWarning() << "Could not rename" << tempFileName << "to" << fileName;
        return file.error();
    }

    if (!autosave)
    {
        /* Set the file name for the current Doc instance and
           set it also in an unmodified state. */
        setFileName(fileName);
        m_doc->resetModified();
    }

    return QFile::NoError;
}

void App::slotLoadDocFromMemory(QString xmlData)
{
    if (xmlData.isEmpty())
        return;

    /* Clear existing document data */
    clearDocument();

    QBuffer databuf;
    databuf.setData(xmlData.simplified().toUtf8());
    databuf.open(QIODevice::ReadOnly | QIODevice::Text);

    //qDebug() << "Buffer data:" << databuf.data();
    QXmlStreamReader doc(&databuf);

    if (doc.hasError())
    {
        qWarning() << Q_FUNC_INFO << "Unable to read from XML in memory";
        return;
    }

    while (!doc.atEnd())
    {
        if (doc.readNext() == QXmlStreamReader::DTD)
            break;
    }
    if (doc.hasError())
    {
        qDebug() << "XML has errors:" << doc.errorString();
        return;
    }

    if (doc.dtdName() == KXMLQLCWorkspace)
        loadXML(doc, true, true);
    else
        qDebug() << "XML doesn't have a Workspace tag";
}

void App::slotSaveAutostart(QString fileName)
{
    /* Set the workspace path before saving the new XML. In this way local files
       can be loaded even if the workspace file will be moved */
    m_doc->setWorkspacePath(QFileInfo(fileName).absolutePath());

    /* Save the document and set workspace name */
    QFile::FileError error = saveXML(fileName);
    handleFileError(error);
}
