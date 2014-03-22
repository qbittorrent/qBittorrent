/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef GUI_H
#define GUI_H

#include <QProcess>
#include <QSystemTrayIcon>
#include <QPointer>
#include "ui_mainwindow.h"
#include "qtorrenthandle.h"
#include "statsdialog.h"

class QBtSession;
class downloadFromURL;
class SearchEngine;
class RSSImp;
class about;
class options_imp;
class TransferListWidget;
class TransferListFiltersWidget;
class PropertiesWidget;
class StatusBar;
class consoleDlg;
class about;
class TorrentCreatorDlg;
class downloadFromURL;
class HidableTabWidget;
class LineEdit;
class ExecutionLog;
class PowerManagement;

QT_BEGIN_NAMESPACE
class QCloseEvent;
class QFileSystemWatcher;
class QShortcut;
class QSplitter;
class QTabWidget;
class QTimer;
QT_END_NAMESPACE

class MainWindow : public QMainWindow, private Ui::MainWindow{
  Q_OBJECT

public:
  // Construct / Destruct
  MainWindow(QWidget *parent=0, const QStringList& torrentCmdLine = QStringList());
  // Methods
  QWidget* getCurrentTabWidget() const;
  TransferListWidget* getTransferList() const { return transferList; }
  QMenu* getTrayIconMenu();
  PropertiesWidget *getProperties() const { return properties; }

public slots:
  void trackerAuthenticationRequired(const QTorrentHandle& h);
  void setTabText(int index, QString text) const;
  void showNotificationBaloon(QString title, QString msg) const;
  void downloadFromURLList(const QStringList& urls);
  void updateAltSpeedsBtn(bool alternative);
  void updateNbTorrents();
  void shutdownCleanUp();

protected slots:
  // GUI related slots
  void dropEvent(QDropEvent *event);
  void dragEnterEvent(QDragEnterEvent *event);
  void toggleVisibility(QSystemTrayIcon::ActivationReason e = QSystemTrayIcon::Trigger);
  void on_actionAbout_triggered();
  void on_actionStatistics_triggered();
  void on_actionCreate_torrent_triggered();
  void on_actionWebsite_triggered() const;
  void on_actionBugReport_triggered() const;
  void balloonClicked();
  void writeSettings();
  void readSettings();
  void on_actionExit_triggered();
  void createTrayIcon();
  void fullDiskError(const QTorrentHandle& h, QString msg) const;
  void handleDownloadFromUrlFailure(QString, QString) const;
  void createSystrayDelayed();
  void tab_changed(int);
  void on_actionLock_qBittorrent_triggered();
  void defineUILockPassword();
  void clearUILockPassword();
  bool unlockUI();
  void notifyOfUpdate(QString);
  void showConnectionSettings();
  void minimizeWindow();
  void updateTrayIconMenu();
  // Keyboard shortcuts
  void createKeyboardShortcuts();
  void displayTransferTab() const;
  void displaySearchTab() const;
  void displayRSSTab() const;
  // Torrent actions
  void on_actionSet_global_upload_limit_triggered();
  void on_actionSet_global_download_limit_triggered();
  void on_actionDocumentation_triggered() const;
  void on_actionOpen_triggered();
  void updateGUI();
  void loadPreferences(bool configure_session=true);
  void processParams(const QString& params);
  void processParams(const QStringList& params);
  void addTorrent(QString path);
  void addUnauthenticatedTracker(const QPair<QTorrentHandle,QString> &tracker);
  void processDownloadedFiles(QString path, QString url);
  void processNewMagnetLink(const QString& link);
  void finishedTorrent(const QTorrentHandle& h) const;
  void askRecursiveTorrentDownloadConfirmation(const QTorrentHandle &h);
  // Options slots
  void on_actionOptions_triggered();
  void optionsSaved();
  // HTTP slots
  void on_actionDownload_from_URL_triggered();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  void handleUpdateCheckFinished(bool update_available, QString new_version, bool invokedByUser);
#endif

protected:
  void closeEvent(QCloseEvent *);
  void showEvent(QShowEvent *);
  bool event(QEvent * event);
  void displayRSSTab(bool enable);
  void displaySearchTab(bool enable);

private:
  QIcon getSystrayIcon() const;

private:
  QFileSystemWatcher *executable_watcher;
  // Bittorrent
  QList<QPair<QTorrentHandle,QString> > unauthenticated_trackers; // Still needed?
  // GUI related
  bool m_posInitialized;
  QTimer *guiUpdater;
  HidableTabWidget *tabs;
  StatusBar *status_bar;
  QPointer<options_imp> options;
  QPointer<consoleDlg> console;
  QPointer<about> aboutDlg;
  QPointer<StatsDialog> statsDlg;
  QPointer<TorrentCreatorDlg> createTorrentDlg;
  QPointer<downloadFromURL> downloadFromURLDialog;
  QPointer<QSystemTrayIcon> systrayIcon;
  QPointer<QTimer> systrayCreator;
  QPointer<QMenu> myTrayIconMenu;
  TransferListWidget *transferList;
  TransferListFiltersWidget *transferListFilters;
  PropertiesWidget *properties;
  bool displaySpeedInTitle;
  bool force_exit;
  bool ui_locked;
  LineEdit *search_filter;
  // Keyboard shortcuts
  QShortcut *switchSearchShortcut;
  QShortcut *switchSearchShortcut2;
  QShortcut *switchTransferShortcut;
  QShortcut *switchRSSShortcut;
  // Widgets
  QAction *prioSeparator;
  QAction *prioSeparatorMenu;
  QSplitter *hSplitter;
  QSplitter *vSplitter;
  // Search
  QPointer<SearchEngine> searchEngine;
  // RSS
  QPointer<RSSImp> rssWidget;
  // Execution Log
  QPointer<ExecutionLog> m_executionLog;
  // Power Management
  PowerManagement *m_pwr;
  QTimer *preventTimer;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  QTimer programUpdateTimer;
#endif

private slots:
    void on_actionSearch_engine_triggered();
    void on_actionRSS_Reader_triggered();
    void on_actionSpeed_in_title_bar_triggered();
    void on_actionTop_tool_bar_triggered();
    void on_action_Import_Torrent_triggered();
    void on_actionDonate_money_triggered();
    void on_actionExecution_Logs_triggered(bool checked);
    void on_actionAutoExit_qBittorrent_toggled(bool );
    void on_actionAutoSuspend_system_toggled(bool );
    void on_actionAutoShutdown_system_toggled(bool );
    // Check for active torrents and set preventing from suspend state
    void checkForActiveTorrents();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    void checkProgramUpdate();
#endif
};

#endif
