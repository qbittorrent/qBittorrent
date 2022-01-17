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

#include "torrentscontroller.h"

#include <functional>

#include <QBitArray>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QUrl>

#include "base/bittorrent/categoryoptions.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "apierror.h"
#include "serialize/serialize_torrent.h"

// Tracker keys
const char KEY_TRACKER_URL[] = "url";
const char KEY_TRACKER_STATUS[] = "status";
const char KEY_TRACKER_TIER[] = "tier";
const char KEY_TRACKER_MSG[] = "msg";
const char KEY_TRACKER_PEERS_COUNT[] = "num_peers";
const char KEY_TRACKER_SEEDS_COUNT[] = "num_seeds";
const char KEY_TRACKER_LEECHES_COUNT[] = "num_leeches";
const char KEY_TRACKER_DOWNLOADED_COUNT[] = "num_downloaded";

// Web seed keys
const char KEY_WEBSEED_URL[] = "url";

// Torrent keys (Properties)
const char KEY_PROP_TIME_ELAPSED[] = "time_elapsed";
const char KEY_PROP_SEEDING_TIME[] = "seeding_time";
const char KEY_PROP_ETA[] = "eta";
const char KEY_PROP_CONNECT_COUNT[] = "nb_connections";
const char KEY_PROP_CONNECT_COUNT_LIMIT[] = "nb_connections_limit";
const char KEY_PROP_DOWNLOADED[] = "total_downloaded";
const char KEY_PROP_DOWNLOADED_SESSION[] = "total_downloaded_session";
const char KEY_PROP_UPLOADED[] = "total_uploaded";
const char KEY_PROP_UPLOADED_SESSION[] = "total_uploaded_session";
const char KEY_PROP_DL_SPEED[] = "dl_speed";
const char KEY_PROP_DL_SPEED_AVG[] = "dl_speed_avg";
const char KEY_PROP_UP_SPEED[] = "up_speed";
const char KEY_PROP_UP_SPEED_AVG[] = "up_speed_avg";
const char KEY_PROP_DL_LIMIT[] = "dl_limit";
const char KEY_PROP_UP_LIMIT[] = "up_limit";
const char KEY_PROP_WASTED[] = "total_wasted";
const char KEY_PROP_SEEDS[] = "seeds";
const char KEY_PROP_SEEDS_TOTAL[] = "seeds_total";
const char KEY_PROP_PEERS[] = "peers";
const char KEY_PROP_PEERS_TOTAL[] = "peers_total";
const char KEY_PROP_RATIO[] = "share_ratio";
const char KEY_PROP_REANNOUNCE[] = "reannounce";
const char KEY_PROP_TOTAL_SIZE[] = "total_size";
const char KEY_PROP_PIECES_NUM[] = "pieces_num";
const char KEY_PROP_PIECE_SIZE[] = "piece_size";
const char KEY_PROP_PIECES_HAVE[] = "pieces_have";
const char KEY_PROP_CREATED_BY[] = "created_by";
const char KEY_PROP_LAST_SEEN[] = "last_seen";
const char KEY_PROP_ADDITION_DATE[] = "addition_date";
const char KEY_PROP_COMPLETION_DATE[] = "completion_date";
const char KEY_PROP_CREATION_DATE[] = "creation_date";
const char KEY_PROP_SAVE_PATH[] = "save_path";
const char KEY_PROP_DOWNLOAD_PATH[] = "download_path";
const char KEY_PROP_COMMENT[] = "comment";

// File keys
const char KEY_FILE_INDEX[] = "index";
const char KEY_FILE_NAME[] = "name";
const char KEY_FILE_SIZE[] = "size";
const char KEY_FILE_PROGRESS[] = "progress";
const char KEY_FILE_PRIORITY[] = "priority";
const char KEY_FILE_IS_SEED[] = "is_seed";
const char KEY_FILE_PIECE_RANGE[] = "piece_range";
const char KEY_FILE_AVAILABILITY[] = "availability";

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;
    using Utils::String::parseDouble;

    void applyToTorrents(const QStringList &idList, const std::function<void (BitTorrent::Torrent *torrent)> &func)
    {
        if ((idList.size() == 1) && (idList[0] == QLatin1String("all")))
        {
            for (BitTorrent::Torrent *const torrent : asConst(BitTorrent::Session::instance()->torrents()))
                func(torrent);
        }
        else
        {
            for (const QString &idString : idList)
            {
                const auto hash = BitTorrent::TorrentID::fromString(idString);
                BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
                if (torrent)
                    func(torrent);
            }
        }
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

        const int working = static_cast<int>(BitTorrent::TrackerEntry::Working);
        const int disabled = 0;

        const QString privateMsg {QCoreApplication::translate("TrackerListWidget", "This torrent is private")};
        const bool isTorrentPrivate = torrent->isPrivate();

        const QJsonObject dht
        {
            {KEY_TRACKER_URL, "** [DHT] **"},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : "")},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isDHTEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsDHT},
            {KEY_TRACKER_LEECHES_COUNT, leechesDHT}
        };

        const QJsonObject pex
        {
            {KEY_TRACKER_URL, "** [PeX] **"},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : "")},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isPeXEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsPeX},
            {KEY_TRACKER_LEECHES_COUNT, leechesPeX}
        };

        const QJsonObject lsd
        {
            {KEY_TRACKER_URL, "** [LSD] **"},
            {KEY_TRACKER_TIER, -1},
            {KEY_TRACKER_MSG, (isTorrentPrivate ? privateMsg : "")},
            {KEY_TRACKER_STATUS, ((BitTorrent::Session::instance()->isLSDEnabled() && !isTorrentPrivate) ? working : disabled)},
            {KEY_TRACKER_PEERS_COUNT, 0},
            {KEY_TRACKER_DOWNLOADED_COUNT, 0},
            {KEY_TRACKER_SEEDS_COUNT, seedsLSD},
            {KEY_TRACKER_LEECHES_COUNT, leechesLSD}
        };

        return {dht, pex, lsd};
    }

    QVector<BitTorrent::TorrentID> toTorrentIDs(const QStringList &idStrings)
    {
        QVector<BitTorrent::TorrentID> idList;
        idList.reserve(idStrings.size());
        for (const QString &hash : idStrings)
            idList << BitTorrent::TorrentID::fromString(hash);
        return idList;
    }
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
//   - filter (string): all, downloading, seeding, completed, paused, resumed, active, inactive, stalled, stalled_uploading, stalled_downloading
//   - category (string): torrent category for filtering by it (empty string means "uncategorized"; no "category" param presented means "any category")
//   - tag (string): torrent tag for filtering by it (empty string means "untagged"; no "tag" param presented means "any tag")
//   - hashes (string): filter by hashes, can contain multiple hashes separated by |
//   - sort (string): name of column for sorting by its value
//   - reverse (bool): enable reverse sorting
//   - limit (int): set limit number of torrents returned (if greater than 0, otherwise - unlimited)
//   - offset (int): set offset (if less than 0 - offset from end)
void TorrentsController::infoAction()
{
    const QString filter {params()["filter"]};
    const QString category {params()["category"]};
    const QString tag {params()["tag"]};
    const QString sortedColumn {params()["sort"]};
    const bool reverse {parseBool(params()["reverse"]).value_or(false)};
    int limit {params()["limit"].toInt()};
    int offset {params()["offset"].toInt()};
    const QStringList hashes {params()["hashes"].split('|', Qt::SkipEmptyParts)};

    TorrentIDSet idSet;
    for (const QString &hash : hashes)
        idSet.insert(BitTorrent::TorrentID::fromString(hash));

    const TorrentFilter torrentFilter(filter, (hashes.isEmpty() ? TorrentFilter::AnyID : idSet), category, tag);
    QVariantList torrentList;
    for (const BitTorrent::Torrent *torrent : asConst(BitTorrent::Session::instance()->torrents()))
    {
        if (torrentFilter.match(torrent))
            torrentList.append(serialize(*torrent));
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
            Q_ASSERT(left.type() == right.type());

            switch (static_cast<QMetaType::Type>(left.type()))
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
                qWarning("Unhandled QVariant comparison, type: %d, name: %s", left.type()
                    , QMetaType::typeName(left.type()));
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
    if ((offset >= size) || (offset < 0))
        offset = 0;
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
void TorrentsController::propertiesAction()
{
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonObject dataDict;

    dataDict[KEY_TORRENT_INFOHASHV1] = torrent->infoHash().v1().toString();
    dataDict[KEY_TORRENT_INFOHASHV2] = torrent->infoHash().v2().toString();
    dataDict[KEY_PROP_TIME_ELAPSED] = torrent->activeTime();
    dataDict[KEY_PROP_SEEDING_TIME] = torrent->finishedTime();
    dataDict[KEY_PROP_ETA] = static_cast<double>(torrent->eta());
    dataDict[KEY_PROP_CONNECT_COUNT] = torrent->connectionsCount();
    dataDict[KEY_PROP_CONNECT_COUNT_LIMIT] = torrent->connectionsLimit();
    dataDict[KEY_PROP_DOWNLOADED] = torrent->totalDownload();
    dataDict[KEY_PROP_DOWNLOADED_SESSION] = torrent->totalPayloadDownload();
    dataDict[KEY_PROP_UPLOADED] = torrent->totalUpload();
    dataDict[KEY_PROP_UPLOADED_SESSION] = torrent->totalPayloadUpload();
    dataDict[KEY_PROP_DL_SPEED] = torrent->downloadPayloadRate();
    const qlonglong dlDuration = torrent->activeTime() - torrent->finishedTime();
    dataDict[KEY_PROP_DL_SPEED_AVG] = torrent->totalDownload() / ((dlDuration == 0) ? -1 : dlDuration);
    dataDict[KEY_PROP_UP_SPEED] = torrent->uploadPayloadRate();
    const qlonglong ulDuration = torrent->activeTime();
    dataDict[KEY_PROP_UP_SPEED_AVG] = torrent->totalUpload() / ((ulDuration == 0) ? -1 : ulDuration);
    dataDict[KEY_PROP_DL_LIMIT] = torrent->downloadLimit() <= 0 ? -1 : torrent->downloadLimit();
    dataDict[KEY_PROP_UP_LIMIT] = torrent->uploadLimit() <= 0 ? -1 : torrent->uploadLimit();
    dataDict[KEY_PROP_WASTED] = torrent->wastedSize();
    dataDict[KEY_PROP_SEEDS] = torrent->seedsCount();
    dataDict[KEY_PROP_SEEDS_TOTAL] = torrent->totalSeedsCount();
    dataDict[KEY_PROP_PEERS] = torrent->leechsCount();
    dataDict[KEY_PROP_PEERS_TOTAL] = torrent->totalLeechersCount();
    const qreal ratio = torrent->realRatio();
    dataDict[KEY_PROP_RATIO] = ratio > BitTorrent::Torrent::MAX_RATIO ? -1 : ratio;
    dataDict[KEY_PROP_REANNOUNCE] = torrent->nextAnnounce();
    dataDict[KEY_PROP_TOTAL_SIZE] = torrent->totalSize();
    dataDict[KEY_PROP_PIECES_NUM] = torrent->piecesCount();
    dataDict[KEY_PROP_PIECE_SIZE] = torrent->pieceLength();
    dataDict[KEY_PROP_PIECES_HAVE] = torrent->piecesHave();
    dataDict[KEY_PROP_CREATED_BY] = torrent->creator();
    dataDict[KEY_PROP_ADDITION_DATE] = static_cast<double>(torrent->addedTime().toSecsSinceEpoch());
    if (torrent->hasMetadata())
    {
        dataDict[KEY_PROP_LAST_SEEN] = torrent->lastSeenComplete().isValid() ? torrent->lastSeenComplete().toSecsSinceEpoch() : -1;
        dataDict[KEY_PROP_COMPLETION_DATE] = torrent->completedTime().isValid() ? torrent->completedTime().toSecsSinceEpoch() : -1;
        dataDict[KEY_PROP_CREATION_DATE] = static_cast<double>(torrent->creationDate().toSecsSinceEpoch());
    }
    else
    {
        dataDict[KEY_PROP_LAST_SEEN] = -1;
        dataDict[KEY_PROP_COMPLETION_DATE] = -1;
        dataDict[KEY_PROP_CREATION_DATE] = -1;
    }
    dataDict[KEY_PROP_SAVE_PATH] = Utils::Fs::toNativePath(torrent->savePath());
    dataDict[KEY_PROP_DOWNLOAD_PATH] = Utils::Fs::toNativePath(torrent->downloadPath());
    dataDict[KEY_PROP_COMMENT] = torrent->comment();

    setResult(dataDict);
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
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray trackerList = getStickyTrackers(torrent);

    for (const BitTorrent::TrackerEntry &tracker : asConst(torrent->trackers()))
    {
        trackerList << QJsonObject
        {
            {KEY_TRACKER_URL, tracker.url},
            {KEY_TRACKER_TIER, tracker.tier},
            {KEY_TRACKER_STATUS, static_cast<int>(tracker.status)},
            {KEY_TRACKER_MSG, tracker.message},
            {KEY_TRACKER_PEERS_COUNT, tracker.numPeers},
            {KEY_TRACKER_SEEDS_COUNT, tracker.numSeeds},
            {KEY_TRACKER_LEECHES_COUNT, tracker.numLeeches},
            {KEY_TRACKER_DOWNLOADED_COUNT, tracker.numDownloaded}
        };
    }

    setResult(trackerList);
}

// Returns the web seeds for a torrent in JSON format.
// The return value is a JSON-formatted list of dictionaries.
// The dictionary keys are:
//   - "url": Web seed URL
void TorrentsController::webseedsAction()
{
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
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
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const int filesCount = torrent->filesCount();
    QVector<int> fileIndexes;
    const auto idxIt = params().constFind(QLatin1String("indexes"));
    if (idxIt != params().cend())
    {
        const QStringList indexStrings = idxIt.value().split('|');
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
        const QVector<BitTorrent::DownloadPriority> priorities = torrent->filePriorities();
        const QVector<qreal> fp = torrent->filesProgress();
        const QVector<qreal> fileAvailability = torrent->availableFileFractions();
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
                {KEY_FILE_NAME, Utils::Fs::toUniformPath(torrent->filePath(index))}
            };

            const BitTorrent::TorrentInfo::PieceRange idx = info.filePieces(index);
            fileDict[KEY_FILE_PIECE_RANGE] = QJsonArray {idx.first(), idx.last()};

            if (index == 0)
                fileDict[KEY_FILE_IS_SEED] = torrent->isSeed();

            fileList.append(fileDict);
        }
    }

    setResult(fileList);
}

// Returns an array of hashes (of each pieces respectively) for a torrent in JSON format.
// The return value is a JSON-formatted array of strings (hex strings).
void TorrentsController::pieceHashesAction()
{
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QJsonArray pieceHashes;
    if (torrent->hasMetadata())
    {
        const QVector<QByteArray> hashes = torrent->info().pieceHashes();
        for (const QByteArray &hash : hashes)
            pieceHashes.append(QString(hash.toHex()));
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
    requireParams({"hash"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
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
    const QString urls = params()["urls"];
    const QString cookie = params()["cookie"];

    const bool skipChecking = parseBool(params()["skip_checking"]).value_or(false);
    const bool seqDownload = parseBool(params()["sequentialDownload"]).value_or(false);
    const bool firstLastPiece = parseBool(params()["firstLastPiecePrio"]).value_or(false);
    const std::optional<bool> addPaused = parseBool(params()["paused"]);
    const QString savepath = params()["savepath"].trimmed();
    const QString downloadPath = params()["downloadPath"].trimmed();
    const std::optional<bool> useDownloadPath = parseBool(params()["useDownloadPath"]);
    const QString category = params()["category"];
    const QStringList tags = params()["tags"].split(',', Qt::SkipEmptyParts);
    const QString torrentName = params()["rename"].trimmed();
    const int upLimit = parseInt(params()["upLimit"]).value_or(-1);
    const int dlLimit = parseInt(params()["dlLimit"]).value_or(-1);
    const double ratioLimit = parseDouble(params()["ratioLimit"]).value_or(BitTorrent::Torrent::USE_GLOBAL_RATIO);
    const int seedingTimeLimit = parseInt(params()["seedingTimeLimit"]).value_or(BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME);
    const std::optional<bool> autoTMM = parseBool(params()["autoTMM"]);

    const QString contentLayoutParam = params()["contentLayout"];
    const std::optional<BitTorrent::TorrentContentLayout> contentLayout = (!contentLayoutParam.isEmpty()
            ? Utils::String::toEnum(contentLayoutParam, BitTorrent::TorrentContentLayout::Original)
            : std::optional<BitTorrent::TorrentContentLayout> {});

    QList<QNetworkCookie> cookies;
    if (!cookie.isEmpty())
    {
        const QStringList cookiesStr = cookie.split("; ");
        for (QString cookieStr : cookiesStr)
        {
            cookieStr = cookieStr.trimmed();
            int index = cookieStr.indexOf('=');
            if (index > 1)
            {
                QByteArray name = cookieStr.left(index).toLatin1();
                QByteArray value = cookieStr.right(cookieStr.length() - index - 1).toLatin1();
                cookies += QNetworkCookie(name, value);
            }
        }
    }

    BitTorrent::AddTorrentParams addTorrentParams;
    // TODO: Check if destination actually exists
    addTorrentParams.skipChecking = skipChecking;
    addTorrentParams.sequential = seqDownload;
    addTorrentParams.firstLastPiecePriority = firstLastPiece;
    addTorrentParams.addPaused = addPaused;
    addTorrentParams.contentLayout = contentLayout;
    addTorrentParams.savePath = savepath;
    addTorrentParams.downloadPath = downloadPath;
    addTorrentParams.useDownloadPath = useDownloadPath;
    addTorrentParams.category = category;
    addTorrentParams.tags.insert(tags.cbegin(), tags.cend());
    addTorrentParams.name = torrentName;
    addTorrentParams.uploadLimit = upLimit;
    addTorrentParams.downloadLimit = dlLimit;
    addTorrentParams.seedingTimeLimit = seedingTimeLimit;
    addTorrentParams.ratioLimit = ratioLimit;
    addTorrentParams.useAutoTMM = autoTMM;

    bool partialSuccess = false;
    for (QString url : asConst(urls.split('\n')))
    {
        url = url.trimmed();
        if (!url.isEmpty())
        {
            Net::DownloadManager::instance()->setCookiesFromUrl(cookies, QUrl::fromEncoded(url.toUtf8()));
            partialSuccess |= BitTorrent::Session::instance()->addTorrent(url, addTorrentParams);
        }
    }

    for (auto it = data().constBegin(); it != data().constEnd(); ++it)
    {
        const nonstd::expected<BitTorrent::TorrentInfo, QString> result = BitTorrent::TorrentInfo::load(it.value());
        if (!result)
        {
            throw APIError(APIErrorType::BadData
                           , tr("Error: '%1' is not a valid torrent file.").arg(it.key()));
        }

        partialSuccess |= BitTorrent::Session::instance()->addTorrent(result.value(), addTorrentParams);
    }

    if (partialSuccess)
        setResult(QLatin1String("Ok."));
    else
        setResult(QLatin1String("Fails."));
}

void TorrentsController::addTrackersAction()
{
    requireParams({"hash", "urls"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    QVector<BitTorrent::TrackerEntry> trackers;
    for (const QString &urlStr : asConst(params()["urls"].split('\n')))
    {
        const QUrl url {urlStr.trimmed()};
        if (url.isValid())
            trackers.append({url.toString()});
    }
    torrent->addTrackers(trackers);
}

void TorrentsController::editTrackerAction()
{
    requireParams({"hash", "origUrl", "newUrl"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    const QString origUrl = params()["origUrl"];
    const QString newUrl = params()["newUrl"];

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QUrl origTrackerUrl(origUrl);
    const QUrl newTrackerUrl(newUrl);
    if (origTrackerUrl == newTrackerUrl)
        return;
    if (!newTrackerUrl.isValid())
        throw APIError(APIErrorType::BadParams, "New tracker URL is invalid");

    QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    bool match = false;
    for (BitTorrent::TrackerEntry &tracker : trackers)
    {
        const QUrl trackerUrl(tracker.url);
        if (trackerUrl == newTrackerUrl)
            throw APIError(APIErrorType::Conflict, "New tracker URL already exists");
        if (trackerUrl == origTrackerUrl)
        {
            match = true;
            tracker.url = newTrackerUrl.toString();
        }
    }
    if (!match)
        throw APIError(APIErrorType::Conflict, "Tracker not found");

    torrent->replaceTrackers(trackers);

    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TorrentsController::removeTrackersAction()
{
    requireParams({"hash", "urls"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QStringList urls = params()["urls"].split('|');

    const QVector<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    QVector<BitTorrent::TrackerEntry> remainingTrackers;
    remainingTrackers.reserve(trackers.size());
    for (const BitTorrent::TrackerEntry &entry : trackers)
    {
        if (!urls.contains(entry.url))
            remainingTrackers.push_back(entry);
    }

    if (remainingTrackers.size() == trackers.size())
        throw APIError(APIErrorType::Conflict, "No trackers were removed");

    torrent->replaceTrackers(remainingTrackers);

    if (!torrent->isPaused())
        torrent->forceReannounce();
}

void TorrentsController::addPeersAction()
{
    requireParams({"hashes", "peers"});

    const QStringList hashes = params()["hashes"].split('|');
    const QStringList peers = params()["peers"].split('|');

    QVector<BitTorrent::PeerAddress> peerList;
    peerList.reserve(peers.size());
    for (const QString &peer : peers)
    {
        const BitTorrent::PeerAddress addr = BitTorrent::PeerAddress::parse(peer.trimmed());
        if (!addr.ip.isNull())
            peerList.append(addr);
    }

    if (peerList.isEmpty())
        throw APIError(APIErrorType::BadParams, "No valid peers were specified");

    QJsonObject results;

    applyToTorrents(hashes, [peers, peerList, &results](BitTorrent::Torrent *const torrent)
    {
        const int peersAdded = std::count_if(peerList.cbegin(), peerList.cend(), [torrent](const BitTorrent::PeerAddress &peer)
        {
            return torrent->connectPeer(peer);
        });

        results[torrent->id().toString()] = QJsonObject
        {
            {"added", peersAdded},
            {"failed", (peers.size() - peersAdded)}
        };
    });

    setResult(results);
}

void TorrentsController::pauseAction()
{
    requireParams({"hashes"});

    const QStringList hashes = params()["hashes"].split('|');
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->pause(); });
}

void TorrentsController::resumeAction()
{
    requireParams({"hashes"});

    const QStringList idStrings = params()["hashes"].split('|');
    applyToTorrents(idStrings, [](BitTorrent::Torrent *const torrent) { torrent->resume(); });
}

void TorrentsController::filePrioAction()
{
    requireParams({"hash", "id", "priority"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    bool ok = false;
    const auto priority = static_cast<BitTorrent::DownloadPriority>(params()["priority"].toInt(&ok));
    if (!ok)
        throw APIError(APIErrorType::BadParams, tr("Priority must be an integer"));

    if (!BitTorrent::isValidDownloadPriority(priority))
        throw APIError(APIErrorType::BadParams, tr("Priority is not valid"));

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);
    if (!torrent->hasMetadata())
        throw APIError(APIErrorType::Conflict, tr("Torrent's metadata has not yet downloaded"));

    const int filesCount = torrent->filesCount();
    QVector<BitTorrent::DownloadPriority> priorities = torrent->filePriorities();
    bool priorityChanged = false;
    for (const QString &fileID : params()["id"].split('|'))
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
    requireParams({"hashes"});

    const QStringList idList {params()["hashes"].split('|')};
    QJsonObject map;
    for (const QString &id : idList)
    {
        int limit = -1;
        const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(BitTorrent::TorrentID::fromString(id));
        if (torrent)
            limit = torrent->uploadLimit();
        map[id] = limit;
    }

    setResult(map);
}

void TorrentsController::downloadLimitAction()
{
    requireParams({"hashes"});

    const QStringList idList {params()["hashes"].split('|')};
    QJsonObject map;
    for (const QString &id : idList)
    {
        int limit = -1;
        const BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(BitTorrent::TorrentID::fromString(id));
        if (torrent)
            limit = torrent->downloadLimit();
        map[id] = limit;
    }

    setResult(map);
}

void TorrentsController::setUploadLimitAction()
{
    requireParams({"hashes", "limit"});

    qlonglong limit = params()["limit"].toLongLong();
    if (limit == 0)
        limit = -1;

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [limit](BitTorrent::Torrent *const torrent) { torrent->setUploadLimit(limit); });
}

void TorrentsController::setDownloadLimitAction()
{
    requireParams({"hashes", "limit"});

    qlonglong limit = params()["limit"].toLongLong();
    if (limit == 0)
        limit = -1;

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [limit](BitTorrent::Torrent *const torrent) { torrent->setDownloadLimit(limit); });
}

void TorrentsController::setShareLimitsAction()
{
    requireParams({"hashes", "ratioLimit", "seedingTimeLimit"});

    const qreal ratioLimit = params()["ratioLimit"].toDouble();
    const qlonglong seedingTimeLimit = params()["seedingTimeLimit"].toLongLong();
    const QStringList hashes = params()["hashes"].split('|');

    applyToTorrents(hashes, [ratioLimit, seedingTimeLimit](BitTorrent::Torrent *const torrent)
    {
        torrent->setRatioLimit(ratioLimit);
        torrent->setSeedingTimeLimit(seedingTimeLimit);
    });
}

void TorrentsController::toggleSequentialDownloadAction()
{
    requireParams({"hashes"});

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->toggleSequentialDownload(); });
}

void TorrentsController::toggleFirstLastPiecePrioAction()
{
    requireParams({"hashes"});

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->toggleFirstLastPiecePriority(); });
}

void TorrentsController::setSuperSeedingAction()
{
    requireParams({"hashes", "value"});

    const bool value {parseBool(params()["value"]).value_or(false)};
    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [value](BitTorrent::Torrent *const torrent) { torrent->setSuperSeeding(value); });
}

void TorrentsController::setForceStartAction()
{
    requireParams({"hashes", "value"});

    const bool value {parseBool(params()["value"]).value_or(false)};
    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [value](BitTorrent::Torrent *const torrent)
    {
        torrent->resume(value ? BitTorrent::TorrentOperatingMode::Forced : BitTorrent::TorrentOperatingMode::AutoManaged);
    });
}

void TorrentsController::deleteAction()
{
    requireParams({"hashes", "deleteFiles"});

    const QStringList hashes {params()["hashes"].split('|')};
    const DeleteOption deleteOption = parseBool(params()["deleteFiles"]).value_or(false)
            ? DeleteTorrentAndFiles : DeleteTorrent;
    applyToTorrents(hashes, [deleteOption](const BitTorrent::Torrent *torrent)
    {
        BitTorrent::Session::instance()->deleteTorrent(torrent->id(), deleteOption);
    });
}

void TorrentsController::increasePrioAction()
{
    requireParams({"hashes"});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()["hashes"].split('|')};
    BitTorrent::Session::instance()->increaseTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::decreasePrioAction()
{
    requireParams({"hashes"});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()["hashes"].split('|')};
    BitTorrent::Session::instance()->decreaseTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::topPrioAction()
{
    requireParams({"hashes"});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()["hashes"].split('|')};
    BitTorrent::Session::instance()->topTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::bottomPrioAction()
{
    requireParams({"hashes"});

    if (!BitTorrent::Session::instance()->isQueueingSystemEnabled())
        throw APIError(APIErrorType::Conflict, tr("Torrent queueing must be enabled"));

    const QStringList hashes {params()["hashes"].split('|')};
    BitTorrent::Session::instance()->bottomTorrentsQueuePos(toTorrentIDs(hashes));
}

void TorrentsController::setLocationAction()
{
    requireParams({"hashes", "location"});

    const QStringList hashes {params()["hashes"].split('|')};
    const QString newLocation {params()["location"].trimmed()};

    if (newLocation.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Save path cannot be empty"));

    // try to create the location if it does not exist
    if (!QDir(newLocation).mkpath("."))
        throw APIError(APIErrorType::Conflict, tr("Cannot make save path"));

    // check permissions
    if (!QFileInfo(newLocation).isWritable())
        throw APIError(APIErrorType::AccessDenied, tr("Cannot write to directory"));

    applyToTorrents(hashes, [newLocation](BitTorrent::Torrent *const torrent)
    {
        LogMsg(tr("WebUI Set location: moving \"%1\", from \"%2\" to \"%3\"")
            .arg(torrent->name(), Utils::Fs::toNativePath(torrent->savePath()), Utils::Fs::toNativePath(newLocation)));
        torrent->setAutoTMMEnabled(false);
        torrent->setSavePath(Utils::Fs::expandPathAbs(newLocation));
    });
}

void TorrentsController::setSavePathAction()
{
    requireParams({"id", "path"});

    const QStringList ids {params()["id"].split('|')};
    const QString newPath {params()["path"]};

    if (newPath.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Save path cannot be empty"));

    // try to create the directory if it does not exist
    if (!QDir(newPath).mkpath("."))
        throw APIError(APIErrorType::Conflict, tr("Cannot create target directory"));

    // check permissions
    if (!QFileInfo(newPath).isWritable())
        throw APIError(APIErrorType::AccessDenied, tr("Cannot write to directory"));

    applyToTorrents(ids, [&newPath](BitTorrent::Torrent *const torrent)
    {
        if (!torrent->isAutoTMMEnabled())
            torrent->setSavePath(newPath);
    });
}

void TorrentsController::setDownloadPathAction()
{
    requireParams({"id", "path"});

    const QStringList ids {params()["id"].split('|')};
    const QString newPath {params()["path"]};

    if (!newPath.isEmpty())
    {
        // try to create the directory if it does not exist
        if (!QDir(newPath).mkpath("."))
            throw APIError(APIErrorType::Conflict, tr("Cannot create target directory"));

        // check permissions
        if (!QFileInfo(newPath).isWritable())
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
    requireParams({"hash", "name"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    QString name = params()["name"].trimmed();

    if (name.isEmpty())
        throw APIError(APIErrorType::Conflict, tr("Incorrect torrent name"));

    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    name.replace(QRegularExpression("\r?\n|\r"), " ");
    torrent->setName(name);
}

void TorrentsController::setAutoManagementAction()
{
    requireParams({"hashes", "enable"});

    const QStringList hashes {params()["hashes"].split('|')};
    const bool isEnabled {parseBool(params()["enable"]).value_or(false)};

    applyToTorrents(hashes, [isEnabled](BitTorrent::Torrent *const torrent)
    {
        torrent->setAutoTMMEnabled(isEnabled);
    });
}

void TorrentsController::recheckAction()
{
    requireParams({"hashes"});

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->forceRecheck(); });
}

void TorrentsController::reannounceAction()
{
    requireParams({"hashes"});

    const QStringList hashes {params()["hashes"].split('|')};
    applyToTorrents(hashes, [](BitTorrent::Torrent *const torrent) { torrent->forceReannounce(); });
}

void TorrentsController::setCategoryAction()
{
    requireParams({"hashes", "category"});

    const QStringList hashes {params()["hashes"].split('|')};
    const QString category {params()["category"]};

    applyToTorrents(hashes, [category](BitTorrent::Torrent *const torrent)
    {
        if (!torrent->setCategory(category))
            throw APIError(APIErrorType::Conflict, tr("Incorrect category name"));
    });
}

void TorrentsController::createCategoryAction()
{
    requireParams({"category"});

    const QString category = params()["category"];
    if (category.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Category cannot be empty"));

    if (!BitTorrent::Session::isValidCategoryName(category))
        throw APIError(APIErrorType::Conflict, tr("Incorrect category name"));

    const QString savePath = params()["savePath"];
    const auto useDownloadPath = parseBool(params()["downloadPathEnabled"]);
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = savePath;
    if (useDownloadPath.has_value())
    {
        const QString downloadPath = params()["downloadPath"];
        categoryOptions.downloadPath = {useDownloadPath.value(), downloadPath};
    }

    if (!BitTorrent::Session::instance()->addCategory(category, categoryOptions))
        throw APIError(APIErrorType::Conflict, tr("Unable to create category"));
}

void TorrentsController::editCategoryAction()
{
    requireParams({"category", "savePath"});

    const QString category = params()["category"];
    if (category.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("Category cannot be empty"));

    const QString savePath = params()["savePath"];
    const auto useDownloadPath = parseBool(params()["downloadPathEnabled"]);
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = savePath;
    if (useDownloadPath.has_value())
    {
        const QString downloadPath = params()["downloadPath"];
        categoryOptions.downloadPath = {useDownloadPath.value(), downloadPath};
    }

    if (!BitTorrent::Session::instance()->editCategory(category, categoryOptions))
        throw APIError(APIErrorType::Conflict, tr("Unable to edit category"));
}

void TorrentsController::removeCategoriesAction()
{
    requireParams({"categories"});

    const QStringList categories {params()["categories"].split('\n')};
    for (const QString &category : categories)
        BitTorrent::Session::instance()->removeCategory(category);
}

void TorrentsController::categoriesAction()
{
    const auto session = BitTorrent::Session::instance();

    QJsonObject categories;
    const QStringList categoriesList = session->categories();
    for (const auto &categoryName : categoriesList)
    {
        const BitTorrent::CategoryOptions categoryOptions = session->categoryOptions(categoryName);
        QJsonObject category = categoryOptions.toJSON();
        // adjust it to be compatible with exisitng WebAPI
        category[QLatin1String("savePath")] = category.take(QLatin1String("save_path"));
        category.insert(QLatin1String("name"), categoryName);
        categories[categoryName] = category;
    }

    setResult(categories);
}

void TorrentsController::addTagsAction()
{
    requireParams({"hashes", "tags"});

    const QStringList hashes {params()["hashes"].split('|')};
    const QStringList tags {params()["tags"].split(',', Qt::SkipEmptyParts)};

    for (const QString &tag : tags)
    {
        const QString tagTrimmed {tag.trimmed()};
        applyToTorrents(hashes, [&tagTrimmed](BitTorrent::Torrent *const torrent)
        {
            torrent->addTag(tagTrimmed);
        });
    }
}

void TorrentsController::removeTagsAction()
{
    requireParams({"hashes"});

    const QStringList hashes {params()["hashes"].split('|')};
    const QStringList tags {params()["tags"].split(',', Qt::SkipEmptyParts)};

    for (const QString &tag : tags)
    {
        const QString tagTrimmed {tag.trimmed()};
        applyToTorrents(hashes, [&tagTrimmed](BitTorrent::Torrent *const torrent)
        {
            torrent->removeTag(tagTrimmed);
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
    requireParams({"tags"});

    const QStringList tags {params()["tags"].split(',', Qt::SkipEmptyParts)};

    for (const QString &tag : tags)
        BitTorrent::Session::instance()->addTag(tag.trimmed());
}

void TorrentsController::deleteTagsAction()
{
    requireParams({"tags"});

    const QStringList tags {params()["tags"].split(',', Qt::SkipEmptyParts)};
    for (const QString &tag : tags)
        BitTorrent::Session::instance()->removeTag(tag.trimmed());
}

void TorrentsController::tagsAction()
{
    QJsonArray result;
    for (const QString &tag : asConst(BitTorrent::Session::instance()->tags()))
        result << tag;
    setResult(result);
}

void TorrentsController::renameFileAction()
{
    requireParams({"hash", "oldPath", "newPath"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QString oldPath = params()["oldPath"];
    const QString newPath = params()["newPath"];

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
    requireParams({"hash", "oldPath", "newPath"});

    const auto id = BitTorrent::TorrentID::fromString(params()["hash"]);
    BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->findTorrent(id);
    if (!torrent)
        throw APIError(APIErrorType::NotFound);

    const QString oldPath = params()["oldPath"];
    const QString newPath = params()["newPath"];

    try
    {
        torrent->renameFolder(oldPath, newPath);
    }
    catch (const RuntimeError &error)
    {
        throw APIError(APIErrorType::Conflict, error.message());
    }
}
