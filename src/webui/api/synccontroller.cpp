/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include <algorithm>

#include <QJsonObject>
#include <QMetaObject>
#include <QThread>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/net/geoipmanager.h"
#include "base/preferences.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "freediskspacechecker.h"
#include "isessionmanager.h"
#include "serialize/serialize_torrent.h"

namespace
{
    const int FREEDISKSPACE_CHECK_TIMEOUT = 30000;

    // Sync main data keys
    const char KEY_SYNC_MAINDATA_QUEUEING[] = "queueing";
    const char KEY_SYNC_MAINDATA_REFRESH_INTERVAL[] = "refresh_interval";
    const char KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS[] = "use_alt_speed_limits";

    // Sync torrent peers keys
    const char KEY_SYNC_TORRENT_PEERS_SHOW_FLAGS[] = "show_flags";

    // Peer keys
    const char KEY_PEER_CLIENT[] = "client";
    const char KEY_PEER_CONNECTION_TYPE[] = "connection";
    const char KEY_PEER_COUNTRY[] = "country";
    const char KEY_PEER_COUNTRY_CODE[] = "country_code";
    const char KEY_PEER_DOWN_SPEED[] = "dl_speed";
    const char KEY_PEER_FILES[] = "files";
    const char KEY_PEER_FLAGS[] = "flags";
    const char KEY_PEER_FLAGS_DESCRIPTION[] = "flags_desc";
    const char KEY_PEER_IP[] = "ip";
    const char KEY_PEER_PORT[] = "port";
    const char KEY_PEER_PROGRESS[] = "progress";
    const char KEY_PEER_RELEVANCE[] = "relevance";
    const char KEY_PEER_TOT_DOWN[] = "downloaded";
    const char KEY_PEER_TOT_UP[] = "uploaded";
    const char KEY_PEER_UP_SPEED[] = "up_speed";

    // TransferInfo keys
    const char KEY_TRANSFER_CONNECTION_STATUS[] = "connection_status";
    const char KEY_TRANSFER_DHT_NODES[] = "dht_nodes";
    const char KEY_TRANSFER_DLDATA[] = "dl_info_data";
    const char KEY_TRANSFER_DLRATELIMIT[] = "dl_rate_limit";
    const char KEY_TRANSFER_DLSPEED[] = "dl_info_speed";
    const char KEY_TRANSFER_FREESPACEONDISK[] = "free_space_on_disk";
    const char KEY_TRANSFER_UPDATA[] = "up_info_data";
    const char KEY_TRANSFER_UPRATELIMIT[] = "up_rate_limit";
    const char KEY_TRANSFER_UPSPEED[] = "up_info_speed";

    // Statistics keys
    const char KEY_TRANSFER_ALLTIME_DL[] = "alltime_dl";
    const char KEY_TRANSFER_ALLTIME_UL[] = "alltime_ul";
    const char KEY_TRANSFER_AVERAGE_TIME_QUEUE[] = "average_time_queue";
    const char KEY_TRANSFER_GLOBAL_RATIO[] = "global_ratio";
    const char KEY_TRANSFER_QUEUED_IO_JOBS[] = "queued_io_jobs";
    const char KEY_TRANSFER_READ_CACHE_HITS[] = "read_cache_hits";
    const char KEY_TRANSFER_READ_CACHE_OVERLOAD[] = "read_cache_overload";
    const char KEY_TRANSFER_TOTAL_BUFFERS_SIZE[] = "total_buffers_size";
    const char KEY_TRANSFER_TOTAL_PEER_CONNECTIONS[] = "total_peer_connections";
    const char KEY_TRANSFER_TOTAL_QUEUED_SIZE[] = "total_queued_size";
    const char KEY_TRANSFER_TOTAL_WASTE_SESSION[] = "total_wasted_session";
    const char KEY_TRANSFER_WRITE_CACHE_OVERLOAD[] = "write_cache_overload";

    const char KEY_FULL_UPDATE[] = "full_update";
    const char KEY_RESPONSE_ID[] = "rid";
    const char KEY_SUFFIX_REMOVED[] = "_removed";

    void processMap(const QVariantMap &prevData, const QVariantMap &data, QVariantMap &syncData);
    void processHash(QVariantHash prevData, const QVariantHash &data, QVariantMap &syncData, QVariantList &removedItems);
    void processList(QVariantList prevData, const QVariantList &data, QVariantList &syncData, QVariantList &removedItems);
    QVariantMap generateSyncData(int acceptedResponseId, const QVariantMap &data, QVariantMap &lastAcceptedData, QVariantMap &lastData);

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

        const quint64 atd = session->getAlltimeDL();
        const quint64 atu = session->getAlltimeUL();
        map[KEY_TRANSFER_ALLTIME_DL] = atd;
        map[KEY_TRANSFER_ALLTIME_UL] = atu;
        map[KEY_TRANSFER_TOTAL_WASTE_SESSION] = sessionStatus.totalWasted;
        map[KEY_TRANSFER_GLOBAL_RATIO] = ((atd > 0) && (atu > 0)) ? Utils::String::fromDouble(static_cast<qreal>(atu) / atd, 2) : "-";
        map[KEY_TRANSFER_TOTAL_PEER_CONNECTIONS] = sessionStatus.peersCount;

        const qreal readRatio = cacheStatus.readRatio;  // TODO: remove when LIBTORRENT_VERSION_NUM >= 20000
        map[KEY_TRANSFER_READ_CACHE_HITS] = (readRatio > 0) ? Utils::String::fromDouble(100 * readRatio, 2) : "0";
        map[KEY_TRANSFER_TOTAL_BUFFERS_SIZE] = cacheStatus.totalUsedBuffers * 16 * 1024;

        map[KEY_TRANSFER_WRITE_CACHE_OVERLOAD] = ((sessionStatus.diskWriteQueue > 0) && (sessionStatus.peersCount > 0))
            ? Utils::String::fromDouble((100. * sessionStatus.diskWriteQueue / sessionStatus.peersCount), 2)
            : QLatin1String("0");
        map[KEY_TRANSFER_READ_CACHE_OVERLOAD] = ((sessionStatus.diskReadQueue > 0) && (sessionStatus.peersCount > 0))
            ? Utils::String::fromDouble((100. * sessionStatus.diskReadQueue / sessionStatus.peersCount), 2)
            : QLatin1String("0");

        map[KEY_TRANSFER_QUEUED_IO_JOBS] = cacheStatus.jobQueueLength;
        map[KEY_TRANSFER_AVERAGE_TIME_QUEUE] = cacheStatus.averageJobTime;
        map[KEY_TRANSFER_TOTAL_QUEUED_SIZE] = cacheStatus.queuedBytes;

        map[KEY_TRANSFER_DHT_NODES] = sessionStatus.dhtNodes;
        map[KEY_TRANSFER_CONNECTION_STATUS] = session->isListening()
            ? (sessionStatus.hasIncomingConnections ? "connected" : "firewalled")
            : "disconnected";

        return map;
    }

    // Compare two structures (prevData, data) and calculate difference (syncData).
    // Structures encoded as map.
    void processMap(const QVariantMap &prevData, const QVariantMap &data, QVariantMap &syncData)
    {
        // initialize output variable
        syncData.clear();

        for (auto i = data.cbegin(); i != data.cend(); ++i)
        {
            const QString &key = i.key();
            const QVariant &value = i.value();
            QVariantList removedItems;

            switch (static_cast<QMetaType::Type>(value.type()))
            {
            case QMetaType::QVariantMap:
            {
                    QVariantMap map;
                    processMap(prevData[key].toMap(), value.toMap(), map);
                    if (!map.isEmpty())
                        syncData[key] = map;
                }
                break;
            case QMetaType::QVariantHash:
            {
                    QVariantMap map;
                    processHash(prevData[key].toHash(), value.toHash(), map, removedItems);
                    if (!map.isEmpty())
                        syncData[key] = map;
                    if (!removedItems.isEmpty())
                        syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
                }
                break;
            case QMetaType::QVariantList:
            {
                    QVariantList list;
                    processList(prevData[key].toList(), value.toList(), list, removedItems);
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
                if (prevData[key] != value)
                    syncData[key] = value;
                break;
            default:
                Q_ASSERT_X(false, "processMap"
                           , QString("Unexpected type: %1")
                           .arg(QMetaType::typeName(static_cast<QMetaType::Type>(value.type())))
                           .toUtf8().constData());
            }
        }
    }

    // Compare two lists of structures (prevData, data) and calculate difference (syncData, removedItems).
    // Structures encoded as map.
    // Lists are encoded as hash table (indexed by structure key value) to improve ease of searching for removed items.
    void processHash(QVariantHash prevData, const QVariantHash &data, QVariantMap &syncData, QVariantList &removedItems)
    {
        // initialize output variables
        syncData.clear();
        removedItems.clear();

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
                switch (i.value().type())
                {
                case QVariant::Map:
                    if (!prevData.contains(i.key()))
                    {
                        // new list item found - append it to syncData
                        syncData[i.key()] = i.value();
                    }
                    else
                    {
                        QVariantMap map;
                        processMap(prevData[i.key()].toMap(), i.value().toMap(), map);
                        // existing list item found - remove it from prevData
                        prevData.remove(i.key());
                        if (!map.isEmpty())
                        {
                            // changed list item found - append its changes to syncData
                            syncData[i.key()] = map;
                        }
                    }
                    break;
                case QVariant::StringList:
                    if (!prevData.contains(i.key()))
                    {
                        // new list item found - append it to syncData
                        syncData[i.key()] = i.value();
                    }
                    else
                    {
                        QVariantList list;
                        QVariantList removedList;
                        processList(prevData[i.key()].toList(), i.value().toList(), list, removedList);
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
                    Q_ASSERT(false);
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
    }

    // Compare two lists of simple value (prevData, data) and calculate difference (syncData, removedItems).
    void processList(QVariantList prevData, const QVariantList &data, QVariantList &syncData, QVariantList &removedItems)
    {
        // initialize output variables
        syncData.clear();
        removedItems.clear();

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
                    // new list item found - append it to syncData
                    syncData.append(item);
                else
                    // unchanged list item found - remove it from prevData
                    prevData.removeOne(item);
            }

            if (!prevData.isEmpty())
                // prevData contains only items that are missing now -
                // put them in removedItems
                removedItems = prevData;
        }
    }

    QVariantMap generateSyncData(int acceptedResponseId, const QVariantMap &data, QVariantMap &lastAcceptedData, QVariantMap &lastData)
    {
        QVariantMap syncData;
        bool fullUpdate = true;
        int lastResponseId = 0;
        if (acceptedResponseId > 0)
        {
            lastResponseId = lastData[KEY_RESPONSE_ID].toInt();

            if (lastResponseId == acceptedResponseId)
                lastAcceptedData = lastData;

            int lastAcceptedResponseId = lastAcceptedData[KEY_RESPONSE_ID].toInt();

            if (lastAcceptedResponseId == acceptedResponseId)
            {
                processMap(lastAcceptedData, data, syncData);
                fullUpdate = false;
            }
        }

        if (fullUpdate)
        {
            lastAcceptedData.clear();
            syncData = data;
            syncData[KEY_FULL_UPDATE] = true;
        }

        lastResponseId = (lastResponseId % 1000000) + 1;  // cycle between 1 and 1000000
        lastData = data;
        lastData[KEY_RESPONSE_ID] = lastResponseId;
        syncData[KEY_RESPONSE_ID] = lastResponseId;

        return syncData;
    }
}

SyncController::SyncController(ISessionManager *sessionManager, QObject *parent)
    : APIController(sessionManager, parent)
{
    m_freeDiskSpaceThread = new QThread(this);
    m_freeDiskSpaceChecker = new FreeDiskSpaceChecker();
    m_freeDiskSpaceChecker->moveToThread(m_freeDiskSpaceThread);

    connect(m_freeDiskSpaceThread, &QThread::finished, m_freeDiskSpaceChecker, &QObject::deleteLater);
    connect(m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::checked, this, &SyncController::freeDiskSpaceSizeUpdated);

    m_freeDiskSpaceThread->start();
    invokeChecker();
    m_freeDiskSpaceElapsedTimer.start();
}

SyncController::~SyncController()
{
    m_freeDiskSpaceThread->quit();
    m_freeDiskSpaceThread->wait();
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
    const auto *session = BitTorrent::Session::instance();

    QVariantMap data;

    QVariantMap lastResponse = sessionManager()->session()->getData(QLatin1String("syncMainDataLastResponse")).toMap();
    QVariantMap lastAcceptedResponse = sessionManager()->session()->getData(QLatin1String("syncMainDataLastAcceptedResponse")).toMap();

    QVariantHash torrents;
    QHash<QString, QStringList> trackers;
    for (const BitTorrent::Torrent *torrent : asConst(session->torrents()))
    {
        const BitTorrent::TorrentID torrentID = torrent->id();

        QVariantMap map = serialize(*torrent);
        map.remove(KEY_TORRENT_ID);

        // Calculated last activity time can differ from actual value by up to 10 seconds (this is a libtorrent issue).
        // So we don't need unnecessary updates of last activity time in response.
        const auto iterTorrents = lastResponse.find("torrents");
        if (iterTorrents != lastResponse.end())
        {
            const QVariantHash lastResponseTorrents = iterTorrents->toHash();
            const auto iterID = lastResponseTorrents.find(torrentID.toString());

            if (iterID != lastResponseTorrents.end())
            {
                const QVariantMap torrentData = iterID->toMap();
                const auto iterLastActivity = torrentData.find(KEY_TORRENT_LAST_ACTIVITY_TIME);

                if (iterLastActivity != torrentData.end())
                {
                    const int lastValue = iterLastActivity->toInt();
                    if (qAbs(lastValue - map[KEY_TORRENT_LAST_ACTIVITY_TIME].toInt()) < 15)
                        map[KEY_TORRENT_LAST_ACTIVITY_TIME] = lastValue;
                }
            }
        }

        for (const BitTorrent::TrackerEntry &tracker : asConst(torrent->trackers()))
            trackers[tracker.url] << torrentID.toString();

        torrents[torrentID.toString()] = map;
    }
    data["torrents"] = torrents;

    QVariantHash categories;
    const QStringMap categoriesList = session->categories();
    for (auto it = categoriesList.cbegin(); it != categoriesList.cend(); ++it)
    {
        const QString &key = it.key();
        categories[key] = QVariantMap
        {
            {"name", key},
            {"savePath", it.value()}
        };
    }
    data["categories"] = categories;

    QVariantList tags;
    for (const QString &tag : asConst(session->tags()))
        tags << tag;
    data["tags"] = tags;

    QVariantHash trackersHash;
    for (auto i = trackers.constBegin(); i != trackers.constEnd(); ++i)
    {
        trackersHash[i.key()] = i.value();
    }
    data["trackers"] = trackersHash;

    QVariantMap serverState = getTransferInfo();
    serverState[KEY_TRANSFER_FREESPACEONDISK] = getFreeDiskSpace();
    serverState[KEY_SYNC_MAINDATA_QUEUEING] = session->isQueueingSystemEnabled();
    serverState[KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS] = session->isAltGlobalSpeedLimitEnabled();
    serverState[KEY_SYNC_MAINDATA_REFRESH_INTERVAL] = session->refreshInterval();
    data["server_state"] = serverState;

    const int acceptedResponseId {params()["rid"].toInt()};
    setResult(QJsonObject::fromVariantMap(generateSyncData(acceptedResponseId, data, lastAcceptedResponse, lastResponse)));

    sessionManager()->session()->setData(QLatin1String("syncMainDataLastResponse"), lastResponse);
    sessionManager()->session()->setData(QLatin1String("syncMainDataLastAcceptedResponse"), lastAcceptedResponse);
}

// GET param:
//   - hash (string): torrent hash (ID)
//   - rid (int): last response id
void SyncController::torrentPeersAction()
{
    auto lastResponse = sessionManager()->session()->getData(QLatin1String("syncTorrentPeersLastResponse")).toMap();
    auto lastAcceptedResponse = sessionManager()->session()->getData(QLatin1String("syncTorrentPeersLastAcceptedResponse")).toMap();

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QVariantMap data;
    QVariantHash peers;

    const QVector<BitTorrent::PeerInfo> peersList = torrent->peers();

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
            {KEY_PEER_PROGRESS, pi.progress()},
            {KEY_PEER_DOWN_SPEED, pi.payloadDownSpeed()},
            {KEY_PEER_UP_SPEED, pi.payloadUpSpeed()},
            {KEY_PEER_TOT_DOWN, pi.totalDownload()},
            {KEY_PEER_TOT_UP, pi.totalUpload()},
            {KEY_PEER_CONNECTION_TYPE, pi.connectionType()},
            {KEY_PEER_FLAGS, pi.flags()},
            {KEY_PEER_FLAGS_DESCRIPTION, pi.flagsDescription()},
            {KEY_PEER_RELEVANCE, pi.relevance()},
            {KEY_PEER_FILES, torrent->info().filesForPiece(pi.downloadingPieceIndex()).join('\n')}
        };

        if (resolvePeerCountries)
        {
            peer[KEY_PEER_COUNTRY_CODE] = pi.country().toLower();
            peer[KEY_PEER_COUNTRY] = Net::GeoIPManager::CountryName(pi.country());
        }

        peers[pi.address().toString()] = peer;
    }
    data["peers"] = peers;

    const int acceptedResponseId {params()["rid"].toInt()};
    setResult(QJsonObject::fromVariantMap(generateSyncData(acceptedResponseId, data, lastAcceptedResponse, lastResponse)));

    sessionManager()->session()->setData(QLatin1String("syncTorrentPeersLastResponse"), lastResponse);
    sessionManager()->session()->setData(QLatin1String("syncTorrentPeersLastAcceptedResponse"), lastAcceptedResponse);
}

qint64 SyncController::getFreeDiskSpace()
{
    if (m_freeDiskSpaceElapsedTimer.hasExpired(FREEDISKSPACE_CHECK_TIMEOUT))
    {
        invokeChecker();
        m_freeDiskSpaceElapsedTimer.restart();
    }

    return m_freeDiskSpace;
}

void SyncController::freeDiskSpaceSizeUpdated(qint64 freeSpaceSize)
{
    m_freeDiskSpace = freeSpaceSize;
}

void SyncController::invokeChecker() const
{
    QMetaObject::invokeMethod(m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::check, Qt::QueuedConnection);
}
