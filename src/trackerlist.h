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

#ifndef TRACKERLIST_H
#define TRACKERLIST_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStringList>
#include <QMenu>
#include <QSettings>
#include <QHash>
#include <QAction>
#include <QColor>
#include "propertieswidget.h"
#include "trackersadditiondlg.h"
#include "misc.h"
#include "bittorrent.h"

enum TrackerListColumn {COL_URL, COL_STATUS, COL_PEERS, COL_MSG};
#define NB_STICKY_ITEM 1

class TrackerList: public QTreeWidget {
  Q_OBJECT

private:
  PropertiesWidget *properties;
  QHash<QString, QTreeWidgetItem*> tracker_items;
  QTreeWidgetItem* dht_item;

public:
  TrackerList(PropertiesWidget *properties): QTreeWidget(), properties(properties) {
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
    header << tr("URL");
    header << tr("Status");
    header << tr("Peers");
    header << tr("Message");
    setHeaderItem(new QTreeWidgetItem(header));
    dht_item = new QTreeWidgetItem(QStringList(tr("[DHT]")));
    insertTopLevelItem(0, dht_item);
    setRowColor(0, QColor("grey"));
    loadSettings();
  }

  ~TrackerList() {
    saveSettings();
  }

protected:
  QList<QTreeWidgetItem*> getSelectedTrackerItems() const {
    QList<QTreeWidgetItem*> selected_items = selectedItems();
    QList<QTreeWidgetItem*> selected_trackers;
    foreach(QTreeWidgetItem *item, selectedItems()) {
      if(indexOfTopLevelItem(item) >= NB_STICKY_ITEM) { // Ignore STICKY ITEMS
        selected_trackers << item;
      }
    }
    return selected_trackers;
  }

public slots:

  void setRowColor(int row, QColor color) {
    unsigned int nbColumns = columnCount();
    QTreeWidgetItem *item = topLevelItem(row);
    for(unsigned int i=0; i<nbColumns; ++i) {
      item->setData(i, Qt::ForegroundRole, color);
    }
  }

  void clear() {
    qDeleteAll(tracker_items.values());
    tracker_items.clear();
    dht_item->setText(COL_PEERS, "");
    dht_item->setText(COL_STATUS, "");
    dht_item->setText(COL_MSG, "");
  }

  void loadStickyItems(const QTorrentHandle &h, QHash<QString, TrackerInfos> trackers_data) {
    // load DHT information
    if(properties->getBTSession()->isDHTEnabled() && !h.priv()) {
      dht_item->setText(COL_STATUS, tr("Working"));
    } else {
      dht_item->setText(COL_STATUS, tr("Disabled"));
    }
    dht_item->setText(COL_PEERS, QString::number(trackers_data.value("dht", TrackerInfos("dht")).num_peers));
    if(h.priv()) {
      dht_item->setText(COL_MSG, tr("This torrent is private"));
    }
  }

  void loadTrackers() {
    // Load trackers from torrent handle
    QTorrentHandle h = properties->getCurrentTorrent();
    if(!h.is_valid()) return;
    QHash<QString, TrackerInfos> trackers_data = properties->getBTSession()->getTrackersInfo(h.hash());
    loadStickyItems(h, trackers_data);
    // Load actual trackers information
    QStringList old_trackers_urls = tracker_items.keys();
    std::vector<announce_entry> trackers = h.trackers();
    std::vector<announce_entry>::iterator it;
    for(it = trackers.begin(); it != trackers.end(); it++) {
      QStringList item_list;
      QString tracker_url = misc::toQString((*it).url);
      QTreeWidgetItem *item = tracker_items.value(tracker_url, 0);
      if(!item) {
        item = new QTreeWidgetItem();
        item->setText(COL_URL, tracker_url);
        addTopLevelItem(item);
        tracker_items[tracker_url] = item;
      } else {
        old_trackers_urls.removeOne(tracker_url);
      }
      if((*it).verified) {
        item->setText(COL_STATUS, tr("Working"));
      } else {
        if((*it).updating && (*it).fails == 0) {
          item->setText(COL_STATUS, tr("Updating..."));
        } else {
          if((*it).fails > 0) {
            item->setText(COL_STATUS, tr("Not working"));
          } else {
            item->setText(COL_STATUS, tr("Not contacted yet"));
          }
        }
      }
      item->setText(COL_PEERS, QString::number(trackers_data.value(tracker_url, TrackerInfos(tracker_url)).num_peers));
      item->setText(COL_MSG, trackers_data.value(tracker_url, TrackerInfos(tracker_url)).last_message);
    }
    // Remove old trackers
    foreach(const QString &tracker, old_trackers_urls) {
      delete tracker_items.take(tracker);
    }
  }

  // Ask the user for new trackers and add them to the torrent
  void askForTrackers(){
    QStringList trackers = TrackersAdditionDlg::asForTrackers();
    if(!trackers.empty()) {
      QTorrentHandle h = properties->getCurrentTorrent();
      if(!h.is_valid()) return;
      foreach(const QString& tracker, trackers) {
        announce_entry url(tracker.toStdString());
        url.tier = 0;
        h.add_tracker(url);
      }
      // Reannounce to new trackers
      h.force_reannounce();
      // Reload tracker list
      loadTrackers();
      // XXX: I don't think this is necessary now
      //BTSession->saveTrackerFile(h.hash());
    }
  }

  void deleteSelectedTrackers(){
    QTorrentHandle h = properties->getCurrentTorrent();
    if(!h.is_valid()) {
      clear();
      return;
    }
    QList<QTreeWidgetItem *> selected_items = getSelectedTrackerItems();
    if(selected_items.isEmpty()) return;
    QStringList urls_to_remove;
    foreach(QTreeWidgetItem *item, selected_items){
      QString tracker_url = item->data(COL_URL, Qt::DisplayRole).toString();
      urls_to_remove << tracker_url;
      tracker_items.remove(tracker_url);
      delete item;
    }
    // Iterate of trackers and remove selected ones
    std::vector<announce_entry> trackers = h.trackers();
    std::vector<announce_entry>::iterator it = trackers.begin();
    while(it != trackers.end()) {
      int index = urls_to_remove.indexOf(misc::toQString((*it).url));
      if(index >= 0) {
        trackers.erase(it);
        urls_to_remove.removeAt(index);
      } else {
        it++;
      }
    }
    h.replace_trackers(trackers);
    h.force_reannounce();
    // Reload Trackers
    loadTrackers();
    //XXX: I don't think this is necessary
    //BTSession->saveTrackerFile(h.hash());
  }

  void showTrackerListMenu(QPoint) {
    QList<QTreeWidgetItem*> selected_items = getSelectedTrackerItems();
    QMenu menu;
    // Add actions
    QAction *addAct = menu.addAction(QIcon(":/Icons/oxygen/list-add.png"), tr("Add a new tracker"));
    QAction *delAct = 0;
    if(!getSelectedTrackerItems().isEmpty()) {
      delAct = menu.addAction(QIcon(":/Icons/oxygen/list-remove.png"), "Remove tracker");
    }
    QAction *act = menu.exec(QCursor::pos());
    if(act == addAct) {
      askForTrackers();
      return;
    }
    if(act == delAct) {
      deleteSelectedTrackers();
      return;
    }
  }

  void loadSettings() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    QVariantList contentColsWidths = settings.value(QString::fromUtf8("TorrentProperties/Trackers/trackersColsWidth"), QVariantList()).toList();
    if(!contentColsWidths.empty()) {
      for(int i=0; i<contentColsWidths.size(); ++i) {
        setColumnWidth(i, contentColsWidths.at(i).toInt());
      }
    } else {
      setColumnWidth(0, 300);
    }
  }

  void saveSettings() const {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    QVariantList contentColsWidths;
    for(int i=0; i<columnCount(); ++i) {
      contentColsWidths.append(columnWidth(i));
    }
    settings.setValue(QString::fromUtf8("TorrentProperties/Trackers/trackersColsWidth"), contentColsWidths);
  }

};

#endif // TRACKERLIST_H
