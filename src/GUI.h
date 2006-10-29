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

#include <QMainWindow>
#include <QMap>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCloseEvent>
#include <QSystemTrayIcon>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/fingerprint.hpp>
#include <libtorrent/session_settings.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>

#include "ui_MainWindow.h"
#include "options_imp.h"
#include "about_imp.h"
#include "previewSelect.h"
#include "trackerLogin.h"

#define TIME_TRAY_BALLOON 5000

class createtorrent;
class QTimer;
class DLListDelegate;
class SearchListDelegate;
class downloadThread;
class downloadFromURL;

using namespace libtorrent;
namespace fs = boost::filesystem;

class GUI : public QMainWindow, private Ui::MainWindow{
  Q_OBJECT

  private:
    // Bittorrent
    session *s;
    std::pair<unsigned short, unsigned short> listenPorts;
    QMap<QString, torrent_handle> handles;
    QTimer *checkConnect;
    QTimer *timerScan;
    QMap<QString, QStringList> trackerErrors;
    trackerLogin *tracker_login;
    QList<QPair<torrent_handle,std::string> > unauthenticated_trackers;
    downloadThread *downloader;
    downloadFromURL *downloadFromURLDialog;
    bool DHTEnabled;
    // GUI related
    options_imp *options;
    createtorrent *createWindow;
    QTimer *refresher;
    QSystemTrayIcon *myTrayIcon;
    QMenu *myTrayIconMenu;
    about *aboutdlg;
    QStandardItemModel *DLListModel;
    DLListDelegate *DLDelegate;
    QStandardItemModel *SearchListModel;
    SearchListDelegate *SearchDelegate;
    QStringList supported_preview_extensions;
    unsigned int nbTorrents;
    QLabel *connecStatusLblIcon;
    // Preview
    previewSelect *previewSelection;
    QProcess *previewProcess;
    // Search related
    QMap<QString, QString> searchResultsUrls;
    QProcess *searchProcess;
    bool search_stopped;
    bool no_search_results;
    QByteArray search_result_line_truncated;
    unsigned long nb_search_results;
    QTcpServer *tcpServer;
    QTcpSocket *clientConnection;

  protected slots:
    // GUI related slots
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void centerWindow();
    void toggleVisibility(QSystemTrayIcon::ActivationReason e);
    void showAbout();
    void setInfoBar(const QString& info, const QString& color="black");
    void updateDlList();
    void showCreateWindow();
    void clearLog();
    void AnotherInstanceConnected();
    void readParamsInFile();
    void saveCheckedSearchEngines(int) const;
    void saveColWidthDLList() const;
    void saveColWidthSearchList() const;
    void loadCheckedSearchEngines();
    bool loadColWidthDLList();
    bool loadColWidthSearchList();
    void saveWindowSize() const;
    void loadWindowSize();
    void sortDownloadList(int index);
    void sortDownloadListFloat(int index, Qt::SortOrder sortOrder);
    void sortDownloadListString(int index, Qt::SortOrder sortOrder);
    void sortSearchList(int index);
    void sortSearchListInt(int index, Qt::SortOrder sortOrder);
    void sortSearchListString(int index, Qt::SortOrder sortOrder);
    void displayDLListMenu(const QPoint& pos);
    void selectGivenRow(const QModelIndex& index);
    void togglePausedState(const QModelIndex& index);
    void displayInfoBarMenu(const QPoint& pos);
    void displayGUIMenu(const QPoint& pos);
    void previewFileSelection();
    void previewFile(const QString& filePath);
    void cleanTempPreviewFile(int, QProcess::ExitStatus);
    void balloonClicked();
    // Torrent actions
    void showProperties(const QModelIndex &index);
    void propertiesSelection();
    void addTorrent(const QString& path, bool fromScanDir = false, const QString& from_url = QString());
    void pauseAll();
    void startAll();
    void pauseSelection();
    void startSelection();
    void askForTorrents();
    void deletePermanently();
    void deleteSelection();
    void resumeUnfinished();
    void saveFastResumeData() const;
    void checkConnectionStatus();
    void scanDirectory();
    void setGlobalRatio(float ratio);
    void configureSession();
    void processParams(const QStringList& params);
    void addUnauthenticatedTracker(QPair<torrent_handle,std::string> tracker);
    void processDownloadedFile(QString url, QString file_path, int return_code, QString errorBuffer);
    void downloadFromURLList(const QStringList& url_list);
    bool loadFilteredFiles(torrent_handle &h);
    bool hasFilteredFiles(const QString& fileName);
    void reloadTorrent(const torrent_handle &h, bool compact_mode = true);
    // Search slots
    void on_search_button_clicked();
    void on_stop_search_button_clicked();
    void on_clear_button_clicked();
    void on_download_button_clicked();
    void on_update_nova_button_clicked();
    void appendSearchResult(const QString& line);
    void searchFinished(int exitcode,QProcess::ExitStatus);
    void readSearchOutput();
    void searchStarted();
    void downloadSelectedItem(const QModelIndex& index);
    // Utils slots
    void setRowColor(int row, const QString& color, bool inDLList=true);
    // Options slots
    void showOptions() const;
    void OptionsSaved(const QString& info);
    // HTTP slots
    void downloadFromUrl(const QString& url);
    void askForTorrentUrl();

  public slots:
    void setLocale(QString locale);

  protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);

  public:
    // Construct / Destruct
    GUI(QWidget *parent=0, QStringList torrentCmdLine=QStringList());
    ~GUI();
    // Methods
    int getRowFromName(const QString& name) const;
    float getNovaVersion(const QString& novaPath) const;
    QByteArray getNovaChangelog(const QString& novaPath) const;
    void updateNova() const;
    bool isFilePreviewPossible(const torrent_handle& h) const;
    QString getSavePath(QString fileName);
};

#endif
