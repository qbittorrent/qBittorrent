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
#include "geoipmanager.h"
#include "peeraddition.h"
#include "speedlimitdlg.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <vector>
#include "qinisettings.h"

using namespace libtorrent;

PeerListWidget::PeerListWidget(PropertiesWidget *parent): QTreeView(parent), properties(parent), display_flags(false) {
  // Visual settings
  setRootIsDecorated(false);
  setItemsExpandable(false);
  setAllColumnsShowFocus(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  // List Model
  listModel = new QStandardItemModel(0, 8);
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
  hideColumn(IP_HIDDEN);
  // Context menu
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showPeerListMenu(QPoint)));
  // List delegate
  listDelegate = new PeerListDelegate(this);
  setItemDelegate(listDelegate);
  // Enable sorting
  setSortingEnabled(true);
  // Load settings
  loadSettings();
  // IP to Hostname resolver
  updatePeerHostNameResolutionState();
  // SIGNAL/SLOT
  connect(header(), SIGNAL(sectionClicked(int)), SLOT(handleSortColumnChanged(int)));
  handleSortColumnChanged(header()->sortIndicatorSection());
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
  if(Preferences().resolvePeerHostNames()) {
    if(!resolver) {
      resolver = new ReverseResolution(this);
      connect(resolver, SIGNAL(ip_resolved(QString,QString)), this, SLOT(handleResolved(QString,QString)));
      resolver->start();
      loadPeers(properties->getCurrentTorrent(), true);
    }
  } else {
    if(resolver)
      resolver->asyncDelete();
  }
}

void PeerListWidget::updatePeerCountryResolutionState() {
  if(Preferences().resolvePeerCountries() != display_flags) {
    display_flags = !display_flags;
    if(display_flags) {
      const QTorrentHandle h = properties->getCurrentTorrent();
      if(!h.is_valid()) return;
      loadPeers(h);
    }
  }
}

void PeerListWidget::showPeerListMenu(QPoint) {
  QMenu menu;
  bool empty_menu = true;
  QTorrentHandle h = properties->getCurrentTorrent();
  if(!h.is_valid()) return;
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList selectedPeerIPs;
  foreach(const QModelIndex &index, selectedIndexes) {
    int row = proxyModel->mapToSource(index).row();
    QString myip = listModel->data(listModel->index(row, IP_HIDDEN)).toString();
    selectedPeerIPs << myip;
  }
  // Add Peer Action
  QAction *addPeerAct = 0;
  if(!h.is_queued() && !h.is_checking()) {
    addPeerAct = menu.addAction(QIcon(":/Icons/oxygen/user-group-new.png"), tr("Add a new peer..."));
    empty_menu = false;
  }
  // Per Peer Speed limiting actions
  QAction *upLimitAct = 0;
  QAction *dlLimitAct = 0;
  QAction *banAct = 0;
  QAction *copyIPAct = 0;
  if(!selectedPeerIPs.isEmpty()) {
    copyIPAct = menu.addAction(QIcon(":/Icons/oxygen/edit-copy.png"), tr("Copy IP"));
    menu.addSeparator();
    dlLimitAct = menu.addAction(QIcon(":/Icons/skin/download.png"), tr("Limit download rate..."));
    upLimitAct = menu.addAction(QIcon(":/Icons/skin/seeding.png"), tr("Limit upload rate..."));
    menu.addSeparator();
    banAct = menu.addAction(QIcon(":/Icons/oxygen/user-group-delete.png"), tr("Ban peer permanently"));
    empty_menu = false;
  }
  if(empty_menu) return;
  QAction *act = menu.exec(QCursor::pos());
  if(act == 0) return;
  if(act == addPeerAct) {
    libtorrent::asio::ip::tcp::endpoint ep = PeerAdditionDlg::askForPeerEndpoint();
    if(ep != libtorrent::asio::ip::tcp::endpoint()) {
      try {
        h.connect_peer(ep);
        QMessageBox::information(0, tr("Peer addition"), tr("The peer was added to this torrent."));
      } catch(std::exception) {
        QMessageBox::critical(0, tr("Peer addition"), tr("The peer could not be added to this torrent."));
      }
    } else {
      qDebug("No peer was added");
    }
    return;
  }
  if(act == upLimitAct) {
    limitUpRateSelectedPeers(selectedPeerIPs);
    return;
  }
  if(act == dlLimitAct) {
    limitDlRateSelectedPeers(selectedPeerIPs);
    return;
  }
  if(act == banAct) {
    banSelectedPeers(selectedPeerIPs);
    return;
  }
  if(act == copyIPAct) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    QApplication::clipboard()->setText(selectedPeerIPs.join("\r\n"));
#else
    QApplication::clipboard()->setText(selectedPeerIPs.join("\n"));
#endif
  }
}

void PeerListWidget::banSelectedPeers(QStringList peer_ips) {
  // Confirm first
  int ret = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to ban permanently the selected peers?"),
                                  tr("&Yes"), tr("&No"),
                                  QString(), 0, 1);
  if(ret) return;
  foreach(const QString &ip, peer_ips) {
    qDebug("Banning peer %s...", ip.toLocal8Bit().data());
    QBtSession::instance()->addConsoleMessage(tr("Manually banning peer %1...").arg(ip));
    QBtSession::instance()->banIP(ip);
  }
  // Refresh list
  loadPeers(properties->getCurrentTorrent());
}

void PeerListWidget::limitUpRateSelectedPeers(QStringList peer_ips) {
  QTorrentHandle h = properties->getCurrentTorrent();
  if(!h.is_valid()) return;
  bool ok=false;
  long limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Upload rate limiting"), -1, Preferences().getGlobalUploadLimit()*1024.);
  if(!ok) return;
  foreach(const QString &ip, peer_ips) {
    libtorrent::asio::ip::tcp::endpoint ep = peerEndpoints.value(ip, libtorrent::asio::ip::tcp::endpoint());
    if(ep != libtorrent::asio::ip::tcp::endpoint()) {
      qDebug("Settings Upload limit of %.1f Kb/s to peer %s", limit/1024., ip.toLocal8Bit().data());
      try {
        h.set_peer_upload_limit(ep, limit);
      }catch(std::exception) {
        std::cerr << "Impossible to apply upload limit to peer" << std::endl;
      }
    } else {
      qDebug("The selected peer no longer exists...");
    }
  }
}

void PeerListWidget::limitDlRateSelectedPeers(QStringList peer_ips) {
  QTorrentHandle h = properties->getCurrentTorrent();
  if(!h.is_valid()) return;
  bool ok=false;
  long limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Download rate limiting"), -1, Preferences().getGlobalDownloadLimit()*1024.);
  if(!ok) return;
  foreach(const QString &ip, peer_ips) {
    libtorrent::asio::ip::tcp::endpoint ep = peerEndpoints.value(ip, libtorrent::asio::ip::tcp::endpoint());
    if(ep != libtorrent::asio::ip::tcp::endpoint()) {
      qDebug("Settings Download limit of %.1f Kb/s to peer %s", limit/1024., ip.toLocal8Bit().data());
      try {
        h.set_peer_download_limit(ep, limit);
      }catch(std::exception) {
        std::cerr << "Impossible to apply download limit to peer" << std::endl;
      }
    } else {
      qDebug("The selected peer no longer exists...");
    }
  }
}


void PeerListWidget::clear() {
  qDebug("clearing peer list");
  peerItems.clear();
  peerEndpoints.clear();
  missingFlags.clear();
  int nbrows = listModel->rowCount();
  if(nbrows > 0) {
    qDebug("Cleared %d peers", nbrows);
    listModel->removeRows(0,  nbrows);
  }
}

void PeerListWidget::loadSettings() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QList<int> contentColsWidths = misc::intListfromStringList(settings.value(QString::fromUtf8("TorrentProperties/Peers/peersColsWidth")).toStringList());
  if(!contentColsWidths.empty()) {
    for(int i=0; i<contentColsWidths.size(); ++i) {
      setColumnWidth(i, contentColsWidths.at(i));
    }
  }
  // Load sorted column
  QString sortedCol = settings.value(QString::fromUtf8("TorrentProperties/Peers/PeerListSortedCol"), QString()).toString();
  if(!sortedCol.isEmpty()) {
    Qt::SortOrder sortOrder;
    if(sortedCol.endsWith(QString::fromUtf8("d")))
      sortOrder = Qt::DescendingOrder;
    else
      sortOrder = Qt::AscendingOrder;
    sortedCol.chop(1);
    int index = sortedCol.toInt();
    sortByColumn(index, sortOrder);
  }
}

void PeerListWidget::saveSettings() const {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  QStringList contentColsWidths;
  for(int i=0; i<listModel->columnCount(); ++i) {
    contentColsWidths << QString::number(columnWidth(i));
  }
  settings.setValue(QString::fromUtf8("TorrentProperties/Peers/peersColsWidth"), contentColsWidths);
  // Save sorted column
  Qt::SortOrder sortOrder = header()->sortIndicatorOrder();
  QString sortOrderLetter;
  if(sortOrder == Qt::AscendingOrder)
    sortOrderLetter = QString::fromUtf8("a");
  else
    sortOrderLetter = QString::fromUtf8("d");
  int index = header()->sortIndicatorSection();
  settings.setValue(QString::fromUtf8("TorrentProperties/Peers/PeerListSortedCol"), QVariant(QString::number(index)+sortOrderLetter));
}

void PeerListWidget::loadPeers(const QTorrentHandle &h, bool force_hostname_resolution) {
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
      if(force_hostname_resolution) {
        if(resolver) {
          QString host = resolver->getHostFromCache(peer.ip);
          if(host.isNull()) {
            resolver->resolve(peer.ip);
          } else {
            qDebug("Got peer IP from cache");
            handleResolved(peer_ip, host);
          }
        }
      }
    } else {
      // Add new peer
      peerItems[peer_ip] = addPeer(peer_ip, peer);
      peerEndpoints[peer_ip] = peer.ip;
    }
  }
  // Delete peers that are gone
  QSetIterator<QString> it(old_peers_set);
  while(it.hasNext()) {
    QString ip = it.next();
    missingFlags.remove(ip);
    peerEndpoints.remove(ip);
    QStandardItem *item = peerItems.take(ip);
    listModel->removeRow(item->row());
  }
}

QStandardItem* PeerListWidget::addPeer(QString ip, peer_info peer) {
  int row = listModel->rowCount();
  // Adding Peer to peer list
  listModel->insertRow(row);
  QString host;
  if(resolver) {
    host = resolver->getHostFromCache(peer.ip);
  }
  if(host.isNull())
    listModel->setData(listModel->index(row, IP), ip);
  else
    listModel->setData(listModel->index(row, IP), host);
  listModel->setData(listModel->index(row, IP_HIDDEN), ip);
  // Resolve peer host name is asked
  if(resolver && host.isNull())
    resolver->resolve(peer.ip);
  if(display_flags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country);
    if(!ico.isNull()) {
      listModel->setData(listModel->index(row, IP), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country);
      listModel->setData(listModel->index(row, IP), country_name, Qt::ToolTipRole);
    } else {
      missingFlags.insert(ip);
    }
  }
  listModel->setData(listModel->index(row, CLIENT), misc::toQStringU(peer.client));
  listModel->setData(listModel->index(row, PROGRESS), peer.progress);
  listModel->setData(listModel->index(row, DOWN_SPEED), peer.payload_down_speed);
  listModel->setData(listModel->index(row, UP_SPEED), peer.payload_up_speed);
  listModel->setData(listModel->index(row, TOT_DOWN), (qulonglong)peer.total_download);
  listModel->setData(listModel->index(row, TOT_UP), (qulonglong)peer.total_upload);
  return listModel->item(row, IP);
}

void PeerListWidget::updatePeer(QString ip, peer_info peer) {
  QStandardItem *item = peerItems.value(ip);
  int row = item->row();
  if(display_flags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country);
    if(!ico.isNull()) {
      listModel->setData(listModel->index(row, IP), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country);
      listModel->setData(listModel->index(row, IP), country_name, Qt::ToolTipRole);
      missingFlags.remove(ip);
    }
  }
  listModel->setData(listModel->index(row, CLIENT), misc::toQStringU(peer.client));
  listModel->setData(listModel->index(row, PROGRESS), peer.progress);
  listModel->setData(listModel->index(row, DOWN_SPEED), peer.payload_down_speed);
  listModel->setData(listModel->index(row, UP_SPEED), peer.payload_up_speed);
  listModel->setData(listModel->index(row, TOT_DOWN), (qulonglong)peer.total_download);
  listModel->setData(listModel->index(row, TOT_UP), (qulonglong)peer.total_upload);
}

void PeerListWidget::handleResolved(QString ip, QString hostname) {
  QStandardItem *item = peerItems.value(ip, 0);
  if(item) {
    qDebug("Resolved %s -> %s", qPrintable(ip), qPrintable(hostname));
    item->setData(hostname);
    //listModel->setData(listModel->index(item->row(), IP), hostname);
  }
}

void PeerListWidget::handleSortColumnChanged(int col)
{
  if(col == 0) {
    qDebug("Sorting by decoration");
    proxyModel->setSortRole(Qt::ToolTipRole);
  } else {
    proxyModel->setSortRole(Qt::DisplayRole);
  }
}
