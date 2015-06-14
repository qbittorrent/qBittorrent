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

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QMessageBox>

#include "core/net/reverseresolution.h"
#include "core/bittorrent/torrenthandle.h"
#include "core/bittorrent/peerinfo.h"
#include "core/preferences.h"
#include "core/logger.h"
#include "propertieswidget.h"
#include "geoipmanager.h"
#include "peeraddition.h"
#include "speedlimitdlg.h"
#include "guiiconprovider.h"
#include "peerlistdelegate.h"
#include "peerlistsortmodel.h"
#include "peerlistwidget.h"

PeerListWidget::PeerListWidget(PropertiesWidget *parent):
  QTreeView(parent), m_properties(parent), m_displayFlags(false)
{
  // Load settings
  loadSettings();
  // Visual settings
  setUniformRowHeights(true);
  setRootIsDecorated(false);
  setItemsExpandable(false);
  setAllColumnsShowFocus(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  // List Model
  m_listModel = new QStandardItemModel(0, PeerListDelegate::COL_COUNT);
  m_listModel->setHeaderData(PeerListDelegate::COUNTRY, Qt::Horizontal, QVariant()); // Country flag column
  m_listModel->setHeaderData(PeerListDelegate::IP, Qt::Horizontal, tr("IP"));
  m_listModel->setHeaderData(PeerListDelegate::PORT, Qt::Horizontal, tr("Port"));
  m_listModel->setHeaderData(PeerListDelegate::FLAGS, Qt::Horizontal, tr("Flags"));
  m_listModel->setHeaderData(PeerListDelegate::CONNECTION, Qt::Horizontal, tr("Connection"));
  m_listModel->setHeaderData(PeerListDelegate::CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
  m_listModel->setHeaderData(PeerListDelegate::PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  m_listModel->setHeaderData(PeerListDelegate::DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
  m_listModel->setHeaderData(PeerListDelegate::UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
  m_listModel->setHeaderData(PeerListDelegate::TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
  m_listModel->setHeaderData(PeerListDelegate::TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
  m_listModel->setHeaderData(PeerListDelegate::RELEVANCE, Qt::Horizontal, tr("Relevance", "i.e: How relevant this peer is to us. How many pieces it has that we don't."));
  // Proxy model to support sorting without actually altering the underlying model
  m_proxyModel = new PeerListSortModel();
  m_proxyModel->setDynamicSortFilter(true);
  m_proxyModel->setSourceModel(m_listModel);
  m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
  setModel(m_proxyModel);
  //Explicitly set the column visibility. When columns are added/removed
  //between versions this prevents some of them being hidden due to
  //incorrect restoreState() being used.
  for (unsigned int i=0; i<PeerListDelegate::IP_HIDDEN; i++)
    showColumn(i);
  hideColumn(PeerListDelegate::IP_HIDDEN);
  hideColumn(PeerListDelegate::COL_COUNT);
  if (!Preferences::instance()->resolvePeerCountries())
    hideColumn(PeerListDelegate::COUNTRY);
  //To also mitigate the above issue, we have to resize each column when
  //its size is 0, because explicitly 'showing' the column isn't enough
  //in the above scenario.
  for (unsigned int i=0; i<PeerListDelegate::IP_HIDDEN; i++)
    if (!columnWidth(i))
      resizeColumnToContents(i);
  // Context menu
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showPeerListMenu(QPoint)));
  // List delegate
  m_listDelegate = new PeerListDelegate(this);
  setItemDelegate(m_listDelegate);
  // Enable sorting
  setSortingEnabled(true);
  // IP to Hostname resolver
  updatePeerHostNameResolutionState();
  // SIGNAL/SLOT
  connect(header(), SIGNAL(sectionClicked(int)), SLOT(handleSortColumnChanged(int)));
  handleSortColumnChanged(header()->sortIndicatorSection());
}

PeerListWidget::~PeerListWidget()
{
  saveSettings();
  delete m_proxyModel;
  delete m_listModel;
  delete m_listDelegate;
  if (m_resolver)
    delete m_resolver;
}

void PeerListWidget::updatePeerHostNameResolutionState()
{
  if (Preferences::instance()->resolvePeerHostNames()) {
    if (!m_resolver) {
      m_resolver = new Net::ReverseResolution(this);
      connect(m_resolver, SIGNAL(ipResolved(QString,QString)), SLOT(handleResolved(QString,QString)));
      loadPeers(m_properties->getCurrentTorrent(), true);
    }
  } else {
    if (m_resolver)
      delete m_resolver;
  }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
  if (Preferences::instance()->resolvePeerCountries() != m_displayFlags) {
    m_displayFlags = !m_displayFlags;
    if (m_displayFlags)
      loadPeers(m_properties->getCurrentTorrent());
  }
}

void PeerListWidget::showPeerListMenu(const QPoint&)
{
  QMenu menu;
  bool empty_menu = true;
  BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
  if (!torrent) return;

  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList selectedPeerIPs;
  QStringList selectedPeerIPPort;
  foreach (const QModelIndex &index, selectedIndexes) {
    int row = m_proxyModel->mapToSource(index).row();
    QString myip = m_listModel->data(m_listModel->index(row, PeerListDelegate::IP_HIDDEN)).toString();
    QString myport = m_listModel->data(m_listModel->index(row, PeerListDelegate::PORT)).toString();
    selectedPeerIPs << myip;
    selectedPeerIPPort << myip + ":" + myport;
  }
  // Add Peer Action
  QAction *addPeerAct = 0;
  if (!torrent->isQueued() && !torrent->isChecking()) {
    addPeerAct = menu.addAction(GuiIconProvider::instance()->getIcon("user-group-new"), tr("Add a new peer..."));
    empty_menu = false;
  }
  QAction *banAct = 0;
  QAction *copyPeerAct = 0;
  if (!selectedPeerIPs.isEmpty()) {
    copyPeerAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy selected"));
    menu.addSeparator();
    banAct = menu.addAction(GuiIconProvider::instance()->getIcon("user-group-delete"), tr("Ban peer permanently"));
    empty_menu = false;
  }
  if (empty_menu) return;
  QAction *act = menu.exec(QCursor::pos());
  if (act == 0) return;
  if (act == addPeerAct) {
    BitTorrent::PeerAddress addr = PeerAdditionDlg::askForPeerAddress();
    if (!addr.ip.isNull()) {
      if (torrent->connectPeer(addr))
        QMessageBox::information(0, tr("Peer addition"), tr("The peer was added to this torrent."));
      else
        QMessageBox::critical(0, tr("Peer addition"), tr("The peer could not be added to this torrent."));
    }
    else {
      qDebug("No peer was added");
    }
    return;
  }
  if (act == banAct) {
    banSelectedPeers(selectedPeerIPs);
    return;
  }
  if (act == copyPeerAct) {
#if defined(Q_OS_WIN) || defined(Q_OS_OS2)
    QApplication::clipboard()->setText(selectedPeerIPPort.join("\r\n"));
#else
    QApplication::clipboard()->setText(selectedPeerIPPort.join("\n"));
#endif
  }
}

void PeerListWidget::banSelectedPeers(const QStringList& peer_ips)
{
  // Confirm first
  int ret = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to ban permanently the selected peers?"),
                                  tr("&Yes"), tr("&No"),
                                  QString(), 0, 1);
  if (ret)
    return;

  foreach (const QString &ip, peer_ips) {
    qDebug("Banning peer %s...", ip.toLocal8Bit().data());
    Logger::instance()->addMessage(tr("Manually banning peer %1...").arg(ip));
    BitTorrent::Session::instance()->banIP(ip);
  }
  // Refresh list
  loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::clear() {
  qDebug("clearing peer list");
  m_peerItems.clear();
  m_peerAddresses.clear();
  m_missingFlags.clear();
  int nbrows = m_listModel->rowCount();
  if (nbrows > 0) {
    qDebug("Cleared %d peers", nbrows);
    m_listModel->removeRows(0,  nbrows);
  }
}

void PeerListWidget::loadSettings() {
  header()->restoreState(Preferences::instance()->getPeerListState());
}

void PeerListWidget::saveSettings() const {
  Preferences::instance()->setPeerListState(header()->saveState());
}

void PeerListWidget::loadPeers(BitTorrent::TorrentHandle *const torrent, bool force_hostname_resolution) {
  if (!torrent) return;

  QList<BitTorrent::PeerInfo> peers = torrent->peers();
  QSet<QString> old_peers_set = m_peerItems.keys().toSet();

  foreach (const BitTorrent::PeerInfo &peer, peers) {
    BitTorrent::PeerAddress addr = peer.address();
    if (addr.ip.isNull()) continue;

    QString peer_ip = addr.ip.toString();
    if (m_peerItems.contains(peer_ip)) {
      // Update existing peer
      updatePeer(peer_ip, torrent, peer);
      old_peers_set.remove(peer_ip);
      if (force_hostname_resolution && m_resolver) {
        m_resolver->resolve(peer_ip);
      }
    } else {
      // Add new peer
      m_peerItems[peer_ip] = addPeer(peer_ip, torrent, peer);
      m_peerAddresses[peer_ip] = addr;
      // Resolve peer host name is asked
      if (m_resolver)
        m_resolver->resolve(peer_ip);
    }
  }
  // Delete peers that are gone
  QSetIterator<QString> it(old_peers_set);
  while(it.hasNext()) {
    const QString& ip = it.next();
    m_missingFlags.remove(ip);
    m_peerAddresses.remove(ip);
    QStandardItem *item = m_peerItems.take(ip);
    m_listModel->removeRow(item->row());
  }
}

QStandardItem* PeerListWidget::addPeer(const QString& ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer) {
  int row = m_listModel->rowCount();
  // Adding Peer to peer list
  m_listModel->insertRow(row);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ip);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ip, Qt::ToolTipRole);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PORT), peer.address().port);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP_HIDDEN), ip);
  if (m_displayFlags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country());
    if (!ico.isNull()) {
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country());
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), country_name, Qt::ToolTipRole);
    } else {
      m_missingFlags.insert(ip);
    }
  }
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), peer.connectionType());
  QString flags, tooltip;
  getFlags(peer, flags, tooltip);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), flags);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), tooltip, Qt::ToolTipRole);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), peer.client());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payloadDownSpeed());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payloadUpSpeed());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), peer.totalDownload());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), peer.totalUpload());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::RELEVANCE), getPeerRelevance(torrent->pieces(), peer.pieces()));
  return m_listModel->item(row, PeerListDelegate::IP);
}

void PeerListWidget::updatePeer(const QString &ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer) {
  QStandardItem *item = m_peerItems.value(ip);
  int row = item->row();
  if (m_displayFlags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country());
    if (!ico.isNull()) {
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country());
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), country_name, Qt::ToolTipRole);
      m_missingFlags.remove(ip);
    }
  }
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), peer.connectionType());
  QString flags, tooltip;
  getFlags(peer, flags, tooltip);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PORT), peer.address().port);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), flags);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), tooltip, Qt::ToolTipRole);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), peer.client());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payloadDownSpeed());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payloadUpSpeed());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), peer.totalDownload());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), peer.totalUpload());
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::RELEVANCE), getPeerRelevance(torrent->pieces(), peer.pieces()));
}

void PeerListWidget::handleResolved(const QString &ip, const QString &hostname) {
  QStandardItem *item = m_peerItems.value(ip, 0);
  if (item) {
    qDebug("Resolved %s -> %s", qPrintable(ip), qPrintable(hostname));
    item->setData(hostname, Qt::DisplayRole);
  }
}

void PeerListWidget::handleSortColumnChanged(int col)
{
  if (col == PeerListDelegate::COUNTRY) {
    qDebug("Sorting by decoration");
    m_proxyModel->setSortRole(Qt::ToolTipRole);
  } else {
    m_proxyModel->setSortRole(Qt::DisplayRole);
  }
}

void PeerListWidget::getFlags(const BitTorrent::PeerInfo &peer, QString& flags, QString& tooltip)
{  
  if (peer.isInteresting()) {
    //d = Your client wants to download, but peer doesn't want to send (interested and choked)
    if (peer.isRemoteChocked()) {
      flags += "d ";
      tooltip += tr("interested(local) and choked(peer)");
      tooltip += ", ";
    }
    else {
      //D = Currently downloading (interested and not choked)
      flags += "D ";
      tooltip += tr("interested(local) and unchoked(peer)");
      tooltip += ", ";
    }
  }

  if (peer.isRemoteInterested()) {
    //u = Peer wants your client to upload, but your client doesn't want to (interested and choked)
    if (peer.isChocked()) {
      flags += "u ";
      tooltip += tr("interested(peer) and choked(local)");
      tooltip += ", ";
    }
    else {
      //U = Currently uploading (interested and not choked)
      flags += "U ";
      tooltip += tr("interested(peer) and unchoked(local)");
      tooltip += ", ";
    }
  }

  //O = Optimistic unchoke
  if (peer.optimisticUnchoke()) {
    flags += "O ";
    tooltip += tr("optimistic unchoke");
    tooltip += ", ";
  }

  //S = Peer is snubbed
  if (peer.isSnubbed()) {
    flags += "S ";
    tooltip += tr("peer snubbed");
    tooltip += ", ";
  }

  //I = Peer is an incoming connection
  if (!peer.isLocalConnection()) {
    flags += "I ";
    tooltip += tr("incoming connection");
    tooltip += ", ";
  }

  //K = Peer is unchoking your client, but your client is not interested
  if (!peer.isRemoteChocked() && !peer.isInteresting()) {
    flags += "K ";
    tooltip += tr("not interested(local) and unchoked(peer)");
    tooltip += ", ";
  }

  //? = Your client unchoked the peer but the peer is not interested
  if (!peer.isChocked() && !peer.isRemoteInterested()) {
    flags += "? ";
    tooltip += tr("not interested(peer) and unchoked(local)");
    tooltip += ", ";
  }

  //X = Peer was included in peerlists obtained through Peer Exchange (PEX)
  if (peer.fromPeX()) {
    flags += "X ";
    tooltip += tr("peer from PEX");
    tooltip += ", ";
  }

  //H = Peer was obtained through DHT
  if (peer.fromDHT()) {
    flags += "H ";
    tooltip += tr("peer from DHT");
    tooltip += ", ";
  }

  //E = Peer is using Protocol Encryption (all traffic)
  if (peer.isRC4Encrypted()) {
    flags += "E ";
    tooltip += tr("encrypted traffic");
    tooltip += ", ";
  }

  //e = Peer is using Protocol Encryption (handshake)
  if (peer.isPlaintextEncrypted()) {
    flags += "e ";
    tooltip += tr("encrypted handshake");
    tooltip += ", ";
  }

  //P = Peer is using uTorrent uTP

  if (peer.useUTPSocket()) {
    flags += "P ";
    tooltip += QString::fromUtf8("μTP");
    tooltip += ", ";
  }

  //L = Peer is local
  if (peer.fromLSD()) {
    flags += "L";
    tooltip += tr("peer from LSD");
  }

  flags = flags.trimmed();
  tooltip = tooltip.trimmed();
  if (tooltip.endsWith(',', Qt::CaseInsensitive))
    tooltip.chop(1);
}

qreal PeerListWidget::getPeerRelevance(const QBitArray &allPieces, const QBitArray &peerPieces)
{
    int localMissing = 0;
    int remoteHaves = 0;

    for (int i = 0; i < allPieces.size(); ++i) {
        if (!allPieces[i]) {
            ++localMissing;
            if (peerPieces[i])
                ++remoteHaves;
        }
    }

    if (localMissing == 0)
        return 0.0;

    return static_cast<qreal>(remoteHaves) / localMissing;
}
