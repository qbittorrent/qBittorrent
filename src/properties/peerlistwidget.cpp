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
#include "iconprovider.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <vector>
#include "qinisettings.h"

using namespace libtorrent;

PeerListWidget::PeerListWidget(PropertiesWidget *parent):
  QTreeView(parent), m_properties(parent), m_displayFlags(false)
{
  // Load settings
  loadSettings();
  // Visual settings
  setRootIsDecorated(false);
  setItemsExpandable(false);
  setAllColumnsShowFocus(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  // List Model
  m_listModel = new QStandardItemModel(0, PeerListDelegate::COL_COUNT);
  m_listModel->setHeaderData(PeerListDelegate::IP, Qt::Horizontal, tr("IP"));
  m_listModel->setHeaderData(PeerListDelegate::CONNECTION, Qt::Horizontal, tr("Connection"));
  m_listModel->setHeaderData(PeerListDelegate::CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
  m_listModel->setHeaderData(PeerListDelegate::PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
  m_listModel->setHeaderData(PeerListDelegate::DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
  m_listModel->setHeaderData(PeerListDelegate::UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
  m_listModel->setHeaderData(PeerListDelegate::TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
  m_listModel->setHeaderData(PeerListDelegate::TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
  // Proxy model to support sorting without actually altering the underlying model
  m_proxyModel = new QSortFilterProxyModel();
  m_proxyModel->setDynamicSortFilter(true);
  m_proxyModel->setSourceModel(m_listModel);
  setModel(m_proxyModel);
  hideColumn(PeerListDelegate::IP_HIDDEN);
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
  if (Preferences().resolvePeerHostNames()) {
    if (!m_resolver) {
      m_resolver = new ReverseResolution(this);
      connect(m_resolver, SIGNAL(ip_resolved(QString,QString)), SLOT(handleResolved(QString,QString)));
      loadPeers(m_properties->getCurrentTorrent(), true);
    }
  } else {
    if (m_resolver)
      delete m_resolver;
  }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
  if (Preferences().resolvePeerCountries() != m_displayFlags) {
    m_displayFlags = !m_displayFlags;
    if (m_displayFlags)
      loadPeers(m_properties->getCurrentTorrent());
  }
}

void PeerListWidget::showPeerListMenu(const QPoint&)
{
  QMenu menu;
  bool empty_menu = true;
  QTorrentHandle h = m_properties->getCurrentTorrent();
  if (!h.is_valid()) return;
  QModelIndexList selectedIndexes = selectionModel()->selectedRows();
  QStringList selectedPeerIPs;
  foreach (const QModelIndex &index, selectedIndexes) {
    int row = m_proxyModel->mapToSource(index).row();
    QString myip = m_listModel->data(m_listModel->index(row, PeerListDelegate::IP_HIDDEN)).toString();
    selectedPeerIPs << myip;
  }
  // Add Peer Action
  QAction *addPeerAct = 0;
  if (!h.is_queued() && !h.is_checking()) {
    addPeerAct = menu.addAction(IconProvider::instance()->getIcon("user-group-new"), tr("Add a new peer..."));
    empty_menu = false;
  }
  // Per Peer Speed limiting actions
  QAction *upLimitAct = 0;
  QAction *dlLimitAct = 0;
  QAction *banAct = 0;
  QAction *copyIPAct = 0;
  if (!selectedPeerIPs.isEmpty()) {
    copyIPAct = menu.addAction(IconProvider::instance()->getIcon("edit-copy"), tr("Copy IP"));
    menu.addSeparator();
    dlLimitAct = menu.addAction(QIcon(":/Icons/skin/download.png"), tr("Limit download rate..."));
    upLimitAct = menu.addAction(QIcon(":/Icons/skin/seeding.png"), tr("Limit upload rate..."));
    menu.addSeparator();
    banAct = menu.addAction(IconProvider::instance()->getIcon("user-group-delete"), tr("Ban peer permanently"));
    empty_menu = false;
  }
  if (empty_menu) return;
  QAction *act = menu.exec(QCursor::pos());
  if (act == 0) return;
  if (act == addPeerAct) {
    boost::asio::ip::tcp::endpoint ep = PeerAdditionDlg::askForPeerEndpoint();
    if (ep != boost::asio::ip::tcp::endpoint()) {
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
  if (act == upLimitAct) {
    limitUpRateSelectedPeers(selectedPeerIPs);
    return;
  }
  if (act == dlLimitAct) {
    limitDlRateSelectedPeers(selectedPeerIPs);
    return;
  }
  if (act == banAct) {
    banSelectedPeers(selectedPeerIPs);
    return;
  }
  if (act == copyIPAct) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    QApplication::clipboard()->setText(selectedPeerIPs.join("\r\n"));
#else
    QApplication::clipboard()->setText(selectedPeerIPs.join("\n"));
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
    QBtSession::instance()->addConsoleMessage(tr("Manually banning peer %1...").arg(ip));
    QBtSession::instance()->banIP(ip);
  }
  // Refresh list
  loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::limitUpRateSelectedPeers(const QStringList& peer_ips)
{
  if (peer_ips.empty())
    return;
  QTorrentHandle h = m_properties->getCurrentTorrent();
  if (!h.is_valid())
    return;

  bool ok = false;
  int cur_limit = -1;
#if LIBTORRENT_VERSION_MINOR > 15
  boost::asio::ip::tcp::endpoint first_ep = m_peerEndpoints.value(peer_ips.first(),
                                                                  boost::asio::ip::tcp::endpoint());
  if (first_ep != boost::asio::ip::tcp::endpoint())
    cur_limit = h.get_peer_upload_limit(first_ep);
#endif
  long limit = SpeedLimitDialog::askSpeedLimit(&ok,
                                               tr("Upload rate limiting"),
                                               cur_limit,
                                               Preferences().getGlobalUploadLimit()*1024.);
  if (!ok)
    return;

  foreach (const QString &ip, peer_ips) {
    boost::asio::ip::tcp::endpoint ep = m_peerEndpoints.value(ip, boost::asio::ip::tcp::endpoint());
    if (ep != boost::asio::ip::tcp::endpoint()) {
      qDebug("Settings Upload limit of %.1f Kb/s to peer %s", limit/1024., ip.toLocal8Bit().data());
      try {
        h.set_peer_upload_limit(ep, limit);
      } catch(std::exception) {
        std::cerr << "Impossible to apply upload limit to peer" << std::endl;
      }
    } else {
      qDebug("The selected peer no longer exists...");
    }
  }
}

void PeerListWidget::limitDlRateSelectedPeers(const QStringList& peer_ips)
{
  QTorrentHandle h = m_properties->getCurrentTorrent();
  if (!h.is_valid())
    return;
  bool ok = false;
  int cur_limit = -1;
#if LIBTORRENT_VERSION_MINOR > 15
  boost::asio::ip::tcp::endpoint first_ep = m_peerEndpoints.value(peer_ips.first(),
                                                                  boost::asio::ip::tcp::endpoint());
  if (first_ep != boost::asio::ip::tcp::endpoint())
    cur_limit = h.get_peer_download_limit(first_ep);
#endif
  long limit = SpeedLimitDialog::askSpeedLimit(&ok, tr("Download rate limiting"), cur_limit, Preferences().getGlobalDownloadLimit()*1024.);
  if (!ok)
    return;

  foreach (const QString &ip, peer_ips) {
    boost::asio::ip::tcp::endpoint ep = m_peerEndpoints.value(ip, boost::asio::ip::tcp::endpoint());
    if (ep != boost::asio::ip::tcp::endpoint()) {
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
  m_peerItems.clear();
  m_peerEndpoints.clear();
  m_missingFlags.clear();
  int nbrows = m_listModel->rowCount();
  if (nbrows > 0) {
    qDebug("Cleared %d peers", nbrows);
    m_listModel->removeRows(0,  nbrows);
  }
}

void PeerListWidget::loadSettings() {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  header()->restoreState(settings.value("TorrentProperties/Peers/PeerListState").toByteArray());
}

void PeerListWidget::saveSettings() const {
  QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
  settings.setValue("TorrentProperties/Peers/PeerListState", header()->saveState());
}

void PeerListWidget::loadPeers(const QTorrentHandle &h, bool force_hostname_resolution) {
  if (!h.is_valid())
    return;
  boost::system::error_code ec;
  std::vector<peer_info> peers;
  h.get_peer_info(peers);
  QSet<QString> old_peers_set = m_peerItems.keys().toSet();

  std::vector<peer_info>::const_iterator itr = peers.begin();
  std::vector<peer_info>::const_iterator itrend = peers.end();
  for ( ; itr != itrend; ++itr) {
    peer_info peer = *itr;
    std::string ip_str = peer.ip.address().to_string(ec);
    if (ec || ip_str.empty())
      continue;
    QString peer_ip = misc::toQString(ip_str);
    if (m_peerItems.contains(peer_ip)) {
      // Update existing peer
      updatePeer(peer_ip, peer);
      old_peers_set.remove(peer_ip);
      if (force_hostname_resolution && m_resolver) {
        m_resolver->resolve(peer.ip);
      }
    } else {
      // Add new peer
      m_peerItems[peer_ip] = addPeer(peer_ip, peer);
      m_peerEndpoints[peer_ip] = peer.ip;
    }
  }
  // Delete peers that are gone
  QSetIterator<QString> it(old_peers_set);
  while(it.hasNext()) {
    const QString& ip = it.next();
    m_missingFlags.remove(ip);
    m_peerEndpoints.remove(ip);
    QStandardItem *item = m_peerItems.take(ip);
    m_listModel->removeRow(item->row());
  }
}

QStandardItem* PeerListWidget::addPeer(const QString& ip, const peer_info& peer) {
  int row = m_listModel->rowCount();
  // Adding Peer to peer list
  m_listModel->insertRow(row);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ip);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP_HIDDEN), ip);
  // Resolve peer host name is asked
  if (m_resolver)
    m_resolver->resolve(peer.ip);
  if (m_displayFlags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country);
    if (!ico.isNull()) {
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country);
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), country_name, Qt::ToolTipRole);
    } else {
      m_missingFlags.insert(ip);
    }
  }
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), getConnectionString(peer.connection_type));
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), misc::toQStringU(peer.client));
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payload_down_speed);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payload_up_speed);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), (qulonglong)peer.total_download);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), (qulonglong)peer.total_upload);
  return m_listModel->item(row, PeerListDelegate::IP);
}

void PeerListWidget::updatePeer(const QString& ip, const peer_info& peer) {
  QStandardItem *item = m_peerItems.value(ip);
  int row = item->row();
  if (m_displayFlags) {
    const QIcon ico = GeoIPManager::CountryISOCodeToIcon(peer.country);
    if (!ico.isNull()) {
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ico, Qt::DecorationRole);
      const QString country_name = GeoIPManager::CountryISOCodeToName(peer.country);
      m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), country_name, Qt::ToolTipRole);
      m_missingFlags.remove(ip);
    }
  }
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), getConnectionString(peer.connection_type));
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), misc::toQStringU(peer.client));
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payload_down_speed);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payload_up_speed);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), (qulonglong)peer.total_download);
  m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), (qulonglong)peer.total_upload);
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
  if (col == 0) {
    qDebug("Sorting by decoration");
    m_proxyModel->setSortRole(Qt::ToolTipRole);
  } else {
    m_proxyModel->setSortRole(Qt::DisplayRole);
  }
}

QString PeerListWidget::getConnectionString(int connection_type)
{
  QString connection;
  switch(connection_type) {
#if LIBTORRENT_VERSION_MINOR > 15
  case peer_info::bittorrent_utp:
    connection = "uTP";
    break;
  case peer_info::http_seed:
#endif
  case peer_info::web_seed:
    connection = "Web";
    break;
  default:
    connection = "BT";
    break;
  }
  return connection;
}
