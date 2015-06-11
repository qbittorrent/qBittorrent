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

#ifndef PEERLISTWIDGET_H
#define PEERLISTWIDGET_H

#include <QTreeView>
#include <QHash>
#include <QPointer>
#include <QSet>

namespace Net
{
    class ReverseResolution;
}

class PeerListDelegate;
class PeerListSortModel;
class PropertiesWidget;

QT_BEGIN_NAMESPACE
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
QT_END_NAMESPACE

namespace BitTorrent
{

class TorrentHandle;
class PeerInfo;
struct PeerAddress;

}

class PeerListWidget : public QTreeView {
  Q_OBJECT

public:
  PeerListWidget(PropertiesWidget *parent);
  ~PeerListWidget();

public slots:
  void loadPeers(BitTorrent::TorrentHandle *const torrent, bool force_hostname_resolution = false);
  QStandardItem *addPeer(const QString &ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer);
  void updatePeer(const QString &ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer);
  void handleResolved(const QString &ip, const QString &hostname);
  void updatePeerHostNameResolutionState();
  void updatePeerCountryResolutionState();
  void clear();

protected slots:
  void loadSettings();
  void saveSettings() const;
  void showPeerListMenu(const QPoint&);
  void banSelectedPeers(const QStringList& peer_ips);
  void handleSortColumnChanged(int col);

private:
  static void getFlags(const BitTorrent::PeerInfo &peer, QString &flags, QString &tooltip);
  qreal getPeerRelevance(const QBitArray &allPieces, const QBitArray &peerPieces);

private:
  QStandardItemModel *m_listModel;
  PeerListDelegate *m_listDelegate;
  PeerListSortModel *m_proxyModel;
  QHash<QString, QStandardItem*> m_peerItems;
  QHash<QString, BitTorrent::PeerAddress> m_peerAddresses;
  QSet<QString> m_missingFlags;
  QPointer<Net::ReverseResolution> m_resolver;
  PropertiesWidget *m_properties;
  bool m_displayFlags;
};

#endif // PEERLISTWIDGET_H
