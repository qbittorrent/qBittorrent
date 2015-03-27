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
#include "fs_utils.h"
#include "qbtsession.h"
#include "torrentpersistentdata.h"
#include "qtorrentfilter.h"
#include "jsonutils.h"

#include <QDebug>
#include <QVariant>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QMetaType>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
#include <QElapsedTimer>
#endif

#include <libtorrent/session_status.hpp>
#include <libtorrent/session.hpp>

using namespace libtorrent;

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

// Tracker keys
static const char KEY_TRACKER_URL[] = "url";
static const char KEY_TRACKER_STATUS[] = "status";
static const char KEY_TRACKER_MSG[] = "msg";
static const char KEY_TRACKER_PEERS[] = "num_peers";

// Torrent keys (Properties)
static const char KEY_PROP_SAVE_PATH[] = "save_path";
static const char KEY_PROP_CREATION_DATE[] = "creation_date";
static const char KEY_PROP_PIECE_SIZE[] = "piece_size";
static const char KEY_PROP_COMMENT[] = "comment";
static const char KEY_PROP_WASTED[] = "total_wasted";
static const char KEY_PROP_UPLOADED[] = "total_uploaded";
static const char KEY_PROP_UPLOADED_SESSION[] = "total_uploaded_session";
static const char KEY_PROP_DOWNLOADED[] = "total_downloaded";
static const char KEY_PROP_DOWNLOADED_SESSION[] = "total_downloaded_session";
static const char KEY_PROP_UP_LIMIT[] = "up_limit";
static const char KEY_PROP_DL_LIMIT[] = "dl_limit";
static const char KEY_PROP_TIME_ELAPSED[] = "time_elapsed";
static const char KEY_PROP_SEEDING_TIME[] = "seeding_time";
static const char KEY_PROP_CONNECT_COUNT[] = "nb_connections";
static const char KEY_PROP_CONNECT_COUNT_LIMIT[] = "nb_connections_limit";
static const char KEY_PROP_RATIO[] = "share_ratio";

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

static const char KEY_FULL_UPDATE[] = "full_update";
static const char KEY_RESPONSE_ID[] = "rid";
static const char KEY_SUFFIX_REMOVED[] = "_removed";

QVariantMap getTranserInfoMap();
QVariantMap toMap(const QTorrentHandle& h);
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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        , type_(QVariant::Invalid)
#endif
    {
    }

    bool operator()(QVariant torrent1, QVariant torrent2)
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        if (type_ == QVariant::Invalid)
            type_ = torrent1.toMap().value(key_).type();

        switch (type_) {
        case QVariant::Int:
            return greaterThan_ ? torrent1.toMap().value(key_).toInt() > torrent2.toMap().value(key_).toInt()
                   : torrent1.toMap().value(key_).toInt() < torrent2.toMap().value(key_).toInt();
        case QVariant::LongLong:
            return greaterThan_ ? torrent1.toMap().value(key_).toLongLong() > torrent2.toMap().value(key_).toLongLong()
                   : torrent1.toMap().value(key_).toLongLong() < torrent2.toMap().value(key_).toLongLong();
        case QVariant::ULongLong:
            return greaterThan_ ? torrent1.toMap().value(key_).toULongLong() > torrent2.toMap().value(key_).toULongLong()
                   : torrent1.toMap().value(key_).toULongLong() < torrent2.toMap().value(key_).toULongLong();
        case QMetaType::Float:
            return greaterThan_ ? torrent1.toMap().value(key_).toFloat() > torrent2.toMap().value(key_).toFloat()
                   : torrent1.toMap().value(key_).toFloat() < torrent2.toMap().value(key_).toFloat();
        case QVariant::Double:
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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
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
 *   - "label": Torrent label
 */
QByteArray btjson::getTorrents(QString filter, QString label,
                               QString sortedColumn, bool reverse, int limit, int offset)
{
    QVariantList torrent_list;

    std::vector<torrent_handle> torrents = QBtSession::instance()->getTorrents();
    std::vector<torrent_handle>::const_iterator it = torrents.begin();
    std::vector<torrent_handle>::const_iterator end = torrents.end();

    QTorrentFilter torrentFilter(filter, label);
    for (; it != end; ++it) {
        QTorrentHandle torrent = QTorrentHandle(*it);

        if (torrentFilter.apply(torrent))
            torrent_list.append(toMap(torrent));
    }

    std::sort(torrent_list.begin(), torrent_list.end(), QTorrentCompare(sortedColumn, reverse));
    int size = torrent_list.size();
    // normalize offset
    if (offset < 0)
        offset = size + offset;
    if ((offset >= size) || (offset < 0))
        offset = 0;
    // normalize limit
    if (limit <= 0)
        limit = -1; // unlimited

    if ((limit > 0) || (offset > 0))
        return json::toJson(torrent_list.mid(offset, limit));
    else
        return json::toJson(torrent_list);
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
 *  - "connection_status": conection status
 *  - "dht_nodes": DHT nodes count
 *  - "dl_info_data": bytes downloaded
 *  - "dl_info_speed": download speed
 *  - "dl_rate_limit: downlaod rate limit
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

    std::vector<torrent_handle> torrentsList = QBtSession::instance()->getTorrents();
    std::vector<torrent_handle>::const_iterator it = torrentsList.begin();
    std::vector<torrent_handle>::const_iterator end = torrentsList.end();

    for (; it != end; ++it) {
        QTorrentHandle torrent = QTorrentHandle(*it);
        QVariantMap map = toMap(torrent);
        map.remove(KEY_TORRENT_HASH);
        torrents[torrent.hash()] = map;
    }

    data["torrents"] = torrents;

    QVariantList labels;
    foreach (QString s, Preferences::instance()->getTorrentLabels())
        labels << s;

    data["labels"] = labels;

    QVariantMap serverState = getTranserInfoMap();
    serverState[KEY_SYNC_MAINDATA_QUEUEING] = QBtSession::instance()->isQueueingEnabled();
    serverState[KEY_SYNC_MAINDATA_USE_ALT_SPEED_LIMITS] = Preferences::instance()->isAltBandwidthEnabled();
    serverState[KEY_SYNC_MAINDATA_REFRESH_INTERVAL] = Preferences::instance()->getRefreshInterval();
    data["server_state"] = serverState;

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
    CACHED_VARIABLE_FOR_HASH(QVariantList, tracker_list, CACHE_DURATION_MS, hash);
    try {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        QHash<QString, TrackerInfos> trackers_data = QBtSession::instance()->getTrackersInfo(hash);
        std::vector<announce_entry> vect_trackers = h.trackers();
        std::vector<announce_entry>::const_iterator it = vect_trackers.begin();
        std::vector<announce_entry>::const_iterator end = vect_trackers.end();
        for (; it != end; ++it) {
            QVariantMap tracker_dict;
            const QString tracker_url = misc::toQString(it->url);
            tracker_dict[KEY_TRACKER_URL] = tracker_url;
            const TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
            QString status;
            if (it->verified) {
                status = tr("Working");
            }
            else {
                if (it->updating && it->fails == 0)
                    status = tr("Updating...");
                else
                    status = it->fails > 0 ? tr("Not working") : tr("Not contacted yet");
            }
            tracker_dict[KEY_TRACKER_STATUS] = status;
            tracker_dict[KEY_TRACKER_PEERS] = static_cast<qulonglong>(trackers_data.value(tracker_url, TrackerInfos(tracker_url)).num_peers);
            tracker_dict[KEY_TRACKER_MSG] = data.last_message.trimmed();

            tracker_list.append(tracker_dict);
        }
    }
    catch (const std::exception& e) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent: " << misc::toQStringU(e.what());
        return QByteArray();
    }

    return json::toJson(tracker_list);
}

/**
 * Returns the properties for a torrent in JSON format.
 *
 * The return value is a JSON-formatted dictionary.
 * The dictionary keys are:
 *   - "save_path": Torrent save path
 *   - "creation_date": Torrent creation date
 *   - "piece_size": Torrent piece size
 *   - "comment": Torrent comment
 *   - "total_wasted": Total data wasted for torrent
 *   - "total_uploaded": Total data uploaded for torrent
 *   - "total_uploaded_session": Total data uploaded this session
 *   - "total_downloaded": Total data uploaded for torrent
 *   - "total_downloaded_session": Total data downloaded this session
 *   - "up_limit": Torrent upload limit
 *   - "dl_limit": Torrent download limit
 *   - "time_elapsed": Torrent elapsed time
 *   - "seeding_time": Torrent elapsed time while complete
 *   - "nb_connections": Torrent connection count
 *   - "nb_connections_limit": Torrent connection count limit
 *   - "share_ratio": Torrent share ratio
 */
QByteArray btjson::getPropertiesForTorrent(const QString& hash)
{
    CACHED_VARIABLE_FOR_HASH(QVariantMap, data, CACHE_DURATION_MS, hash);
    try {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

        libtorrent::torrent_status status = h.status(torrent_handle::query_accurate_download_counters);
        if (!status.has_metadata)
            return QByteArray();

        // Save path
        QString save_path = fsutils::toNativePath(TorrentPersistentData::instance()->getSavePath(hash));
        if (save_path.isEmpty())
            save_path = fsutils::toNativePath(h.save_path());
        data[KEY_PROP_SAVE_PATH] = save_path;
        data[KEY_PROP_CREATION_DATE] = h.creation_date_unix();
        data[KEY_PROP_PIECE_SIZE] = static_cast<qlonglong>(h.piece_length());
        data[KEY_PROP_COMMENT] = h.comment();
        data[KEY_PROP_WASTED] = static_cast<qlonglong>(status.total_failed_bytes + status.total_redundant_bytes);
        data[KEY_PROP_UPLOADED] = static_cast<qlonglong>(status.all_time_upload);
        data[KEY_PROP_UPLOADED_SESSION] = static_cast<qlonglong>(status.total_payload_upload);
        data[KEY_PROP_DOWNLOADED] = static_cast<qlonglong>(status.all_time_download);
        data[KEY_PROP_DOWNLOADED_SESSION] = static_cast<qlonglong>(status.total_payload_download);
        data[KEY_PROP_UP_LIMIT] = h.upload_limit() <= 0 ? -1 : h.upload_limit();
        data[KEY_PROP_DL_LIMIT] = h.download_limit() <= 0 ? -1 : h.download_limit();
        data[KEY_PROP_TIME_ELAPSED] = status.active_time;
        data[KEY_PROP_SEEDING_TIME] = status.seeding_time;
        data[KEY_PROP_CONNECT_COUNT] = status.num_connections;
        data[KEY_PROP_CONNECT_COUNT_LIMIT] = status.connections_limit;
        const qreal ratio = QBtSession::instance()->getRealRatio(status);
        data[KEY_PROP_RATIO] = ratio > QBtSession::MAX_RATIO ? -1 : ratio;
    }
    catch (const std::exception& e) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent: " << misc::toQStringU(e.what());
        return QByteArray();
    }

    return json::toJson(data);
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
    CACHED_VARIABLE_FOR_HASH(QVariantList, file_list, CACHE_DURATION_MS, hash);
    try {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (!h.has_metadata())
            return QByteArray();

        const std::vector<int> priorities = h.file_priorities();
        std::vector<size_type> fp;
        h.file_progress(fp);
        for (int i = 0; i < h.num_files(); ++i) {
            QVariantMap file_dict;
            QString fileName = h.filename_at(i);
            if (fileName.endsWith(".!qB", Qt::CaseInsensitive))
                fileName.chop(4);
            file_dict[KEY_FILE_NAME] = fsutils::toNativePath(fileName);
            const size_type size = h.filesize_at(i);
            file_dict[KEY_FILE_SIZE] = static_cast<qlonglong>(size);
            file_dict[KEY_FILE_PROGRESS] = (size > 0) ? (fp[i] / (double) size) : 1.;
            file_dict[KEY_FILE_PRIORITY] = priorities[i];
            if (i == 0)
                file_dict[KEY_FILE_IS_SEED] = h.is_seed();

            file_list.append(file_dict);
        }
    }
    catch (const std::exception& e) {
        qWarning() << Q_FUNC_INFO << "Invalid torrent: " << misc::toQStringU(e.what());
        return QByteArray();
    }

    return json::toJson(file_list);
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
    session_status sessionStatus = QBtSession::instance()->getSessionStatus();
    session_settings sessionSettings = QBtSession::instance()->getSession()->settings();
    map[KEY_TRANSFER_DLSPEED] = sessionStatus.payload_download_rate;
    map[KEY_TRANSFER_DLDATA] = static_cast<qlonglong>(sessionStatus.total_payload_download);
    map[KEY_TRANSFER_UPSPEED] = sessionStatus.payload_upload_rate;
    map[KEY_TRANSFER_UPDATA] = static_cast<qlonglong>(sessionStatus.total_payload_upload);
    map[KEY_TRANSFER_DLRATELIMIT] = sessionSettings.download_rate_limit;
    map[KEY_TRANSFER_UPRATELIMIT] = sessionSettings.upload_rate_limit;
    map[KEY_TRANSFER_DHT_NODES] = sessionStatus.dht_nodes;
    if (!QBtSession::instance()->getSession()->is_listening())
        map[KEY_TRANSFER_CONNECTION_STATUS] = "disconnected";
    else
        map[KEY_TRANSFER_CONNECTION_STATUS] = sessionStatus.has_incoming_connections ? "connected" : "firewalled";
    return map;
}

QByteArray btjson::getTorrentsRatesLimits(QStringList &hashes, bool downloadLimits)
{
    QVariantMap map;

    foreach (const QString &hash, hashes) {
        int limit = -1;
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid())
            limit = downloadLimits ? h.download_limit() : h.upload_limit();
        map[hash] = limit;
    }

   return json::toJson(map);
}

QVariantMap toMap(const QTorrentHandle& h)
{
    libtorrent::torrent_status status = h.status(torrent_handle::query_accurate_download_counters);

    QVariantMap ret;
    ret[KEY_TORRENT_HASH] =  h.hash();
    ret[KEY_TORRENT_NAME] =  h.name();
    ret[KEY_TORRENT_SIZE] =  static_cast<qlonglong>(status.total_wanted);
    ret[KEY_TORRENT_PROGRESS] = h.progress(status);
    ret[KEY_TORRENT_DLSPEED] = status.download_payload_rate;
    ret[KEY_TORRENT_UPSPEED] = status.upload_payload_rate;
    if (QBtSession::instance()->isQueueingEnabled() && h.queue_position(status) >= 0)
        ret[KEY_TORRENT_PRIORITY] = h.queue_position(status);
    else
        ret[KEY_TORRENT_PRIORITY] = -1;
    ret[KEY_TORRENT_SEEDS] = status.num_seeds;
    ret[KEY_TORRENT_NUM_COMPLETE] = status.num_complete;
    ret[KEY_TORRENT_LEECHS] = status.num_peers - status.num_seeds;
    ret[KEY_TORRENT_NUM_INCOMPLETE] = status.num_incomplete;
    const qreal ratio = QBtSession::instance()->getRealRatio(status);
    ret[KEY_TORRENT_RATIO] = (ratio > QBtSession::MAX_RATIO) ? -1 : ratio;
    ret[KEY_TORRENT_STATE] = h.torrentState().toString();
    ret[KEY_TORRENT_ETA] = h.eta();
    ret[KEY_TORRENT_SEQUENTIAL_DOWNLOAD] = status.sequential_download;
    if (h.has_metadata())
        ret[KEY_TORRENT_FIRST_LAST_PIECE_PRIO] = h.first_last_piece_first();
    ret[KEY_TORRENT_LABEL] = TorrentPersistentData::instance()->getLabel(h.hash());
    ret[KEY_TORRENT_SUPER_SEEDING] = status.super_seeding;

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

        switch (data[key].type()) {
        case QVariant::Map: {
                QVariantMap map;
                processMap(prevData[key].toMap(), data[key].toMap(), map);
                if (!map.isEmpty())
                    syncData[key] = map;
            }
            break;
        case QVariant::Hash: {
                QVariantMap map;
                processHash(prevData[key].toHash(), data[key].toHash(), map, removedItems);
                if (!map.isEmpty())
                    syncData[key] = map;
                if (!removedItems.isEmpty())
                    syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
            }
            break;
        case QVariant::List: {
                QVariantList list;
                processList(prevData[key].toList(), data[key].toList(), list, removedItems);
                if (!list.isEmpty())
                    syncData[key] = list;
                if (!removedItems.isEmpty())
                    syncData[key + KEY_SUFFIX_REMOVED] = removedItems;
            }
            break;
        case QVariant::String:
        case QVariant::LongLong:
        case QMetaType::Float:
        case QVariant::Int:
        case QVariant::Bool:
        case QVariant::Double:
        case QVariant::ULongLong:
        case QVariant::UInt:
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
        syncData[KEY_FULL_UPDATE] = true;
    }

    lastResponseId = lastResponseId % 1000000 + 1;  // cycle between 1 and 1000000
    lastData = data;
    lastData[KEY_RESPONSE_ID] = lastResponseId;
    syncData[KEY_RESPONSE_ID] = lastResponseId;

    return syncData;
}
