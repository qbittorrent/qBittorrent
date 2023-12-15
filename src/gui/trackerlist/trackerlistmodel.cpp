/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "trackerlistmodel.h"

#include <chrono>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>

#include <QColor>
#include <QList>
#include <QPointer>
#include <QScopeGuard>
#include <QTimer>

#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/utils/misc.h"

using namespace std::chrono_literals;
using namespace boost::multi_index;

namespace
{
    const std::chrono::milliseconds ANNOUNCE_TIME_REFRESH_INTERVAL = 4s;

    const char STR_WORKING[] = QT_TRANSLATE_NOOP("TrackerListModel", "Working");
    const char STR_DISABLED[] = QT_TRANSLATE_NOOP("TrackerListModel", "Disabled");
    const char STR_TORRENT_DISABLED[] = QT_TRANSLATE_NOOP("TrackerListModel", "Disabled for this torrent");
    const char STR_PRIVATE_MSG[] = QT_TRANSLATE_NOOP("TrackerListModel", "This torrent is private");

    QString prettyCount(const int val)
    {
        return (val > -1) ? QString::number(val) : TrackerListModel::tr("N/A");
    }

    QString toString(const BitTorrent::TrackerEntryStatus status)
    {
        switch (status)
        {
        case BitTorrent::TrackerEntryStatus::Working:
            return TrackerListModel::tr(STR_WORKING);
        case BitTorrent::TrackerEntryStatus::Updating:
            return TrackerListModel::tr("Updating...");
        case BitTorrent::TrackerEntryStatus::NotWorking:
            return TrackerListModel::tr("Not working");
        case BitTorrent::TrackerEntryStatus::TrackerError:
            return TrackerListModel::tr("Tracker error");
        case BitTorrent::TrackerEntryStatus::Unreachable:
            return TrackerListModel::tr("Unreachable");
        case BitTorrent::TrackerEntryStatus::NotContacted:
            return TrackerListModel::tr("Not contacted yet");
        }
        return TrackerListModel::tr("Invalid status!");
    }

    QString statusDHT(const BitTorrent::Torrent *torrent)
    {
        if (!torrent->session()->isDHTEnabled())
            return TrackerListModel::tr(STR_DISABLED);

        if (torrent->isPrivate() || torrent->isDHTDisabled())
            return TrackerListModel::tr(STR_TORRENT_DISABLED);

        return TrackerListModel::tr(STR_WORKING);
    }

    QString statusPeX(const BitTorrent::Torrent *torrent)
    {
        if (!torrent->session()->isPeXEnabled())
            return TrackerListModel::tr(STR_DISABLED);

        if (torrent->isPrivate() || torrent->isPEXDisabled())
            return TrackerListModel::tr(STR_TORRENT_DISABLED);

        return TrackerListModel::tr(STR_WORKING);
    }

    QString statusLSD(const BitTorrent::Torrent *torrent)
    {
        if (!torrent->session()->isLSDEnabled())
            return TrackerListModel::tr(STR_DISABLED);

        if (torrent->isPrivate() || torrent->isLSDDisabled())
            return TrackerListModel::tr(STR_TORRENT_DISABLED);

        return TrackerListModel::tr(STR_WORKING);
    }
}

std::size_t hash_value(const QString &string)
{
    return qHash(string);
}

struct TrackerListModel::Item final
{
    QString name {};
    int tier = -1;
    int btVersion = -1;
    BitTorrent::TrackerEntryStatus status = BitTorrent::TrackerEntryStatus::NotContacted;
    QString message {};

    int numPeers = -1;
    int numSeeds = -1;
    int numLeeches = -1;
    int numDownloaded = -1;

    QDateTime nextAnnounceTime {};
    QDateTime minAnnounceTime {};

    qint64 secsToNextAnnounce = 0;
    qint64 secsToMinAnnounce = 0;
    QDateTime announceTimestamp;

    std::weak_ptr<Item> parentItem {};

    multi_index_container<std::shared_ptr<Item>, indexed_by<
            random_access<>,
            hashed_unique<tag<struct ByID>, composite_key<
                    Item,
                    member<Item, QString, &Item::name>,
                    member<Item, int, &Item::btVersion>
            >>
    >> childItems {};

    Item(QStringView name, QStringView message);
    explicit Item(const BitTorrent::TrackerEntry &trackerEntry);
    Item(const std::shared_ptr<Item> &parentItem, const BitTorrent::TrackerEndpointEntry &endpointEntry);

    void fillFrom(const BitTorrent::TrackerEntry &trackerEntry);
    void fillFrom(const BitTorrent::TrackerEndpointEntry &endpointEntry);
};

class TrackerListModel::Items final : public multi_index_container<
        std::shared_ptr<Item>,
        indexed_by<
                random_access<>,
                hashed_unique<tag<struct ByName>, member<Item, QString, &Item::name>>>>
{
};

TrackerListModel::Item::Item(const QStringView name, const QStringView message)
    : name {name.toString()}
    , message {message.toString()}
{
}

TrackerListModel::Item::Item(const BitTorrent::TrackerEntry &trackerEntry)
    : name {trackerEntry.url}
{
    fillFrom(trackerEntry);
}

TrackerListModel::Item::Item(const std::shared_ptr<Item> &parentItem, const BitTorrent::TrackerEndpointEntry &endpointEntry)
    : name {endpointEntry.name}
    , btVersion {endpointEntry.btVersion}
    , parentItem {parentItem}
{
    fillFrom(endpointEntry);
}

void TrackerListModel::Item::fillFrom(const BitTorrent::TrackerEntry &trackerEntry)
{
    Q_ASSERT(parentItem.expired());
    Q_ASSERT(trackerEntry.url == name);

    tier = trackerEntry.tier;
    status = trackerEntry.status;
    message = trackerEntry.message;
    numPeers = trackerEntry.numPeers;
    numSeeds = trackerEntry.numSeeds;
    numLeeches = trackerEntry.numLeeches;
    numDownloaded = trackerEntry.numDownloaded;
    nextAnnounceTime = trackerEntry.nextAnnounceTime;
    minAnnounceTime = trackerEntry.minAnnounceTime;
    secsToNextAnnounce = 0;
    secsToMinAnnounce = 0;
    announceTimestamp = QDateTime();
}

void TrackerListModel::Item::fillFrom(const BitTorrent::TrackerEndpointEntry &endpointEntry)
{
    Q_ASSERT(!parentItem.expired());
    Q_ASSERT(endpointEntry.name == name);
    Q_ASSERT(endpointEntry.btVersion == btVersion);

    status = endpointEntry.status;
    message = endpointEntry.message;
    numPeers = endpointEntry.numPeers;
    numSeeds = endpointEntry.numSeeds;
    numLeeches = endpointEntry.numLeeches;
    numDownloaded = endpointEntry.numDownloaded;
    nextAnnounceTime = endpointEntry.nextAnnounceTime;
    minAnnounceTime = endpointEntry.minAnnounceTime;
    secsToNextAnnounce = 0;
    secsToMinAnnounce = 0;
    announceTimestamp = QDateTime();
}

TrackerListModel::TrackerListModel(BitTorrent::Session *btSession, QObject *parent)
    : QAbstractItemModel(parent)
    , m_btSession {btSession}
    , m_items {std::make_unique<Items>()}
    , m_announceRefreshTimer {new QTimer(this)}
{
    Q_ASSERT(m_btSession);

    m_announceRefreshTimer->setSingleShot(true);
    connect(m_announceRefreshTimer, &QTimer::timeout, this, &TrackerListModel::refreshAnnounceTimes);

    connect(m_btSession, &BitTorrent::Session::trackersAdded, this
            , [this](BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &newTrackers)
    {
        if (torrent == m_torrent)
            onTrackersAdded(newTrackers);
    });
    connect(m_btSession, &BitTorrent::Session::trackersRemoved, this
            , [this](BitTorrent::Torrent *torrent, const QStringList &deletedTrackers)
    {
        if (torrent == m_torrent)
            onTrackersRemoved(deletedTrackers);
    });
    connect(m_btSession, &BitTorrent::Session::trackersChanged, this
            , [this](BitTorrent::Torrent *torrent)
    {
        if (torrent == m_torrent)
            onTrackersChanged();
    });
    connect(m_btSession, &BitTorrent::Session::trackerEntriesUpdated, this
            , [this](BitTorrent::Torrent *torrent, const QHash<QString, BitTorrent::TrackerEntry> &updatedTrackers)
    {
        if (torrent == m_torrent)
            onTrackersUpdated(updatedTrackers);
    });
}

TrackerListModel::~TrackerListModel() = default;

void TrackerListModel::setTorrent(BitTorrent::Torrent *torrent)
{
    beginResetModel();
    [[maybe_unused]] const auto modelResetGuard = qScopeGuard([this] { endResetModel(); });

    if (m_torrent)
        m_items->clear();

    m_torrent = torrent;

    if (m_torrent)
        populate();
    else
        m_announceRefreshTimer->stop();
}

BitTorrent::Torrent *TrackerListModel::torrent() const
{
    return m_torrent;
}

void TrackerListModel::populate()
{
    Q_ASSERT(m_torrent);

    const QList<BitTorrent::TrackerEntry> trackerEntries = m_torrent->trackers();
    m_items->reserve(trackerEntries.size() + STICKY_ROW_COUNT);

    const QString &privateTorrentMessage = m_torrent->isPrivate() ? tr(STR_PRIVATE_MSG) : u""_s;
    m_items->emplace_back(std::make_shared<Item>(u"** [DHT] **", privateTorrentMessage));
    m_items->emplace_back(std::make_shared<Item>(u"** [PeX] **", privateTorrentMessage));
    m_items->emplace_back(std::make_shared<Item>(u"** [LSD] **", privateTorrentMessage));

    using TorrentPtr = QPointer<const BitTorrent::Torrent>;
    m_torrent->fetchPeerInfo([this, torrent = TorrentPtr(m_torrent)](const QList<BitTorrent::PeerInfo> &peers)
    {
       if (torrent != m_torrent)
           return;

       // XXX: libtorrent should provide this info...
       // Count peers from DHT, PeX, LSD
       uint seedsDHT = 0, seedsPeX = 0, seedsLSD = 0, peersDHT = 0, peersPeX = 0, peersLSD = 0;
       for (const BitTorrent::PeerInfo &peer : peers)
       {
            if (peer.isConnecting())
                continue;

            if (peer.isSeed())
            {
                if (peer.fromDHT())
                    ++seedsDHT;

                if (peer.fromPeX())
                    ++seedsPeX;

                if (peer.fromLSD())
                    ++seedsLSD;
            }
            else
            {
                if (peer.fromDHT())
                    ++peersDHT;

                if (peer.fromPeX())
                    ++peersPeX;

                if (peer.fromLSD())
                    ++peersLSD;
            }
        }

        auto &itemsByPos = m_items->get<0>();
        itemsByPos.modify((itemsByPos.begin() + ROW_DHT), [&seedsDHT, &peersDHT](std::shared_ptr<Item> &item)
        {
            item->numSeeds = seedsDHT;
            item->numLeeches = peersDHT;
            return true;
        });
        itemsByPos.modify((itemsByPos.begin() + ROW_PEX), [&seedsPeX, &peersPeX](std::shared_ptr<Item> &item)
        {
            item->numSeeds = seedsPeX;
            item->numLeeches = peersPeX;
            return true;
        });
        itemsByPos.modify((itemsByPos.begin() + ROW_LSD), [&seedsLSD, &peersLSD](std::shared_ptr<Item> &item)
        {
            item->numSeeds = seedsLSD;
            item->numLeeches = peersLSD;
            return true;
        });

        emit dataChanged(index(ROW_DHT, COL_SEEDS), index(ROW_LSD, COL_LEECHES));
    });

    for (const BitTorrent::TrackerEntry &trackerEntry : trackerEntries)
        addTrackerItem(trackerEntry);

    m_announceTimestamp = QDateTime::currentDateTime();
    m_announceRefreshTimer->start(ANNOUNCE_TIME_REFRESH_INTERVAL);
}

std::shared_ptr<TrackerListModel::Item> TrackerListModel::createTrackerItem(const BitTorrent::TrackerEntry &trackerEntry)
{
    auto item = std::make_shared<Item>(trackerEntry);
    for (const auto &[id, endpointEntry] : trackerEntry.endpointEntries.asKeyValueRange())
    {
        item->childItems.emplace_back(std::make_shared<Item>(item, endpointEntry));
    }

    return item;
}

void TrackerListModel::addTrackerItem(const BitTorrent::TrackerEntry &trackerEntry)
{
    [[maybe_unused]] const auto &[iter, res] = m_items->emplace_back(createTrackerItem(trackerEntry));
    Q_ASSERT(res);
}

void TrackerListModel::updateTrackerItem(const std::shared_ptr<Item> &item, const BitTorrent::TrackerEntry &trackerEntry)
{
    QSet<std::pair<QString, int>> endpointItemIDs;
    QList<std::shared_ptr<Item>> newEndpointItems;
    for (const auto &[id, endpointEntry] : trackerEntry.endpointEntries.asKeyValueRange())
    {
        endpointItemIDs.insert(id);

        auto &itemsByID = item->childItems.get<ByID>();
        if (const auto &iter = itemsByID.find(std::make_tuple(id.first, id.second)); iter != itemsByID.end())
        {
            (*iter)->fillFrom(endpointEntry);
        }
        else
        {
            newEndpointItems.emplace_back(std::make_shared<Item>(item, endpointEntry));
        }
    }

    const auto &itemsByPos = m_items->get<0>();
    const auto trackerRow = std::distance(itemsByPos.begin(), itemsByPos.iterator_to(item));
    const auto trackerIndex = index(trackerRow, 0);

    auto it = item->childItems.begin();
    while (it != item->childItems.end())
    {
        if (const auto endpointItemID = std::make_pair((*it)->name, (*it)->btVersion)
                ; endpointItemIDs.contains(endpointItemID))
        {
            ++it;
        }
        else
        {
            const auto row = std::distance(item->childItems.begin(), it);
            beginRemoveRows(trackerIndex, row, row);
            it = item->childItems.erase(it);
            endRemoveRows();
        }
    }

    const auto numRows = rowCount(trackerIndex);
    emit dataChanged(index(0, 0, trackerIndex), index((numRows - 1), (columnCount(trackerIndex) - 1), trackerIndex));

    if (!newEndpointItems.isEmpty())
    {
        beginInsertRows(trackerIndex, numRows, (numRows + newEndpointItems.size() - 1));
        for (const auto &newEndpointItem : asConst(newEndpointItems))
            item->childItems.get<0>().push_back(newEndpointItem);
        endInsertRows();
    }

    item->fillFrom(trackerEntry);
    emit dataChanged(trackerIndex, index(trackerRow, (columnCount() - 1)));
}

void TrackerListModel::refreshAnnounceTimes()
{
    if (!m_torrent)
        return;

    m_announceTimestamp = QDateTime::currentDateTime();
    emit dataChanged(index(0, COL_NEXT_ANNOUNCE), index((rowCount() - 1), COL_MIN_ANNOUNCE));
    for (int i = 0; i < rowCount(); ++i)
    {
        const QModelIndex parentIndex = index(i, 0);
        emit dataChanged(index(0, COL_NEXT_ANNOUNCE, parentIndex), index((rowCount(parentIndex) - 1), COL_MIN_ANNOUNCE, parentIndex));
    }

    m_announceRefreshTimer->start(ANNOUNCE_TIME_REFRESH_INTERVAL);
}

int TrackerListModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return COL_COUNT;
}

int TrackerListModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_items->size();

    const auto *item = static_cast<Item *>(parent.internalPointer());
    Q_ASSERT(item);
    if (!item) [[unlikely]]
        return 0;

    return item->childItems.size();
}

QVariant TrackerListModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    switch (role)
    {
    case Qt::DisplayRole:
        switch (section)
        {
        case COL_URL:
            return tr("URL/Announce endpoint");
        case COL_TIER:
            return tr("Tier");
        case COL_PROTOCOL:
            return tr("Protocol");
        case COL_STATUS:
            return tr("Status");
        case COL_PEERS:
            return tr("Peers");
        case COL_SEEDS:
            return tr("Seeds");
        case COL_LEECHES:
            return tr("Leeches");
        case COL_TIMES_DOWNLOADED:
            return tr("Times Downloaded");
        case COL_MSG:
            return tr("Message");
        case COL_NEXT_ANNOUNCE:
            return tr("Next announce");
        case COL_MIN_ANNOUNCE:
            return tr("Min announce");
        default:
            return {};
        }

    case Qt::TextAlignmentRole:
        switch (section)
        {
        case COL_TIER:
        case COL_PEERS:
        case COL_SEEDS:
        case COL_LEECHES:
        case COL_TIMES_DOWNLOADED:
        case COL_NEXT_ANNOUNCE:
        case COL_MIN_ANNOUNCE:
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        default:
            return {};
        }

    default:
        return {};
    }
}

QVariant TrackerListModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    auto *itemPtr = static_cast<Item *>(index.internalPointer());
    Q_ASSERT(itemPtr);
    if (!itemPtr) [[unlikely]]
        return {};

    if (itemPtr->announceTimestamp != m_announceTimestamp)
    {
        itemPtr->secsToNextAnnounce = std::max<qint64>(0, m_announceTimestamp.secsTo(itemPtr->nextAnnounceTime));
        itemPtr->secsToMinAnnounce = std::max<qint64>(0, m_announceTimestamp.secsTo(itemPtr->minAnnounceTime));
        itemPtr->announceTimestamp = m_announceTimestamp;
    }

    const bool isEndpoint = !itemPtr->parentItem.expired();

    switch (role)
    {
    case Qt::TextAlignmentRole:
        switch (index.column())
        {
        case COL_TIER:
        case COL_PROTOCOL:
        case COL_PEERS:
        case COL_SEEDS:
        case COL_LEECHES:
        case COL_TIMES_DOWNLOADED:
        case COL_NEXT_ANNOUNCE:
        case COL_MIN_ANNOUNCE:
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        default:
            return {};
        }

    case Qt::ForegroundRole:
        // TODO: Make me configurable via UI Theme
        if (!index.parent().isValid() && (index.row() < STICKY_ROW_COUNT))
            return QColorConstants::Svg::grey;
        return {};

    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column())
        {
        case COL_URL:
            return itemPtr->name;
        case COL_TIER:
            return (isEndpoint || (index.row() < STICKY_ROW_COUNT)) ? QString() : QString::number(itemPtr->tier);
        case COL_PROTOCOL:
            return isEndpoint ? tr("v%1").arg(itemPtr->btVersion) : QString();
        case COL_STATUS:
            if (isEndpoint)
                return toString(itemPtr->status);
            if (index.row() == ROW_DHT)
                return statusDHT(m_torrent);
            if (index.row() == ROW_PEX)
                return statusPeX(m_torrent);
            if (index.row() == ROW_LSD)
                return statusLSD(m_torrent);
            return toString(itemPtr->status);
        case COL_PEERS:
            return prettyCount(itemPtr->numPeers);
        case COL_SEEDS:
            return prettyCount(itemPtr->numSeeds);
        case COL_LEECHES:
            return prettyCount(itemPtr->numLeeches);
        case COL_TIMES_DOWNLOADED:
            return prettyCount(itemPtr->numDownloaded);
        case COL_MSG:
            return itemPtr->message;
        case COL_NEXT_ANNOUNCE:
            return Utils::Misc::userFriendlyDuration(itemPtr->secsToNextAnnounce, -1, Utils::Misc::TimeResolution::Seconds);
        case COL_MIN_ANNOUNCE:
            return Utils::Misc::userFriendlyDuration(itemPtr->secsToMinAnnounce, -1, Utils::Misc::TimeResolution::Seconds);
        default:
            return {};
        }

    case SortRole:
        switch (index.column())
        {
        case COL_URL:
            return itemPtr->name;
        case COL_TIER:
            return isEndpoint ? -1 : itemPtr->tier;
        case COL_PROTOCOL:
            return isEndpoint ? itemPtr->btVersion : -1;
        case COL_STATUS:
            return toString(itemPtr->status);
        case COL_PEERS:
            return itemPtr->numPeers;
        case COL_SEEDS:
            return itemPtr->numSeeds;
        case COL_LEECHES:
            return itemPtr->numLeeches;
        case COL_TIMES_DOWNLOADED:
            return itemPtr->numDownloaded;
        case COL_MSG:
            return itemPtr->message;
        case COL_NEXT_ANNOUNCE:
            return itemPtr->secsToNextAnnounce;
        case COL_MIN_ANNOUNCE:
            return itemPtr->secsToMinAnnounce;
        default:
            return {};
        }

    default:
        break;
    }

    return {};
}

QModelIndex TrackerListModel::index(const int row, const int column, const QModelIndex &parent) const
{
    if ((column < 0) || (column >= columnCount()))
        return {};

    if ((row < 0) || (row >= rowCount(parent)))
        return {};

    const std::shared_ptr<Item> item = parent.isValid()
            ? m_items->at(static_cast<std::size_t>(parent.row()))->childItems.at(row)
            : m_items->at(static_cast<std::size_t>(row));
    return createIndex(row, column, item.get());
}

QModelIndex TrackerListModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    const auto *item = static_cast<Item *>(index.internalPointer());
    Q_ASSERT(item);
    if (!item) [[unlikely]]
        return {};

    const std::shared_ptr<Item> parentItem = item->parentItem.lock();
    if (!parentItem)
        return {};

    const auto &itemsByName = m_items->get<ByName>();
    auto itemsByNameIter = itemsByName.find(parentItem->name);
    Q_ASSERT(itemsByNameIter != itemsByName.end());
    if (itemsByNameIter == itemsByName.end()) [[unlikely]]
        return {};

    const auto &itemsByPosIter = m_items->project<0>(itemsByNameIter);
    const auto row = std::distance(m_items->get<0>().begin(), itemsByPosIter);

    // From https://doc.qt.io/qt-6/qabstractitemmodel.html#parent:
    // A common convention used in models that expose tree data structures is that only items
    // in the first column have children. For that case, when reimplementing this function in
    // a subclass the column of the returned QModelIndex would be 0.
    return createIndex(row, 0, parentItem.get());
}

void TrackerListModel::onTrackersAdded(const QList<BitTorrent::TrackerEntry> &newTrackers)
{
    const auto row = rowCount();
    beginInsertRows({}, row, (row + newTrackers.size() - 1));
    for (const BitTorrent::TrackerEntry &trackerEntry : newTrackers)
        addTrackerItem(trackerEntry);
    endInsertRows();
}

void TrackerListModel::onTrackersRemoved(const QStringList &deletedTrackers)
{
    for (const QString &trackerURL : deletedTrackers)
    {
        auto &itemsByName = m_items->get<ByName>();
        if (auto iter = itemsByName.find(trackerURL); iter != itemsByName.end())
        {
            const auto &iterByPos = m_items->project<0>(iter);
            const auto row = std::distance(m_items->get<0>().begin(), iterByPos);
            beginRemoveRows({}, row, row);
            itemsByName.erase(iter);
            endRemoveRows();
        }
    }
}

void TrackerListModel::onTrackersChanged()
{
    QSet<QString> trackerItemIDs;
    for (int i = 0; i < STICKY_ROW_COUNT; ++i)
        trackerItemIDs.insert(m_items->at(i)->name);

    QList<std::shared_ptr<Item>> newTrackerItems;
    for (const BitTorrent::TrackerEntry &trackerEntry : m_torrent->trackers())
    {
        trackerItemIDs.insert(trackerEntry.url);

        auto &itemsByName = m_items->get<ByName>();
        if (const auto &iter = itemsByName.find(trackerEntry.url); iter != itemsByName.end())
        {
            updateTrackerItem(*iter, trackerEntry);
        }
        else
        {
            newTrackerItems.emplace_back(createTrackerItem(trackerEntry));
        }
    }

    auto it = m_items->begin();
    while (it != m_items->end())
    {
        if (trackerItemIDs.contains((*it)->name))
        {
            ++it;
        }
        else
        {
            const auto row = std::distance(m_items->begin(), it);
            beginRemoveRows({}, row, row);
            it = m_items->erase(it);
            endRemoveRows();
        }
    }

    if (!newTrackerItems.isEmpty())
    {
        const auto numRows = rowCount();
        beginInsertRows({}, numRows, (numRows + newTrackerItems.size() - 1));
        for (const auto &newTrackerItem : asConst(newTrackerItems))
            m_items->get<0>().push_back(newTrackerItem);
        endInsertRows();
    }
}

void TrackerListModel::onTrackersUpdated(const QHash<QString, BitTorrent::TrackerEntry> &updatedTrackers)
{
    for (const auto &[url, entry] : updatedTrackers.asKeyValueRange())
    {
        auto &itemsByName = m_items->get<ByName>();
        if (const auto &iter = itemsByName.find(entry.url); iter != itemsByName.end()) [[likely]]
        {
            updateTrackerItem(*iter, entry);
        }
    }
}
