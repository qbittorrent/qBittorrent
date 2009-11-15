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

#include "peerlistwidget.h"
#include "peerlistdelegate.h"
#include "reverseresolution.h"
#include "preferences.h"
#include "propertieswidget.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QSettings>
#include <vector>

PeerListWidget::PeerListWidget(PropertiesWidget *parent): properties(parent) {
  // Visual settings
  setRootIsDecorated(false);
  setItemsExpandable(false);
  setAllColumnsShowFocus(true);
  // List Model
  listModel = new QStandardItemModel(0, 7);
  listModel->setHeaderData(IP, Qt::Horizontal, tr("IP"));
  listModel->setHeaderData(CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
  listModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  listModel->setHeaderData(DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
  listModel->setHeaderData(UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
  listModel->setHeaderData(TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
  listModel->setHeaderData(TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
  // Proxy model to support sorting without actually altering the underlying model
  proxyModel = new QSortFilterProxyModel();
  proxyModel->setDynamicSortFilter(true);
  proxyModel->setSourceModel(listModel);
  setModel(proxyModel);
  // List delegate
  listDelegate = new PeerListDelegate(this);
  setItemDelegate(listDelegate);
  // Enable sorting
  setSortingEnabled(true);
  // Load settings
  loadSettings();
  // IP to Hostname resolver
  updatePeerHostNameResolutionState();
}

PeerListWidget::~PeerListWidget() {
  saveSettings();
  delete proxyModel;
  delete listModel;
  delete listDelegate;
  if(resolver)
    delete resolver;
}

void PeerListWidget::updatePeerHostNameResolutionState() {
  if(Preferences::resolvePeerHostNames()) {
    if(!resolver) {
      resolver = new ReverseResolution(this);
      connect(resolver, SIGNAL(ip_resolved(QString,QString)), this, SLOT(handleResolved(QString,QString)));
      resolver->start();
      clear();
      loadPeers(properties->getCurrentTorrent());
    }
  } else {
    if(resolver)
      resolver->asyncDelete();
  }
}

void PeerListWidget::clear() {
  qDebug("clearing peer list");
  peerItems.clear();
  int nbrows = listModel->rowCount();
  if(nbrows > 0) {
    qDebug("Cleared %d peers", nbrows);
    listModel->removeRows(0,  nbrows);
  }
}

void PeerListWidget::loadSettings() {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QVariantList contentColsWidths = settings.value(QString::fromUtf8("TorrentProperties/Peers/peersColsWidth"), QVariantList()).toList();
  if(!contentColsWidths.empty()) {
    for(int i=0; i<contentColsWidths.size(); ++i) {
      setColumnWidth(i, contentColsWidths.at(i).toInt());
    }
  }
}

void PeerListWidget::saveSettings() const {
  QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QVariantList contentColsWidths;
  for(int i=0; i<listModel->columnCount(); ++i) {
    contentColsWidths.append(columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("TorrentProperties/Peers/peersColsWidth"), contentColsWidths);
}

void PeerListWidget::loadPeers(const QTorrentHandle &h) {
  if(!h.is_valid()) return;
  std::vector<peer_info> peers;
  h.get_peer_info(peers);
  std::vector<peer_info>::iterator itr;
  QSet<QString> old_peers_set = peerItems.keys().toSet();
  for(itr = peers.begin(); itr != peers.end(); itr++) {
    peer_info peer = *itr;
    QString peer_ip = misc::toQString(peer.ip.address().to_string());
    if(peerItems.contains(peer_ip)) {
      // Update existing peer
      updatePeer(peer_ip, peer);
      old_peers_set.remove(peer_ip);
    } else {
      // Add new peer
      peerItems[peer_ip] = addPeer(peer_ip, peer);
    }
  }
  // Delete peers that are gone
  QSetIterator<QString> it(old_peers_set);
  while(it.hasNext()) {
    QStandardItem *item = peerItems.take(it.next());
    listModel->removeRow(item->row());
  }
}

QStandardItem* PeerListWidget::addPeer(QString ip, peer_info peer) {
  int row = listModel->rowCount();
  // Adding Peer to peer list
  listModel->insertRow(row);
  listModel->setData(listModel->index(row, IP), ip);
  // Resolve peer host name is asked
  if(resolver)
    resolver->resolve(peer.ip);
  listModel->setData(listModel->index(row, CLIENT), misc::toQString(peer.client));
  listModel->setData(listModel->index(row, PROGRESS), peer.progress);
  listModel->setData(listModel->index(row, DOWN_SPEED), peer.payload_down_speed);
  listModel->setData(listModel->index(row, UP_SPEED), peer.payload_up_speed);
  listModel->setData(listModel->index(row, TOT_DOWN), peer.total_download);
  listModel->setData(listModel->index(row, TOT_UP), peer.total_upload);
  return listModel->item(row, IP);
}

void PeerListWidget::updatePeer(QString ip, peer_info peer) {
  QStandardItem *item = peerItems.value(ip);
  int row = item->row();
  listModel->setData(listModel->index(row, CLIENT), misc::toQString(peer.client));
  listModel->setData(listModel->index(row, PROGRESS), peer.progress);
  listModel->setData(listModel->index(row, DOWN_SPEED), peer.payload_down_speed);
  listModel->setData(listModel->index(row, UP_SPEED), peer.payload_up_speed);
  listModel->setData(listModel->index(row, TOT_DOWN), peer.total_download);
  listModel->setData(listModel->index(row, TOT_UP), peer.total_upload);
}

void PeerListWidget::handleResolved(QString ip, QString hostname) {
  qDebug("%s was resolved to %s", ip.toLocal8Bit().data(), hostname.toLocal8Bit().data());
  QStandardItem *item = peerItems.value(ip, 0);
  if(item) {
    qDebug("item was updated");
    //item->setData(hostname);
    listModel->setData(listModel->indexFromItem(item), hostname);
  }
}
