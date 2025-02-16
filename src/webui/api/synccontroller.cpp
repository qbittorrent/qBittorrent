/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "synccontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaObject>

#include "base/algorithm.h"
#include "base/bittorrent/cachestatus.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/net/geoipmanager.h"
#include "base/preferences.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "serialize/serialize_torrent.h"

namespace
{
    // Sync main data keys
    const QString KEY_SYNC_MAINDATA_QUEUEING = u"queueing"_s;
    const QString KEY_SYNC_MAINDATA_REFRESH_INTERVAL = u"refresh_interval"_s;
    const QString KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS = u"use_alt_speed_limits"_s;
    const QString KEY_SYNC_MAINDATA_USE_SUBCATEGORIES = u"use_subcategories"_s;

    // Sync torrent peers keys
    const QString KEY_SYNC_TORRENT_PEERS_SHOW_FLAGS = u"show_flags"_s;

    // Peer keys
    const QString KEY_PEER_CLIENT = u"client"_s;
    const QString KEY_PEER_ID_CLIENT = u"peer_id_client"_s;
    const QString KEY_PEER_CONNECTION_TYPE = u"connection"_s;
    const QString KEY_PEER_COUNTRY = u"country"_s;
    const QString KEY_PEER_COUNTRY_CODE = u"country_code"_s;
    const QString KEY_PEER_DOWN_SPEED = u"dl_speed"_s;
    const QString KEY_PEER_FILES = u"files"_s;
    const QString KEY_PEER_FLAGS = u"flags"_s;
    const QString KEY_PEER_FLAGS_DESCRIPTION = u"flags_desc"_s;
    const QString KEY_PEER_IP = u"ip"_s;
    const QString KEY_PEER_PORT = u"port"_s;
    const QString KEY_PEER_PROGRESS = u"progress"_s;
    const QString KEY_PEER_RELEVANCE = u"relevance"_s;
    const QString KEY_PEER_TOT_DOWN = u"downloaded"_s;
    const QString KEY_PEER_TOT_UP = u"uploaded"_s;
    const QString KEY_PEER_UP_SPEED = u"up_speed"_s;

    // TransferInfo keys
    const QString KEY_TRANSFER_CONNECTION_STATUS = u"connection_status"_s;
    const QString KEY_TRANSFER_DHT_NODES = u"dht_nodes"_s;
    const QString KEY_TRANSFER_DLDATA = u"dl_info_data"_s;
    const QString KEY_TRANSFER_DLRATELIMIT = u"dl_rate_limit"_s;
    const QString KEY_TRANSFER_DLSPEED = u"dl_info_speed"_s;
    const QString KEY_TRANSFER_FREESPACEONDISK = u"free_space_on_disk"_s;
    const QString KEY_TRANSFER_LAST_EXTERNAL_ADDRESS_V4 = u"last_external_address_v4"_s;
    const QString KEY_TRANSFER_LAST_EXTERNAL_ADDRESS_V6 = u"last_external_address_v6"_s;
    const QString KEY_TRANSFER_UPDATA = u"up_info_data"_s;
    const QString KEY_TRANSFER_UPRATELIMIT = u"up_rate_limit"_s;
    const QString KEY_TRANSFER_UPSPEED = u"up_info_speed"_s;

    // Statistics keys
    const QString KEY_TRANSFER_ALLTIME_DL = u"alltime_dl"_s;
    const QString KEY_TRANSFER_ALLTIME_UL = u"alltime_ul"_s;
    const QString KEY_TRANSFER_AVERAGE_TIME_QUEUE = u"average_time_queue"_s;
    const QString KEY_TRANSFER_GLOBAL_RATIO = u"global_ratio"_s;
    const QString KEY_TRANSFER_QUEUED_IO_JOBS = u"queued_io_jobs"_s;
    const QString KEY_TRANSFER_READ_CACHE_HITS = u"read_cache_hits"_s;
    const QString KEY_TRANSFER_READ_CACHE_OVERLOAD = u"read_cache_overload"_s;
    const QString KEY_TRANSFER_TOTAL_BUFFERS_SIZE = u"total_buffers_size"_s;
    const QString KEY_TRANSFER_TOTAL_PEER_CONNECTIONS = u"total_peer_connections"_s;
    const QString KEY_TRANSFER_TOTAL_QUEUED_SIZE = u"total_queued_size"_s;
    const QString KEY_TRANSFER_TOTAL_WASTE_SESSION = u"total_wasted_session"_s;
    const QString KEY_TRANSFER_WRITE_CACHE_OVERLOAD = u"write_cache_overload"_s;

    const QString KEY_SUFFIX_REMOVED = u"_removed"_s;

    const QString KEY_CATEGORIES = u"categories"_s;
    const QString KEY_CATEGORIES_REMOVED = KEY_CATEGORIES + KEY_SUFFIX_REMOVED;
    const QString KEY_TAGS = u"tags"_s;
    const QString KEY_TAGS_REMOVED = KEY_TAGS + KEY_SUFFIX_REMOVED;
    const QString KEY_TORRENTS = u"torrents"_s;
    const QString KEY_TORRENTS_REMOVED = KEY_TORRENTS + KEY_SUFFIX_REMOVED;
    const QString KEY_TRACKERS = u"trackers"_s;
    const QString KEY_TRACKERS_REMOVED = KEY_TRACKERS + KEY_SUFFIX_REMOVED;
    const QString KEY_SERVER_STATE = u"server_state"_s;
    const QString KEY_FULL_UPDATE = u"full_update"_s;
    const QString KEY_RESPONSE_ID = u"rid"_s;

    QVariantMap processMap(const QVariantMap &prevData, const QVariantMap &data);
    std::pair<QVariantMap, QVariantList> processHash(QVariantHash prevData, const QVariantHash &data);
    std::pair<QVariantList, QVariantList> processList(QVariantList prevData, const QVariantList &data);
    QJsonObject generateSyncData(int acceptedResponseId, const QVariantMap &data, QVariantMap &lastAcceptedData, QVariantMap &lastData);

    QVariantMap getTransferInfo()
    {
        QVariantMap map;
        const auto *session = BitTorrent::Session::instance();

        const BitTorrent::SessionStatus &sessionStatus = session->status();
        const BitTorrent::CacheStatus &cacheStatus = session->cacheStatus();
        map[KEY_TRANSFER_DLSPEED] = sessionStatus.payloadDownloadRate;
        map[KEY_TRANSFER_DLDATA] = sessionStatus.totalPayloadDownload;
        map[KEY_TRANSFER_UPSPEED] = sessionStatus.payloadUploadRate;
        map[KEY_TRANSFER_UPDATA] = sessionStatus.totalPayloadUpload;
        map[KEY_TRANSFER_DLRATELIMIT] = session->downloadSpeedLimit();
        map[KEY_TRANSFER_UPRATELIMIT] = session->uploadSpeedLimit();

        const qint64 atd = sessionStatus.allTimeDownload;
        const qint64 atu = sessionStatus.allTimeUpload;
        map[KEY_TRANSFER_ALLTIME_DL] = atd;
        map[KEY_TRANSFER_ALLTIME_UL] = atu;
        map[KEY_TRANSFER_TOTAL_WASTE_SESSION] = sessionStatus.totalWasted;
        map[KEY_TRANSFER_GLOBAL_RATIO] = ((atd > 0) && (atu > 0)) ? Utils::String::fromDouble(static_cast<qreal>(atu) / atd, 2) : u"-"_s;
        map[KEY_TRANSFER_TOTAL_PEER_CONNECTIONS] = sessionStatus.peersCount;

        const qreal readRatio = cacheStatus.readRatio;  // TODO: remove when LIBTORRENT_VERSION_NUM >= 20000
        map[KEY_TRANSFER_READ_CACHE_HITS] = (readRatio > 0) ? Utils::String::fromDouble(100 * readRatio, 2) : u"0"_s;
        map[KEY_TRANSFER_TOTAL_BUFFERS_SIZE] = cacheStatus.totalUsedBuffers * 16 * 1024;

        map[KEY_TRANSFER_WRITE_CACHE_OVERLOAD] = ((sessionStatus.diskWriteQueue > 0) && (sessionStatus.peersCount > 0))
            ? Utils::String::fromDouble((100. * sessionStatus.diskWriteQueue / sessionStatus.peersCount), 2)
            : u"0"_s;
        map[KEY_TRANSFER_READ_CACHE_OVERLOAD] = ((sessionStatus.diskReadQueue > 0) && (sessionStatus.peersCount > 0))
            ? Utils::String::fromDouble((100. * sessionStatus.diskReadQueue / sessionStatus.peersCount), 2)
            : u"0"_s;

        map[KEY_TRANSFER_QUEUED_IO_JOBS] = cacheStatus.jobQueueLength;
        map[KEY_TRANSFER_AVERAGE_TIME_QUEUE] = cacheStatus.averageJobTime;
        map[KEY_TRANSFER_TOTAL_QUEUED_SIZE] = cacheStatus.queuedBytes;

        map[KEY_TRANSFER_LAST_EXTERNAL_ADDRESS_V4] = session->lastExternalIPv4Address();
        map[KEY_TRANSFER_LAST_EXTERNAL_ADDRESS_V6] = session->lastExternalIPv6Address();
        map[KEY_TRANSFER_DHT_NODES] = sessionStatus.dhtNodes;
        map[KEY_TRANSFER_CONNECTION_STATUS] = session->isListening()
            ? (sessionStatus.hasIncomingConnections ? u"connected"_s : u"firewalled"_s)
            : u"disconnected"_s;

        return map;
    }

    // Compare two structures (prevData, data) and calculate difference (syncData).
    // Structures encoded as map.
    QVariantMap processMap(const QVariantMap &prevData, const QVariantMap &data)
    {
        // initialize output variable
        QVariantMap syncData;

        for (auto i = data.cbegin(); i != data.cend(); ++i)
        {
            const QString &key = i.key();
            const QVariant &value = i.value();

            switch (value.userType())
            {
            case QMetaType::QVariantMap:
                {
                    const QVariantMap map = processMap(prevData[key].toMap(), value.toMap());
                    if (!map.isEmpty())
                        syncData[key] = map;
                }
                break;
            case QMetaType::QVariantHash:
                {
                    const auto [map, removedItems] = processHash(prevData[key].toHash(), value.toHash());
                    if (!map.isEmpty())
                        syncData[key] = map;
                    if (!removedItems.isEmpty())
                        syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
                }
                break;
            case QMetaType::QVariantList:
                {
                    const auto [list, removedItems] = processList(prevData[key].toList(), value.toList());
                    if (!list.isEmpty())
                        syncData[key] = list;
                    if (!removedItems.isEmpty())
                        syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
                }
                break;
            case QMetaType::QString:
            case QMetaType::LongLong:
            case QMetaType::Float:
            case QMetaType::Int:
            case QMetaType::Bool:
            case QMetaType::Double:
            case QMetaType::ULongLong:
            case QMetaType::UInt:
            case QMetaType::QDateTime:
            case QMetaType::Nullptr:
            case QMetaType::UnknownType:
                if (prevData[key] != value)
                    syncData[key] = value;
                break;
            default:
                Q_ASSERT_X(false, "processMap"
                        , u"Unexpected type: %1"_s.arg(QString::fromLatin1(value.metaType().name()))
                                .toUtf8().constData());
            }
        }

        return syncData;
    }

    // Compare two lists of structures (prevData, data) and calculate difference (syncData, removedItems).
    // Structures encoded as map.
    // Lists are encoded as hash table (indexed by structure key value) to improve ease of searching for removed items.
    std::pair<QVariantMap, QVariantList> processHash(QVariantHash prevData, const QVariantHash &data)
    {
        // initialize output variables
        std::pair<QVariantMap, QVariantList> result;
        auto &[syncData, removedItems] = result;

        if (prevData.isEmpty())
        {
            // If list was empty before, then difference is a whole new list.
            for (auto i = data.cbegin(); i != data.cend(); ++i)
                syncData[i.key()] = i.value();
        }
        else
        {
            for (auto i = data.cbegin(); i != data.cend(); ++i)
            {
                switch (i.value().userType())
                {
                case QMetaType::QVariantMap:
                    if (!prevData.contains(i.key()))
                    {
                        // new list item found - append it to syncData
                        syncData[i.key()] = i.value();
                    }
                    else
                    {
                        const QVariantMap map = processMap(prevData[i.key()].toMap(), i.value().toMap());
                        // existing list item found - remove it from prevData
                        prevData.remove(i.key());
                        if (!map.isEmpty())
                        {
                            // changed list item found - append its changes to syncData
                            syncData[i.key()] = map;
                        }
                    }
                    break;
                case QMetaType::QStringList:
                    if (!prevData.contains(i.key()))
                    {
                        // new list item found - append it to syncData
                        syncData[i.key()] = i.value();
                    }
                    else
                    {
                        const auto [list, removedList] = processList(prevData[i.key()].toList(), i.value().toList());
                        // existing list item found - remove it from prevData
                        prevData.remove(i.key());
                        if (!list.isEmpty() || !removedList.isEmpty())
                        {
                            // changed list item found - append entire list to syncData
                            syncData[i.key()] = i.value();
                        }
                    }
                    break;
                default:
                    Q_UNREACHABLE();
                    break;
                }
            }

            if (!prevData.isEmpty())
            {
                // prevData contains only items that are missing now -
                // put them in removedItems
                for (auto i = prevData.cbegin(); i != prevData.cend(); ++i)
                    removedItems << i.key();
            }
        }

        return result;
    }

    // Compare two lists of simple value (prevData, data) and calculate difference (syncData, removedItems).
    std::pair<QVariantList, QVariantList> processList(QVariantList prevData, const QVariantList &data)
    {
        // initialize output variables
        std::pair<QVariantList, QVariantList> result;
        auto &[syncData, removedItems] = result;

        if (prevData.isEmpty())
        {
            // If list was empty before, then difference is a whole new list.
            syncData = data;
        }
        else
        {
            for (const QVariant &item : data)
            {
                if (!prevData.contains(item))
                {
                    // new list item found - append it to syncData
                    syncData.append(item);
                }
                else
                {
                    // unchanged list item found - remove it from prevData
                    prevData.removeOne(item);
                }
            }

            if (!prevData.isEmpty())
            {
                // prevData contains only items that are missing now -
                // put them in removedItems
                removedItems = prevData;
            }
        }

        return result;
    }

    QJsonObject generateSyncData(int acceptedResponseId, const QVariantMap &data, QVariantMap &lastAcceptedData, QVariantMap &lastData)
    {
        QVariantMap syncData;
        bool fullUpdate = true;
        const int lastResponseId = (acceptedResponseId > 0) ? lastData[KEY_RESPONSE_ID].toInt() : 0;
        if (lastResponseId > 0)
        {
            if (lastResponseId == acceptedResponseId)
                lastAcceptedData = lastData;

            if (const int lastAcceptedResponseId = lastAcceptedData[KEY_RESPONSE_ID].toInt()
                    ; lastAcceptedResponseId == acceptedResponseId)
            {
                fullUpdate = false;
            }
        }

        if (fullUpdate)
        {
            lastAcceptedData.clear();
            syncData = data;
            syncData[KEY_FULL_UPDATE] = true;
        }
        else
        {
            syncData = processMap(lastAcceptedData, data);
        }

        const int responseId = (lastResponseId % 1000000) + 1;  // cycle between 1 and 1000000
        lastData = data;
        lastData[KEY_RESPONSE_ID] = responseId;
        syncData[KEY_RESPONSE_ID] = responseId;

        return QJsonObject::fromVariantMap(syncData);
    }
}

SyncController::SyncController(IApplication *app, QObject *parent)
    : APIController(app, parent)
{
}

void SyncController::updateFreeDiskSpace(const qint64 freeDiskSpace)
{
    m_freeDiskSpace = freeDiskSpace;
}

// The function returns the changed data from the server to synchronize with the web client.
// Return value is map in JSON format.
// Map contain the key:
//  - "Rid": ID response
// Map can contain the keys:
//  - "full_update": full data update flag
//  - "torrents": dictionary contains information about torrents.
//  - "torrents_removed": a list of hashes of removed torrents
//  - "categories": map of categories info
//  - "categories_removed": list of removed categories
//  - "trackers": dictionary contains information about trackers
//  - "trackers_removed": a list of removed trackers
//  - "server_state": map contains information about the state of the server
// The keys of the 'torrents' dictionary are hashes of torrents.
// Each value of the 'torrents' dictionary contains map. The map can contain following keys:
//  - "name": Torrent name
//  - "size": Torrent size
//  - "progress": Torrent progress
//  - "dlspeed": Torrent download speed
//  - "upspeed": Torrent upload speed
//  - "priority": Torrent queue position (-1 if queuing is disabled)
//  - "num_seeds": Torrent seeds connected to
//  - "num_complete": Torrent seeds in the swarm
//  - "num_leechs": Torrent leechers connected to
//  - "num_incomplete": Torrent leechers in the swarm
//  - "ratio": Torrent share ratio
//  - "eta": Torrent ETA
//  - "state": Torrent state
//  - "seq_dl": Torrent sequential download state
//  - "f_l_piece_prio": Torrent first last piece priority state
//  - "completion_on": Torrent copletion time
//  - "tracker": Torrent tracker
//  - "dl_limit": Torrent download limit
//  - "up_limit": Torrent upload limit
//  - "downloaded": Amount of data downloaded
//  - "uploaded": Amount of data uploaded
//  - "downloaded_session": Amount of data downloaded since program open
//  - "uploaded_session": Amount of data uploaded since program open
//  - "amount_left": Amount of data left to download
//  - "save_path": Torrent save path
//  - "download_path": Torrent download path
//  - "completed": Amount of data completed
//  - "max_ratio": Upload max share ratio
//  - "max_seeding_time": Upload max seeding time
//  - "ratio_limit": Upload share ratio limit
//  - "seeding_time_limit": Upload seeding time limit
//  - "seen_complete": Indicates the time when the torrent was last seen complete/whole
//  - "last_activity": Last time when a chunk was downloaded/uploaded
//  - "total_size": Size including unwanted data
// Server state map may contain the following keys:
//  - "connection_status": connection status
//  - "dht_nodes": DHT nodes count
//  - "dl_info_data": bytes downloaded
//  - "dl_info_speed": download speed
//  - "dl_rate_limit: download rate limit
//  - "last_external_address_v4": last external address v4
//  - "last_external_address_v6": last external address v6
//  - "up_info_data: bytes uploaded
//  - "up_info_speed: upload speed
//  - "up_rate_limit: upload speed limit
//  - "queueing": queue system usage flag
//  - "refresh_interval": torrents table refresh interval
//  - "free_space_on_disk": Free space on the default save path
// GET param:
//   - rid (int): last response id
void SyncController::maindataAction()
{
    if (m_maindataAcceptedID < 0)
    {
        makeMaindataSnapshot();

        const auto *btSession = BitTorrent::Session::instance();
        connect(btSession, &BitTorrent::Session::categoryAdded, this, &SyncController::onCategoryAdded);
        connect(btSession, &BitTorrent::Session::categoryRemoved, this, &SyncController::onCategoryRemoved);
        connect(btSession, &BitTorrent::Session::categoryOptionsChanged, this, &SyncController::onCategoryOptionsChanged);
        connect(btSession, &BitTorrent::Session::subcategoriesSupportChanged, this, &SyncController::onSubcategoriesSupportChanged);
        connect(btSession, &BitTorrent::Session::tagAdded, this, &SyncController::onTagAdded);
        connect(btSession, &BitTorrent::Session::tagRemoved, this, &SyncController::onTagRemoved);
        connect(btSession, &BitTorrent::Session::torrentAdded, this, &SyncController::onTorrentAdded);
        connect(btSession, &BitTorrent::Session::torrentAboutToBeRemoved, this, &SyncController::onTorrentAboutToBeRemoved);
        connect(btSession, &BitTorrent::Session::torrentCategoryChanged, this, &SyncController::onTorrentCategoryChanged);
        connect(btSession, &BitTorrent::Session::torrentMetadataReceived, this, &SyncController::onTorrentMetadataReceived);
        connect(btSession, &BitTorrent::Session::torrentStopped, this, &SyncController::onTorrentStopped);
        connect(btSession, &BitTorrent::Session::torrentStarted, this, &SyncController::onTorrentStarted);
        connect(btSession, &BitTorrent::Session::torrentSavePathChanged, this, &SyncController::onTorrentSavePathChanged);
        connect(btSession, &BitTorrent::Session::torrentSavingModeChanged, this, &SyncController::onTorrentSavingModeChanged);
        connect(btSession, &BitTorrent::Session::torrentTagAdded, this, &SyncController::onTorrentTagAdded);
        connect(btSession, &BitTorrent::Session::torrentTagRemoved, this, &SyncController::onTorrentTagRemoved);
        connect(btSession, &BitTorrent::Session::torrentsUpdated, this, &SyncController::onTorrentsUpdated);
        connect(btSession, &BitTorrent::Session::trackersAdded, this, &SyncController::onTorrentTrackersChanged);
        connect(btSession, &BitTorrent::Session::trackersRemoved, this, &SyncController::onTorrentTrackersChanged);
        connect(btSession, &BitTorrent::Session::trackersChanged, this, &SyncController::onTorrentTrackersChanged);
    }

    const int acceptedID = params()[u"rid"_s].toInt();
    bool fullUpdate = true;
    if ((acceptedID > 0) && (m_maindataLastSentID > 0))
    {
        if (m_maindataLastSentID == acceptedID)
        {
            m_maindataAcceptedID = acceptedID;
            m_maindataSyncBuf = {};
        }

        if (m_maindataAcceptedID == acceptedID)
        {
            // We are still able to send changes for the current state of the data having by client.
            fullUpdate = false;
        }
    }

    const int id = (m_maindataLastSentID % 1000000) + 1;  // cycle between 1 and 1000000
    setResult(generateMaindataSyncData(id, fullUpdate));
    m_maindataLastSentID = id;
}

void SyncController::makeMaindataSnapshot()
{
    m_knownTrackers.clear();
    m_maindataAcceptedID = 0;
    m_maindataSnapshot = {};

    const auto *session = BitTorrent::Session::instance();

    for (const BitTorrent::Torrent *torrent : asConst(session->torrents()))
    {
        const BitTorrent::TorrentID torrentID = torrent->id();

        QVariantMap serializedTorrent = serialize(*torrent);
        serializedTorrent.remove(KEY_TORRENT_ID);

        for (const BitTorrent::TrackerEntryStatus &status : asConst(torrent->trackers()))
            m_knownTrackers[status.url].insert(torrentID);

        m_maindataSnapshot.torrents[torrentID.toString()] = serializedTorrent;
    }

    const QStringList categoriesList = session->categories();
    for (const auto &categoryName : categoriesList)
    {
        const BitTorrent::CategoryOptions categoryOptions = session->categoryOptions(categoryName);
        QJsonObject category = categoryOptions.toJSON();
        // adjust it to be compatible with existing WebAPI
        category[u"savePath"_s] = category.take(u"save_path"_s);
        category.insert(u"name"_s, categoryName);
        m_maindataSnapshot.categories[categoryName] = category.toVariantMap();
    }

    for (const Tag &tag : asConst(session->tags()))
        m_maindataSnapshot.tags.append(tag.toString());

    for (auto trackersIter = m_knownTrackers.cbegin(); trackersIter != m_knownTrackers.cend(); ++trackersIter)
    {
        QStringList torrentIDs;
        for (const BitTorrent::TorrentID &torrentID : asConst(trackersIter.value()))
            torrentIDs.append(torrentID.toString());

        m_maindataSnapshot.trackers[trackersIter.key()] = torrentIDs;
    }

    m_maindataSnapshot.serverState = getTransferInfo();
    m_maindataSnapshot.serverState[KEY_TRANSFER_FREESPACEONDISK] = m_freeDiskSpace;
    m_maindataSnapshot.serverState[KEY_SYNC_MAINDATA_QUEUEING] = session->isQueueingSystemEnabled();
    m_maindataSnapshot.serverState[KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS] = session->isAltGlobalSpeedLimitEnabled();
    m_maindataSnapshot.serverState[KEY_SYNC_MAINDATA_REFRESH_INTERVAL] = session->refreshInterval();
    m_maindataSnapshot.serverState[KEY_SYNC_MAINDATA_USE_SUBCATEGORIES] = session->isSubcategoriesEnabled();
}

QJsonObject SyncController::generateMaindataSyncData(const int id, const bool fullUpdate)
{
    // if need to update existing sync data
    for (const QString &category : asConst(m_updatedCategories))
        m_maindataSyncBuf.removedCategories.removeOne(category);
    for (const QString &category : asConst(m_removedCategories))
        m_maindataSyncBuf.categories.remove(category);

    for (const QString &tag : asConst(m_addedTags))
        m_maindataSyncBuf.removedTags.removeOne(tag);
    for (const QString &tag : asConst(m_removedTags))
        m_maindataSyncBuf.tags.removeOne(tag);

    for (const BitTorrent::TorrentID &torrentID : asConst(m_updatedTorrents))
        m_maindataSyncBuf.removedTorrents.removeOne(torrentID.toString());
    for (const BitTorrent::TorrentID &torrentID : asConst(m_removedTorrents))
        m_maindataSyncBuf.torrents.remove(torrentID.toString());

    for (const QString &tracker : asConst(m_updatedTrackers))
        m_maindataSyncBuf.removedTrackers.removeOne(tracker);
    for (const QString &tracker : asConst(m_removedTrackers))
        m_maindataSyncBuf.trackers.remove(tracker);

    const auto *session = BitTorrent::Session::instance();

    for (const QString &categoryName : asConst(m_updatedCategories))
    {
        const BitTorrent::CategoryOptions categoryOptions = session->categoryOptions(categoryName);
        auto category = categoryOptions.toJSON().toVariantMap();
        // adjust it to be compatible with existing WebAPI
        category[u"savePath"_s] = category.take(u"save_path"_s);
        category.insert(u"name"_s, categoryName);

        auto &categorySnapshot = m_maindataSnapshot.categories[categoryName];
        if (const QVariantMap syncData = processMap(categorySnapshot, category); !syncData.isEmpty())
        {
            m_maindataSyncBuf.categories[categoryName] = syncData;
            categorySnapshot = category;
        }
    }
    m_updatedCategories.clear();

    for (const QString &category : asConst(m_removedCategories))
    {
        m_maindataSyncBuf.removedCategories.append(category);
        m_maindataSnapshot.categories.remove(category);
    }
    m_removedCategories.clear();

    for (const QString &tag : asConst(m_addedTags))
    {
        m_maindataSyncBuf.tags.append(tag);
        m_maindataSnapshot.tags.append(tag);
    }
    m_addedTags.clear();

    for (const QString &tag : asConst(m_removedTags))
    {
        m_maindataSyncBuf.removedTags.append(tag);
        m_maindataSnapshot.tags.removeOne(tag);
    }
    m_removedTags.clear();

    for (const BitTorrent::TorrentID &torrentID : asConst(m_updatedTorrents))
    {
        const BitTorrent::Torrent *torrent = session->getTorrent(torrentID);
        Q_ASSERT(torrent);

        QVariantMap serializedTorrent = serialize(*torrent);
        serializedTorrent.remove(KEY_TORRENT_ID);

        auto &torrentSnapshot = m_maindataSnapshot.torrents[torrentID.toString()];

        if (const QVariantMap syncData = processMap(torrentSnapshot, serializedTorrent); !syncData.isEmpty())
        {
            m_maindataSyncBuf.torrents[torrentID.toString()] = syncData;
            torrentSnapshot = serializedTorrent;
        }
    }
    m_updatedTorrents.clear();

    for (const BitTorrent::TorrentID &torrentID : asConst(m_removedTorrents))
    {
        m_maindataSyncBuf.removedTorrents.append(torrentID.toString());
        m_maindataSnapshot.torrents.remove(torrentID.toString());
    }
    m_removedTorrents.clear();

    for (const QString &tracker : asConst(m_updatedTrackers))
    {
        const QSet<BitTorrent::TorrentID> torrentIDs = m_knownTrackers[tracker];
        QStringList serializedTorrentIDs;
        serializedTorrentIDs.reserve(torrentIDs.size());
        for (const BitTorrent::TorrentID &torrentID : torrentIDs)
            serializedTorrentIDs.append(torrentID.toString());

        m_maindataSyncBuf.trackers[tracker] = serializedTorrentIDs;
        m_maindataSnapshot.trackers[tracker] = serializedTorrentIDs;
    }
    m_updatedTrackers.clear();

    for (const QString &tracker : asConst(m_removedTrackers))
    {
        m_maindataSyncBuf.removedTrackers.append(tracker);
        m_maindataSnapshot.trackers.remove(tracker);
    }
    m_removedTrackers.clear();

    QVariantMap serverState = getTransferInfo();
    serverState[KEY_TRANSFER_FREESPACEONDISK] = m_freeDiskSpace;
    serverState[KEY_SYNC_MAINDATA_QUEUEING] = session->isQueueingSystemEnabled();
    serverState[KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS] = session->isAltGlobalSpeedLimitEnabled();
    serverState[KEY_SYNC_MAINDATA_REFRESH_INTERVAL] = session->refreshInterval();
    serverState[KEY_SYNC_MAINDATA_USE_SUBCATEGORIES] = session->isSubcategoriesEnabled();
    if (const QVariantMap syncData = processMap(m_maindataSnapshot.serverState, serverState); !syncData.isEmpty())
    {
        m_maindataSyncBuf.serverState = syncData;
        m_maindataSnapshot.serverState = serverState;
    }

    QJsonObject syncData;
    syncData[KEY_RESPONSE_ID] = id;
    if (fullUpdate)
    {
        m_maindataSyncBuf = m_maindataSnapshot;
        syncData[KEY_FULL_UPDATE] = true;
    }

    if (!m_maindataSyncBuf.categories.isEmpty())
    {
        QJsonObject categories;
        for (auto it = m_maindataSyncBuf.categories.cbegin(); it != m_maindataSyncBuf.categories.cend(); ++it)
            categories[it.key()] = QJsonObject::fromVariantMap(it.value());
        syncData[KEY_CATEGORIES] = categories;
    }
    if (!m_maindataSyncBuf.removedCategories.isEmpty())
        syncData[KEY_CATEGORIES_REMOVED] = QJsonArray::fromStringList(m_maindataSyncBuf.removedCategories);

    if (!m_maindataSyncBuf.tags.isEmpty())
        syncData[KEY_TAGS] = QJsonArray::fromVariantList(m_maindataSyncBuf.tags);
    if (!m_maindataSyncBuf.removedTags.isEmpty())
        syncData[KEY_TAGS_REMOVED] = QJsonArray::fromStringList(m_maindataSyncBuf.removedTags);

    if (!m_maindataSyncBuf.torrents.isEmpty())
    {
        QJsonObject torrents;
        for (auto it = m_maindataSyncBuf.torrents.cbegin(); it != m_maindataSyncBuf.torrents.cend(); ++it)
            torrents[it.key()] = QJsonObject::fromVariantMap(it.value());
        syncData[KEY_TORRENTS] = torrents;
    }
    if (!m_maindataSyncBuf.removedTorrents.isEmpty())
        syncData[KEY_TORRENTS_REMOVED] = QJsonArray::fromStringList(m_maindataSyncBuf.removedTorrents);

    if (!m_maindataSyncBuf.trackers.isEmpty())
    {
        QJsonObject trackers;
        for (auto it = m_maindataSyncBuf.trackers.cbegin(); it != m_maindataSyncBuf.trackers.cend(); ++it)
            trackers[it.key()] = QJsonArray::fromStringList(it.value());
        syncData[KEY_TRACKERS] = trackers;
    }
    if (!m_maindataSyncBuf.removedTrackers.isEmpty())
        syncData[KEY_TRACKERS_REMOVED] = QJsonArray::fromStringList(m_maindataSyncBuf.removedTrackers);

    if (!m_maindataSyncBuf.serverState.isEmpty())
        syncData[KEY_SERVER_STATE] = QJsonObject::fromVariantMap(m_maindataSyncBuf.serverState);

    return syncData;
}

// GET param:
//   - hash (string): torrent hash (ID)
//   - rid (int): last response id
void SyncController::torrentPeersAction()
{
    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QVariantMap data;
    QVariantHash peers;

    const QList<BitTorrent::PeerInfo> peersList = torrent->peers();

    bool resolvePeerCountries = Preferences::instance()->resolvePeerCountries();

    data[KEY_SYNC_TORRENT_PEERS_SHOW_FLAGS] = resolvePeerCountries;

    for (const BitTorrent::PeerInfo &pi : peersList)
    {
        if (pi.address().ip.isNull()) continue;

        QVariantMap peer =
        {
            {KEY_PEER_IP, pi.address().ip.toString()},
            {KEY_PEER_PORT, pi.address().port},
            {KEY_PEER_CLIENT, pi.client()},
            {KEY_PEER_ID_CLIENT, pi.peerIdClient()},
            {KEY_PEER_PROGRESS, pi.progress()},
            {KEY_PEER_DOWN_SPEED, pi.payloadDownSpeed()},
            {KEY_PEER_UP_SPEED, pi.payloadUpSpeed()},
            {KEY_PEER_TOT_DOWN, pi.totalDownload()},
            {KEY_PEER_TOT_UP, pi.totalUpload()},
            {KEY_PEER_CONNECTION_TYPE, pi.connectionType()},
            {KEY_PEER_FLAGS, pi.flags()},
            {KEY_PEER_FLAGS_DESCRIPTION, pi.flagsDescription()},
            {KEY_PEER_RELEVANCE, pi.relevance()}
        };

        if (torrent->hasMetadata())
        {
            const PathList filePaths = torrent->info().filesForPiece(pi.downloadingPieceIndex());
            QStringList filesForPiece;
            filesForPiece.reserve(filePaths.size());
            for (const Path &filePath : filePaths)
                filesForPiece.append(filePath.toString());
            peer.insert(KEY_PEER_FILES, filesForPiece.join(u'\n'));
        }

        if (resolvePeerCountries)
        {
            peer[KEY_PEER_COUNTRY_CODE] = pi.country().toLower();
            peer[KEY_PEER_COUNTRY] = Net::GeoIPManager::CountryName(pi.country());
        }

        peers[pi.address().toString()] = peer;
    }
    data[u"peers"_s] = peers;

    const int acceptedResponseId = params()[u"rid"_s].toInt();
    setResult(generateSyncData(acceptedResponseId, data, m_lastAcceptedPeersResponse, m_lastPeersResponse));
}

void SyncController::onCategoryAdded(const QString &categoryName)
{
    m_removedCategories.remove(categoryName);
    m_updatedCategories.insert(categoryName);
}

void SyncController::onCategoryRemoved(const QString &categoryName)
{
    m_updatedCategories.remove(categoryName);
    m_removedCategories.insert(categoryName);
}

void SyncController::onCategoryOptionsChanged(const QString &categoryName)
{
    Q_ASSERT(!m_removedCategories.contains(categoryName));

    m_updatedCategories.insert(categoryName);
}

void SyncController::onSubcategoriesSupportChanged()
{
    const QStringList categoriesList = BitTorrent::Session::instance()->categories();
    for (const auto &categoryName : categoriesList)
    {
        if (!m_maindataSnapshot.categories.contains(categoryName))
        {
            m_removedCategories.remove(categoryName);
            m_updatedCategories.insert(categoryName);
        }
    }
}

void SyncController::onTagAdded(const Tag &tag)
{
    m_removedTags.remove(tag.toString());
    m_addedTags.insert(tag.toString());
}

void SyncController::onTagRemoved(const Tag &tag)
{
    m_addedTags.remove(tag.toString());
    m_removedTags.insert(tag.toString());
}

void SyncController::onTorrentAdded(BitTorrent::Torrent *torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    m_removedTorrents.remove(torrentID);
    m_updatedTorrents.insert(torrentID);

    for (const BitTorrent::TrackerEntryStatus &status : asConst(torrent->trackers()))
    {
        m_knownTrackers[status.url].insert(torrentID);
        m_updatedTrackers.insert(status.url);
        m_removedTrackers.remove(status.url);
    }
}

void SyncController::onTorrentAboutToBeRemoved(BitTorrent::Torrent *torrent)
{
    const BitTorrent::TorrentID torrentID = torrent->id();

    m_updatedTorrents.remove(torrentID);
    m_removedTorrents.insert(torrentID);

    for (const BitTorrent::TrackerEntryStatus &status : asConst(torrent->trackers()))
    {
        const auto iter = m_knownTrackers.find(status.url);
        Q_ASSERT(iter != m_knownTrackers.end());
        if (iter == m_knownTrackers.end()) [[unlikely]]
            continue;

        QSet<BitTorrent::TorrentID> &torrentIDs = iter.value();
        torrentIDs.remove(torrentID);
        if (torrentIDs.isEmpty())
        {
            m_knownTrackers.erase(iter);
            m_updatedTrackers.remove(status.url);
            m_removedTrackers.insert(status.url);
        }
        else
        {
            m_updatedTrackers.insert(status.url);
        }
    }
}

void SyncController::onTorrentCategoryChanged(BitTorrent::Torrent *torrent
        , [[maybe_unused]] const QString &oldCategory)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentMetadataReceived(BitTorrent::Torrent *torrent)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentStopped(BitTorrent::Torrent *torrent)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentStarted(BitTorrent::Torrent *torrent)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentSavePathChanged(BitTorrent::Torrent *torrent)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentSavingModeChanged(BitTorrent::Torrent *torrent)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentTagAdded(BitTorrent::Torrent *torrent, [[maybe_unused]] const Tag &tag)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentTagRemoved(BitTorrent::Torrent *torrent, [[maybe_unused]] const Tag &tag)
{
    m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentsUpdated(const QList<BitTorrent::Torrent *> &torrents)
{
    for (const BitTorrent::Torrent *torrent : torrents)
        m_updatedTorrents.insert(torrent->id());
}

void SyncController::onTorrentTrackersChanged(BitTorrent::Torrent *torrent)
{
    using namespace BitTorrent;

    const QList<TrackerEntryStatus> trackers = torrent->trackers();

    QSet<QString> currentTrackers;
    currentTrackers.reserve(trackers.size());
    for (const TrackerEntryStatus &status : trackers)
        currentTrackers.insert(status.url);

    const TorrentID torrentID = torrent->id();
    Algorithm::removeIf(m_knownTrackers
        , [this, torrentID, currentTrackers](const QString &knownTracker, QSet<TorrentID> &torrentIDs)
    {
        if (auto idIter = torrentIDs.find(torrentID)
                ; (idIter != torrentIDs.end()) && !currentTrackers.contains(knownTracker))
        {
            torrentIDs.erase(idIter);
            if (torrentIDs.isEmpty())
            {
                m_updatedTrackers.remove(knownTracker);
                m_removedTrackers.insert(knownTracker);
                return true;
            }

            m_updatedTrackers.insert(knownTracker);
            return false;
        }

        if (currentTrackers.contains(knownTracker) && !torrentIDs.contains(torrentID))
        {
            torrentIDs.insert(torrentID);
            m_updatedTrackers.insert(knownTracker);
            return false;
        }

        return false;
    });

    for (const QString &currentTracker : asConst(currentTrackers))
    {
        if (!m_knownTrackers.contains(currentTracker))
        {
            m_knownTrackers.insert(currentTracker, {torrentID});
            m_updatedTrackers.insert(currentTracker);
            m_removedTrackers.remove(currentTracker);
        }
    }
}
