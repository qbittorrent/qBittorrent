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

#include <algorithm>

#include <QtGlobal>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QHostAddress>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVector>
#include <QWheelEvent>

#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/geoipmanager.h"
#include "base/net/reverseresolution.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/uithememanager.h"
#include "peerlistsortmodel.h"
#include "peersadditiondialog.h"
#include "propertieswidget.h"

struct PeerEndpoint
{
    BitTorrent::PeerAddress address;
    QString connectionType; // matches return type of `PeerInfo::connectionType()`
};

bool operator==(const PeerEndpoint &left, const PeerEndpoint &right)
{
    return (left.address == right.address) && (left.connectionType == right.connectionType);
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
std::size_t qHash(const PeerEndpoint &peerEndpoint, const std::size_t seed = 0)
{
    return qHashMulti(seed, peerEndpoint.address, peerEndpoint.connectionType);
}
#else
uint qHash(const PeerEndpoint &peerEndpoint, const uint seed = 0)
{
    return (qHash(peerEndpoint.address, seed) ^ ::qHash(peerEndpoint.connectionType));
}
#endif

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
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false);
    header()->setTextElideMode(Qt::ElideRight);

    // List Model
    m_listModel = new QStandardItemModel(0, PeerListColumns::COL_COUNT, this);
    m_listModel->setHeaderData(PeerListColumns::COUNTRY, Qt::Horizontal, tr("Country/Region")); // Country flag column
    m_listModel->setHeaderData(PeerListColumns::IP, Qt::Horizontal, tr("IP"));
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, tr("Port"));
    m_listModel->setHeaderData(PeerListColumns::FLAGS, Qt::Horizontal, tr("Flags"));
    m_listModel->setHeaderData(PeerListColumns::CONNECTION, Qt::Horizontal, tr("Connection"));
    m_listModel->setHeaderData(PeerListColumns::CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, tr("Relevance", "i.e: How relevant this peer is to us. How many pieces it has that we don't."));
    m_listModel->setHeaderData(PeerListColumns::DOWNLOADING_PIECE, Qt::Horizontal, tr("Files", "i.e. files that are being downloaded right now"));
    // Set header text alignment
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    // Proxy model to support sorting without actually altering the underlying model
    m_proxyModel = new PeerListSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_listModel);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    setModel(m_proxyModel);
    hideColumn(PeerListColumns::IP_HIDDEN);
    hideColumn(PeerListColumns::COL_COUNT);
    m_resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (!m_resolveCountries)
        hideColumn(PeerListColumns::COUNTRY);
    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if (!isColumnHidden(i))
        {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(PeerListColumns::IP, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    }
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PeerListWidget::showPeerListMenu);
    // Enable sorting
    setSortingEnabled(true);
    // IP to Hostname resolver
    updatePeerHostNameResolutionState();
    // SIGNAL/SLOT
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &PeerListWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionClicked, this, &PeerListWidget::handleSortColumnChanged);
    connect(header(), &QHeaderView::sectionMoved, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &PeerListWidget::saveSettings);
    handleSortColumnChanged(header()->sortIndicatorSection());
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &PeerListWidget::copySelectedPeers);
}

PeerListWidget::~PeerListWidget()
{
    saveSettings();
}

void PeerListWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((i == PeerListColumns::COUNTRY) && !Preferences::instance()->resolvePeerCountries())
            continue;

        const auto columnName = m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction *action = menu->addAction(columnName, this, [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            setColumnHidden(i, !checked);

            if (checked && (columnWidth(i) <= 5))
                resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void PeerListWidget::updatePeerHostNameResolutionState()
{
    if (Preferences::instance()->resolvePeerHostNames())
    {
        if (!m_resolver)
        {
            m_resolver = new Net::ReverseResolution(this);
            connect(m_resolver, &Net::ReverseResolution::ipResolved, this, &PeerListWidget::handleResolved);
            loadPeers(m_properties->getCurrentTorrent());
        }
    }
    else
    {
        delete m_resolver;
        m_resolver = nullptr;
    }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
    const bool resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (resolveCountries == m_resolveCountries)
        return;

    m_resolveCountries = resolveCountries;
    if (m_resolveCountries)
    {
        loadPeers(m_properties->getCurrentTorrent());
        showColumn(PeerListColumns::COUNTRY);
        if (columnWidth(PeerListColumns::COUNTRY) <= 0)
            resizeColumnToContents(PeerListColumns::COUNTRY);
    }
    else
    {
        hideColumn(PeerListColumns::COUNTRY);
    }
}

void PeerListWidget::showPeerListMenu()
{
    BitTorrent::Torrent *torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);

    QAction *addNewPeer = menu->addAction(UIThemeManager::instance()->getIcon(u"peers-add"_qs), tr("Add peers...")
        , this, [this, torrent]()
    {
        const QVector<BitTorrent::PeerAddress> peersList = PeersAdditionDialog::askForPeers(this);
        const int peerCount = std::count_if(peersList.cbegin(), peersList.cend(), [torrent](const BitTorrent::PeerAddress &peer)
        {
            return torrent->connectPeer(peer);
        });
        if (peerCount < peersList.length())
            QMessageBox::information(this, tr("Adding peers"), tr("Some peers cannot be added. Check the Log for details."));
        else if (peerCount > 0)
            QMessageBox::information(this, tr("Adding peers"), tr("Peers are added to this torrent."));
    });
    QAction *copyPeers = menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_qs), tr("Copy IP:port")
        , this, &PeerListWidget::copySelectedPeers);
    menu->addSeparator();
    QAction *banPeers = menu->addAction(UIThemeManager::instance()->getIcon(u"peers-remove"_qs), tr("Ban peer permanently")
        , this, &PeerListWidget::banSelectedPeers);

    // disable actions
    const auto disableAction = [](QAction *action, const QString &tooltip)
    {
        action->setEnabled(false);
        action->setToolTip(tooltip);
    };

    if (torrent->isPrivate())
        disableAction(addNewPeer, tr("Cannot add peers to a private torrent"));
    else if (torrent->isChecking())
        disableAction(addNewPeer, tr("Cannot add peers when the torrent is checking"));
    else if (torrent->isQueued())
        disableAction(addNewPeer, tr("Cannot add peers when the torrent is queued"));

    if (selectionModel()->selectedRows().isEmpty())
    {
        const QString tooltip = tr("No peer was selected");
        disableAction(copyPeers, tooltip);
        disableAction(banPeers, tooltip);
    }

    menu->popup(QCursor::pos());
}

void PeerListWidget::banSelectedPeers()
{
    // Store selected rows first as selected peers may disconnect
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();

    QVector<QString> selectedIPs;
    selectedIPs.reserve(selectedIndexes.size());

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        selectedIPs += ip;
    }

    // Confirm before banning peer
    const QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Ban peer permanently")
        , tr("Are you sure you want to permanently ban the selected peers?"));
    if (btn != QMessageBox::Yes) return;

    for (const QString &ip : selectedIPs)
    {
        BitTorrent::Session::instance()->banIP(ip);
        LogMsg(tr("Peer \"%1\" is manually banned").arg(ip));
    }
    // Refresh list
    loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::copySelectedPeers()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    QStringList selectedPeers;

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        const QString port = m_listModel->item(row, PeerListColumns::PORT)->text();

        if (!ip.contains(u'.'))  // IPv6
            selectedPeers << (u'[' + ip + u"]:" + port);
        else  // IPv4
            selectedPeers << (ip + u':' + port);
    }

    QApplication::clipboard()->setText(selectedPeers.join(u'\n'));
}

void PeerListWidget::clear()
{
    m_peerItems.clear();
    m_itemsByIP.clear();
    const int nbrows = m_listModel->rowCount();
    if (nbrows > 0)
        m_listModel->removeRows(0, nbrows);
}

void PeerListWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getPeerListState());
}

void PeerListWidget::saveSettings() const
{
    Preferences::instance()->setPeerListState(header()->saveState());
}

void PeerListWidget::loadPeers(const BitTorrent::Torrent *torrent)
{
    if (!torrent) return;

    const QVector<BitTorrent::PeerInfo> peers = torrent->peers();
    QSet<PeerEndpoint> existingPeers;
    for (auto i = m_peerItems.cbegin(); i != m_peerItems.cend(); ++i)
        existingPeers << i.key();

    for (const BitTorrent::PeerInfo &peer : peers)
    {
        if (peer.address().ip.isNull()) continue;

        bool isNewPeer = false;
        updatePeer(torrent, peer, isNewPeer);
        if (!isNewPeer)
        {
            const PeerEndpoint peerEndpoint {peer.address(), peer.connectionType()};
            existingPeers.remove(peerEndpoint);
        }
    }

    // Remove peers that are gone
    for (const PeerEndpoint &peerEndpoint : asConst(existingPeers))
    {
        QStandardItem *item = m_peerItems.take(peerEndpoint);

        QSet<QStandardItem *> &items = m_itemsByIP[peerEndpoint.address.ip];
        items.remove(item);
        if (items.isEmpty())
            m_itemsByIP.remove(peerEndpoint.address.ip);

        m_listModel->removeRow(item->row());
    }
}

void PeerListWidget::updatePeer(const BitTorrent::Torrent *torrent, const BitTorrent::PeerInfo &peer, bool &isNewPeer)
{
    const PeerEndpoint peerEndpoint {peer.address(), peer.connectionType()};
    const QString peerIp = peerEndpoint.address.ip.toString();
    const Qt::Alignment intDataTextAlignment = Qt::AlignRight | Qt::AlignVCenter;

    const auto setModelData =
        [this] (const int row, const int column, const QString &displayData
                , const QVariant &underlyingData, const Qt::Alignment textAlignmentData = {}
                , const QString &toolTip = {})
    {
        const QMap<int, QVariant> data =
        {
            {Qt::DisplayRole, displayData},
            {PeerListSortModel::UnderlyingDataRole, underlyingData},
            {Qt::TextAlignmentRole, QVariant {textAlignmentData}},
            {Qt::ToolTipRole, toolTip}
        };
        m_listModel->setItemData(m_listModel->index(row, column), data);
    };

    auto itemIter = m_peerItems.find(peerEndpoint);
    isNewPeer = (itemIter == m_peerItems.end());
    if (isNewPeer)
    {
        // new item
        const int row = m_listModel->rowCount();
        m_listModel->insertRow(row);

        setModelData(row, PeerListColumns::IP, peerIp, peerIp, {}, peerIp);
        setModelData(row, PeerListColumns::PORT, QString::number(peer.address().port), peer.address().port, intDataTextAlignment);
        setModelData(row, PeerListColumns::IP_HIDDEN, peerIp, peerIp);

        itemIter = m_peerItems.insert(peerEndpoint, m_listModel->item(row, PeerListColumns::IP));
        m_itemsByIP[peerEndpoint.address.ip].insert(itemIter.value());
    }

    const int row = (*itemIter)->row();
    const bool hideValues = Preferences::instance()->getHideZeroValues();

    setModelData(row, PeerListColumns::CONNECTION, peer.connectionType(), peer.connectionType());
    setModelData(row, PeerListColumns::FLAGS, peer.flags(), peer.flags(), {}, peer.flagsDescription());
    const QString client = peer.client().toHtmlEscaped();
    setModelData(row, PeerListColumns::CLIENT, client, client, {}, client);
    setModelData(row, PeerListColumns::PROGRESS, (Utils::String::fromDouble(peer.progress() * 100, 1) + u'%'), peer.progress(), intDataTextAlignment);
    const QString downSpeed = (hideValues && (peer.payloadDownSpeed() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.payloadDownSpeed(), true);
    setModelData(row, PeerListColumns::DOWN_SPEED, downSpeed, peer.payloadDownSpeed(), intDataTextAlignment);
    const QString upSpeed = (hideValues && (peer.payloadUpSpeed() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.payloadUpSpeed(), true);
    setModelData(row, PeerListColumns::UP_SPEED, upSpeed, peer.payloadUpSpeed(), intDataTextAlignment);
    const QString totalDown = (hideValues && (peer.totalDownload() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.totalDownload());
    setModelData(row, PeerListColumns::TOT_DOWN, totalDown, peer.totalDownload(), intDataTextAlignment);
    const QString totalUp = (hideValues && (peer.totalUpload() <= 0)) ? QString {} : Utils::Misc::friendlyUnit(peer.totalUpload());
    setModelData(row, PeerListColumns::TOT_UP, totalUp, peer.totalUpload(), intDataTextAlignment);
    setModelData(row, PeerListColumns::RELEVANCE, (Utils::String::fromDouble(peer.relevance() * 100, 1) + u'%'), peer.relevance(), intDataTextAlignment);

    const PathList filePaths = torrent->info().filesForPiece(peer.downloadingPieceIndex());
    QStringList downloadingFiles;
    downloadingFiles.reserve(filePaths.size());
    for (const Path &filePath : filePaths)
        downloadingFiles.append(filePath.toString());
    const QString downloadingFilesDisplayValue = downloadingFiles.join(u';');
    setModelData(row, PeerListColumns::DOWNLOADING_PIECE, downloadingFilesDisplayValue, downloadingFilesDisplayValue, {}, downloadingFiles.join(u'\n'));

    if (m_resolver)
        m_resolver->resolve(peerEndpoint.address.ip);

    if (m_resolveCountries)
    {
        const QIcon icon = UIThemeManager::instance()->getFlagIcon(peer.country());
        if (!icon.isNull())
        {
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), icon, Qt::DecorationRole);
            const QString countryName = Net::GeoIPManager::CountryName(peer.country());
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), countryName, Qt::ToolTipRole);
        }
    }
}

int PeerListWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = header()->count(); i < iMax; ++i)
    {
        if (!isColumnHidden(i))
            ++count;
    }

    return count;
}

void PeerListWidget::handleResolved(const QHostAddress &ip, const QString &hostname) const
{
    if (hostname.isEmpty())
        return;

    const QSet<QStandardItem *> items = m_itemsByIP.value(ip);
    for (QStandardItem *item : items)
        item->setData(hostname, Qt::DisplayRole);
}

void PeerListWidget::handleSortColumnChanged(const int col)
{
    if (col == PeerListColumns::COUNTRY)
        m_proxyModel->setSortRole(Qt::ToolTipRole);
    else
        m_proxyModel->setSortRole(PeerListSortModel::UnderlyingDataRole);
}

void PeerListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
            , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
            , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
