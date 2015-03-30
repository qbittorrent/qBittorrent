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

#include <QTreeWidgetItem>
#include <QStringList>
#include <QMenu>
#include <QHash>
#include <QAction>
#include <QColor>
#include <QDebug>
#include <QUrl>
#include <QMessageBox>

#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/bittorrent/peerinfo.h"
#include "core/bittorrent/trackerentry.h"
#include "core/preferences.h"
#include "core/utils/misc.h"
#include "propertieswidget.h"
#include "trackersadditiondlg.h"
#include "guiiconprovider.h"
#include "autoexpandabledialog.h"
#include "trackerlist.h"

TrackerList::TrackerList(PropertiesWidget *properties): QTreeWidget(), properties(properties) {
  // Graphical settings
  setRootIsDecorated(false);
  setAllColumnsShowFocus(true);
  setItemsExpandable(false);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  // Context menu
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTrackerListMenu(QPoint)));
  // Set header
  QStringList header;
  header << "#";
  header << tr("URL");
  header << tr("Status");
  header << tr("Peers");
  header << tr("Message");
  setHeaderItem(new QTreeWidgetItem(header));
  dht_item = new QTreeWidgetItem(QStringList() << "" << "** [DHT] **");
  insertTopLevelItem(0, dht_item);
  setRowColor(0, QColor("grey"));
  pex_item = new QTreeWidgetItem(QStringList() << "" << "** [PeX] **");
  insertTopLevelItem(1, pex_item);
  setRowColor(1, QColor("grey"));
  lsd_item = new QTreeWidgetItem(QStringList() << "" << "** [LSD] **");
  insertTopLevelItem(2, lsd_item);
  setRowColor(2, QColor("grey"));
  editHotkey = new QShortcut(QKeySequence("F2"), this, SLOT(editSelectedTracker()), 0, Qt::WidgetShortcut);
  connect(this, SIGNAL(doubleClicked(QModelIndex)), SLOT(editSelectedTracker()));
  deleteHotkey = new QShortcut(QKeySequence(QKeySequence::Delete), this, SLOT(deleteSelectedTrackers()), 0, Qt::WidgetShortcut);

  loadSettings();
}

TrackerList::~TrackerList() {
  delete editHotkey;
  delete deleteHotkey;
  saveSettings();
}

QList<QTreeWidgetItem*> TrackerList::getSelectedTrackerItems() const {
  const QList<QTreeWidgetItem*> selected_items = selectedItems();
  QList<QTreeWidgetItem*> selected_trackers;
  foreach (QTreeWidgetItem *item, selected_items) {
    if (indexOfTopLevelItem(item) >= NB_STICKY_ITEM) { // Ignore STICKY ITEMS
      selected_trackers << item;
    }
  }
  return selected_trackers;
}

void TrackerList::setRowColor(int row, QColor color) {
  unsigned int nbColumns = columnCount();
  QTreeWidgetItem *item = topLevelItem(row);
  for (unsigned int i=0; i<nbColumns; ++i) {
    item->setData(i, Qt::ForegroundRole, color);
  }
}

void TrackerList::moveSelectionUp() {
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) {
    clear();
    return;
  }
  QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
  if (selected_items.isEmpty()) return;
  bool change = false;
  foreach (QTreeWidgetItem *item, selected_items) {
    int index = indexOfTopLevelItem(item);
    if (index > NB_STICKY_ITEM) {
      insertTopLevelItem(index-1, takeTopLevelItem(index));
      change = true;
    }
  }
  if (!change) return;
  // Restore selection
  QItemSelectionModel *selection = selectionModel();
  foreach (QTreeWidgetItem *item, selected_items) {
    selection->select(indexFromItem(item), QItemSelectionModel::Rows|QItemSelectionModel::Select);
  }
  setSelectionModel(selection);
  // Update torrent trackers
  QList<BitTorrent::TrackerEntry> trackers;
  for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
    QString tracker_url = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
    BitTorrent::TrackerEntry e(tracker_url);
    e.setTier(i - NB_STICKY_ITEM);
    trackers.append(e);
  }

  torrent->replaceTrackers(trackers);
  // Reannounce
  if (!torrent->isPaused())
    torrent->forceReannounce();
}

void TrackerList::moveSelectionDown() {
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) {
    clear();
    return;
  }
  QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
  if (selected_items.isEmpty()) return;
  bool change = false;
  for (int i=selectedItems().size()-1; i>= 0; --i) {
    int index = indexOfTopLevelItem(selected_items.at(i));
    if (index < topLevelItemCount()-1) {
      insertTopLevelItem(index+1, takeTopLevelItem(index));
      change = true;
    }
  }
  if (!change) return;
  // Restore selection
  QItemSelectionModel *selection = selectionModel();
  foreach (QTreeWidgetItem *item, selected_items) {
    selection->select(indexFromItem(item), QItemSelectionModel::Rows|QItemSelectionModel::Select);
  }
  setSelectionModel(selection);
  // Update torrent trackers
  QList<BitTorrent::TrackerEntry> trackers;
  for (int i = NB_STICKY_ITEM; i < topLevelItemCount(); ++i) {
    QString tracker_url = topLevelItem(i)->data(COL_URL, Qt::DisplayRole).toString();
    BitTorrent::TrackerEntry e(tracker_url);
    e.setTier(i - NB_STICKY_ITEM);
    trackers.append(e);
  }

  torrent->replaceTrackers(trackers);
  // Reannounce
  if (!torrent->isPaused())
    torrent->forceReannounce();
}

void TrackerList::clear() {
  qDeleteAll(tracker_items.values());
  tracker_items.clear();
  dht_item->setText(COL_PEERS, "");
  dht_item->setText(COL_STATUS, "");
  dht_item->setText(COL_MSG, "");
  pex_item->setText(COL_PEERS, "");
  pex_item->setText(COL_STATUS, "");
  pex_item->setText(COL_MSG, "");
  lsd_item->setText(COL_PEERS, "");
  lsd_item->setText(COL_STATUS, "");
  lsd_item->setText(COL_MSG, "");
}

void TrackerList::loadStickyItems(BitTorrent::TorrentHandle *const torrent) {
  QString working = tr("Working");
  QString disabled = tr("Disabled");

  // load DHT information
  if (BitTorrent::Session::instance()->isDHTEnabled() && !torrent->isPrivate())
    dht_item->setText(COL_STATUS, working);
  else
    dht_item->setText(COL_STATUS, disabled);

  // Load PeX Information
  if (BitTorrent::Session::instance()->isPexEnabled() && !torrent->isPrivate())
    pex_item->setText(COL_STATUS, working);
  else
    pex_item->setText(COL_STATUS, disabled);

  // Load LSD Information
  if (BitTorrent::Session::instance()->isLSDEnabled() && !torrent->isPrivate())
    lsd_item->setText(COL_STATUS, working);
  else
    lsd_item->setText(COL_STATUS, disabled);

  if (torrent->isPrivate()) {
    QString privateMsg = tr("This torrent is private");
    dht_item->setText(COL_MSG, privateMsg);
    pex_item->setText(COL_MSG, privateMsg);
    lsd_item->setText(COL_MSG, privateMsg);
  }

  // XXX: libtorrent should provide this info...
  // Count peers from DHT, LSD, PeX
  uint nb_dht = 0, nb_lsd = 0, nb_pex = 0;
  foreach (const BitTorrent::PeerInfo &peer, torrent->peers()) {
    if (peer.fromDHT())
      ++nb_dht;
    if (peer.fromLSD())
      ++nb_lsd;
    if (peer.fromPeX())
      ++nb_pex;
  }
  dht_item->setText(COL_PEERS, QString::number(nb_dht));
  pex_item->setText(COL_PEERS, QString::number(nb_pex));
  lsd_item->setText(COL_PEERS, QString::number(nb_lsd));
}

void TrackerList::loadTrackers() {
  // Load trackers from torrent handle
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) return;

  loadStickyItems(torrent);
  // Load actual trackers information
  QHash<QString, BitTorrent::TrackerInfo> trackers_data = torrent->trackerInfos();
  QStringList old_trackers_urls = tracker_items.keys();
  foreach (const BitTorrent::TrackerEntry &entry, torrent->trackers()) {
    QString trackerUrl = entry.url();
    QTreeWidgetItem *item = tracker_items.value(trackerUrl, 0);
    if (!item) {
      item = new QTreeWidgetItem();
      item->setText(COL_URL, trackerUrl);
      addTopLevelItem(item);
      tracker_items[trackerUrl] = item;
    } else {
      old_trackers_urls.removeOne(trackerUrl);
    }
    item->setText(COL_TIER, QString::number(entry.tier()));
    BitTorrent::TrackerInfo data = trackers_data.value(trackerUrl);
    QString error_message = data.lastMessage.trimmed();
    switch (entry.status()) {
    case BitTorrent::TrackerEntry::Working:
        item->setText(COL_STATUS, tr("Working"));
        item->setText(COL_MSG, "");
        break;
    case BitTorrent::TrackerEntry::Updating:
        item->setText(COL_STATUS, tr("Updating..."));
        item->setText(COL_MSG, "");
        break;
    case BitTorrent::TrackerEntry::NotWorking:
        item->setText(COL_STATUS, tr("Not working"));
        item->setText(COL_MSG, error_message);
        break;
    case BitTorrent::TrackerEntry::NotContacted:
        item->setText(COL_STATUS, tr("Not contacted yet"));
        item->setText(COL_MSG, "");
        break;
    }

    item->setText(COL_PEERS, QString::number(trackers_data.value(trackerUrl).numPeers));
  }
  // Remove old trackers
  foreach (const QString &tracker, old_trackers_urls) {
    delete tracker_items.take(tracker);
  }
}

// Ask the user for new trackers and add them to the torrent
void TrackerList::askForTrackers() {
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) return;

  QList<BitTorrent::TrackerEntry> trackers;
  foreach (const QString &tracker, TrackersAdditionDlg::askForTrackers(torrent))
      trackers << tracker;
  torrent->addTrackers(trackers);
}

void TrackerList::copyTrackerUrl() {
    QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
    if (selected_items.isEmpty()) return;
    QStringList urls_to_copy;
    foreach (QTreeWidgetItem *item, selected_items) {
      QString tracker_url = item->data(COL_URL, Qt::DisplayRole).toString();
      qDebug() << QString("Copy: ") + tracker_url;
      urls_to_copy << tracker_url;
    }
    QApplication::clipboard()->setText(urls_to_copy.join("\n"));
}


void TrackerList::deleteSelectedTrackers() {
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) {
    clear();
    return;
  }

  QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
  if (selected_items.isEmpty()) return;

  QStringList urls_to_remove;
  foreach (QTreeWidgetItem *item, selected_items) {
    QString tracker_url = item->data(COL_URL, Qt::DisplayRole).toString();
    urls_to_remove << tracker_url;
    tracker_items.remove(tracker_url);
    delete item;
  }

  // Iterate of trackers and remove selected ones
  QList<BitTorrent::TrackerEntry> remaining_trackers;
  QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
  foreach (const BitTorrent::TrackerEntry &entry, trackers) {
    if (!urls_to_remove.contains(entry.url())) {
      remaining_trackers.push_back(entry);
    }
  }

  torrent->replaceTrackers(remaining_trackers);
  if (!torrent->isPaused())
    torrent->forceReannounce();
}

void TrackerList::editSelectedTracker() {
    BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
    if (!torrent) return;

    QString hash = torrent->hash();

    QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
    if (selected_items.isEmpty())
      return;
    // During multi-select only process item selected last
    QUrl tracker_url = selected_items.last()->text(COL_URL);

    bool ok;
    QUrl new_tracker_url = AutoExpandableDialog::getText(this, tr("Tracker editing"), tr("Tracker URL:"),
                                                         QLineEdit::Normal, tracker_url.toString(), &ok).trimmed();
    if (!ok)
      return;

    if (!new_tracker_url.isValid()) {
      QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL entered is invalid."));
      return;
    }
    if (new_tracker_url == tracker_url)
      return;

    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    bool match = false;
    for (int i = 0; i < trackers.size(); ++i) {
      BitTorrent::TrackerEntry &entry = trackers[i];
      if (new_tracker_url == QUrl(entry.url())) {
        QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL already exists."));
        return;
      }

      if (tracker_url == QUrl(entry.url()) && !match) {
        BitTorrent::TrackerEntry new_entry(new_tracker_url.toString());
        new_entry.setTier(entry.tier());
        match = true;
        entry = new_entry;
      }
    }

    torrent->replaceTrackers(trackers);
    if (!torrent->isPaused()) {
      torrent->forceReannounce();
      torrent->forceDHTAnnounce();
    }
}

#if LIBTORRENT_VERSION_NUM >= 10000
void TrackerList::reannounceSelected() {
    BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
    if (!torrent) return;

    QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
    if (selected_items.isEmpty()) return;

    QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    for (int i = 0; i < trackers.size(); ++i) {
      foreach (QTreeWidgetItem* w, selected_items) {
        if (w->text(COL_URL) == trackers[i].url()) {
          torrent->forceReannounce(i);
          break;
        }
      }
    }

    loadTrackers();
}

#endif

void TrackerList::showTrackerListMenu(QPoint) {
  BitTorrent::TorrentHandle *const torrent = properties->getCurrentTorrent();
  if (!torrent) return;
  //QList<QTreeWidgetItem*> selected_items = getSelectedTrackerItems();
  QMenu menu;
  // Add actions
  QAction *addAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-add"), tr("Add a new tracker..."));
  QAction *copyAct = 0;
  QAction *delAct = 0;
  QAction *editAct = 0;
  if (!getSelectedTrackerItems().isEmpty()) {
    delAct = menu.addAction(GuiIconProvider::instance()->getIcon("list-remove"), tr("Remove tracker"));
    copyAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy tracker URL"));
    editAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-rename"),tr("Edit selected tracker URL"));
  }
#if LIBTORRENT_VERSION_NUM >= 10000
  QAction *reannounceSelAct = NULL;
#endif
  QAction *reannounceAct = NULL;
  if (!torrent->isPaused()) {
#if LIBTORRENT_VERSION_NUM >= 10000
    reannounceSelAct = menu.addAction(GuiIconProvider::instance()->getIcon("view-refresh"), tr("Force reannounce to selected trackers"));
#endif
    menu.addSeparator();
    reannounceAct = menu.addAction(GuiIconProvider::instance()->getIcon("view-refresh"), tr("Force reannounce to all trackers"));
  }
  QAction *act = menu.exec(QCursor::pos());
  if (act == 0) return;
  if (act == addAct) {
    askForTrackers();
    return;
  }
  if (act == copyAct) {
    copyTrackerUrl();
    return;
  }
  if (act == delAct) {
    deleteSelectedTrackers();
    return;
  }
#if LIBTORRENT_VERSION_NUM >= 10000
  if (act == reannounceSelAct) {
    reannounceSelected();
    return;
  }
#endif
  if (act == reannounceAct) {
    properties->getCurrentTorrent()->forceReannounce();
    return;
  }
  if (act == editAct) {
    editSelectedTracker();
    return;
  }
}

void TrackerList::loadSettings() {
  if (!header()->restoreState(Preferences::instance()->getPropTrackerListState())) {
    setColumnWidth(0, 30);
    setColumnWidth(1, 300);
  }
}

void TrackerList::saveSettings() const {
  Preferences::instance()->setPropTrackerListState(header()->saveState());
}
