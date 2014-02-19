#include "gui/formmain.h"

#include "core/defs.h"
#include "core/settings.h"
#include "core/systemfactory.h"
#include "core/databasefactory.h"
#include "gui/formabout.h"
#include "gui/formsettings.h"
#include "gui/feedsview.h"
#include "gui/messagebox.h"
#include "gui/webbrowser.h"
#include "gui/iconthemefactory.h"
#include "gui/systemtrayicon.h"
#include "gui/tabbar.h"
#include "gui/statusbar.h"
#include "gui/feedmessageviewer.h"
#include "qtsingleapplication/qtsingleapplication.h"
#include "gui/formupdate.h"

#include <QCloseEvent>
#include <QSessionManager>
#include <QRect>
#include <QDesktopWidget>
#include <QReadWriteLock>
#include <QTimer>


FormMain *FormMain::s_instance;

FormMain::FormMain(QWidget *parent)
  : QMainWindow(parent), m_ui(new Ui::FormMain) {
  m_ui->setupUi(this);

  // Initialize singleton.
  s_instance = this;

  m_statusBar = new StatusBar(this);
  setStatusBar(m_statusBar);

  // Prepare main window and tabs.
  prepareMenus();

  // Establish connections.
  createConnections();

  // Prepare tabs.
  m_ui->m_tabWidget->initializeTabs();

  setupIcons();
  loadSize();
}

FormMain::~FormMain() {
  delete m_ui;
}

FormMain *FormMain::instance() {
  return s_instance;
}

QList<QAction*> FormMain::allActions() {
  QList<QAction*> actions;

  // Add basic actions.
  actions << m_ui->m_actionImport << m_ui->m_actionExport <<
             m_ui->m_actionSettings << m_ui->m_actionQuit <<
             m_ui->m_actionFullscreen << m_ui->m_actionAboutGuard <<
             m_ui->m_actionSwitchFeedsListVisibility << m_ui->m_actionSwitchMainWindow;

  // Add web browser actions
  actions << m_ui->m_actionAddBrowser << m_ui->m_actionCloseCurrentTab <<
             m_ui->m_actionCloseAllTabs;

  // Add feeds/messages actions.
  actions << m_ui->m_actionOpenSelectedSourceArticlesExternally <<
             m_ui->m_actionOpenSelectedSourceArticlesInternally <<
             m_ui->m_actionOpenSelectedMessagesInternally <<
             m_ui->m_actionMarkAllFeedsRead <<
             m_ui->m_actionMarkSelectedFeedsAsRead <<
             m_ui->m_actionMarkSelectedFeedsAsUnread <<
             m_ui->m_actionClearSelectedFeeds <<
             m_ui->m_actionMarkSelectedMessagesAsRead <<
             m_ui->m_actionMarkSelectedMessagesAsUnread <<
             m_ui->m_actionSwitchImportanceOfSelectedMessages <<
             m_ui->m_actionDeleteSelectedMessages <<
             m_ui->m_actionUpdateAllFeeds <<
             m_ui->m_actionUpdateSelectedFeedsCategories <<
             m_ui->m_actionEditSelectedFeedCategory <<
             m_ui->m_actionDeleteSelectedFeedCategory <<
             m_ui->m_actionViewSelectedItemsNewspaperMode <<
             m_ui->m_actionAddStandardCategory <<
             m_ui->m_actionAddStandardFeed <<
             m_ui->m_actionSelectNextFeedCategory <<
             m_ui->m_actionSelectPreviousFeedCategory <<
             m_ui->m_actionSelectNextMessage <<
             m_ui->m_actionSelectPreviousMessage;

  return actions;
}

void FormMain::prepareMenus() {
  // Setup menu for tray icon.
  if (SystemTrayIcon::isSystemTrayAvailable()) {
#if defined(Q_OS_WIN)
    m_trayMenu = new TrayIconMenu(APP_NAME, this);
#else
    m_trayMenu = new QMenu(APP_NAME, this);
#endif

    // Add "check for updates" item on some platforms.
    m_ui->m_actionCheckForUpdates->setIcon(IconThemeFactory::instance()->fromTheme("check-for-updates"));
    m_ui->m_actionCheckForUpdates->setToolTip(tr("Check if new update for the application is available for download."));

    // Add needed items to the menu.
    m_trayMenu->addAction(m_ui->m_actionSwitchMainWindow);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_ui->m_actionUpdateAllFeeds);
    m_trayMenu->addAction(m_ui->m_actionMarkAllFeedsRead);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_ui->m_actionSettings);
    m_trayMenu->addAction(m_ui->m_actionQuit);

    qDebug("Creating tray icon menu.");
  }
}

void FormMain::processExecutionMessage(const QString &message) {
  qDebug("Received '%s' execution message from another application instance.",
         qPrintable(message));

  if (message == APP_IS_RUNNING) {
    if (SystemTrayIcon::isSystemTrayActivated()) {
      SystemTrayIcon::instance()->showMessage(APP_NAME,
                                              tr("Application is already running."),
                                              QSystemTrayIcon::Information,
                                              TRAY_ICON_BUBBLE_TIMEOUT);
    }

    display();
  }
}

void FormMain::quit() {
  qDebug("Quitting the application.");
  qApp->quit();
}

void FormMain::switchFullscreenMode() {
  if (!isFullScreen()) {
    showFullScreen();
  } else {
    showNormal();
  }
}

void FormMain::switchVisibility() {
  if (isVisible()) {
    hide();
  }
  else {
    display();
  }
}

void FormMain::display() {
  // Make sure window is not minimized.
  setWindowState(windowState() & ~Qt::WindowMinimized);

  // Display the window and make sure it is raised on top.
  show();
  activateWindow();
  raise();

  // Raise alert event. Check the documentation for more info on this.
  QtSingleApplication::alert(this);
}

void FormMain::onCommitData(QSessionManager &manager) {
  qDebug("OS asked application to commit its data.");

  manager.setRestartHint(QSessionManager::RestartNever);
  manager.release();
}

void FormMain::onSaveState(QSessionManager &manager) {
  qDebug("OS asked application to save its state.");

  manager.setRestartHint(QSessionManager::RestartNever);
  manager.release();
}

void FormMain::onAboutToQuit() {
  // Make sure that we obtain close lock
  // BEFORE even trying to quit the application.
  bool locked_safely = SystemFactory::instance()->applicationCloseLock()->tryLock(CLOSE_LOCK_TIMEOUT);

  qDebug("Cleaning up resources and saving application state.");
  m_ui->m_tabWidget->feedMessageViewer()->quit();

  if (Settings::instance()->value(APP_CFG_MESSAGES, "clear_read_on_exit", false).toBool()) {
    m_ui->m_tabWidget->feedMessageViewer()->feedsView()->clearAllReadMessages();
  }

  DatabaseFactory::instance()->saveDatabase();
  saveSize();

  if (locked_safely) {
    // Application obtained permission to close
    // in a safety way.
    qDebug("Close lock was obtained safely.");

    // We locked the lock to exit peacefully, unlock it to avoid warnings.
    SystemFactory::instance()->applicationCloseLock()->unlock();
  }
  else {
    // Request for write lock timed-out. This means
    // that some critical action can be processed right now.
    qDebug("Close lock timed-out.");
  }
}

void FormMain::setupIcons() {
  IconThemeFactory *icon_theme_factory = IconThemeFactory::instance();

  // Setup icons of this main window.
  m_ui->m_actionSettings->setIcon(icon_theme_factory->fromTheme("application-settings"));
  m_ui->m_actionQuit->setIcon(icon_theme_factory->fromTheme("application-exit"));
  m_ui->m_actionAboutGuard->setIcon(icon_theme_factory->fromTheme("application-about"));
  m_ui->m_actionImport->setIcon(icon_theme_factory->fromTheme("document-import"));
  m_ui->m_actionExport->setIcon(icon_theme_factory->fromTheme("document-export"));
  m_ui->m_actionDefragmentDatabase->setIcon(icon_theme_factory->fromTheme("defragment-database"));

  // View.
  m_ui->m_actionSwitchMainWindow->setIcon(icon_theme_factory->fromTheme("view-switch"));
  m_ui->m_actionFullscreen->setIcon(icon_theme_factory->fromTheme("view-fullscreen"));
  m_ui->m_actionSwitchFeedsListVisibility->setIcon(icon_theme_factory->fromTheme("view-switch"));

  // Web browser.
  m_ui->m_actionAddBrowser->setIcon(icon_theme_factory->fromTheme("list-add"));
  m_ui->m_actionCloseCurrentTab->setIcon(icon_theme_factory->fromTheme("list-remove"));
  m_ui->m_actionCloseAllTabs->setIcon(icon_theme_factory->fromTheme("list-remove"));
  m_ui->m_menuCurrentTab->setIcon(icon_theme_factory->fromTheme("list-current"));

  // Feeds/messages.
  m_ui->m_menuAddItem->setIcon(icon_theme_factory->fromTheme("item-new"));
  m_ui->m_actionUpdateAllFeeds->setIcon(icon_theme_factory->fromTheme("item-update-all"));
  m_ui->m_actionUpdateSelectedFeedsCategories->setIcon(icon_theme_factory->fromTheme("item-update-selected"));
  m_ui->m_actionClearSelectedFeeds->setIcon(icon_theme_factory->fromTheme("mail-remove"));
  m_ui->m_actionClearAllFeeds->setIcon(icon_theme_factory->fromTheme("mail-remove"));
  m_ui->m_actionDeleteSelectedFeedCategory->setIcon(icon_theme_factory->fromTheme("item-remove"));
  m_ui->m_actionDeleteSelectedMessages->setIcon(icon_theme_factory->fromTheme("mail-remove"));
  m_ui->m_actionAddStandardCategory->setIcon(icon_theme_factory->fromTheme("item-new"));
  m_ui->m_actionAddStandardFeed->setIcon(icon_theme_factory->fromTheme("item-new"));
  m_ui->m_actionEditSelectedFeedCategory->setIcon(icon_theme_factory->fromTheme("item-edit"));
  m_ui->m_actionMarkAllFeedsRead->setIcon(icon_theme_factory->fromTheme("mail-mark-read"));
  m_ui->m_actionMarkSelectedFeedsAsRead->setIcon(icon_theme_factory->fromTheme("mail-mark-read"));
  m_ui->m_actionMarkSelectedFeedsAsUnread->setIcon(icon_theme_factory->fromTheme("mail-mark-unread"));
  m_ui->m_actionMarkSelectedMessagesAsRead->setIcon(icon_theme_factory->fromTheme("mail-mark-read"));
  m_ui->m_actionMarkSelectedMessagesAsUnread->setIcon(icon_theme_factory->fromTheme("mail-mark-unread"));
  m_ui->m_actionSwitchImportanceOfSelectedMessages->setIcon(icon_theme_factory->fromTheme("mail-mark-favorite"));
  m_ui->m_actionOpenSelectedSourceArticlesInternally->setIcon(icon_theme_factory->fromTheme("item-open"));
  m_ui->m_actionOpenSelectedSourceArticlesExternally->setIcon(icon_theme_factory->fromTheme("item-open"));
  m_ui->m_actionOpenSelectedMessagesInternally->setIcon(icon_theme_factory->fromTheme("item-open"));
  m_ui->m_actionViewSelectedItemsNewspaperMode->setIcon(icon_theme_factory->fromTheme("item-newspaper"));

  m_ui->m_actionSelectNextFeedCategory->setIcon(icon_theme_factory->fromTheme("go-down"));
  m_ui->m_actionSelectPreviousFeedCategory->setIcon(icon_theme_factory->fromTheme("go-up"));
  m_ui->m_actionSelectNextMessage->setIcon(icon_theme_factory->fromTheme("go-down"));
  m_ui->m_actionSelectPreviousMessage->setIcon(icon_theme_factory->fromTheme("go-up"));


  // Setup icons for underlying components: opened web browsers...
  foreach (WebBrowser *browser, WebBrowser::runningWebBrowsers()) {
    browser->setupIcons();
  }

  // Setup icons on TabWidget too.
  m_ui->m_tabWidget->setupIcons();
}

void FormMain::loadSize() {
  QRect screen = qApp->desktop()->screenGeometry();
  Settings *settings = Settings::instance();

  // Reload main window size & position.
  resize(settings->value(APP_CFG_GUI,
                         "window_size",
                         size()).toSize());
  move(settings->value(APP_CFG_GUI,
                       "window_position",
                       screen.center() - rect().center()).toPoint());

  // If user exited the application while in fullsreen mode,
  // then re-enable it now.
  if (settings->value(APP_CFG_GUI, "start_in_fullscreen", false).toBool()) {
    switchFullscreenMode();
  }

  // Adjust dimensions of "feeds & messages" widget.
  m_ui->m_tabWidget->feedMessageViewer()->loadSize();
}

void FormMain::saveSize() {
  Settings *settings = Settings::instance();

  settings->setValue(APP_CFG_GUI, "window_position", pos());
  settings->setValue(APP_CFG_GUI, "window_size", size());
  settings->setValue(APP_CFG_GUI, "start_in_fullscreen", isFullScreen());

  m_ui->m_tabWidget->feedMessageViewer()->saveSize();
}

void FormMain::createConnections() {
  // Status bar connections.
  connect(m_statusBar->fullscreenSwitcher(), SIGNAL(clicked()),
          m_ui->m_actionFullscreen, SLOT(trigger()));

  // Core connections.
  connect(qApp, SIGNAL(commitDataRequest(QSessionManager&)),
          this, SLOT(onCommitData(QSessionManager&)));
  connect(qApp, SIGNAL(saveStateRequest(QSessionManager&)),
          this, SLOT(onSaveState(QSessionManager&)));

  // Menu "File" connections.
  connect(m_ui->m_actionQuit, SIGNAL(triggered()), this, SLOT(quit()));

  // Menu "View" connections.
  connect(m_ui->m_actionFullscreen, SIGNAL(triggered()), this, SLOT(switchFullscreenMode()));
  connect(m_ui->m_actionSwitchMainWindow, SIGNAL(triggered()), this, SLOT(switchVisibility()));

  // Menu "Tools" connections.
  connect(m_ui->m_actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));

  // Menu "Help" connections.
  connect(m_ui->m_actionAboutGuard, SIGNAL(triggered()), this, SLOT(showAbout()));
  connect(m_ui->m_actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(showUpdates()));

  // General connections.
  connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));

  // Menu "Web browser" connections.
  connect(m_ui->m_tabWidget, SIGNAL(currentChanged(int)),
          this, SLOT(loadWebBrowserMenu(int)));
  connect(m_ui->m_actionCloseCurrentTab, SIGNAL(triggered()),
          m_ui->m_tabWidget, SLOT(closeCurrentTab()));
  connect(m_ui->m_actionAddBrowser, SIGNAL(triggered()),
          m_ui->m_tabWidget, SLOT(addEmptyBrowser()));
  connect(m_ui->m_actionCloseAllTabs, SIGNAL(triggered()),
          m_ui->m_tabWidget, SLOT(closeAllTabsExceptCurrent()));
}

void FormMain::loadWebBrowserMenu(int index) {
  WebBrowser *active_browser = m_ui->m_tabWidget->widget(index)->webBrowser();

  m_ui->m_menuCurrentTab->clear();
  if (active_browser != NULL) {
    m_ui->m_menuCurrentTab->addActions(active_browser->globalMenu());

    if (m_ui->m_menuCurrentTab->actions().size() == 0) {
      m_ui->m_menuCurrentTab->insertAction(NULL, m_ui->m_actionNoActions);
    }
  }

  m_ui->m_actionCloseCurrentTab->setEnabled(m_ui->m_tabWidget->tabBar()->tabType(index) == TabBar::Closable);
}

void FormMain::changeEvent(QEvent *event) {
  switch (event->type()) {
    case QEvent::WindowStateChange: {
      if (this->windowState() & Qt::WindowMinimized &&
          SystemTrayIcon::isSystemTrayActivated() &&
          Settings::instance()->value(APP_CFG_GUI,
                                      "hide_when_minimized",
                                      false).toBool()) {
        QTimer::singleShot(250, this, SLOT(hide()));
      }

      break;
    }

    default:
      break;
  }

  QMainWindow::changeEvent(event);
}

void FormMain::showAbout() {
  QPointer<FormAbout> form_pointer = new FormAbout(this);
  form_pointer.data()->exec();
  delete form_pointer.data();
}

void FormMain::showUpdates() {
  if (!SystemFactory::instance()->applicationCloseLock()->tryLock()) {
    if (SystemTrayIcon::isSystemTrayActivated()) {
      SystemTrayIcon::instance()->showMessage(tr("Cannot check for updates"),
                                              tr("You cannot check for updates because feed update is ongoing."),
                                              QSystemTrayIcon::Warning);
    }
    else {
      MessageBox::show(this,
                       QMessageBox::Warning,
                       tr("Cannot check for updates"),
                       tr("You cannot check for updates because feed update is ongoing."));
    }

    return;
  }

  QPointer<FormUpdate> form_update = new FormUpdate(this);
  form_update.data()->exec();
  delete form_update.data();
}

void FormMain::showSettings() {
  QPointer<FormSettings> form_pointer = new FormSettings(this);
  form_pointer.data()->exec();
  delete form_pointer.data();
}
