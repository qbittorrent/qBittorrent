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

#include <QShortcut>
#include <QWidget>
#include "ui_propertieswidget.h"
#include "core/bittorrent/torrenthandle.h"


class TransferListWidget;
class TorrentContentFilterModel;
class PropListDelegate;
class torrent_file;
class PeerListWidget;
class TrackerList;
class MainWindow;
class DownloadedPiecesBar;
class PieceAvailabilityBar;
class PropTabBar;
class LineEdit;

QT_BEGIN_NAMESPACE
class QAction;
class QTimer;
QT_END_NAMESPACE

class PropertiesWidget : public QWidget, private Ui::PropertiesWidget {
  Q_OBJECT
  Q_DISABLE_COPY(PropertiesWidget)

public:
  enum SlideState {REDUCED, VISIBLE};

public:
  PropertiesWidget(QWidget *parent, MainWindow* main_window, TransferListWidget *transferList);
  ~PropertiesWidget();
  BitTorrent::TorrentHandle *getCurrentTorrent() const;
  TrackerList* getTrackerList() const { return trackerList; }
  PeerListWidget* getPeerList() const { return peersList; }
  QTreeView* getFilesList() const { return filesList; }

protected:
  QPushButton* getButtonFromIndex(int index);
  bool applyPriorities();

protected slots:
  void loadTorrentInfos(BitTorrent::TorrentHandle *const torrent);
  void updateTorrentInfos(BitTorrent::TorrentHandle *const torrent);
  void loadUrlSeeds();
  void askWebSeed();
  void deleteSelectedUrlSeeds();
  void copySelectedWebSeedsToClipboard() const;
  void editWebSeed();
  void displayFilesListMenu(const QPoint& pos);
  void displayWebSeedListMenu(const QPoint& pos);
  void filteredFilesChanged();
  void showPiecesDownloaded(bool show);
  void showPiecesAvailability(bool show);
  void renameSelectedFile();
  void openSelectedFile();

public slots:
  void setVisibility(bool visible);
  void loadDynamicData();
  void clear();
  void readSettings();
  void saveSettings();
  void reloadPreferences();
  void openDoubleClickedFile(const QModelIndex &);
  void loadTrackers(BitTorrent::TorrentHandle *const torrent);

private:
  void openFile(const QModelIndex &index);
  void openFolder(const QModelIndex &index, bool containing_folder);

private:
  TransferListWidget *transferList;
  MainWindow *main_window;
  BitTorrent::TorrentHandle *m_torrent;
  QTimer *refreshTimer;
  SlideState state;
  TorrentContentFilterModel *PropListModel;
  PropListDelegate *PropDelegate;
  PeerListWidget *peersList;
  TrackerList *trackerList;
  QList<int> slideSizes;
  DownloadedPiecesBar *downloaded_pieces;
  PieceAvailabilityBar *pieces_availability;
  PropTabBar *m_tabBar;
  LineEdit *m_contentFilerLine;
  QShortcut *editHotkeyFile;
  QShortcut *editHotkeyWeb;
  QShortcut *deleteHotkeyWeb;
  QShortcut *openHotkeyFile;

private slots:
  void filterText(const QString& filter);
  void updateSavePath(BitTorrent::TorrentHandle *const torrent);
};

#endif // PROPERTIESWIDGET_H
