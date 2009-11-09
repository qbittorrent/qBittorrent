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
#include "ui_MainWindow.h"
#include "qtorrenthandle.h"

class bittorrent;
class createtorrent;
class QTimer;
class downloadFromURL;
class SearchEngine;
class QLocalServer;
class QLocalSocket;
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
class TransferListWidget;
class TransferListFiltersWidget;
class QSplitter;
class PropertiesWidget;
class QVBoxLayout;

class GUI : public QMainWindow, private Ui::MainWindow{
  Q_OBJECT

  private:
    // Bittorrent
    bittorrent *BTSession;
    QTimer *checkConnect;
    QTimer *scrapeTimer;
    QList<QPair<QTorrentHandle,QString> > unauthenticated_trackers;
    // GUI related
    QTabWidget *tabs;
    QPointer<options_imp> options;
    QSystemTrayIcon *myTrayIcon;
    QPointer<QTimer> systrayCreator;
    QMenu *myTrayIconMenu;
    TransferListWidget *transferList;
    TransferListFiltersWidget *transferListFilters;
    PropertiesWidget *properties;
    QVBoxLayout *hSplitter;
    QWidget *rightPanel;
    QSplitter *vSplitter;
    QLabel *connecStatusLblIcon;
    bool systrayIntegration;
    bool displaySpeedInTitle;
    bool force_exit;
    //unsigned int refreshInterval;
    QLabel *dlSpeedLbl;
    QLabel *upSpeedLbl;
    QLabel *ratioLbl;
    QLabel *DHTLbl;
    QFrame *statusSep1;
    QFrame *statusSep2;
    QFrame *statusSep3;
    QFrame *statusSep4;
    // Keyboard shortcuts
    QShortcut *switchSearchShortcut;
    QShortcut *switchSearchShortcut2;
    QShortcut *switchDownShortcut;
    QShortcut *switchUpShortcut;
    QShortcut *switchRSSShortcut;
    QAction *prioSeparator;
    QAction *prioSeparator2;
    // Search
    SearchEngine *searchEngine;
    // RSS
    RSSImp *rssWidget;
    // Web UI
    QPointer<HttpServer> httpServer;
    // Misc
    QLocalServer *localServer;
    QLocalSocket *clientConnection;

  protected slots:
    // GUI related slots
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void toggleVisibility(QSystemTrayIcon::ActivationReason e);
    void on_actionAbout_triggered();
    void on_actionCreate_torrent_triggered();
    void on_actionWebsite_triggered() const;
    void on_actionBugReport_triggered() const;
    void on_actionShow_console_triggered();
    void readParamsOnSocket();
    void acceptConnection();
    void previewFile(QString filePath);
    void balloonClicked();
    void writeSettings();
    void readSettings();
    void on_actionExit_triggered();
    void createTrayIcon();
    void fullDiskError(QTorrentHandle& h, QString msg) const;
    void handleDownloadFromUrlFailure(QString, QString) const;
    void createSystrayDelayed();
    // Keyboard shortcuts
    void createKeyboardShortcuts();
    void displayDownTab() const;
    void displayUpTab() const;
    void displaySearchTab() const;
    void displayRSSTab() const;
    // Torrent actions
    void on_actionSet_global_upload_limit_triggered();
    void on_actionSet_global_download_limit_triggered();
    void on_actionDocumentation_triggered() const;
    void on_actionOpen_triggered();
    void checkConnectionStatus();
    void configureSession(bool deleteOptions);
    void processParams(const QStringList& params);
    void addTorrent(QString path);
    void addUnauthenticatedTracker(QPair<QTorrentHandle,QString> tracker);
    void processDownloadedFiles(QString path, QString url);
    void downloadFromURLList(const QStringList& urls);
    void finishedTorrent(QTorrentHandle& h) const;
    //void updateLists(bool force=false);
    bool initWebUi(QString username, QString password, int port);
    void scrapeTrackers();
    // Options slots
    void on_actionOptions_triggered();
    void OptionsSaved(bool deleteOptions);
    // HTTP slots
    void on_actionDownload_from_URL_triggered();


  public slots:
    void trackerAuthenticationRequired(QTorrentHandle& h);
    void setTabText(int index, QString text) const;
    void updateRatio();
    void showNotificationBaloon(QString title, QString msg) const;

  protected:
    void closeEvent(QCloseEvent *);
    void showEvent(QShowEvent *);
    bool event(QEvent * event);
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
