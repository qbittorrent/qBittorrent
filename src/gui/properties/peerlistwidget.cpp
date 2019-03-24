/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
 */

#include "peerlistwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QWheelEvent>

#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/logger.h"
#include "base/net/geoipmanager.h"
#include "base/net/reverseresolution.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "guiiconprovider.h"
#include "peerlistdelegate.h"
#include "peerlistsortmodel.h"
#include "peersadditiondialog.h"
#include "propertieswidget.h"
#include "speedlimitdialog.h"

PeerListWidget::PeerListWidget(PropertiesWidget *parent)
    : QTreeView(parent)
    , m_properties(parent)
{
    // Load settings
    loadSettings();
    // Visual settings
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setStretchLastSection(false);
    // List Model
    m_listModel = new QStandardItemModel(0, PeerListDelegate::COL_COUNT, this);
    m_listModel->setHeaderData(PeerListDelegate::COUNTRY, Qt::Horizontal, tr("Country")); // Country flag column
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
    m_listModel->setHeaderData(PeerListDelegate::DOWNLOADING_PIECE, Qt::Horizontal, tr("Files", "i.e. files that are being downloaded right now"));
    // Set header text alignment
    m_listModel->setHeaderData(PeerListDelegate::PORT, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::PROGRESS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::DOWN_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::UP_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::TOT_DOWN, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::TOT_UP, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListDelegate::RELEVANCE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    // Proxy model to support sorting without actually altering the underlying model
    m_proxyModel = new PeerListSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_listModel);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    setModel(m_proxyModel);
    hideColumn(PeerListDelegate::IP_HIDDEN);
    hideColumn(PeerListDelegate::COL_COUNT);
    m_resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (!m_resolveCountries)
        hideColumn(PeerListDelegate::COUNTRY);
    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < PeerListDelegate::IP_HIDDEN; ++i) {
        if (!isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(PeerListDelegate::IP, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < PeerListDelegate::IP_HIDDEN; ++i)
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PeerListWidget::showPeerListMenu);
    // List delegate
    m_listDelegate = new PeerListDelegate(this);
    setItemDelegate(m_listDelegate);
    // Enable sorting
    setSortingEnabled(true);
    // IP to Hostname resolver
    updatePeerHostNameResolutionState();
    // SIGNAL/SLOT
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &PeerListWidget::displayToggleColumnsMenu);
    connect(header(), &QHeaderView::sectionClicked, this, &PeerListWidget::handleSortColumnChanged);
    connect(header(), &QHeaderView::sectionMoved, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &PeerListWidget::saveSettings);
    handleSortColumnChanged(header()->sortIndicatorSection());
    m_copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(m_copyHotkey, &QShortcut::activated, this, &PeerListWidget::copySelectedPeers);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(this->header());
    this->header()->setParent(this);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

PeerListWidget::~PeerListWidget()
{
    saveSettings();
    if (m_resolver)
        delete m_resolver;
}

void PeerListWidget::displayToggleColumnsMenu(const QPoint &)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction *> actions;
    for (int i = 0; i < PeerListDelegate::IP_HIDDEN; ++i) {
        if ((i == PeerListDelegate::COUNTRY) && !Preferences::instance()->resolvePeerCountries()) {
            actions.append(nullptr); // keep the index in sync
            continue;
        }
        QAction *myAct = hideshowColumn.addAction(m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!isColumnHidden(i));
        actions.append(myAct);
    }
    int visibleCols = 0;
    for (int i = 0; i < PeerListDelegate::IP_HIDDEN; ++i) {
        if (!isColumnHidden(i))
            ++visibleCols;

        if (visibleCols > 1)
            break;
    }

    // Call menu
    QAction *act = hideshowColumn.exec(QCursor::pos());
    if (act) {
        int col = actions.indexOf(act);
        Q_ASSERT(col >= 0);
        Q_ASSERT(visibleCols > 0);
        if (!isColumnHidden(col) && (visibleCols == 1))
            return;
        setColumnHidden(col, !isColumnHidden(col));
        if (!isColumnHidden(col) && (columnWidth(col) <= 5))
            resizeColumnToContents(col);
        saveSettings();
    }
}

void PeerListWidget::updatePeerHostNameResolutionState()
{
    if (Preferences::instance()->resolvePeerHostNames()) {
        if (!m_resolver) {
            m_resolver = new Net::ReverseResolution(this);
            connect(m_resolver.data(), &Net::ReverseResolution::ipResolved, this, &PeerListWidget::handleResolved);
            loadPeers(m_properties->getCurrentTorrent(), true);
        }
    }
    else if (m_resolver) {
        delete m_resolver;
    }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
    if (Preferences::instance()->resolvePeerCountries() != m_resolveCountries) {
        m_resolveCountries = !m_resolveCountries;
        if (m_resolveCountries) {
            loadPeers(m_properties->getCurrentTorrent());
            showColumn(PeerListDelegate::COUNTRY);
            if (columnWidth(PeerListDelegate::COUNTRY) <= 0)
                resizeColumnToContents(PeerListDelegate::COUNTRY);
        }
        else {
            hideColumn(PeerListDelegate::COUNTRY);
        }
    }
}

void PeerListWidget::showPeerListMenu(const QPoint &)
{
    QMenu menu;
    bool emptyMenu = true;
    BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    // Add Peer Action
    QAction *addPeerAct = nullptr;
    if (!torrent->isQueued() && !torrent->isChecking()) {
        addPeerAct = menu.addAction(GuiIconProvider::instance()->getIcon("user-group-new"), tr("Add a new peer..."));
        emptyMenu = false;
    }
    QAction *banAct = nullptr;
    QAction *copyPeerAct = nullptr;
    if (!selectionModel()->selectedRows().isEmpty()) {
        copyPeerAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy IP:port"));
        menu.addSeparator();
        banAct = menu.addAction(GuiIconProvider::instance()->getIcon("user-group-delete"), tr("Ban peer permanently"));
        emptyMenu = false;
    }
    if (emptyMenu) return;
    QAction *act = menu.exec(QCursor::pos());
    if (!act) return;

    if (act == addPeerAct) {
        const QList<BitTorrent::PeerAddress> peersList = PeersAdditionDialog::askForPeers(this);
        int peerCount = 0;
        for (const BitTorrent::PeerAddress &addr : peersList) {
            if (torrent->connectPeer(addr)) {
                qDebug("Adding peer %s...", qUtf8Printable(addr.ip.toString()));
                Logger::instance()->addMessage(tr("Manually adding peer '%1'...").arg(addr.ip.toString()));
                ++peerCount;
            }
            else {
                Logger::instance()->addMessage(tr("The peer '%1' could not be added to this torrent.").arg(addr.ip.toString()), Log::WARNING);
            }
        }
        if (peerCount < peersList.length())
            QMessageBox::information(this, tr("Peer addition"), tr("Some peers could not be added. Check the Log for details."));
        else if (peerCount > 0)
            QMessageBox::information(this, tr("Peer addition"), tr("The peers were added to this torrent."));
        return;
    }
    if (act == banAct) {
        banSelectedPeers();
        return;
    }
    if (act == copyPeerAct) {
        copySelectedPeers();
        return;
    }
}

void PeerListWidget::banSelectedPeers()
{
    // Confirm first
    int ret = QMessageBox::question(this, tr("Ban peer permanently"), tr("Are you sure you want to ban permanently the selected peers?"),
                                    tr("&Yes"), tr("&No"),
                                    QString(), 0, 1);
    if (ret) return;

    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    for (const QModelIndex &index : selectedIndexes) {
        int row = m_proxyModel->mapToSource(index).row();
        QString ip = m_listModel->data(m_listModel->index(row, PeerListDelegate::IP_HIDDEN)).toString();
        qDebug("Banning peer %s...", ip.toLocal8Bit().data());
        Logger::instance()->addMessage(tr("Manually banning peer '%1'...").arg(ip));
        BitTorrent::Session::instance()->banIP(ip);
    }
    // Refresh list
    loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::copySelectedPeers()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    QStringList selectedPeers;
    for (const QModelIndex &index : selectedIndexes) {
        int row = m_proxyModel->mapToSource(index).row();
        QString ip = m_listModel->data(m_listModel->index(row, PeerListDelegate::IP_HIDDEN)).toString();
        QString myport = m_listModel->data(m_listModel->index(row, PeerListDelegate::PORT)).toString();
        if (ip.indexOf('.') == -1) // IPv6
            selectedPeers << '[' + ip + "]:" + myport;
        else // IPv4
            selectedPeers << ip + ':' + myport;
    }
    QApplication::clipboard()->setText(selectedPeers.join('\n'));
}

void PeerListWidget::clear()
{
    qDebug("clearing peer list");
    m_peerItems.clear();
    m_peerAddresses.clear();
    m_missingFlags.clear();
    int nbrows = m_listModel->rowCount();
    if (nbrows > 0) {
        qDebug("Cleared %d peers", nbrows);
        m_listModel->removeRows(0, nbrows);
    }
}

void PeerListWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getPeerListState());
}

void PeerListWidget::saveSettings() const
{
    Preferences::instance()->setPeerListState(header()->saveState());
}

void PeerListWidget::loadPeers(BitTorrent::TorrentHandle *const torrent, bool forceHostnameResolution)
{
    if (!torrent) return;

    const QList<BitTorrent::PeerInfo> peers = torrent->peers();
    QSet<QString> oldPeersSet = m_peerItems.keys().toSet();

    for (const BitTorrent::PeerInfo &peer : peers) {
        BitTorrent::PeerAddress addr = peer.address();
        if (addr.ip.isNull()) continue;

        QString peerIp = addr.ip.toString();
        if (m_peerItems.contains(peerIp)) {
            // Update existing peer
            updatePeer(peerIp, torrent, peer);
            oldPeersSet.remove(peerIp);
            if (forceHostnameResolution && m_resolver)
                m_resolver->resolve(peerIp);
        }
        else {
            // Add new peer
            m_peerItems[peerIp] = addPeer(peerIp, torrent, peer);
            m_peerAddresses[peerIp] = addr;
            // Resolve peer host name is asked
            if (m_resolver)
                m_resolver->resolve(peerIp);
        }
    }
    // Delete peers that are gone
    QSetIterator<QString> it(oldPeersSet);
    while (it.hasNext()) {
        const QString &ip = it.next();
        m_missingFlags.remove(ip);
        m_peerAddresses.remove(ip);
        QStandardItem *item = m_peerItems.take(ip);
        m_listModel->removeRow(item->row());
    }
}

QStandardItem *PeerListWidget::addPeer(const QString &ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer)
{
    int row = m_listModel->rowCount();
    // Adding Peer to peer list
    m_listModel->insertRow(row);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ip);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP), ip, Qt::ToolTipRole);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::PORT), peer.address().port);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::IP_HIDDEN), ip);
    if (m_resolveCountries) {
        const QIcon ico = GuiIconProvider::instance()->getFlagIcon(peer.country());
        if (!ico.isNull()) {
            m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), ico, Qt::DecorationRole);
            const QString countryName = Net::GeoIPManager::CountryName(peer.country());
            m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), countryName, Qt::ToolTipRole);
        }
        else {
            m_missingFlags.insert(ip);
        }
    }
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), peer.connectionType());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), peer.flags());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), peer.flagsDescription(), Qt::ToolTipRole);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), peer.client().toHtmlEscaped());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payloadDownSpeed());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payloadUpSpeed());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), peer.totalDownload());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), peer.totalUpload());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::RELEVANCE), peer.relevance());
    QStringList downloadingFiles(torrent->info().filesForPiece(peer.downloadingPieceIndex()));
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWNLOADING_PIECE), downloadingFiles.join(QLatin1Char(';')));
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWNLOADING_PIECE), downloadingFiles.join(QLatin1Char('\n')), Qt::ToolTipRole);

    return m_listModel->item(row, PeerListDelegate::IP);
}

void PeerListWidget::updatePeer(const QString &ip, BitTorrent::TorrentHandle *const torrent, const BitTorrent::PeerInfo &peer)
{
    QStandardItem *item = m_peerItems.value(ip);
    int row = item->row();
    if (m_resolveCountries) {
        const QIcon ico = GuiIconProvider::instance()->getFlagIcon(peer.country());
        if (!ico.isNull()) {
            m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), ico, Qt::DecorationRole);
            const QString countryName = Net::GeoIPManager::CountryName(peer.country());
            m_listModel->setData(m_listModel->index(row, PeerListDelegate::COUNTRY), countryName, Qt::ToolTipRole);
            m_missingFlags.remove(ip);
        }
    }
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::CONNECTION), peer.connectionType());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::PORT), peer.address().port);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), peer.flags());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::FLAGS), peer.flagsDescription(), Qt::ToolTipRole);
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::CLIENT), peer.client().toHtmlEscaped());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::PROGRESS), peer.progress());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWN_SPEED), peer.payloadDownSpeed());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::UP_SPEED), peer.payloadUpSpeed());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_DOWN), peer.totalDownload());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::TOT_UP), peer.totalUpload());
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::RELEVANCE), peer.relevance());
    QStringList downloadingFiles(torrent->info().filesForPiece(peer.downloadingPieceIndex()));
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWNLOADING_PIECE), downloadingFiles.join(QLatin1String(";")));
    m_listModel->setData(m_listModel->index(row, PeerListDelegate::DOWNLOADING_PIECE), downloadingFiles.join(QLatin1String("\n")), Qt::ToolTipRole);
}

void PeerListWidget::handleResolved(const QString &ip, const QString &hostname)
{
    QStandardItem *item = m_peerItems.value(ip, nullptr);
    if (item) {
        qDebug("Resolved %s -> %s", qUtf8Printable(ip), qUtf8Printable(hostname));
        item->setData(hostname, Qt::DisplayRole);
    }
}

void PeerListWidget::handleSortColumnChanged(int col)
{
    if (col == PeerListDelegate::COUNTRY) {
        qDebug("Sorting by decoration");
        m_proxyModel->setSortRole(Qt::ToolTipRole);
    }
    else {
        m_proxyModel->setSortRole(Qt::DisplayRole);
    }
}

void PeerListWidget::wheelEvent(QWheelEvent *event)
{
    event->accept();

    if (event->modifiers() & Qt::ShiftModifier) {
        // Shift + scroll = horizontal scroll
        QWheelEvent scrollHEvent(event->pos(), event->globalPos(), event->delta(), event->buttons(), event->modifiers(), Qt::Horizontal);
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
