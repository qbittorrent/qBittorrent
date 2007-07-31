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
 * Contact : chris@qbittorrent.org
 */

#ifndef GUI_H
#define GUI_H

#include <QProcess>
#include <QSystemTrayIcon>
#include <libtorrent/torrent_handle.hpp>

#include "ui_MainWindow.h"

class bittorrent;
class createtorrent;
class QTimer;
class FinishedTorrents;
class DLListDelegate;
class downloadThread;
class downloadFromURL;
class SearchEngine;
class QTcpServer;
class QTcpSocket;
class QCloseEvent;
class RSSImp;
class QShortcut;
class about;
class previewSelect;
class options_imp;
class QStandardItemModel;

using namespace libtorrent;
namespace fs = boost::filesystem;

class GUI : public QMainWindow, private Ui::MainWindow{
  Q_OBJECT

  private:
    // Bittorrent
    bittorrent *BTSession;
    QTimer *checkConnect;
    QList<QPair<torrent_handle,std::string> > unauthenticated_trackers;
    downloadFromURL *downloadFromURLDialog;
    // GUI related
    options_imp *options;
    createtorrent *createWindow;
    QTimer *refresher;
    QSystemTrayIcon *myTrayIcon;
    QMenu *myTrayIconMenu;
    about *aboutdlg;
    QStandardItemModel *DLListModel;
    DLListDelegate *DLDelegate;
    FinishedTorrents *finishedTorrentTab;
    unsigned int nbTorrents;
    QLabel *connecStatusLblIcon;
    bool systrayIntegration;
    bool force_exit;
    bool delayedSorting;
    Qt::SortOrder delayedSortingOrder;
    // Keyboard shortcuts
    QShortcut *switchSearchShortcut;
    QShortcut *switchDownShortcut;
    QShortcut *switchUpShortcut;
    QShortcut *switchRSSShortcut;
    // Preview
    previewSelect *previewSelection;
    QProcess *previewProcess;
    // Search
    SearchEngine *searchEngine;
    // RSS
    RSSImp *rssWidget;
    // Misc
    QTcpServer *tcpServer;
    QTcpSocket *clientConnection;

  protected slots:
    // GUI related slots
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void toggleVisibility(QSystemTrayIcon::ActivationReason e);
    void on_actionAbout_triggered();
    void setInfoBar(QString info, QString color="black");
    void updateDlList(bool force=false);
    void on_actionCreate_torrent_triggered();
    void on_actionClearLog_triggered();
    void on_actionWebsite_triggered();
    void on_actionBugReport_triggered();
    void readParamsOnSocket();
    void acceptConnection();
    void saveColWidthDLList() const;
    bool loadColWidthDLList();
    void sortDownloadList(int index, Qt::SortOrder startSortOrder=Qt::AscendingOrder, bool fromLoadColWidth=false);
    void sortDownloadListFloat(int index, Qt::SortOrder sortOrder);
    void sortDownloadListString(int index, Qt::SortOrder sortOrder);
    void displayDLListMenu(const QPoint& pos);
    void togglePausedState(const QModelIndex& index);
    void displayInfoBarMenu(const QPoint& pos);
    void on_actionPreview_file_triggered();
    void previewFile(QString filePath);
    void cleanTempPreviewFile(int, QProcess::ExitStatus);
    void balloonClicked();
    void writeSettings();
    void readSettings();
    void on_actionExit_triggered();
    void createTrayIcon();
    void addLogPeerBlocked(QString);
    void addFastResumeRejectedAlert(QString);
    // Keyboard shortcuts
    void createKeyboardShortcuts();
    void displayDownTab();
    void displayUpTab();
    void displaySearchTab();
    void displayRSSTab();
    void handleDownloadFromUrlFailure(QString, QString) const;
    // Torrent actions
    void showProperties(const QModelIndex &index);
    void on_actionTorrent_Properties_triggered();
    void on_actionPause_triggered();
    void on_actionPause_All_triggered();
    void restoreInDownloadList(torrent_handle h);
    void on_actionStart_triggered();
    void on_actionStart_All_triggered();
    void on_actionOpen_triggered();
    void on_actionDelete_Permanently_triggered();
    void on_actionDelete_triggered();
    void on_actionSet_download_limit_triggered();
    void on_actionSet_upload_limit_triggered();
    void on_actionSet_global_upload_limit_triggered();
    void on_actionSet_global_download_limit_triggered();
    void on_actionDocumentation_triggered();
    void checkConnectionStatus();
    void configureSession(bool deleteOptions);
    void processParams(const QStringList& params);
    void addUnauthenticatedTracker(QPair<torrent_handle,std::string> tracker);
    void processScannedFiles(const QStringList& params);
    void processDownloadedFiles(QString path, QString url);
    void downloadFromURLList(const QStringList& urls);
    void displayDownloadingUrlInfos(QString url);
    // Utils slots
    void setRowColor(int row, QString color);
    // Options slots
    void on_actionOptions_triggered();
    void OptionsSaved(QString info, bool deleteOptions);
    // HTTP slots
    void on_actionDownload_from_URL_triggered();


  public slots:
    void torrentAdded(QString path, torrent_handle& h, bool fastResume);
    void torrentDuplicate(QString path);
    void torrentCorrupted(QString path);
    void finishedTorrent(torrent_handle& h);
    void fullDiskError(torrent_handle& h);
    void portListeningFailure();
    void trackerAuthenticationRequired(torrent_handle& h);
    void setTabText(int index, QString text);
    void updateFileSizeAndProgress(QString hash);
    void sortProgressColumnDelayed();
    void addUrlSeedError(QString url, QString msg);

  protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);

  public:
    // Construct / Destruct
    GUI(QWidget *parent=0, QStringList torrentCmdLine=QStringList());
    ~GUI();
    // Methods
    int getRowFromHash(QString hash) const;
    unsigned int getCurrentTabIndex() const;
    QPoint screenCenter() const;
};

#endif
