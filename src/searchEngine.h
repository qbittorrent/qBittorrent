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

#ifndef SEARCH_H
#define SEARCH_H

#define TIME_TRAY_BALLOON 5000

#include <QProcess>
#include "ui_search.h"

class QStandardItemModel;
class SearchListDelegate;
class bittorrent;
class QSystemTrayIcon;
class downloadThread;

class SearchEngine : public QWidget, public Ui::search_engine{
  Q_OBJECT

  private:
    // Search related
    QHash<QString, QString> searchResultsUrls;
    QProcess *searchProcess;
    bool search_stopped;
    bool no_search_results;
    QByteArray search_result_line_truncated;
    unsigned long nb_search_results;
    QCompleter *searchCompleter;
    QStringList searchHistory;
    QStandardItemModel *SearchListModel;
    SearchListDelegate *SearchDelegate;
    bittorrent *BTSession;
    QSystemTrayIcon *myTrayIcon;
    bool systrayIntegration;
    downloadThread *downloader;

  public:
    SearchEngine(bittorrent *BTSession, QSystemTrayIcon *myTrayIcon, bool systrayIntegration);
    ~SearchEngine();
    float getNovaVersion(const QString& novaPath) const;
    QByteArray getNovaChangelog(const QString& novaPath) const;
    bool loadColWidthSearchList();

  public slots:
    // Search slots
    void on_search_button_clicked();
    void on_stop_search_button_clicked();
    void on_clear_button_clicked();
    void on_download_button_clicked();
    void on_update_nova_button_clicked();
    void appendSearchResult(const QString& line);
    void searchFinished(int exitcode,QProcess::ExitStatus);
    void readSearchOutput();
    void setRowColor(int row, const QString& color);
    void searchStarted();
    void downloadSelectedItem(const QModelIndex& index);
    void startSearchHistory();
    void loadCheckedSearchEngines();
    void updateNova() const;
    void saveSearchHistory();
    void saveColWidthSearchList() const;
    void saveCheckedSearchEngines(int) const;
    void sortSearchList(int index);
    void sortSearchListInt(int index, Qt::SortOrder sortOrder);
    void sortSearchListString(int index, Qt::SortOrder sortOrder);
    void novaUpdateDownloaded(const QString& url, const QString& path);
};

#endif
