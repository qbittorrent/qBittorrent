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

#ifndef PROPERTIESWIDGET_H
#define PROPERTIESWIDGET_H

#include <QWidget>
#include "ui_propertieswidget.h"
#include "qtorrenthandle.h"


class TransferListWidget;
class QTimer;
class Bittorrent;
class TorrentFilesModel;
class PropListDelegate;
class QAction;
class torrent_file;
class PeerListWidget;
class TrackerList;
class GUI;
class DownloadedPiecesBar;
class PieceAvailabilityBar;

enum Tab {MAIN_TAB, TRACKERS_TAB, PEERS_TAB, URLSEEDS_TAB, FILES_TAB};
enum SlideState {REDUCED, VISIBLE};

class PropertiesWidget : public QWidget, private Ui::PropertiesWidget {
  Q_OBJECT

private:
  TransferListWidget *transferList;
  GUI *main_window;
  QTorrentHandle h;
  QTimer *refreshTimer;
  Bittorrent* BTSession;
  SlideState state;
  TorrentFilesModel *PropListModel;
  PropListDelegate *PropDelegate;
  PeerListWidget *peersList;
  TrackerList *trackerList;
  QList<int> slideSizes;
  DownloadedPiecesBar *downloaded_pieces;
  PieceAvailabilityBar *pieces_availability;

public:
  PropertiesWidget(QWidget *parent, GUI* main_window, TransferListWidget *transferList, Bittorrent* BTSession);
  ~PropertiesWidget();
  QTorrentHandle getCurrentTorrent() const;
  Bittorrent* getBTSession() const;
  TrackerList* getTrackerList() const { return trackerList; }
  PeerListWidget* getPeerList() const { return peersList; }
  QTreeView* getFilesList() const { return filesList; }

protected:
  QPushButton* getButtonFromIndex(int index);
  bool applyPriorities();

protected slots:
  void loadTorrentInfos(QTorrentHandle &h);
  void updateTorrentInfos(QTorrentHandle &h);
  void loadUrlSeeds();
  void on_main_infos_button_clicked();
  void on_trackers_button_clicked();
  void on_peers_button_clicked();
  void on_url_seeds_button_clicked();
  void on_files_button_clicked();
  void askWebSeed();
  void deleteSelectedUrlSeeds();
  void displayFilesListMenu(const QPoint& pos);
  void on_changeSavePathButton_clicked();
  void filteredFilesChanged();
  void showPiecesDownloaded(bool show);
  void showPiecesAvailability(bool show);
  void updateSavePath(QTorrentHandle& h);
  void renameSelectedFile();

public slots:
  void loadDynamicData();
  void reduce();
  void slide();
  void clear();
  void readSettings();
  void saveSettings();
  void reloadPreferences();
  void openDoubleClickedFile(QModelIndex);

};

#endif // PROPERTIESWIDGET_H
