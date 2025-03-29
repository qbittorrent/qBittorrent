/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentscontroller.h"

#include <concepts>
#include <functional>

#include <QBitArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QUrl>

#include "base/addtorrentmanager.h"
#include "base/bittorrent/categoryoptions.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sslparameters.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/interfaces/iapplication.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/torrentfilter.h"
#include "base/utils/datetime.h"
#include "base/utils/fs.h"
#include "base/utils/sslkey.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "serialize/serialize_torrent.h"

// Tracker keys
const QString KEY_TRACKER_URL = u"url"_s;
const QString KEY_TRACKER_STATUS = u"status"_s;
const QString KEY_TRACKER_TIER = u"tier"_s;
const QString KEY_TRACKER_MSG = u"msg"_s;
const QString KEY_TRACKER_PEERS_COUNT = u"num_peers"_s;
const QString KEY_TRACKER_SEEDS_COUNT = u"num_seeds"_s;
const QString KEY_TRACKER_LEECHES_COUNT = u"num_leeches"_s;
const QString KEY_TRACKER_DOWNLOADED_COUNT = u"num_downloaded"_s;

// Web seed keys
const QString KEY_WEBSEED_URL = u"url"_s;

// Torrent keys (Properties)
const QString KEY_PROP_TIME_ELAPSED = u"time_elapsed"_s;
const QString KEY_PROP_SEEDING_TIME = u"seeding_time"_s;
const QString KEY_PROP_ETA = u"eta"_s;
const QString KEY_PROP_CONNECT_COUNT = u"nb_connections"_s;
const QString KEY_PROP_CONNECT_COUNT_LIMIT = u"nb_connections_limit"_s;
const QString KEY_PROP_DOWNLOADED = u"total_downloaded"_s;
const QString KEY_PROP_DOWNLOADED_SESSION = u"total_downloaded_session"_s;
const QString KEY_PROP_UPLOADED = u"total_uploaded"_s;
const QString KEY_PROP_UPLOADED_SESSION = u"total_uploaded_session"_s;
const QString KEY_PROP_DL_SPEED = u"dl_speed"_s;
const QString KEY_PROP_DL_SPEED_AVG = u"dl_speed_avg"_s;
const QString KEY_PROP_UP_SPEED = u"up_speed"_s;
const QString KEY_PROP_UP_SPEED_AVG = u"up_speed_avg"_s;
const QString KEY_PROP_DL_LIMIT = u"dl_limit"_s;
const QString KEY_PROP_UP_LIMIT = u"up_limit"_s;
const QString KEY_PROP_WASTED = u"total_wasted"_s;
const QString KEY_PROP_SEEDS = u"seeds"_s;
const QString KEY_PROP_SEEDS_TOTAL = u"seeds_total"_s;
const QString KEY_PROP_PEERS = u"peers"_s;
const QString KEY_PROP_PEERS_TOTAL = u"peers_total"_s;
const QString KEY_PROP_RATIO = u"share_ratio"_s;
const QString KEY_PROP_POPULARITY = u"popularity"_s;
const QString KEY_PROP_REANNOUNCE = u"reannounce"_s;
const QString KEY_PROP_TOTAL_SIZE = u"total_size"_s;
const QString KEY_PROP_PIECES_NUM = u"pieces_num"_s;
const QString KEY_PROP_PIECE_SIZE = u"piece_size"_s;
const QString KEY_PROP_PIECES_HAVE = u"pieces_have"_s;
const QString KEY_PROP_CREATED_BY = u"created_by"_s;
const QString KEY_PROP_LAST_SEEN = u"last_seen"_s;
const QString KEY_PROP_ADDITION_DATE = u"addition_date"_s;
const QString KEY_PROP_COMPLETION_DATE = u"completion_date"_s;
const QString KEY_PROP_CREATION_DATE = u"creation_date"_s;
const QString KEY_PROP_SAVE_PATH = u"save_path"_s;
const QString KEY_PROP_DOWNLOAD_PATH = u"download_path"_s;
const QString KEY_PROP_COMMENT = u"comment"_s;
const QString KEY_PROP_IS_PRIVATE = u"is_private"_s; // deprecated, "private" should be used instead
const QString KEY_PROP_PRIVATE = u"private"_s;
const QString KEY_PROP_SSL_CERTIFICATE = u"ssl_certificate"_s;
const QString KEY_PROP_SSL_PRIVATEKEY = u"ssl_private_key"_s;
const QString KEY_PROP_SSL_DHPARAMS = u"ssl_dh_params"_s;
const QString KEY_PROP_HAS_METADATA = u"has_metadata"_s;
const QString KEY_PROP_PROGRESS = u"progress"_s;
const QString KEY_PROP_TRACKERS = u"trackers"_s;


// File keys
const QString KEY_FILE_INDEX = u"index"_s;
const QString KEY_FILE_NAME = u"name"_s;
const QString KEY_FILE_SIZE = u"size"_s;
const QString KEY_FILE_PROGRESS = u"progress"_s;
const QString KEY_FILE_PRIORITY = u"priority"_s;
const QString KEY_FILE_IS_SEED = u"is_seed"_s;
const QString KEY_FILE_PIECE_RANGE = u"piece_range"_s;
const QString KEY_FILE_AVAILABILITY = u"availability"_s;

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;
    using Utils::String::parseDouble;

    const QSet<QString> SUPPORTED_WEB_SEED_SCHEMES {u"http"_s, u"https"_s, u"ftp"_s};

    template <typename Func>
    void applyToTorrents(const QStringList &idList, Func func)
        requires std::invocable<Func, BitTorrent::Torrent *>
    {
        if ((idList.size() == 1) && (idList[0] == u"all"))
        {
            for (BitTorrent::Torrent *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
                func(torrent);
        }
        else
        {
            for (const QString &idString : idList)
            {
                const auto hash = BitTorrent::TorrentID::fromString(idString);
                BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(hash);
                if (torrent)
                    func(torrent);
            }
        }
    }

    std::optional<QString> getOptionalString(const StringMap &params, const QString &name)
    {
        const auto it = params.constFind(name);
        if (it == params.cend())
            return std::nullopt;

        return it.value();
    }

    std::optional<Tag> getOptionalTag(const StringMap &params, const QString &name)
    {
        const auto it = params.constFind(name);
        if (it == params.cend())
            return std::nullopt;

        return Tag(it.value());
    }

    QJsonArray getStickyTrackers(const BitTorrent::Torrent *const torrent)
    {
        int seedsDHT = 0, seedsPeX = 0, seedsLSD = 0, leechesDHT = 0, leechesPeX = 0, leechesLSD = 0;
        for (const BitTorrent::PeerInfo &peer : asConst(torrent->peers()))
        {
            if (peer.isConnecting()) continue;

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
                    ++leechesDHT;
                if (peer.fromPeX())
                    ++leechesPeX;
                if (peer.fromLSD())
                    ++leechesLSD;
            }
        }

        const int working = static_cast<int>(BitTorrent::TrackerEndpointState::Working);
        const int disabled = 0;

        const QString privateMsg {QCoreApplication::translate("TrackerListWidget", "This torrent is private")};
        const bool isTorrentPrivate = torrent->isPrivate();

        const QJsonObject dht
        {
            {KEY_TRACKER_URL, u"** [DHT] **"_s},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : u""_s)},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isDHTEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsDHT},
            {KEY_TRACKER_LEECHES_COUNT, leechesDHT}
        };

        const QJsonObject pex
        {
            {KEY_TRACKER_URL, u"** [PeX] **"_s},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : u""_s)},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isPeXEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsPeX},
            {KEY_TRACKER_LEECHES_COUNT, leechesPeX}
        };

        const QJsonObject lsd
        {
            {KEY_TRACKER_URL, u"** [LSD] **"_s},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : u""_s)},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isLSDEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsLSD},
            {KEY_TRACKER_LEECHES_COUNT, leechesLSD}
        };

        return {dht, pex, lsd};
    }

    QJsonArray getTrackers(const BitTorrent::Torrent *const torrent)
    {
        QJsonArray trackerList;

        for (const BitTorrent::TrackerEntryStatus &tracker : asConst(torrent->trackers()))
        {
            const bool isNotWorking = (tracker.state == BitTorrent::TrackerEndpointState::NotWorking)
                    || (tracker.state == BitTorrent::TrackerEndpointState::TrackerError)
                    || (tracker.state == BitTorrent::TrackerEndpointState::Unreachable);
            trackerList << QJsonObject
            {
                {KEY_TRACKER_URL, tracker.url},
                {KEY_TRACKER_TIER, tracker.tier},
                {KEY_TRACKER_STATUS, static_cast<int>((isNotWorking ? BitTorrent::TrackerEndpointState::NotWorking : tracker.state))},
                {KEY_TRACKER_MSG, tracker.message},
                {KEY_TRACKER_PEERS_COUNT, tracker.numPeers},
                {KEY_TRACKER_SEEDS_COUNT, tracker.numSeeds},
                {KEY_TRACKER_LEECHES_COUNT, tracker.numLeeches},
                {KEY_TRACKER_DOWNLOADED_COUNT, tracker.numDownloaded}
            };
        }

        return trackerList;
    }

    QList<BitTorrent::TorrentID> toTorrentIDs(const QStringList &idStrings)
    {
        QList<BitTorrent::TorrentID> idList;
        idList.reserve(idStrings.size());
        for (const QString &hash : idStrings)
            idList << BitTorrent::TorrentID::fromString(hash);
        return idList;
    }

    nonstd::expected<QUrl, QString> validateWebSeedUrl(const QString &urlStr)
    {
        const QString normalizedUrlStr = QUrl::fromPercentEncoding(urlStr.toLatin1());

        const QUrl url {normalizedUrlStr, QUrl::StrictMode};
        if (!url.isValid())
            return nonstd::make_unexpected(TorrentsController::tr("\"%1\" is not a valid URL").arg(normalizedUrlStr));

        if (!SUPPORTED_WEB_SEED_SCHEMES.contains(url.scheme()))
            return nonstd::make_unexpected(TorrentsController::tr("URL scheme must be one of [%1]").arg(SUPPORTED_WEB_SEED_SCHEMES.values().join(u", ")));

        return url;
    }
}

void TorrentsController::countAction()
{
    setResult(QString::number(BitTorrent::Session::instance()->torrentsCount()));
}

// Returns all the torrents in JSON format.
// The return value is a JSON-formatted list of dictionaries.
// The dictionary keys are:
//   - "hash": Torrent hash (ID)
//   - "name": Torrent name
//   - "size": Torrent size
//   - "progress": Torrent progress
//   - "dlspeed": Torrent download speed
//   - "upspeed": Torrent upload speed
//   - "priority": Torrent queue position (-1 if queuing is disabled)
//   - "num_seeds": Torrent seeds connected to
//   - "num_complete": Torrent seeds in the swarm
//   - "num_leechs": Torrent leechers connected to
//   - "num_incomplete": Torrent leechers in the swarm
//   - "ratio": Torrent share ratio
//   - "eta": Torrent ETA
//   - "state": Torrent state
//   - "seq_dl": Torrent sequential download state
//   - "f_l_piece_prio": Torrent first last piece priority state
//   - "force_start": Torrent force start state
//   - "category": Torrent category
// GET params:
//   - filter (string): all, downloading, seeding, completed, stopped, running, active, inactive, stalled, stalled_uploading, stalled_downloading
//   - category (string): torrent category for filtering by it (empty string means "uncategorized"; no "category" param presented means "any category")
//   - tag (string): torrent tag for filtering by it (empty string means "untagged"; no "tag" param presented means "any tag")
//   - hashes (string): filter by hashes, can contain multiple hashes separated by |
//   - private (bool): filter torrents that are from private trackers (true) or not (false). Empty means any torrent (no filtering)
//   - includeTrackers (bool): include trackers in list output (true) or not (false). Empty means not included
//   - sort (string): name of column for sorting by its value
//   - reverse (bool): enable reverse sorting
//   - limit (int): set limit number of torrents returned (if greater than 0, otherwise - unlimited)
//   - offset (int): set offset (if less than 0 - offset from end)
void TorrentsController::infoAction()
{
    const QString filter {params()[u"filter"_s]};
    const std::optional<QString> category = getOptionalString(params(), u"category"_s);
    const std::optional<Tag> tag = getOptionalTag(params(), u"tag"_s);
    const QString sortedColumn {params()[u"sort"_s]};
    const bool reverse {parseBool(params()[u"reverse"_s]).value_or(false)};
    int limit {params()[u"limit"_s].toInt()};
    int offset {params()[u"offset"_s].toInt()};
    const QStringList hashes {params()[u"hashes"_s].split(u'|', Qt::SkipEmptyParts)};
    const std::optional<bool> isPrivate = parseBool(params()[u"private"_s]);
    const bool includeTrackers = parseBool(params()[u"includeTrackers"_s]).value_or(false);

    std::optional<TorrentIDSet> idSet;
    if (!hashes.isEmpty())
    {
        idSet = TorrentIDSet();
        for (const QString &hash : hashes)
            idSet->insert(BitTorrent::TorrentID::fromString(hash));
    }

    const TorrentFilter torrentFilter {filter, idSet, category, tag, isPrivate};
    QVariantList torrentList;
    for (const BitTorrent::Torrent *torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        if (!torrentFilter.match(torrent))
            continue;

        QVariantMap serializedTorrent = serialize(*torrent);

        if (includeTrackers)
            serializedTorrent.insert(KEY_PROP_TRACKERS, getTrackers(torrent));

        torrentList.append(serializedTorrent);
    }

    if (torrentList.isEmpty())
    {
        setResult(QJsonArray {});
        return;
    }

    if (!sortedColumn.isEmpty())
    {
        if (!torrentList[0].toMap().contains(sortedColumn))
            throw APIError(APIErrorType::BadParams, tr("'sort' parameter is invalid"));

        const auto lessThan = [](const QVariant &left, const QVariant &right) -> bool
        {
            Q_ASSERT(left.userType() == right.userType());

            switch (left.userType())
            {
            case QMetaType::Bool:
                return left.value<bool>() < right.value<bool>();
            case QMetaType::Double:
                return left.value<double>() < right.value<double>();
            case QMetaType::Float:
                return left.value<float>() < right.value<float>();
            case QMetaType::Int:
                return left.value<int>() < right.value<int>();
            case QMetaType::LongLong:
                return left.value<qlonglong>() < right.value<qlonglong>();
            case QMetaType::QString:
                return left.value<QString>() < right.value<QString>();
            default:
                qWarning("Unhandled QVariant comparison, type: %d, name: %s"
                        , left.userType(), left.metaType().name());
                break;
            }
            return false;
        };

        std::sort(torrentList.begin(), torrentList.end()
            , [reverse, &sortedColumn, &lessThan](const QVariant &torrent1, const QVariant &torrent2)
        {
            const QVariant value1 {torrent1.toMap().value(sortedColumn)};
            const QVariant value2 {torrent2.toMap().value(sortedColumn)};
            return reverse ? lessThan(value2, value1) : lessThan(value1, value2);
        });
    }

    const int size = torrentList.size();
    // normalize offset
    if (offset < 0)
        offset = size + offset;
    // normalize limit
    if (limit <= 0)
        limit = -1; // unlimited

    if ((limit > 0) || (offset > 0))
        torrentList = torrentList.mid(offset, limit);

    setResult(QJsonArray::fromVariantList(torrentList));
}

// Returns the properties for a torrent in JSON format.
// The return value is a JSON-formatted dictionary.
// The dictionary keys are:
//   - "time_elapsed": Torrent elapsed time
//   - "seeding_time": Torrent elapsed time while complete
//   - "eta": Torrent ETA
//   - "nb_connections": Torrent connection count
//   - "nb_connections_limit": Torrent connection count limit
//   - "total_downloaded": Total data uploaded for torrent
//   - "total_downloaded_session": Total data downloaded this session
//   - "total_uploaded": Total data uploaded for torrent
//   - "total_uploaded_session": Total data uploaded this session
//   - "dl_speed": Torrent download speed
//   - "dl_speed_avg": Torrent average download speed
//   - "up_speed": Torrent upload speed
//   - "up_speed_avg": Torrent average upload speed
//   - "dl_limit": Torrent download limit
//   - "up_limit": Torrent upload limit
//   - "total_wasted": Total data wasted for torrent
//   - "seeds": Torrent connected seeds
//   - "seeds_total": Torrent total number of seeds
//   - "peers": Torrent connected peers
//   - "peers_total": Torrent total number of peers
//   - "share_ratio": Torrent share ratio
//   - "popularity": Torrent popularity
//   - "reannounce": Torrent next reannounce time
//   - "total_size": Torrent total size
//   - "pieces_num": Torrent pieces count
//   - "piece_size": Torrent piece size
//   - "pieces_have": Torrent pieces have
//   - "created_by": Torrent creator
//   - "last_seen": Torrent last seen complete
//   - "addition_date": Torrent addition date
//   - "completion_date": Torrent completion date
//   - "creation_date": Torrent creation date
//   - "save_path": Torrent save path
//   - "download_path": Torrent download path
//   - "comment": Torrent comment
//   - "infohash_v1": Torrent v1 infohash (or empty string for v2 torrents)
//   - "infohash_v2": Torrent v2 infohash (or empty string for v1 torrents)
//   - "hash": Torrent TorrentID (infohashv1 for v1 torrents, truncated infohashv2 for v2/hybrid torrents)
//   - "name": Torrent name
void TorrentsController::propertiesAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const BitTorrent::InfoHash infoHash = torrent->infoHash();
    const qlonglong totalDownload = torrent->totalDownload();
    const qlonglong totalUpload = torrent->totalUpload();
    const qlonglong dlDuration = torrent->activeTime() - torrent->finishedTime();
    const qlonglong ulDuration = torrent->activeTime();
    const int downloadLimit = torrent->downloadLimit();
    const int uploadLimit = torrent->uploadLimit();
    const qreal ratio = torrent->realRatio();
    const qreal popularity = torrent->popularity();
    const bool hasMetadata = torrent->hasMetadata();
    const bool isPrivate = torrent->isPrivate();

    const QJsonObject ret
    {
        {KEY_TORRENT_INFOHASHV1, infoHash.v1().toString()},
        {KEY_TORRENT_INFOHASHV2, infoHash.v2().toString()},
        {KEY_TORRENT_NAME, torrent->name()},
        {KEY_TORRENT_ID, torrent->id().toString()},
        {KEY_PROP_TIME_ELAPSED, torrent->activeTime()},
        {KEY_PROP_SEEDING_TIME, torrent->finishedTime()},
        {KEY_PROP_ETA, torrent->eta()},
        {KEY_PROP_CONNECT_COUNT, torrent->connectionsCount()},
        {KEY_PROP_CONNECT_COUNT_LIMIT, torrent->connectionsLimit()},
        {KEY_PROP_DOWNLOADED, totalDownload},
        {KEY_PROP_DOWNLOADED_SESSION, torrent->totalPayloadDownload()},
        {KEY_PROP_UPLOADED, totalUpload},
        {KEY_PROP_UPLOADED_SESSION, torrent->totalPayloadUpload()},
        {KEY_PROP_DL_SPEED, torrent->downloadPayloadRate()},
        {KEY_PROP_DL_SPEED_AVG, ((dlDuration > 0) ? (totalDownload / dlDuration) : -1)},
        {KEY_PROP_UP_SPEED, torrent->uploadPayloadRate()},
        {KEY_PROP_UP_SPEED_AVG, ((ulDuration > 0) ? (totalUpload / ulDuration) : -1)},
        {KEY_PROP_DL_LIMIT, ((downloadLimit > 0) ? downloadLimit : -1)},
        {KEY_PROP_UP_LIMIT, ((uploadLimit > 0) ? uploadLimit : -1)},
        {KEY_PROP_WASTED, torrent->wastedSize()},
        {KEY_PROP_SEEDS, torrent->seedsCount()},
        {KEY_PROP_SEEDS_TOTAL, torrent->totalSeedsCount()},
        {KEY_PROP_PEERS, torrent->leechsCount()},
        {KEY_PROP_PEERS_TOTAL, torrent->totalLeechersCount()},
        {KEY_PROP_RATIO, ((ratio > BitTorrent::Torrent::MAX_RATIO) ? -1 : ratio)},
        {KEY_PROP_POPULARITY, ((popularity > BitTorrent::Torrent::MAX_RATIO) ? -1 : popularity)},
        {KEY_PROP_REANNOUNCE, torrent->nextAnnounce()},
        {KEY_PROP_TOTAL_SIZE, torrent->totalSize()},
        {KEY_PROP_PIECES_NUM, torrent->piecesCount()},
        {KEY_PROP_PIECE_SIZE, torrent->pieceLength()},
        {KEY_PROP_PIECES_HAVE, torrent->piecesHave()},
        {KEY_PROP_CREATED_BY, torrent->creator()},
        {KEY_PROP_IS_PRIVATE, torrent->isPrivate()}, // used for maintaining backward compatibility
        {KEY_PROP_PRIVATE, (hasMetadata ? isPrivate : QJsonValue())},
        {KEY_PROP_ADDITION_DATE, Utils::DateTime::toSecsSinceEpoch(torrent->addedTime())},
        {KEY_PROP_LAST_SEEN, Utils::DateTime::toSecsSinceEpoch(torrent->lastSeenComplete())},
        {KEY_PROP_COMPLETION_DATE, Utils::DateTime::toSecsSinceEpoch(torrent->completedTime())},
        {KEY_PROP_CREATION_DATE, Utils::DateTime::toSecsSinceEpoch(torrent->creationDate())},
        {KEY_PROP_SAVE_PATH, torrent->savePath().toString()},
        {KEY_PROP_DOWNLOAD_PATH, torrent->downloadPath().toString()},
        {KEY_PROP_COMMENT, torrent->comment()},
        {KEY_PROP_HAS_METADATA, torrent->hasMetadata()},
        {KEY_PROP_PROGRESS, torrent->progress()}
    };

    setResult(ret);
}

// Returns the trackers for a torrent in JSON format.
// The return value is a JSON-formatted list of dictionaries.
// The dictionary keys are:
//   - "url": Tracker URL
//   - "status": Tracker status
//   - "tier": Tracker tier
//   - "num_peers": Number of peers this torrent is currently connected to
//   - "num_seeds": Number of peers that have the whole file
//   - "num_leeches": Number of peers that are still downloading
//   - "num_downloaded": Tracker downloaded count
//   - "msg": Tracker message (last)
void TorrentsController::trackersAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray trackersList = getStickyTrackers(torrent);

    // merge QJsonArray
    for (const auto &tracker : asConst(getTrackers(torrent)))
        trackersList.append(tracker);

    setResult(trackersList);
}

// Returns the web seeds for a torrent in JSON format.
// The return value is a JSON-formatted list of dictionaries.
// The dictionary keys are:
//   - "url": Web seed URL
void TorrentsController::webseedsAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray webSeedList;
    for (const QUrl &webseed : asConst(torrent->urlSeeds()))
    {
        webSeedList.append(QJsonObject
        {
            {KEY_WEBSEED_URL, webseed.toString()}
        });
    }

    setResult(webSeedList);
}

void TorrentsController::addWebSeedsAction()
{
    requireParams({u"hash"_s, u"urls"_s});
    const QStringList paramUrls = params()[u"urls"_s].split(u'|', Qt::SkipEmptyParts);

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QList<QUrl> urls;
    urls.reserve(paramUrls.size());
    for (const QString &urlStr : paramUrls)
    {
        const auto result = validateWebSeedUrl(urlStr);
        if (!result)
            throw APIError(APIErrorType::BadParams, result.error());
        urls << result.value();
    }

    torrent->addUrlSeeds(urls);
}

void TorrentsController::editWebSeedAction()
{
    requireParams({u"hash"_s, u"origUrl"_s, u"newUrl"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const QString origUrlStr = params()[u"origUrl"_s];
    const QString newUrlStr = params()[u"newUrl"_s];

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const auto origUrlResult = validateWebSeedUrl(origUrlStr);
    if (!origUrlResult)
        throw APIError(APIErrorType::BadParams, origUrlResult.error());
    const QUrl origUrl = origUrlResult.value();

    const auto newUrlResult = validateWebSeedUrl(newUrlStr);
    if (!newUrlResult)
        throw APIError(APIErrorType::BadParams, newUrlResult.error());
    const QUrl newUrl = newUrlResult.value();

    if (newUrl != origUrl)
    {
        if (!torrent->urlSeeds().contains(origUrl))
            throw APIError(APIErrorType::Conflict, tr("\"%1\" is not an existing URL").arg(origUrl.toString()));

        torrent->removeUrlSeeds({origUrl});
        torrent->addUrlSeeds({newUrl});
    }
}

void TorrentsController::removeWebSeedsAction()
{
    requireParams({u"hash"_s, u"urls"_s});
    const QStringList paramUrls = params()[u"urls"_s].split(u'|', Qt::SkipEmptyParts);

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QList<QUrl> urls;
    urls.reserve(paramUrls.size());
    for (const QString &urlStr : paramUrls)
    {
        const auto result = validateWebSeedUrl(urlStr);
        if (!result)
            throw APIError(APIErrorType::BadParams, result.error());
        urls << result.value();
    }

    torrent->removeUrlSeeds(urls);
}

// Returns the files in a torrent in JSON format.
// The return value is a JSON-formatted list of dictionaries.
// The dictionary keys are:
//   - "index": File index
//   - "name": File name
//   - "size": File size
//   - "progress": File progress
//   - "priority": File priority
//   - "is_seed": Flag indicating if torrent is seeding/complete
//   - "piece_range": Piece index range, the first number is the starting piece index
//        and the second number is the ending piece index (inclusive)
void TorrentsController::filesAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const int filesCount = torrent->filesCount();
    QList<int> fileIndexes;
    const auto idxIt = params().constFind(u"indexes"_s);
    if (idxIt != params().cend())
    {
        const QStringList indexStrings = idxIt.value().split(u'|');
        fileIndexes.reserve(indexStrings.size());
        std::transform(indexStrings.cbegin(), indexStrings.cend(), std::back_inserter(fileIndexes)
                       , [&filesCount](const QString &indexString) -> int
        {
            bool ok = false;
            const int index = indexString.toInt(&ok);
            if (!ok || (index < 0))
                throw APIError(APIErrorType::Conflict, tr("\"%1\" is not a valid file index.").arg(indexString));
            if (index >= filesCount)
                throw APIError(APIErrorType::Conflict, tr("Index %1 is out of bounds.").arg(indexString));
            return index;
        });
    }
    else
    {
        fileIndexes.reserve(filesCount);
        for (int i = 0; i < filesCount; ++i)
            fileIndexes.append(i);
    }

    QJsonArray fileList;
    if (torrent->hasMetadata())
    {
        const QList<BitTorrent::DownloadPriority> priorities = torrent->filePriorities();
        const QList<qreal> fp = torrent->filesProgress();
        const QList<qreal> fileAvailability = torrent->availableFileFractions();
        const BitTorrent::TorrentInfo info = torrent->info();
        for (const int index : asConst(fileIndexes))
        {
            QJsonObject fileDict =
            {
                {KEY_FILE_INDEX, index},
                {KEY_FILE_PROGRESS, fp[index]},
                {KEY_FILE_PRIORITY, static_cast<int>(priorities[index])},
                {KEY_FILE_SIZE, torrent->fileSize(index)},
                {KEY_FILE_AVAILABILITY, fileAvailability[index]},
                // need to provide paths using a platform-independent separator format
                {KEY_FILE_NAME, torrent->filePath(index).data()}
            };

            const BitTorrent::TorrentInfo::PieceRange idx = info.filePieces(index);
            fileDict[KEY_FILE_PIECE_RANGE] = QJsonArray {idx.first(), idx.last()};

            if (index == 0)
                fileDict[KEY_FILE_IS_SEED] = torrent->isFinished();

            fileList.append(fileDict);
        }
    }

    setResult(fileList);
}

// Returns an array of hashes (of each pieces respectively) for a torrent in JSON format.
// The return value is a JSON-formatted array of strings (hex strings).
void TorrentsController::pieceHashesAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray pieceHashes;
    if (torrent->hasMetadata())
    {
        const QList<QByteArray> hashes = torrent->info().pieceHashes();
        for (const QByteArray &hash : hashes)
            pieceHashes.append(QString::fromLatin1(hash.toHex()));
    }

    setResult(pieceHashes);
}

// Returns an array of states (of each pieces respectively) for a torrent in JSON format.
// The return value is a JSON-formatted array of ints.
// 0: piece not downloaded
// 1: piece requested or downloading
// 2: piece already downloaded
void TorrentsController::pieceStatesAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray pieceStates;
    const QBitArray states = torrent->pieces();
    for (int i = 0; i < states.size(); ++i)
        pieceStates.append(static_cast<int>(states[i]) * 2);

    const QBitArray dlstates = torrent->downloadingPieces();
    for (int i = 0; i < states.size(); ++i)
    {
        if (dlstates[i])
            pieceStates[i] = 1;
    }

    setResult(pieceStates);
}

void TorrentsController::addAction()
{
    const QString urls = params()[u"urls"_s];

    const bool skipChecking = parseBool(params()[u"skip_checking"_s]).value_or(false);
    const bool seqDownload = parseBool(params()[u"sequentialDownload"_s]).value_or(false);
    const bool firstLastPiece = parseBool(params()[u"firstLastPiecePrio"_s]).value_or(false);
    const bool addForced = parseBool(params()[u"forced"_s]).value_or(false);
    const std::optional<bool> addToQueueTop = parseBool(params()[u"addToTopOfQueue"_s]);
    const std::optional<bool> addStopped = parseBool(params()[u"stopped"_s]);
    const QString savepath = params()[u"savepath"_s].trimmed();
    const QString downloadPath = params()[u"downloadPath"_s].trimmed();
    const std::optional<bool> useDownloadPath = parseBool(params()[u"useDownloadPath"_s]);
    const QString category = params()[u"category"_s];
    const QStringList tags = params()[u"tags"_s].split(u',', Qt::SkipEmptyParts);
    const QString torrentName = params()[u"rename"_s].trimmed();
    const int upLimit = parseInt(params()[u"upLimit"_s]).value_or(-1);
    const int dlLimit = parseInt(params()[u"dlLimit"_s]).value_or(-1);
    const double ratioLimit = parseDouble(params()[u"ratioLimit"_s]).value_or(BitTorrent::Torrent::USE_GLOBAL_RATIO);
    const int seedingTimeLimit = parseInt(params()[u"seedingTimeLimit"_s]).value_or(BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME);
    const int inactiveSeedingTimeLimit = parseInt(params()[u"inactiveSeedingTimeLimit"_s]).value_or(BitTorrent::Torrent::USE_GLOBAL_INACTIVE_SEEDING_TIME);
    const BitTorrent::ShareLimitAction shareLimitAction = Utils::String::toEnum(params()[u"shareLimitAction"_s], BitTorrent::ShareLimitAction::Default);
    const std::optional<bool> autoTMM = parseBool(params()[u"autoTMM"_s]);

    const QString stopConditionParam = params()[u"stopCondition"_s];
    const std::optional<BitTorrent::Torrent::StopCondition> stopCondition = (!stopConditionParam.isEmpty()
            ? Utils::String::toEnum(stopConditionParam, BitTorrent::Torrent::StopCondition::None)
            : std::optional<BitTorrent::Torrent::StopCondition> {});

    const QString contentLayoutParam = params()[u"contentLayout"_s];
    const std::optional<BitTorrent::TorrentContentLayout> contentLayout = (!contentLayoutParam.isEmpty()
            ? Utils::String::toEnum(contentLayoutParam, BitTorrent::TorrentContentLayout::Original)
            : std::optional<BitTorrent::TorrentContentLayout> {});

    const BitTorrent::AddTorrentParams addTorrentParams
    {
        // TODO: Check if destination actually exists
        .name = torrentName,
        .category = category,
        .tags = {tags.cbegin(), tags.cend()},
        .savePath = Path(savepath),
        .useDownloadPath = useDownloadPath,
        .downloadPath = Path(downloadPath),
        .sequential = seqDownload,
        .firstLastPiecePriority = firstLastPiece,
        .addForced = addForced,
        .addToQueueTop = addToQueueTop,
        .addStopped = addStopped,
        .stopCondition = stopCondition,
        .filePaths = {},
        .filePriorities = {},
        .skipChecking = skipChecking,
        .contentLayout = contentLayout,
        .useAutoTMM = autoTMM,
        .uploadLimit = upLimit,
        .downloadLimit = dlLimit,
        .seedingTimeLimit = seedingTimeLimit,
        .inactiveSeedingTimeLimit = inactiveSeedingTimeLimit,
        .ratioLimit = ratioLimit,
        .shareLimitAction = shareLimitAction,
        .sslParameters =
        {
            .certificate = QSslCertificate(params()[KEY_PROP_SSL_CERTIFICATE].toLatin1()),
            .privateKey = Utils::SSLKey::load(params()[KEY_PROP_SSL_PRIVATEKEY].toLatin1()),
            .dhParams = params()[KEY_PROP_SSL_DHPARAMS].toLatin1()
        }
    };

    bool partialSuccess = false;
    for (QString url : asConst(urls.split(u'\n')))
    {
        url = url.trimmed();
        if (!url.isEmpty())
        {
            partialSuccess |= app()->addTorrentManager()->addTorrent(url, addTorrentParams);
        }
    }

    const DataMap &torrents = data();
    for (auto it = torrents.constBegin(); it != torrents.constEnd(); ++it)
    {
        if (const auto loadResult = BitTorrent::TorrentDescriptor::load(it.value()))
        {
            partialSuccess |= BitTorrent::Session::instance()->addTorrent(loadResult.value(), addTorrentParams);
        }
        else
        {
            throw APIError(APIErrorType::BadData, tr("Error: '%1' is not a valid torrent file.").arg(it.key()));
        }
    }

    if (partialSuccess)
        setResult(u"Ok."_s);
    else
        setResult(u"Fails."_s);
}

void TorrentsController::addTrackersAction()
{
    requireParams({u"hash"_s, u"urls"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QList<BitTorrent::TrackerEntry> entries = BitTorrent::parseTrackerEntries(params()[u"urls"_s]);
    torrent->addTrackers(entries);
}

void TorrentsController::editTrackerAction()
{
    requireParams({u"hash"_s, u"origUrl"_s, u"newUrl"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const QString origUrl = params()[u"origUrl"_s];
    const QString newUrl = params()[u"newUrl"_s];

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QUrl origTrackerUrl {origUrl};
    const QUrl newTrackerUrl {newUrl};
    if (origTrackerUrl == newTrackerUrl)
        return;
    if (!newTrackerUrl.isValid())
        throw APIError(APIErrorType::BadParams, u"New tracker URL is invalid"_s);

    const QList<BitTorrent::TrackerEntryStatus> currentTrackers = torrent->trackers();
    QList<BitTorrent::TrackerEntry> entries;
    entries.reserve(currentTrackers.size());

    bool match = false;
    for (const BitTorrent::TrackerEntryStatus &tracker : currentTrackers)
    {
        const QUrl trackerUrl {tracker.url};

        if (trackerUrl == newTrackerUrl)
            throw APIError(APIErrorType::Conflict, u"New tracker URL already exists"_s);

        BitTorrent::TrackerEntry entry
        {
            .url = tracker.url,
            .tier = tracker.tier
        };

        if (trackerUrl == origTrackerUrl)
        {
            match = true;
            entry.url = newTrackerUrl.toString();
        }
        entries.append(entry);
    }
    if (!match)
        throw APIError(APIErrorType::Conflict, u"Tracker not found"_s);

    torrent->replaceTrackers(entries);

    if (!torrent->isStopped())
        torrent->forceReannounce();
}

void TorrentsController::removeTrackersAction()
{
    requireParams({u"hash"_s, u"urls"_s});

    const QString hashParam = params()[u"hash"_s];
    const QStringList urlsParam = params()[u"urls"_s].split(u'|', Qt::SkipEmptyParts);

    QStringList urls;
    urls.reserve(urlsParam.size());
    for (const QString &urlStr : urlsParam)
        urls << QUrl::fromPercentEncoding(urlStr.toLatin1());

    QList<BitTorrent::Torrent *> torrents;

    if (hashParam == u"*"_s)
    {
        // remove trackers from all torrents
        torrents = BitTorrent::Session::instance()->torrents();
    }
    else
    {
        // remove trackers from specified torrent
        const auto id = BitTorrent::TorrentID::fromString(hashParam);
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
        if (!torrent)
            throw APIError(APIErrorType::NotFound);

        torrents.append(torrent);
    }

    for (BitTorrent::Torrent *const torrent : asConst(torrents))
        torrent->removeTrackers(urls);
}

void TorrentsController::addPeersAction()
{
    requireParams({u"hashes"_s, u"peers"_s});

    const QStringList hashes = params()[u"hashes"_s].split(u'|');
    const QStringList peers = params()[u"peers"_s].split(u'|');

    QList<BitTorrent::PeerAddress> peerList;
    peerList.reserve(peers.size());
    for (const QString &peer : peers)
    {
        const BitTorrent::PeerAddress addr = BitTorrent::PeerAddress::parse(peer.trimmed());
        if (!addr.ip.isNull())
            peerList.append(addr);
    }

    if (peerList.isEmpty())
        throw APIError(APIErrorType::BadParams, u"No valid peers were specified"_s);

    QJsonObject results;

    applyToTorrents(hashes, [peers, peerList, &results](BitTorrent::Torrent *const torrent)
    {
        const int peersAdded = std::count_if(peerList.cbegin(), peerList.cend(), [torrent](const BitTorrent::PeerAddress &peer)
        {
            return torrent->connectPeer(peer);
        });

        results[torrent->id().toString()] = QJsonObject
        {
            {u"added"_s, peersAdded},
            {u"failed"_s, (peers.size() - peersAdded)}
        };
    });

    setResult(results);
}

void TorrentsController::stopAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes = params()[u"hashes"_s].split(u'|');
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->stop(); });
}

void TorrentsController::startAction()
{
    requireParams({u"hashes"_s});

    const QStringList idStrings = params()[u"hashes"_s].split(u'|');
    applyToTorrents(idStrings, [](BitTorrent::Torrent *const torrent) { torrent->start(); });
}

void TorrentsController::filePrioAction()
{
    requireParams({u"hash"_s, u"id"_s, u"priority"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    bool ok = false;
    const auto priority = static_cast<BitTorrent::DownloadPriority>(params()[u"priority"_s].toInt(&ok));
    if (!ok)
        throw APIError(APIErrorType::BadParams, tr("Priority must be an integer"));

    if (!BitTorrent::isValidDownloadPriority(priority))
        throw APIError(APIErrorType::BadParams, tr("Priority is not valid"));

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);
    if (!torrent->hasMetadata())
        throw APIError(APIErrorType::Conflict, tr("Torrent's metadata has not yet downloaded"));

    const int filesCount = torrent->filesCount();
    QList<BitTorrent::DownloadPriority> priorities = torrent->filePriorities();
    bool priorityChanged = false;
    for (const QString &fileID : params()[u"id"_s].split(u'|'))
    {
        const int id = fileID.toInt(&ok);
        if (!ok)
            throw APIError(APIErrorType::BadParams, tr("File IDs must be integers"));
        if ((id < 0) || (id >= filesCount))
            throw APIError(APIErrorType::Conflict, tr("File ID is not valid"));

        if (priorities[id] != priority)
        {
            priorities[id] = priority;
            priorityChanged = true;
        }
    }

    if (priorityChanged)
        torrent->prioritizeFiles(priorities);
}

void TorrentsController::uploadLimitAction()
{
    requireParams({u"hashes"_s});

    const QStringList idList {params()[u"hashes"_s].split(u'|')};
    QJsonObject map;
    for (const QString &id : idList)
    {
        int limit = -1;
        const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(BitTorrent::TorrentID::fromString(id));
        if (torrent)
            limit = torrent->uploadLimit();
        map[id] = limit;
    }

    setResult(map);
}

void TorrentsController::downloadLimitAction()
{
    requireParams({u"hashes"_s});

    const QStringList idList {params()[u"hashes"_s].split(u'|')};
    QJsonObject map;
    for (const QString &id : idList)
    {
        int limit = -1;
        const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(BitTorrent::TorrentID::fromString(id));
        if (torrent)
            limit = torrent->downloadLimit();
        map[id] = limit;
    }

    setResult(map);
}

void TorrentsController::setUploadLimitAction()
{
    requireParams({u"hashes"_s, u"limit"_s});

    qlonglong limit = params()[u"limit"_s].toLongLong();
    if (limit == 0)
        limit = -1;

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [limit](BitTorrent::Torrent *const torrent) { torrent->setUploadLimit(limit); });
}

void TorrentsController::setDownloadLimitAction()
{
    requireParams({u"hashes"_s, u"limit"_s});

    qlonglong limit = params()[u"limit"_s].toLongLong();
    if (limit == 0)
        limit = -1;

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [limit](BitTorrent::Torrent *const torrent) { torrent->setDownloadLimit(limit); });
}

void TorrentsController::setShareLimitsAction()
{
    requireParams({u"hashes"_s, u"ratioLimit"_s, u"seedingTimeLimit"_s, u"inactiveSeedingTimeLimit"_s});

    const qreal ratioLimit = params()[u"ratioLimit"_s].toDouble();
    const qlonglong seedingTimeLimit = params()[u"seedingTimeLimit"_s].toLongLong();
    const qlonglong inactiveSeedingTimeLimit = params()[u"inactiveSeedingTimeLimit"_s].toLongLong();
    const QStringList hashes = params()[u"hashes"_s].split(u'|');

    applyToTorrents(hashes, [ratioLimit, seedingTimeLimit, inactiveSeedingTimeLimit](BitTorrent::Torrent *const torrent)
    {
        torrent->setRatioLimit(ratioLimit);
        torrent->setSeedingTimeLimit(seedingTimeLimit);
        torrent->setInactiveSeedingTimeLimit(inactiveSeedingTimeLimit);
    });
}

void TorrentsController::toggleSequentialDownloadAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->toggleSequentialDownload(); });
}

void TorrentsController::toggleFirstLastPiecePrioAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->toggleFirstLastPiecePriority(); });
}

void TorrentsController::setSuperSeedingAction()
{
    requireParams({u"hashes"_s, u"value"_s});

    const bool value {parseBool(params()[u"value"_s]).value_or(false)};
    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [value](BitTorrent::Torrent *const torrent) { torrent->setSuperSeeding(value); });
}

void TorrentsController::setForceStartAction()
{
    requireParams({u"hashes"_s, u"value"_s});

    const bool value {parseBool(params()[u"value"_s]).value_or(false)};
    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [value](BitTorrent::Torrent *const torrent)
    {
        torrent->start(value ? BitTorrent::TorrentOperatingMode::Forced : BitTorrent::TorrentOperatingMode::AutoManaged);
    });
}

void TorrentsController::deleteAction()
{
    requireParams({u"hashes"_s, u"deleteFiles"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const BitTorrent::TorrentRemoveOption deleteOption = parseBool(params()[u"deleteFiles"_s]).value_or(false)
            ? BitTorrent::TorrentRemoveOption::RemoveContent : BitTorrent::TorrentRemoveOption::KeepContent;
    applyToTorrents(hashes, [deleteOption](const BitTorrent::Torrent *torrent)
    {
        BitTorrent::Session::instance()->removeTorrent(torrent->id(), deleteOption);
    });
}

void TorrentsController::increasePrioAction()
{
    requireParams({u"hashes"_s});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    BitTorrent::Session::instance()->increaseTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::decreasePrioAction()
{
    requireParams({u"hashes"_s});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    BitTorrent::Session::instance()->decreaseTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::topPrioAction()
{
    requireParams({u"hashes"_s});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    BitTorrent::Session::instance()->topTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::bottomPrioAction()
{
    requireParams({u"hashes"_s});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    BitTorrent::Session::instance()->bottomTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::setLocationAction()
{
    requireParams({u"hashes"_s, u"location"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const Path newLocation {params()[u"location"_s].trimmed()};

    if (newLocation.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Save path cannot be empty"));

    // try to create the location if it does not exist
    if (!Utils::Fs::mkpath(newLocation))
        throw APIError(APIErrorType::Conflict, tr("Cannot make save path"));

    applyToTorrents(hashes, [newLocation](BitTorrent::Torrent *const torrent)
    {
        LogMsg(tr("WebUI Set location: moving \"%1\", from \"%2\" to \"%3\"")
            .arg(torrent->name(), torrent->savePath().toString(), newLocation.toString()));
        torrent->setAutoTMMEnabled(false);
        torrent->setSavePath(newLocation);
    });
}

void TorrentsController::setSavePathAction()
{
    requireParams({u"id"_s, u"path"_s});

    const QStringList ids {params()[u"id"_s].split(u'|')};
    const Path newPath {params()[u"path"_s]};

    if (newPath.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Save path cannot be empty"));

    // try to create the directory if it does not exist
    if (!Utils::Fs::mkpath(newPath))
        throw APIError(APIErrorType::Conflict, tr("Cannot create target directory"));

    // check permissions
    if (!Utils::Fs::isWritable(newPath))
        throw APIError(APIErrorType::AccessDenied, tr("Cannot write to directory"));

    applyToTorrents(ids, [&newPath](BitTorrent::Torrent *const torrent)
    {
        if (!torrent->isAutoTMMEnabled())
            torrent->setSavePath(newPath);
    });
}

void TorrentsController::setDownloadPathAction()
{
    requireParams({u"id"_s, u"path"_s});

    const QStringList ids {params()[u"id"_s].split(u'|')};
    const Path newPath {params()[u"path"_s]};

    if (!newPath.isEmpty())
    {
        // try to create the directory if it does not exist
        if (!Utils::Fs::mkpath(newPath))
            throw APIError(APIErrorType::Conflict, tr("Cannot create target directory"));

        // check permissions
        if (!Utils::Fs::isWritable(newPath))
            throw APIError(APIErrorType::AccessDenied, tr("Cannot write to directory"));
    }

    applyToTorrents(ids, [&newPath](BitTorrent::Torrent *const torrent)
    {
        if (!torrent->isAutoTMMEnabled())
            torrent->setDownloadPath(newPath);
    });
}

void TorrentsController::renameAction()
{
    requireParams({u"hash"_s, u"name"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    QString name = params()[u"name"_s].trimmed();

    if (name.isEmpty())
        throw APIError(APIErrorType::Conflict, tr("Incorrect torrent name"));

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    name.replace(QRegularExpression(u"\r?\n|\r"_s), u" "_s);
    torrent->setName(name);
}

void TorrentsController::setAutoManagementAction()
{
    requireParams({u"hashes"_s, u"enable"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const bool isEnabled {parseBool(params()[u"enable"_s]).value_or(false)};

    applyToTorrents(hashes, [isEnabled](BitTorrent::Torrent *const torrent)
    {
        torrent->setAutoTMMEnabled(isEnabled);
    });
}

void TorrentsController::recheckAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->forceRecheck(); });
}

void TorrentsController::reannounceAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->forceReannounce(); });
}

void TorrentsController::setCategoryAction()
{
    requireParams({u"hashes"_s, u"category"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const QString category {params()[u"category"_s]};

    applyToTorrents(hashes, [category](BitTorrent::Torrent *const torrent)
    {
        if (!torrent->setCategory(category))
            throw APIError(APIErrorType::Conflict, tr("Incorrect category name"));
    });
}

void TorrentsController::createCategoryAction()
{
    requireParams({u"category"_s});

    const QString category = params()[u"category"_s];
    if (category.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Category cannot be empty"));

    if (!BitTorrent::Session::isValidCategoryName(category))
        throw APIError(APIErrorType::Conflict, tr("Incorrect category name"));

    const Path savePath {params()[u"savePath"_s]};
    const auto useDownloadPath = parseBool(params()[u"downloadPathEnabled"_s]);
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = savePath;
    if (useDownloadPath.has_value())
    {
        const Path downloadPath {params()[u"downloadPath"_s]};
        categoryOptions.downloadPath = {useDownloadPath.value(), downloadPath};
    }

    if (!BitTorrent::Session::instance()->addCategory(category, categoryOptions))
        throw APIError(APIErrorType::Conflict, tr("Unable to create category"));
}

void TorrentsController::editCategoryAction()
{
    requireParams({u"category"_s, u"savePath"_s});

    const QString category = params()[u"category"_s];
    if (category.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Category cannot be empty"));

    const Path savePath {params()[u"savePath"_s]};
    const auto useDownloadPath = parseBool(params()[u"downloadPathEnabled"_s]);
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = savePath;
    if (useDownloadPath.has_value())
    {
        const Path downloadPath {params()[u"downloadPath"_s]};
        categoryOptions.downloadPath = {useDownloadPath.value(), downloadPath};
    }

    if (!BitTorrent::Session::instance()->editCategory(category, categoryOptions))
        throw APIError(APIErrorType::Conflict, tr("Unable to edit category"));
}

void TorrentsController::removeCategoriesAction()
{
    requireParams({u"categories"_s});

    const QStringList categories {params()[u"categories"_s].split(u'\n')};
    for (const QString &category : categories)
        BitTorrent::Session::instance()->removeCategory(category);
}

void TorrentsController::categoriesAction()
{
    const auto *session = BitTorrent::Session::instance();

    QJsonObject categories;
    const QStringList categoriesList = session->categories();
    for (const auto &categoryName : categoriesList)
    {
        const BitTorrent::CategoryOptions categoryOptions = session->categoryOptions(categoryName);
        QJsonObject category = categoryOptions.toJSON();
        // adjust it to be compatible with existing WebAPI
        category[u"savePath"_s] = category.take(u"save_path"_s);
        category.insert(u"name"_s, categoryName);
        categories[categoryName] = category;
    }

    setResult(categories);
}

void TorrentsController::addTagsAction()
{
    requireParams({u"hashes"_s, u"tags"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const QStringList tags {params()[u"tags"_s].split(u',', Qt::SkipEmptyParts)};

    for (const QString &tagStr : tags)
    {
        applyToTorrents(hashes, [&tagStr](BitTorrent::Torrent *const torrent)
        {
            torrent->addTag(Tag(tagStr));
        });
    }
}

void TorrentsController::setTagsAction()
{
    requireParams({u"hashes"_s, u"tags"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|', Qt::SkipEmptyParts)};
    const QStringList tags {params()[u"tags"_s].split(u',', Qt::SkipEmptyParts)};

    const TagSet newTags {tags.begin(), tags.end()};
    applyToTorrents(hashes, [&newTags](BitTorrent::Torrent *const torrent)
    {
        TagSet tmpTags {newTags};
        for (const Tag &tag : asConst(torrent->tags()))
        {
            if (tmpTags.erase(tag) == 0)
                torrent->removeTag(tag);
        }
        for (const Tag &tag : tmpTags)
            torrent->addTag(tag);
    });
}

void TorrentsController::removeTagsAction()
{
    requireParams({u"hashes"_s});

    const QStringList hashes {params()[u"hashes"_s].split(u'|')};
    const QStringList tags {params()[u"tags"_s].split(u',', Qt::SkipEmptyParts)};

    for (const QString &tagStr : tags)
    {
        applyToTorrents(hashes, [&tagStr](BitTorrent::Torrent *const torrent)
        {
            torrent->removeTag(Tag(tagStr));
        });
    }

    if (tags.isEmpty())
    {
        applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent)
        {
            torrent->removeAllTags();
        });
    }
}

void TorrentsController::createTagsAction()
{
    requireParams({u"tags"_s});

    const QStringList tags {params()[u"tags"_s].split(u',', Qt::SkipEmptyParts)};

    for (const QString &tagStr : tags)
        BitTorrent::Session::instance()->addTag(Tag(tagStr));
}

void TorrentsController::deleteTagsAction()
{
    requireParams({u"tags"_s});

    const QStringList tags {params()[u"tags"_s].split(u',', Qt::SkipEmptyParts)};
    for (const QString &tagStr : tags)
        BitTorrent::Session::instance()->removeTag(Tag(tagStr));
}

void TorrentsController::tagsAction()
{
    QJsonArray result;
    for (const Tag &tag : asConst(BitTorrent::Session::instance()->tags()))
        result << tag.toString();
    setResult(result);
}

void TorrentsController::renameFileAction()
{
    requireParams({u"hash"_s, u"oldPath"_s, u"newPath"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const Path oldPath {params()[u"oldPath"_s]};
    const Path newPath {params()[u"newPath"_s]};

    try
    {
        torrent->renameFile(oldPath, newPath);
    }
    catch (const RuntimeError &error)
    {
        throw APIError(APIErrorType::Conflict, error.message());
    }
}

void TorrentsController::renameFolderAction()
{
    requireParams({u"hash"_s, u"oldPath"_s, u"newPath"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const Path oldPath {params()[u"oldPath"_s]};
    const Path newPath {params()[u"newPath"_s]};

    try
    {
        torrent->renameFolder(oldPath, newPath);
    }
    catch (const RuntimeError &error)
    {
        throw APIError(APIErrorType::Conflict, error.message());
    }
}

void TorrentsController::exportAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const nonstd::expected<QByteArray, QString> result = torrent->exportToBuffer();
    if (!result)
        throw APIError(APIErrorType::Conflict, tr("Unable to export torrent file. Error: %1").arg(result.error()));

    setResult(result.value(), u"application/x-bittorrent"_s, (id.toString() + u".torrent"));
}

void TorrentsController::SSLParametersAction()
{
    requireParams({u"hash"_s});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const BitTorrent::SSLParameters sslParams = torrent->getSSLParameters();
    const QJsonObject ret
    {
        {KEY_PROP_SSL_CERTIFICATE, QString::fromLatin1(sslParams.certificate.toPem())},
        {KEY_PROP_SSL_PRIVATEKEY, QString::fromLatin1(sslParams.privateKey.toPem())},
        {KEY_PROP_SSL_DHPARAMS, QString::fromLatin1(sslParams.dhParams)}
    };
    setResult(ret);
}

void TorrentsController::setSSLParametersAction()
{
    requireParams({u"hash"_s, KEY_PROP_SSL_CERTIFICATE, KEY_PROP_SSL_PRIVATEKEY, KEY_PROP_SSL_DHPARAMS});

    const auto id = BitTorrent::TorrentID::fromString(params()[u"hash"_s]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const BitTorrent::SSLParameters sslParams
    {
        .certificate = QSslCertificate(params()[KEY_PROP_SSL_CERTIFICATE].toLatin1()),
        .privateKey = Utils::SSLKey::load(params()[KEY_PROP_SSL_PRIVATEKEY].toLatin1()),
        .dhParams = params()[KEY_PROP_SSL_DHPARAMS].toLatin1()
    };
    if (!sslParams.isValid())
        throw APIError(APIErrorType::BadData);

    torrent->setSSLParameters(sslParams);
}
