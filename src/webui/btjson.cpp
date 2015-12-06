/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012, Christophe Dumez
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

#include "btjson.h"
#include "base/utils/misc.h"
#include "base/utils/fs.h"
#include "base/preferences.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/peerinfo.h"
#include "base/torrentfilter.h"
#include "base/net/geoipmanager.h"
#include "jsonutils.h"

#include <QDebug>
#include <QVariant>
#ifndef QBT_USES_QT5
#include <QMetaType>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
#include <QElapsedTimer>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)

#define CACHED_VARIABLE(VARTYPE, VAR, DUR) \
    static VARTYPE VAR; \
    static QElapsedTimer cacheTimer; \
    static bool initialized = false; \
    if (initialized && !cacheTimer.hasExpired(DUR)) \
        return json::toJson(VAR); \
    initialized = true; \
    cacheTimer.start(); \
    VAR = VARTYPE()

#define CACHED_VARIABLE_FOR_HASH(VARTYPE, VAR, DUR, HASH) \
    static VARTYPE VAR; \
    static QString prev_hash; \
    static QElapsedTimer cacheTimer; \
    if (prev_hash == HASH && !cacheTimer.hasExpired(DUR)) \
        return json::toJson(VAR); \
    prev_hash = HASH; \
    cacheTimer.start(); \
    VAR = VARTYPE()

#else
// We don't support caching for Qt < 4.7 at the moment
#define CACHED_VARIABLE(VARTYPE, VAR, DUR) \
    VARTYPE VAR

#define CACHED_VARIABLE_FOR_HASH(VARTYPE, VAR, DUR, HASH) \
    VARTYPE VAR

#endif

// Numerical constants
static const int CACHE_DURATION_MS = 1500; // 1500ms

// Torrent keys
static const char KEY_TORRENT_HASH[] = "hash";
static const char KEY_TORRENT_NAME[] = "name";
static const char KEY_TORRENT_SIZE[] = "size";
static const char KEY_TORRENT_PROGRESS[] = "progress";
static const char KEY_TORRENT_DLSPEED[] = "dlspeed";
static const char KEY_TORRENT_UPSPEED[] = "upspeed";
static const char KEY_TORRENT_PRIORITY[] = "priority";
static const char KEY_TORRENT_SEEDS[] = "num_seeds";
static const char KEY_TORRENT_NUM_COMPLETE[] = "num_complete";
static const char KEY_TORRENT_LEECHS[] = "num_leechs";
static const char KEY_TORRENT_NUM_INCOMPLETE[] = "num_incomplete";
static const char KEY_TORRENT_RATIO[] = "ratio";
static const char KEY_TORRENT_ETA[] = "eta";
static const char KEY_TORRENT_STATE[] = "state";
static const char KEY_TORRENT_SEQUENTIAL_DOWNLOAD[] = "seq_dl";
static const char KEY_TORRENT_FIRST_LAST_PIECE_PRIO[] = "f_l_piece_prio";
static const char KEY_TORRENT_LABEL[] = "label";
static const char KEY_TORRENT_SUPER_SEEDING[] = "super_seeding";
static const char KEY_TORRENT_FORCE_START[] = "force_start";
static const char KEY_TORRENT_SAVE_PATH[] = "save_path";

// Peer keys
static const char KEY_PEER_IP[] = "ip";
static const char KEY_PEER_PORT[] = "port";
static const char KEY_PEER_COUNTRY_CODE[] = "country_code";
static const char KEY_PEER_COUNTRY[] = "country";
static const char KEY_PEER_CLIENT[] = "client";
static const char KEY_PEER_PROGRESS[] = "progress";
static const char KEY_PEER_DOWN_SPEED[] = "dl_speed";
static const char KEY_PEER_UP_SPEED[] = "up_speed";
static const char KEY_PEER_TOT_DOWN[] = "downloaded";
static const char KEY_PEER_TOT_UP[] = "uploaded";
static const char KEY_PEER_CONNECTION_TYPE[] = "connection";
static const char KEY_PEER_FLAGS[] = "flags";
static const char KEY_PEER_FLAGS_DESCRIPTION[] = "flags_desc";
static const char KEY_PEER_RELEVANCE[] = "relevance";

// Tracker keys
static const char KEY_TRACKER_URL[] = "url";
static const char KEY_TRACKER_STATUS[] = "status";
static const char KEY_TRACKER_MSG[] = "msg";
static const char KEY_TRACKER_PEERS[] = "num_peers";

// Web seed keys
static const char KEY_WEBSEED_URL[] = "url";

// Torrent keys (Properties)
static const char KEY_PROP_TIME_ELAPSED[] = "time_elapsed";
static const char KEY_PROP_SEEDING_TIME[] = "seeding_time";
static const char KEY_PROP_ETA[] = "eta";
static const char KEY_PROP_CONNECT_COUNT[] = "nb_connections";
static const char KEY_PROP_CONNECT_COUNT_LIMIT[] = "nb_connections_limit";
static const char KEY_PROP_DOWNLOADED[] = "total_downloaded";
static const char KEY_PROP_DOWNLOADED_SESSION[] = "total_downloaded_session";
static const char KEY_PROP_UPLOADED[] = "total_uploaded";
static const char KEY_PROP_UPLOADED_SESSION[] = "total_uploaded_session";
static const char KEY_PROP_DL_SPEED[] = "dl_speed";
static const char KEY_PROP_DL_SPEED_AVG[] = "dl_speed_avg";
static const char KEY_PROP_UP_SPEED[] = "up_speed";
static const char KEY_PROP_UP_SPEED_AVG[] = "up_speed_avg";
static const char KEY_PROP_DL_LIMIT[] = "dl_limit";
static const char KEY_PROP_UP_LIMIT[] = "up_limit";
static const char KEY_PROP_WASTED[] = "total_wasted";
static const char KEY_PROP_SEEDS[] = "seeds";
static const char KEY_PROP_SEEDS_TOTAL[] = "seeds_total";
static const char KEY_PROP_PEERS[] = "peers";
static const char KEY_PROP_PEERS_TOTAL[] = "peers_total";
static const char KEY_PROP_RATIO[] = "share_ratio";
static const char KEY_PROP_REANNOUNCE[] = "reannounce";
static const char KEY_PROP_TOTAL_SIZE[] = "total_size";
static const char KEY_PROP_PIECES_NUM[] = "pieces_num";
static const char KEY_PROP_PIECE_SIZE[] = "piece_size";
static const char KEY_PROP_PIECES_HAVE[] = "pieces_have";
static const char KEY_PROP_CREATED_BY[] = "created_by";
static const char KEY_PROP_LAST_SEEN[] = "last_seen";
static const char KEY_PROP_ADDITION_DATE[] = "addition_date";
static const char KEY_PROP_COMPLETION_DATE[] = "completion_date";
static const char KEY_PROP_CREATION_DATE[] = "creation_date";
static const char KEY_PROP_SAVE_PATH[] = "save_path";
static const char KEY_PROP_COMMENT[] = "comment";

// File keys
static const char KEY_FILE_NAME[] = "name";
static const char KEY_FILE_SIZE[] = "size";
static const char KEY_FILE_PROGRESS[] = "progress";
static const char KEY_FILE_PRIORITY[] = "priority";
static const char KEY_FILE_IS_SEED[] = "is_seed";

// TransferInfo keys
static const char KEY_TRANSFER_DLSPEED[] = "dl_info_speed";
static const char KEY_TRANSFER_DLDATA[] = "dl_info_data";
static const char KEY_TRANSFER_DLRATELIMIT[] = "dl_rate_limit";
static const char KEY_TRANSFER_UPSPEED[] = "up_info_speed";
static const char KEY_TRANSFER_UPDATA[] = "up_info_data";
static const char KEY_TRANSFER_UPRATELIMIT[] = "up_rate_limit";
static const char KEY_TRANSFER_DHT_NODES[] = "dht_nodes";
static const char KEY_TRANSFER_CONNECTION_STATUS[] = "connection_status";

// Sync main data keys
static const char KEY_SYNC_MAINDATA_QUEUEING[] = "queueing";
static const char KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS[] = "use_alt_speed_limits";
static const char KEY_SYNC_MAINDATA_REFRESH_INTERVAL[] = "refresh_interval";

// Sync torrent peers keys
static const char KEY_SYNC_TORRENT_PEERS_SHOW_FLAGS[] = "show_flags";

static const char KEY_FULL_UPDATE[] = "full_update";
static const char KEY_RESPONSE_ID[] = "rid";
static const char KEY_SUFFIX_REMOVED[] = "_removed";

QVariantMap getTranserInfoMap();
QVariantMap toMap(BitTorrent::TorrentHandle *const torrent);
void processMap(QVariantMap prevData, QVariantMap data, QVariantMap &syncData);
void processHash(QVariantHash prevData, QVariantHash data, QVariantMap &syncData, QVariantList &removedItems);
void processList(QVariantList prevData, QVariantList data, QVariantList &syncData, QVariantList &removedItems);
QVariantMap generateSyncData(int acceptedResponseId, QVariantMap data, QVariantMap &lastAcceptedData,  QVariantMap &lastData);

class QTorrentCompare
{
public:
    QTorrentCompare(QString key, bool greaterThan = false)
        : key_(key)
        , greaterThan_(greaterThan)
#ifndef QBT_USES_QT5
        , type_(QVariant::Invalid)
#endif
    {
    }

    bool operator()(QVariant torrent1, QVariant torrent2)
    {
#ifndef QBT_USES_QT5
        if (type_ == QVariant::Invalid)
            type_ = torrent1.toMap().value(key_).type();

        switch (static_cast<QMetaType::Type>(type_)) {
        case QMetaType::Int:
            return greaterThan_ ? torrent1.toMap().value(key_).toInt() > torrent2.toMap().value(key_).toInt()
                   : torrent1.toMap().value(key_).toInt() < torrent2.toMap().value(key_).toInt();
        case QMetaType::LongLong:
            return greaterThan_ ? torrent1.toMap().value(key_).toLongLong() > torrent2.toMap().value(key_).toLongLong()
                   : torrent1.toMap().value(key_).toLongLong() < torrent2.toMap().value(key_).toLongLong();
        case QMetaType::ULongLong:
            return greaterThan_ ? torrent1.toMap().value(key_).toULongLong() > torrent2.toMap().value(key_).toULongLong()
                   : torrent1.toMap().value(key_).toULongLong() < torrent2.toMap().value(key_).toULongLong();
        case QMetaType::Float:
            return greaterThan_ ? torrent1.toMap().value(key_).toFloat() > torrent2.toMap().value(key_).toFloat()
                   : torrent1.toMap().value(key_).toFloat() < torrent2.toMap().value(key_).toFloat();
        case QMetaType::Double:
            return greaterThan_ ? torrent1.toMap().value(key_).toDouble() > torrent2.toMap().value(key_).toDouble()
                   : torrent1.toMap().value(key_).toDouble() < torrent2.toMap().value(key_).toDouble();
        default:
            return greaterThan_ ? torrent1.toMap().value(key_).toString() > torrent2.toMap().value(key_).toString()
                   : torrent1.toMap().value(key_).toString() < torrent2.toMap().value(key_).toString();
        }
#else
        return greaterThan_ ? torrent1.toMap().value(key_) > torrent2.toMap().value(key_)
               : torrent1.toMap().value(key_) < torrent2.toMap().value(key_);
#endif
    }

private:
    QString key_;
    bool greaterThan_;
#ifndef QBT_USES_QT5
    QVariant::Type type_;
#endif
};

/**
 * Returns all the torrents in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "hash": Torrent hash
 *   - "name": Torrent name
 *   - "size": Torrent size
 *   - "progress: Torrent progress
 *   - "dlspeed": Torrent download speed
 *   - "upspeed": Torrent upload speed
 *   - "priority": Torrent priority (-1 if queuing is disabled)
 *   - "num_seeds": Torrent seeds connected to
 *   - "num_complete": Torrent seeds in the swarm
 *   - "num_leechs": Torrent leechers connected to
 *   - "num_incomplete": Torrent leechers in the swarm
 *   - "ratio": Torrent share ratio
 *   - "eta": Torrent ETA
 *   - "state": Torrent state
 *   - "seq_dl": Torrent sequential download state
 *   - "f_l_piece_prio": Torrent first last piece priority state
 *   - "force_start": Torrent force start state
 *   - "label": Torrent label
 */
QByteArray btjson::getTorrents(QString filter, QString label,
                               QString sortedColumn, bool reverse, int limit, int offset)
{
    QVariantList torrentList;
    TorrentFilter torrentFilter(filter, TorrentFilter::AnyHash, label);
    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents()) {
        if (torrentFilter.match(torrent))
            torrentList.append(toMap(torrent));
    }

    std::sort(torrentList.begin(), torrentList.end(), QTorrentCompare(sortedColumn, reverse));
    int size = torrentList.size();
    // normalize offset
    if (offset < 0)
        offset = size + offset;
    if ((offset >= size) || (offset < 0))
        offset = 0;
    // normalize limit
    if (limit <= 0)
        limit = -1; // unlimited

    if ((limit > 0) || (offset > 0))
        return json::toJson(torrentList.mid(offset, limit));
    else
        return json::toJson(torrentList);
}

/**
 * The function returns the changed data from the server to synchronize with the web client.
 * Return value is map in JSON format.
 * Map contain the key:
 *  - "Rid": ID response
 * Map can contain the keys:
 *  - "full_update": full data update flag
 *  - "torrents": dictionary contains information about torrents.
 *  - "torrents_removed": a list of hashes of removed torrents
 *  - "labels": list of labels
 *  - "labels_removed": list of removed labels
 *  - "server_state": map contains information about the state of the server
 * The keys of the 'torrents' dictionary are hashes of torrents.
 * Each value of the 'torrents' dictionary contains map. The map can contain following keys:
 *  - "name": Torrent name
 *  - "size": Torrent size
 *  - "progress: Torrent progress
 *  - "dlspeed": Torrent download speed
 *  - "upspeed": Torrent upload speed
 *  - "priority": Torrent priority (-1 if queuing is disabled)
 *  - "num_seeds": Torrent seeds connected to
 *  - "num_complete": Torrent seeds in the swarm
 *  - "num_leechs": Torrent leechers connected to
 *  - "num_incomplete": Torrent leechers in the swarm
 *  - "ratio": Torrent share ratio
 *  - "eta": Torrent ETA
 *  - "state": Torrent state
 *  - "seq_dl": Torrent sequential download state
 *  - "f_l_piece_prio": Torrent first last piece priority state
 * Server state map may contain the following keys:
 *  - "connection_status": connection status
 *  - "dht_nodes": DHT nodes count
 *  - "dl_info_data": bytes downloaded
 *  - "dl_info_speed": download speed
 *  - "dl_rate_limit: download rate limit
 *  - "up_info_data: bytes uploaded
 *  - "up_info_speed: upload speed
 *  - "up_rate_limit: upload speed limit
 *  - "queueing": priority system usage flag
 *  - "refresh_interval": torrents table refresh interval
 */
QByteArray btjson::getSyncMainData(int acceptedResponseId, QVariantMap &lastData, QVariantMap &lastAcceptedData)
{
    QVariantMap data;
    QVariantHash torrents;

    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents()) {
        QVariantMap map = toMap(torrent);
        map.remove(KEY_TORRENT_HASH);
        torrents[torrent->hash()] = map;
    }

    data["torrents"] = torrents;

    QVariantList labels;
    foreach (QString s, Preferences::instance()->getTorrentLabels())
        labels << s;

    data["labels"] = labels;

    QVariantMap serverState = getTranserInfoMap();
    serverState[KEY_SYNC_MAINDATA_QUEUEING] = BitTorrent::Session::instance()->isQueueingEnabled();
    serverState[KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS] = Preferences::instance()->isAltBandwidthEnabled();
    serverState[KEY_SYNC_MAINDATA_REFRESH_INTERVAL] = Preferences::instance()->getRefreshInterval();
    data["server_state"] = serverState;

    return json::toJson(generateSyncData(acceptedResponseId, data, lastAcceptedData, lastData));
}

QByteArray btjson::getSyncTorrentPeersData(int acceptedResponseId, QString hash, QVariantMap &lastData, QVariantMap &lastAcceptedData)
{
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (!torrent) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent " << qPrintable(hash);
        return QByteArray();
    }

    QVariantMap data;
    QVariantHash peers;
    QList<BitTorrent::PeerInfo> peersList = torrent->peers();
#ifndef DISABLE_COUNTRIES_RESOLUTION
    bool resolvePeerCountries = Preferences::instance()->resolvePeerCountries();
#else
    bool resolvePeerCountries = false;
#endif

    data[KEY_SYNC_TORRENT_PEERS_SHOW_FLAGS] = resolvePeerCountries;

    foreach (const BitTorrent::PeerInfo &pi, peersList) {
        if (pi.address().ip.isNull()) continue;
        QVariantMap peer;
#ifndef DISABLE_COUNTRIES_RESOLUTION
        if (resolvePeerCountries) {
            peer[KEY_PEER_COUNTRY_CODE] = pi.country().toLower();
            peer[KEY_PEER_COUNTRY] = Net::GeoIPManager::CountryName(pi.country());
        }
#endif
        peer[KEY_PEER_IP] = pi.address().ip.toString();
        peer[KEY_PEER_PORT] = pi.address().port;
        peer[KEY_PEER_CLIENT] = pi.client();
        peer[KEY_PEER_PROGRESS] = pi.progress();
        peer[KEY_PEER_DOWN_SPEED] = pi.payloadDownSpeed();
        peer[KEY_PEER_UP_SPEED] = pi.payloadUpSpeed();
        peer[KEY_PEER_TOT_DOWN] = pi.totalDownload();
        peer[KEY_PEER_TOT_UP] = pi.totalUpload();
        peer[KEY_PEER_CONNECTION_TYPE] = pi.connectionType();
        peer[KEY_PEER_FLAGS] = pi.flags();
        peer[KEY_PEER_FLAGS_DESCRIPTION] = pi.flagsDescription();
        peer[KEY_PEER_RELEVANCE] = pi.relevance();
        peers[pi.address().ip.toString() + ":" + QString::number(pi.address().port)] = peer;
    }

    data["peers"] = peers;

    return json::toJson(generateSyncData(acceptedResponseId, data, lastAcceptedData, lastData));
}

/**
 * Returns the trackers for a torrent in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "url": Tracker URL
 *   - "status": Tracker status
 *   - "num_peers": Tracker peer count
 *   - "msg": Tracker message (last)
 */
QByteArray btjson::getTrackersForTorrent(const QString& hash)
{
    CACHED_VARIABLE_FOR_HASH(QVariantList, trackerList, CACHE_DURATION_MS, hash);
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (!torrent) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent " << qPrintable(hash);
        return QByteArray();
    }

    QHash<QString, BitTorrent::TrackerInfo> trackers_data = torrent->trackerInfos();
    foreach (const BitTorrent::TrackerEntry &tracker, torrent->trackers()) {
        QVariantMap trackerDict;
        trackerDict[KEY_TRACKER_URL] = tracker.url();
        const BitTorrent::TrackerInfo data = trackers_data.value(tracker.url());
        QString status;
        switch (tracker.status()) {
        case BitTorrent::TrackerEntry::NotContacted:
            status = tr("Not contacted yet"); break;
        case BitTorrent::TrackerEntry::Updating:
            status = tr("Updating..."); break;
        case BitTorrent::TrackerEntry::Working:
            status = tr("Working"); break;
        case BitTorrent::TrackerEntry::NotWorking:
            status = tr("Not working"); break;
        }
        trackerDict[KEY_TRACKER_STATUS] = status;
        trackerDict[KEY_TRACKER_PEERS] = data.numPeers;
        trackerDict[KEY_TRACKER_MSG] = data.lastMessage.trimmed();

        trackerList.append(trackerDict);
    }

    return json::toJson(trackerList);
}

/**
 * Returns the web seeds for a torrent in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "url": Web seed URL
 */
QByteArray btjson::getWebSeedsForTorrent(const QString& hash)
{
    CACHED_VARIABLE_FOR_HASH(QVariantList, webSeedList, CACHE_DURATION_MS, hash);
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (!torrent) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent " << qPrintable(hash);
        return QByteArray();
    }

    foreach (const QUrl &webseed, torrent->urlSeeds()) {
        QVariantMap webSeedDict;
        webSeedDict[KEY_WEBSEED_URL] = webseed.toString();
        webSeedList.append(webSeedDict);
    }

    return json::toJson(webSeedList);
}

/**
 * Returns the properties for a torrent in JSON format.
 *
 * The return value is a JSON-formatted dictionary.
 * The dictionary keys are:
 *   - "time_elapsed": Torrent elapsed time
 *   - "seeding_time": Torrent elapsed time while complete
 *   - "eta": Torrent ETA
 *   - "nb_connections": Torrent connection count
 *   - "nb_connections_limit": Torrent connection count limit
 *   - "total_downloaded": Total data uploaded for torrent
 *   - "total_downloaded_session": Total data downloaded this session
 *   - "total_uploaded": Total data uploaded for torrent
 *   - "total_uploaded_session": Total data uploaded this session
 *   - "dl_speed": Torrent download speed
 *   - "dl_speed_avg": Torrent average download speed
 *   - "up_speed": Torrent upload speed
 *   - "up_speed_avg": Torrent average upload speed
 *   - "dl_limit": Torrent download limit
 *   - "up_limit": Torrent upload limit
 *   - "total_wasted": Total data wasted for torrent
 *   - "seeds": Torrent connected seeds
 *   - "seeds_total": Torrent total number of seeds
 *   - "peers": Torrent connected peers
 *   - "peers_total": Torrent total number of peers
 *   - "share_ratio": Torrent share ratio
 *   - "reannounce": Torrent next reannounce time
 *   - "total_size": Torrent total size
 *   - "pieces_num": Torrent pieces count
 *   - "piece_size": Torrent piece size
 *   - "pieces_have": Torrent pieces have
 *   - "created_by": Torrent creator
 *   - "last_seen": Torrent last seen complete
 *   - "addition_date": Torrent addition date
 *   - "completion_date": Torrent completion date
 *   - "creation_date": Torrent creation date
 *   - "save_path": Torrent save path
 *   - "comment": Torrent comment
 */
QByteArray btjson::getPropertiesForTorrent(const QString& hash)
{
    CACHED_VARIABLE_FOR_HASH(QVariantMap, dataDict, CACHE_DURATION_MS, hash);
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (!torrent) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent " << qPrintable(hash);
        return QByteArray();
    }

    dataDict[KEY_PROP_TIME_ELAPSED] = torrent->activeTime();
    dataDict[KEY_PROP_SEEDING_TIME] = torrent->seedingTime();
    dataDict[KEY_PROP_ETA] = torrent->eta();
    dataDict[KEY_PROP_CONNECT_COUNT] = torrent->connectionsCount();
    dataDict[KEY_PROP_CONNECT_COUNT_LIMIT] = torrent->connectionsLimit();
    dataDict[KEY_PROP_DOWNLOADED] = torrent->totalDownload();
    dataDict[KEY_PROP_DOWNLOADED_SESSION] = torrent->totalPayloadDownload();
    dataDict[KEY_PROP_UPLOADED] = torrent->totalUpload();
    dataDict[KEY_PROP_UPLOADED_SESSION] = torrent->totalPayloadUpload();
    dataDict[KEY_PROP_DL_SPEED] = torrent->downloadPayloadRate();
    dataDict[KEY_PROP_DL_SPEED_AVG] = torrent->totalDownload() / (1 + torrent->activeTime() - torrent->finishedTime());
    dataDict[KEY_PROP_UP_SPEED] = torrent->uploadPayloadRate();
    dataDict[KEY_PROP_UP_SPEED_AVG] = torrent->totalUpload() / (1 + torrent->activeTime());
    dataDict[KEY_PROP_DL_LIMIT] = torrent->downloadLimit() <= 0 ? -1 : torrent->downloadLimit();
    dataDict[KEY_PROP_UP_LIMIT] = torrent->uploadLimit() <= 0 ? -1 : torrent->uploadLimit();
    dataDict[KEY_PROP_WASTED] = torrent->wastedSize();
    dataDict[KEY_PROP_SEEDS] = torrent->seedsCount();
    dataDict[KEY_PROP_SEEDS_TOTAL] = torrent->totalSeedsCount();
    dataDict[KEY_PROP_PEERS] = torrent->leechsCount();
    dataDict[KEY_PROP_PEERS_TOTAL] = torrent->totalLeechersCount();
    const qreal ratio = torrent->realRatio();
    dataDict[KEY_PROP_RATIO] = ratio > BitTorrent::TorrentHandle::MAX_RATIO ? -1 : ratio;
    dataDict[KEY_PROP_REANNOUNCE] = torrent->nextAnnounce();
    dataDict[KEY_PROP_TOTAL_SIZE] = torrent->totalSize();
    dataDict[KEY_PROP_PIECES_NUM] = torrent->piecesCount();
    dataDict[KEY_PROP_PIECE_SIZE] = torrent->pieceLength();
    dataDict[KEY_PROP_PIECES_HAVE] = torrent->piecesHave();
    dataDict[KEY_PROP_CREATED_BY] = torrent->creator();
    dataDict[KEY_PROP_ADDITION_DATE] = torrent->addedTime().toTime_t();
    if (torrent->hasMetadata()) {
        dataDict[KEY_PROP_LAST_SEEN] = torrent->lastSeenComplete().isValid() ? (int)torrent->lastSeenComplete().toTime_t() : -1;
        dataDict[KEY_PROP_COMPLETION_DATE] = torrent->completedTime().isValid() ? (int)torrent->completedTime().toTime_t() : -1;
        dataDict[KEY_PROP_CREATION_DATE] = torrent->creationDate().toTime_t();
    }
    else {
        dataDict[KEY_PROP_LAST_SEEN] = -1;
        dataDict[KEY_PROP_COMPLETION_DATE] = -1;
        dataDict[KEY_PROP_CREATION_DATE] = -1;
    }
    dataDict[KEY_PROP_SAVE_PATH] = Utils::Fs::toNativePath(torrent->savePath());
    dataDict[KEY_PROP_COMMENT] = torrent->comment();

    return json::toJson(dataDict);
}

/**
 * Returns the files in a torrent in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "name": File name
 *   - "size": File size
 *   - "progress": File progress
 *   - "priority": File priority
 *   - "is_seed": Flag indicating if torrent is seeding/complete
 */
QByteArray btjson::getFilesForTorrent(const QString& hash)
{
    CACHED_VARIABLE_FOR_HASH(QVariantList, fileList, CACHE_DURATION_MS, hash);
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (!torrent) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent " << qPrintable(hash);
        return QByteArray();
    }

    if (!torrent->hasMetadata())
        return json::toJson(fileList);

    const QVector<int> priorities = torrent->filePriorities();
    QVector<qreal> fp = torrent->filesProgress();
    for (int i = 0; i < torrent->filesCount(); ++i) {
        QVariantMap fileDict;
        QString fileName = torrent->filePath(i);
        if (fileName.endsWith(".!qB", Qt::CaseInsensitive))
            fileName.chop(4);
        fileDict[KEY_FILE_NAME] = Utils::Fs::toNativePath(fileName);
        const qlonglong size = torrent->fileSize(i);
        fileDict[KEY_FILE_SIZE] = size;
        fileDict[KEY_FILE_PROGRESS] = fp[i];
        fileDict[KEY_FILE_PRIORITY] = priorities[i];
        if (i == 0)
            fileDict[KEY_FILE_IS_SEED] = torrent->isSeed();

        fileList.append(fileDict);
    }

    return json::toJson(fileList);
}

/**
 * Returns the global transfer information in JSON format.
 *
 * The return value is a JSON-formatted dictionary.
 * The dictionary keys are:
 *   - "dl_info_speed": Global download rate
 *   - "dl_info_data": Data downloaded this session
 *   - "up_info_speed": Global upload rate
 *   - "up_info_data": Data uploaded this session
 *   - "dl_rate_limit": Download rate limit
 *   - "up_rate_limit": Upload rate limit
 *   - "dht_nodes": DHT nodes connected to
 *   - "connection_status": Connection status
 */
QByteArray btjson::getTransferInfo()
{
    return json::toJson(getTranserInfoMap());
}

QVariantMap getTranserInfoMap()
{
    QVariantMap map;
    BitTorrent::SessionStatus sessionStatus = BitTorrent::Session::instance()->status();
    map[KEY_TRANSFER_DLSPEED] = sessionStatus.payloadDownloadRate();
    map[KEY_TRANSFER_DLDATA] = sessionStatus.totalPayloadDownload();
    map[KEY_TRANSFER_UPSPEED] = sessionStatus.payloadUploadRate();
    map[KEY_TRANSFER_UPDATA] = sessionStatus.totalPayloadUpload();
    map[KEY_TRANSFER_DLRATELIMIT] = BitTorrent::Session::instance()->downloadRateLimit();
    map[KEY_TRANSFER_UPRATELIMIT] = BitTorrent::Session::instance()->uploadRateLimit();
    map[KEY_TRANSFER_DHT_NODES] = sessionStatus.dhtNodes();
    if (!BitTorrent::Session::instance()->isListening())
        map[KEY_TRANSFER_CONNECTION_STATUS] = "disconnected";
    else
        map[KEY_TRANSFER_CONNECTION_STATUS] = sessionStatus.hasIncomingConnections() ? "connected" : "firewalled";
    return map;
}

QByteArray btjson::getTorrentsRatesLimits(QStringList &hashes, bool downloadLimits)
{
    QVariantMap map;

    foreach (const QString &hash, hashes) {
        int limit = -1;
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            limit = downloadLimits ? torrent->downloadLimit() : torrent->uploadLimit();
        map[hash] = limit;
    }

    return json::toJson(map);
}

QVariantMap toMap(BitTorrent::TorrentHandle *const torrent)
{
    QVariantMap ret;
    ret[KEY_TORRENT_HASH] =  QString(torrent->hash());
    ret[KEY_TORRENT_NAME] =  torrent->name();
    ret[KEY_TORRENT_SIZE] =  torrent->wantedSize();
    ret[KEY_TORRENT_PROGRESS] = torrent->progress();
    ret[KEY_TORRENT_DLSPEED] = torrent->downloadPayloadRate();
    ret[KEY_TORRENT_UPSPEED] = torrent->uploadPayloadRate();
    ret[KEY_TORRENT_PRIORITY] = torrent->queuePosition();
    ret[KEY_TORRENT_SEEDS] = torrent->seedsCount();
    ret[KEY_TORRENT_NUM_COMPLETE] = torrent->completeCount();
    ret[KEY_TORRENT_LEECHS] = torrent->leechsCount();
    ret[KEY_TORRENT_NUM_INCOMPLETE] = torrent->incompleteCount();
    const qreal ratio = torrent->realRatio();
    ret[KEY_TORRENT_RATIO] = (ratio > BitTorrent::TorrentHandle::MAX_RATIO) ? -1 : ratio;
    ret[KEY_TORRENT_STATE] = torrent->state().toString();
    ret[KEY_TORRENT_ETA] = torrent->eta();
    ret[KEY_TORRENT_SEQUENTIAL_DOWNLOAD] = torrent->isSequentialDownload();
    if (torrent->hasMetadata())
        ret[KEY_TORRENT_FIRST_LAST_PIECE_PRIO] = torrent->hasFirstLastPiecePriority();
    ret[KEY_TORRENT_LABEL] = torrent->label();
    ret[KEY_TORRENT_SUPER_SEEDING] = torrent->superSeeding();
    ret[KEY_TORRENT_FORCE_START] = torrent->isForced();
    ret[KEY_TORRENT_SAVE_PATH] = Utils::Fs::toNativePath(torrent->savePath());

    return ret;
}

// Compare two structures (prevData, data) and calculate difference (syncData).
// Structures encoded as map.
void processMap(QVariantMap prevData, QVariantMap data, QVariantMap &syncData)
{
    // initialize output variable
    syncData.clear();

    QVariantList removedItems;
    foreach (QString key, data.keys()) {
        removedItems.clear();

        switch (static_cast<QMetaType::Type>(data[key].type())) {
        case QMetaType::QVariantMap: {
                QVariantMap map;
                processMap(prevData[key].toMap(), data[key].toMap(), map);
                if (!map.isEmpty())
                    syncData[key] = map;
            }
            break;
        case QMetaType::QVariantHash: {
                QVariantMap map;
                processHash(prevData[key].toHash(), data[key].toHash(), map, removedItems);
                if (!map.isEmpty())
                    syncData[key] = map;
                if (!removedItems.isEmpty())
                    syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
            }
            break;
        case QMetaType::QVariantList: {
                QVariantList list;
                processList(prevData[key].toList(), data[key].toList(), list, removedItems);
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
            if (prevData[key] != data[key])
                syncData[key] = data[key];
            break;
        default:
            Q_ASSERT(0);
        }
    }
}

// Compare two lists of structures (prevData, data) and calculate difference (syncData, removedItems).
// Structures encoded as map.
// Lists are encoded as hash table (indexed by structure key value) to improve ease of searching for removed items.
void processHash(QVariantHash prevData, QVariantHash data, QVariantMap &syncData, QVariantList &removedItems)
{
    // initialize output variables
    syncData.clear();
    removedItems.clear();

    if (prevData.isEmpty()) {
        // If list was empty before, then difference is a whole new list.
        foreach (QString key, data.keys())
            syncData[key] = data[key];
    }
    else {
        foreach (QString key, data.keys()) {
            switch (data[key].type()) {
            case QVariant::Map:
                if (!prevData.contains(key)) {
                    // new list item found - append it to syncData
                    syncData[key] = data[key];
                }
                else {
                    QVariantMap map;
                    processMap(prevData[key].toMap(), data[key].toMap(), map);
                    // existing list item found - remove it from prevData
                    prevData.remove(key);
                    if (!map.isEmpty())
                        // changed list item found - append its changes to syncData
                        syncData[key] = map;
                }
                break;
            default:
                Q_ASSERT(0);
            }
        }

        if (!prevData.isEmpty()) {
            // prevData contains only items that are missing now -
            // put them in removedItems
            foreach (QString s, prevData.keys())
                removedItems << s;
        }
    }
}

// Compare two lists of simple value (prevData, data) and calculate difference (syncData, removedItems).
void processList(QVariantList prevData, QVariantList data, QVariantList &syncData, QVariantList &removedItems)
{
    // initialize output variables
    syncData.clear();
    removedItems.clear();

    if (prevData.isEmpty()) {
        // If list was empty before, then difference is a whole new list.
        syncData = data;
    }
    else {
        foreach (QVariant item, data) {
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

QVariantMap generateSyncData(int acceptedResponseId, QVariantMap data, QVariantMap &lastAcceptedData,  QVariantMap &lastData)
{
    QVariantMap syncData;
    bool fullUpdate = true;
    int lastResponseId = 0;
    if (acceptedResponseId > 0) {
        lastResponseId = lastData[KEY_RESPONSE_ID].toInt();

        if (lastResponseId == acceptedResponseId)
            lastAcceptedData = lastData;

        int lastAcceptedResponseId = lastAcceptedData[KEY_RESPONSE_ID].toInt();

        if (lastAcceptedResponseId == acceptedResponseId) {
            processMap(lastAcceptedData, data, syncData);
            fullUpdate = false;
        }
    }

    if (fullUpdate) {
        lastAcceptedData.clear();
        syncData = data;

#if (QBT_USES_QT5 && QT_VERSION < QT_VERSION_CHECK(5, 5, 0))
        // QJsonDocument::fromVariant() supports QVariantHash only
        // since Qt5.5, so manually convert data["torrents"]
        QVariantMap torrentsMap;
        QVariantHash torrents = data["torrents"].toHash();
        foreach (const QString &key, torrents.keys())
            torrentsMap[key] = torrents[key];
        syncData["torrents"] = torrentsMap;
#endif

        syncData[KEY_FULL_UPDATE] = true;
    }

    lastResponseId = lastResponseId % 1000000 + 1;  // cycle between 1 and 1000000
    lastData = data;
    lastData[KEY_RESPONSE_ID] = lastResponseId;
    syncData[KEY_RESPONSE_ID] = lastResponseId;

    return syncData;
}
