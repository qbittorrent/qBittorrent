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

#include "ui_MainWindow.h"
#include "qtorrenthandle.h"

class bittorrent;
class createtorrent;
class QTimer;
class DownloadingTorrents;
class FinishedTorrents;
class downloadFromURL;
class SearchEngine;
#ifdef QT_4_4
  class QLocalServer;
  class QLocalSocket;
#else
  class QTcpServer;
  class QTcpSocket;
#endif
class QCloseEvent;
class RSSImp;
class QShortcut;
class about;
class previewSelect;
class options_imp;
class QTabWidget;
class QLabel;
class QModelIndex;
class HttpServer;
class QFrame;

class GUI : public QMainWindow, private Ui::MainWindow{
  Q_OBJECT

  private:
    // Bittorrent
    bittorrent *BTSession;
    QTimer *checkConnect;
    QList<QPair<QTorrentHandle,QString> > unauthenticated_trackers;
    // GUI related
    QTabWidget *tabs;
    options_imp *options;
    QSystemTrayIcon *myTrayIcon;
    QTimer *systrayCreator;
    QMenu *myTrayIconMenu;
    DownloadingTorrents *downloadingTorrentTab;
    FinishedTorrents *finishedTorrentTab;
    QLabel *connecStatusLblIcon;
    bool systrayIntegration;
    bool displaySpeedInTitle;
    bool force_exit;
    unsigned int refreshInterval;
    QTimer *refresher;
    QLabel *dlSpeedLbl;
    QLabel *upSpeedLbl;
    QLabel *ratioLbl;
    QLabel *DHTLbl;
    QFrame *statusSep1;
    QFrame *statusSep2;
    QFrame *statusSep3;
    // Keyboard shortcuts
    QShortcut *switchSearchShortcut;
    QShortcut *switchSearchShortcut2;
    QShortcut *switchDownShortcut;
    QShortcut *switchUpShortcut;
    QShortcut *switchRSSShortcut;
    // Search
    SearchEngine *searchEngine;
    // RSS
    RSSImp *rssWidget;
    // Web UI
    HttpServer *httpServer;
    // Misc
#ifdef QT_4_4
    QLocalServer *localServer;
    QLocalSocket *clientConnection;
#else
    QTcpServer *localServer;
    QTcpSocket *clientConnection;
#endif

  protected slots:
    // GUI related slots
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void toggleVisibility(QSystemTrayIcon::ActivationReason e);
    void on_actionAbout_triggered();
    void on_actionCreate_torrent_triggered();
    void on_actionWebsite_triggered() const;
    void on_actionBugReport_triggered() const;
    void readParamsOnSocket();
    void acceptConnection();
    void togglePausedState(QString hash);
    void torrentDoubleClicked(QString hash, bool finished);
    void on_actionPreview_file_triggered();
    void previewFile(QString filePath);
    void balloonClicked();
    void writeSettings();
    void readSettings();
    void on_actionExit_triggered();
    void createTrayIcon();
    void updateUnfinishedTorrentNumberCalc();
    void updateFinishedTorrentNumberCalc();
    void updateUnfinishedTorrentNumber(unsigned int nb);
    void updateFinishedTorrentNumber(unsigned int nb);
    void fullDiskError(QTorrentHandle& h) const;
    void handleDownloadFromUrlFailure(QString, QString) const;
    void createSystrayDelayed();
    // Keyboard shortcuts
    void createKeyboardShortcuts();
    void displayDownTab() const;
    void displayUpTab() const;
    void displaySearchTab() const;
    void displayRSSTab() const;
    // Torrent actions
    void on_actionTorrent_Properties_triggered();
    void on_actionPause_triggered();
    void on_actionPause_All_triggered();
    void on_actionStart_triggered();
    void on_actionStart_All_triggered();
    void on_actionOpen_triggered();
    void on_actionDelete_Permanently_triggered();
    void on_actionDelete_triggered();
    void on_actionSet_global_upload_limit_triggered();
    void on_actionSet_global_download_limit_triggered();
    void on_actionDocumentation_triggered() const;
    void checkConnectionStatus();
    void configureSession(bool deleteOptions);
    void processParams(const QStringList& params);
    void addTorrent(QString path);
    void addUnauthenticatedTracker(QPair<QTorrentHandle,QString> tracker);
    void processScannedFiles(const QStringList& params);
    void processDownloadedFiles(QString path, QString url);
    void downloadFromURLList(const QStringList& urls);
    void deleteTorrent(QString hash);
    void deleteRatioTorrent(QString fileName);
    void finishedTorrent(QTorrentHandle& h) const;
    void torrentChecked(QString hash) const;
    void updateLists();
    bool initWebUi(QString username, QString password, int port);
    void pauseTorrent(QString hash);
    void on_actionIncreasePriority_triggered();
    void on_actionDecreasePriority_triggered();
    // Options slots
    void on_actionOptions_triggered();
    void OptionsSaved(QString info, bool deleteOptions);
    // HTTP slots
    void on_actionDownload_from_URL_triggered();


  public slots:
    void trackerAuthenticationRequired(QTorrentHandle& h);
    void setTabText(int index, QString text) const;
    void openDestinationFolder() const;
    void goBuyPage() const;
    void updateRatio();

  protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void displayRSSTab(bool enable);

  public:
    // Construct / Destruct
    GUI(QWidget *parent=0, QStringList torrentCmdLine=QStringList());
    ~GUI();
    // Methods
    int getCurrentTabIndex() const;
    QPoint screenCenter() const;
};

#endif
