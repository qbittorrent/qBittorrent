#include "transmissionrpccontroller.h"

#include <filesystem>
#include <iterator>
#include <optional>

#include <QBitArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/net/portforwarder.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/torrentfileguard.h"
#include "base/utils/datetime.h"
#include "base/utils/misc.h"
#include "base/version.h"

#include "apierror.h"
#include "../webapplication.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    class TransmissionAPIError : public APIError
    {
    public:
        explicit TransmissionAPIError(APIErrorType type, const QString &message = {}) : APIError(type, message) {}
    };

    QJsonObject parseTopLevel(const QByteArray &payload)
    {
        QJsonParseError parseErr{};
        const QJsonDocument request = QJsonDocument::fromJson(payload, &parseErr);
        if (request.isNull())
            throw APIError(APIErrorType::BadData, parseErr.errorString());
        if (!request.isObject())
            throw APIError(APIErrorType::BadData, u"Top-level JSON should be an object"_s);

        return request.object();
    }

    QJsonObject units()
    {
        return {
            { u"speed-bytes"_s, QJsonArray{1, 1000, 1'000'000, 1'000'000'000, Q_INT64_C(1'000'000'000'000)}},
            { u"speed-units"_s, QJsonArray{u"B/s"_s, u"kB/s"_s, u"MB/s"_s, u"GB/s"_s, u"TB/s"_s}},
            { u"size-bytes"_s, QJsonArray{1, 1000, 1'000'000, 1'000'000'000, Q_INT64_C(1'000'000'000'000)}},
            { u"size-units"_s, QJsonArray{u"B"_s, u"kB"_s, u"MB"_s, u"GB"_s, u"TB"_s}},
            { u"memory-bytes"_s, QJsonArray{1, 1024, 1'048'576, 1'073'741'824, Q_INT64_C(1'099'511'627'776)}},
            { u"memory-units"_s, QJsonArray{u"B"_s, u"KiB"_s, u"MiB"_s, u"GiB"_s, u"TiB"_s}}
        };
    }

    int minutesAfterMidnight(const QTime &t)
    {
        return t.hour() * 60 + t.minute();
    }

    int transmissionSchedulerDays(Scheduler::Days days)
    {
        constexpr auto getBitmask = [](auto d) {
                                        constexpr auto dayOfWeekBitmask = std::array{1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 0};
                                        return dayOfWeekBitmask[static_cast<int>(d) - static_cast<int>(Scheduler::Days::Monday)];
                                    };
        constexpr auto weekday = getBitmask(Scheduler::Days::Monday)
                                 | getBitmask(Scheduler::Days::Tuesday)
                                 | getBitmask(Scheduler::Days::Wednesday)
                                 | getBitmask(Scheduler::Days::Thursday)
                                 | getBitmask(Scheduler::Days::Friday);
        constexpr auto weekend = getBitmask(Scheduler::Days::Saturday) | getBitmask(Scheduler::Days::Sunday);
        switch (days){
        case Scheduler::Days::EveryDay:
            return weekend | weekday;
        case Scheduler::Days::Weekday:
            return weekday;
        case Scheduler::Days::Weekend:
            return weekend;
        default:
            return getBitmask(days);
        }
    }

    enum class TorrentGetFormat
    {
        Object,
        Table
    };

    TorrentGetFormat torrentGetParseFormatArg(const QJsonValue &format)
    {
        auto onError = [](const QString &actual){
                           throw TransmissionAPIError(APIErrorType::BadParams,
                                                      u"'format' key in 'arguments' to torrent-get must be "
                                                      "either 'object' or 'table' if present, got '%1'"_s.arg(actual));
                       };
        TorrentGetFormat rv = TorrentGetFormat::Object; // default if not present
        if (format.isString()){
            const QString formatStr = format.toString();
            if (formatStr == u"object"_s)
                rv = TorrentGetFormat::Object;
            else if (formatStr == u"table"_s)
                rv = TorrentGetFormat::Table;
            else
                onError(formatStr);
        }
        else if (!format.isUndefined())
            onError(format.toVariant().toString());
        return rv;
    }

    QJsonArray torrentGetFiles(const BitTorrent::Torrent &tor)
    {
        QJsonArray result{};
        const BitTorrent::TorrentInfo info = tor.info();
        const int filesCount = info.filesCount();
        const QVector<qreal> filesProgress = tor.filesProgress();
        const PathList filePaths = tor.filePaths();
        for (int i = 0 ; i < filesCount ; ++i){
            // need to basically undo what fileProgress() does
            const qlonglong fileSize = info.fileSize(i);
            const qint64 byteProgress = fileSize * filesProgress[i];
            const BitTorrent::TorrentInfo::PieceRange pieces = info.filePieces(i);
            result.push_back(QJsonObject
            {
                { u"bytesCompleted"_s, byteProgress },
                { u"length"_s, fileSize },
                { u"name"_s, filePaths[i].toString() },
                { u"beginPiece"_s, pieces.first() },
                { u"endPiece"_s, pieces.last() + 1 }
            });
        }
        return result;
    }

    qint64 convertToTransmissionFilePriority(BitTorrent::DownloadPriority priority)
    {
        using dp = BitTorrent::DownloadPriority;
        switch (priority) {
        case dp::Normal:
            return 0; // TR_PRI_NORMAL
        case dp::High:
        case dp::Maximum:
            return 1; // TR_PRI_HIGH, doesn't map too nicely :(
        default:
            return -1; // TR_PRI_LOW as there's no other choice
        }
    }

    QJsonArray torrentGetFileStats(const BitTorrent::Torrent &tor)
    {
        QJsonArray result{};
        const BitTorrent::TorrentInfo info = tor.info();
        const int filesCount = info.filesCount();
        const QVector<qreal> filesProgress = tor.filesProgress();
        using dp = BitTorrent::DownloadPriority;
        const QVector<dp> filePriorities = tor.filePriorities();
        for (int i = 0 ; i < filesCount ; ++i){
            const qlonglong fileSize = info.fileSize(i);
            const qint64 byteProgress = fileSize * filesProgress[i];
            const dp priority = filePriorities[i];
            result.push_back(QJsonObject
            {
                { u"bytesCompleted"_s, byteProgress },
                { u"wanted"_s, priority != dp::Ignored },
                { u"priority"_s, convertToTransmissionFilePriority(priority) },
            });
        }
        return result;
    }

    QJsonObject torrentGetPeersFrom(const QVector<BitTorrent::PeerInfo> &peers)
    {
        unsigned int fromCache = 0;
        unsigned int fromDht = 0;
        unsigned int fromIncoming = 0;
        unsigned int fromLpd = 0;
        constexpr unsigned int fromLtep = 0;
        unsigned int fromPex = 0;
        unsigned int fromTracker = 0;
        for (const BitTorrent::PeerInfo &p : peers){
            if (p.fromDHT())
                ++fromDht;
            else if (p.fromPeX())
                ++fromPex;
            else if (p.fromLSD())
                ++fromLpd;

#if 0 // TODO perhaps expose these in qBt's PeerInfo
            else if (p.m_nativeInfo.source & lt::peer_info::tracker)
                ++fromTracker;
            else if (p.m_nativeInfo.source & lt::peer_info::resume_data)
                ++fromCache;
            else if (p.m_nativeInfo.source & lt::peer_info::incoming)
                ++fromIncoming;

#endif
        }
#define pair_from_var(v) { u ## #v ## _s, qint64{v} \
}

        return QJsonObject{
            pair_from_var(fromCache),
            pair_from_var(fromDht),
            pair_from_var(fromIncoming),
            pair_from_var(fromLpd),
            pair_from_var(fromLtep),
            pair_from_var(fromPex),
            pair_from_var(fromTracker)
        };
#undef pair_from_var
    }

    bool peerIsBeingUploadedTo(const BitTorrent::PeerInfo &peer)
    {
        return peer.isRemoteInterested() && !peer.isChocked();
    }

    bool peerIsBeingDownloadedFrom(const BitTorrent::PeerInfo &peer)
    {
        return peer.isInteresting() && !peer.isRemoteChocked();
    }

    QJsonArray torrentGetPeers(const QVector<BitTorrent::PeerInfo> &peers)
    {
        QJsonArray result{};
        for (const BitTorrent::PeerInfo &p : peers){
            const BitTorrent::PeerAddress addr = p.address();
            result.push_back(QJsonObject
            {
                {u"address"_s, addr.ip.toString()},
                {u"clientIsChoked"_s, p.isChocked()},
                {u"clientIsInterested"_s, p.isInteresting()},
                {u"clientName"_s, p.client()},
                {u"flagStr"_s, p.flags()},
                {u"isDownloadingFrom"_s, peerIsBeingDownloadedFrom(p)},
                {u"isEncrypted"_s, p.isRC4Encrypted()},
                {u"isIncoming"_s, !p.isLocalConnection()},
                {u"isUTP"_s, p.useUTPSocket()},
                {u"isUploadingTo"_s, peerIsBeingUploadedTo(p)},
                {u"peerIsChoked"_s, p.isRemoteChocked()},
                {u"peerIsInterested"_s, p.isRemoteInterested()},
                {u"port"_s, addr.port},
                {u"progress"_s, p.progress()},
                {u"rateToClient"_s, p.payloadDownSpeed()},
                {u"rateToPeer"_s, p.payloadUpSpeed()}
            });
        }
        return result;
    }

    int torrentGetStatus(const BitTorrent::Torrent &tor)
    {
        if (tor.isPaused()){
            return 0; // TR_STATUS_STOPPED
        }
        int status = 1;
        if (tor.isChecking()){
            status = 2; // TR_STATUS_CHECK
        }
        else if (tor.isDownloading()){
            status = 4; // TR_STATUS_DOWNLOAD
        }
        else if (tor.isUploading()){
            status = 6; // TR_STATUS_SEED
        }
        if (tor.isQueued()){
            status--; // gives us the appropriate _WAIT state or _STOPPED if nothing matched.
        }
        return status;
    }

    QJsonArray torrentGetTrackers(const BitTorrent::Torrent &tor)
    {
        QJsonArray result;
        const QVector<BitTorrent::TrackerEntry> trackers = tor.trackers();
        qint64 id = 0; // should be unique but eh
        for (const BitTorrent::TrackerEntry &t : trackers){
            result.push_back(QJsonObject{
                {u"announce"_s, t.url},
                {u"id"_s, id++},
                {u"scrape"_s, QString{}}, // FIXME not available?
                {u"sitename"_s, QString{}}, // FIXME perhaps duplicate what Transmission does
                {u"tier"_s, t.tier}
            });
        }
        return result;
    }

    QJsonArray torrentGetTrackerStats(const BitTorrent::Torrent &tor)
    {
        QJsonArray result;
        const QVector<BitTorrent::TrackerEntry> trackers = tor.trackers();
        qint64 id = 0; // should be unique but eh

        for (const BitTorrent::TrackerEntry &t : trackers){
            // FIXME there are mostly placeholders here
            result.push_back(QJsonObject{
                {u"announce"_s, t.url},
                {u"announceState"_s, 0},
                {u"downloadCount"_s, t.numDownloaded},
                {u"hasAnnounced"_s, false},
                {u"hasScraped"_s, false},
                {u"host"_s, t.url},
                {u"id"_s, id++},
                {u"isBackup"_s, false},
                {u"lastAnnouncePeerCount"_s, 0},
                {u"lastAnnounceResult"_s, 0},
                {u"lastAnnounceStartTime"_s, 0},
                {u"lastAnnounceSucceeded"_s, false},
                {u"lastAnnounceTime"_s, 0},
                {u"lastAnnounceTimedOut"_s, false},
                {u"lastScrapeResult"_s, 0},
                {u"lastScrapeStartTime"_s, 0},
                {u"lastScrapeSucceeded"_s, false},
                {u"lastScrapeTime"_s, 0},
                {u"lastScrapeTimedOut"_s, false},
                {u"leecherCount"_s, 0},
                {u"nextAnnounceTime"_s, 0},
                {u"nextScrapeTime"_s, 0},
                {u"scrape"_s, QString{}},
                {u"scrapeState"_s, 0},
                {u"seederCount"_s, 0},
                {u"sitename"_s, QString{}},
                {u"tier"_s, t.tier}
            });
        }
        return result;
    }

    struct TorrentGetFieldFuncs
    {
        struct GetArgs
        {
            const BitTorrent::Torrent &tor;
            const QVector<BitTorrent::PeerInfo> peers;
            int id;
        };
        using GetFunc = QJsonValue (*)(const GetArgs &);
        GetFunc get = nullptr;

        struct SetArgs
        {
            void *foobar; // placeholder
        };
        using SetFunc = void (*)(const SetArgs &);
        SetFunc set = nullptr;
    };

    const QHash<QString, TorrentGetFieldFuncs> fieldValueFuncHash = {
        {u"activityDate"_s, { .get = [](const auto &args) -> QJsonValue {
                                         const qlonglong timeSinceActivity = args.tor.timeSinceActivity();
                                         return (timeSinceActivity < 0)
                                                ? Utils::DateTime::toSecsSinceEpoch(args.tor.addedTime())
                                                : (QDateTime::currentDateTime().toSecsSinceEpoch() - timeSinceActivity);
                                     }, .set = nullptr}},
        {u"addedDate"_s, { .get = [](const auto &args) -> QJsonValue {
                                      return Utils::DateTime::toSecsSinceEpoch(args.tor.addedTime());
                                  }, .set = nullptr}},
        {u"availability"_s, { .get = [](const auto &args) -> QJsonValue {
                                         const QVector<int> peersPerPiece = args.tor.pieceAvailability();
                                         const QBitArray ourPieces = args.tor.pieces();
                                         Q_ASSERT(peersPerPiece.size() == ourPieces.size());
                                         QJsonArray result = {};
                                         for (qsizetype i = 0 ; i < ourPieces.size() ; ++i)
                                             result.push_back(ourPieces[i] ? -1 : peersPerPiece[i]);
                                         return result;
                                     }}},
        // qBt doesn't appear to have torrent priorities so just report everything as NORMAL
        {u"bandwidthPriority"_s, {.get = [](const auto &) -> QJsonValue {
                                             return 0;
                                         }, .set = nullptr}},
        {u"comment"_s, {.get = [](const auto &args) -> QJsonValue {
                                   return args.tor.comment();
                               }, .set = nullptr}},
        // not really "ever" - resets on pause in qBt, while Transmission saves this info to resume files
        {u"corruptEver"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return args.tor.wastedSize();
                                   }, .set = nullptr}},
        {u"creator"_s, {.get = [](const auto &args) -> QJsonValue {
                                   return args.tor.creator();
                               }, .set = nullptr}},
        {u"dateCreated"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return Utils::DateTime::toSecsSinceEpoch(args.tor.creationDate());
                                   }, .set = nullptr}},
        {u"desiredAvailable"_s, {.get = [](const auto &args) -> QJsonValue
                                        {
                                            const QVector<int> peersPerPiece = args.tor.pieceAvailability();
                                            const QBitArray ourPieces = args.tor.pieces();
                                            Q_ASSERT(peersPerPiece.size() == ourPieces.size());
                                            qint64 result = 0;
                                            const auto singlePieceSize = args.tor.pieceLength();
                                            for (qsizetype i = 0 ; i < ourPieces.size() ; ++i)
                                                if (!ourPieces[i] && (peersPerPiece[i] >= 1))
                                                    result += singlePieceSize;
                                            return result;
                                        }, .set = nullptr}},
        {u"doneDate"_s, {.get = [](const auto &args) -> QJsonValue {
                                    return Utils::DateTime::toSecsSinceEpoch(args.tor.completedTime());
                                }, .set = nullptr}},
        {u"downloadDir"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return args.tor.savePath().toString();
                                   }, .set = nullptr}},
        {u"downloadedEver"_s, {.get = [](const auto &args) -> QJsonValue {
                                          return args.tor.totalDownload();
                                      }, .set = nullptr}},
        {u"downloadLimit"_s, {.get = [](const auto &args) -> QJsonValue {
                                         return args.tor.downloadLimit();
                                     }, .set = nullptr} },
        {u"downloadLimited"_s, {.get = [](const auto &args) -> QJsonValue {
                                           return args.tor.downloadLimit() != 0;
                                       }, .set = nullptr}},
        // unsupported, so return Transmission's default value.
        {u"editDate"_s, {.get = [](const auto &) -> QJsonValue {
                                    return 0;
                                }, .set = nullptr}},
        {u"error"_s, {.get = [](const auto &args) -> QJsonValue {
                                 if ((args.tor.state() == BitTorrent::TorrentState::MissingFiles) ||
                                     (args.tor.state() == BitTorrent::TorrentState::Error) ){
                                     return 3; // TR_STAT_LOCAL_ERROR - "local trouble"
                                 }

                                 const QVector<BitTorrent::TrackerEntry> trackers = args.tor.trackers();
                                 const bool trackerHasError = std::any_of(trackers.cbegin(), trackers.cend(), [](const BitTorrent::TrackerEntry &entry){
                        return entry.status == BitTorrent::TrackerEntryStatus::TrackerError;
                    });
                                 return trackerHasError ? 2 /* TR_STAT_TRACKER_ERROR */ : 0 /* TR_STAT_OK */;
                             }, .set = nullptr}},
        {u"errorString"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return args.tor.error();
                                   }, .set = nullptr}},
        {u"eta"_s, {.get = [](const auto &args) -> QJsonValue {
                               return args.tor.eta();
                           }, .set = nullptr}},
        {u"etaIdle"_s, {.get = [](const auto &args) -> QJsonValue {
                                   const int maxInactiveSeedingTimeValue = args.tor.maxInactiveSeedingTime();
                                   if (maxInactiveSeedingTimeValue >= 0){
                                       const qint64 inactiveSeedingTimeEta = (maxInactiveSeedingTimeValue * 60) - args.tor.timeSinceActivity();
                                       return std::max(inactiveSeedingTimeEta, qint64{0});
                                   }
                                   else
                                       return 0;
                               }, .set = nullptr}},
        {u"file-count"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.info().filesCount();
                                  }, .set = nullptr}},
        {u"files"_s, {.get = [](const auto &args) -> QJsonValue {
                                 return ::torrentGetFiles(args.tor);
                             }, .set = nullptr}},
        {u"fileStats"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return ::torrentGetFileStats(args.tor);
                                 }, .set = nullptr}},
        // TODO this is for "Bandwidth Groups" - not supported in qbt?
        {u"group"_s, {.get = [](const auto &) -> QJsonValue {
                                 return QString{};
                             }, .set = nullptr}},
        {u"hashString"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.infoHash().v1().toString();
                                  }, .set = nullptr}},
        // looks like this isn't exposed by lbt or I couldn't find it
        {u"haveUnchecked"_s, {.get = [](const auto &) -> QJsonValue {
                                         return 0;
                                     }, .set = nullptr}},
        {u"haveValid"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return args.tor.completedSize();
                                 }, .set = nullptr}},
        // TODO
        {u"honorsSessionLimits"_s, {.get = [](const auto &) -> QJsonValue {
                                               return false;
                                           }, .set = nullptr}},
        {u"id"_s, {.get = [](const auto &args) -> QJsonValue {
                              return args.id;
                          }, .set = nullptr}},
        {u"isFinished"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.isFinished();
                                  }, .set = nullptr}},
        {u"isPrivate"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return args.tor.isPrivate();
                                 }, .set = nullptr}},
        {u"isStalled"_s, {.get = [](const auto &args) -> QJsonValue {
                                     using ts = BitTorrent::TorrentState;
                                     const ts state = args.tor.state();
                                     return state == ts::StalledDownloading || state == ts::StalledUploading;
                                 }, .set = nullptr}},
        {u"labels"_s, {.get = [](const auto &args) -> QJsonValue {
                                  QJsonArray rv{};
                                  for (auto &&t : args.tor.tags())
                                      rv.push_back(t.toString());
                                  return rv;
                              }, .set = nullptr}},
        {u"leftUntilDone"_s, {.get = [](const auto &args) -> QJsonValue {
                                         return args.tor.wantedSize() - args.tor.completedSize();
                                     }, .set = nullptr}},
        {u"magnetLink"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.createMagnetURI();
                                  }, .set = nullptr}},
        // FIXME not really doable, use now() + min_interval at least
        {u"manualAnnounceTime"_s, {.get = [](const auto &) -> QJsonValue {
                                              return -1;
                                          }, .set = nullptr}},
        {u"maxConnectedPeers"_s, {.get = [](const auto &args) -> QJsonValue {
                                             return args.tor.connectionsLimit();
                                         }, .set = nullptr}},
        // TODO?
        {u"metadataPercentComplete"_s, {.get = [](const auto &args) -> QJsonValue {
                                                   return args.tor.hasMetadata() ? 1.0 : 0.0;
                                               }, .set = nullptr}},
        {u"name"_s, {.get = [](const auto &args) -> QJsonValue {
                                return args.tor.name();
                            }, .set = nullptr}},
        {u"peer-limit"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.connectionsLimit();
                                  }, .set = nullptr}},
        {u"peers"_s, {.get = [](const auto &args) -> QJsonValue {
                                 return ::torrentGetPeers(args.peers);
                             }, .set = nullptr}},
        {u"peersConnected"_s, {.get = [](const auto &args) -> QJsonValue {
                                          return args.tor.peersCount();
                                      }, .set = nullptr}},
        {u"peersFrom"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return ::torrentGetPeersFrom(args.peers);
                                 }, .set = nullptr}},
        {u"peersGettingFromUs"_s, {.get = [](const auto &args) -> QJsonValue {
                                              return qint64{std::count_if(args.peers.begin(), args.peers.end(), [](const BitTorrent::PeerInfo &p) {
                            return ::peerIsBeingUploadedTo(p);
                        })};
                                          }, .set = nullptr}},
        {u"peersSendingToUs"_s, {.get = [](const auto &args) -> QJsonValue {
                                            return qint64{std::count_if(args.peers.begin(), args.peers.end(), [](const BitTorrent::PeerInfo &p) {
                            return ::peerIsBeingDownloadedFrom(p);
                        })};
                                        }, .set = nullptr}},
        {u"percentComplete"_s, {.get = [](const auto &args) -> QJsonValue {
                                           return static_cast<qreal>(args.tor.piecesHave()) / args.tor.piecesCount();
                                       }, .set = nullptr}},
        {u"percentDone"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return args.tor.progress();
                                   }, .set = nullptr}},
        {u"pieceCount"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.piecesCount();
                                  }, .set = nullptr}},
        {u"pieceSize"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return args.tor.pieceLength();
                                 }, .set = nullptr}},
        {u"pieces"_s, {.get = [](const auto &args) -> QJsonValue {
                                  const QBitArray pieces = args.tor.pieces();
                                  const QByteArray piecesAsByteAr = QByteArray::fromRawData(pieces.bits(), (pieces.size() + 7) / 8);
                                  return QString::fromLatin1(piecesAsByteAr.toBase64());
                              }, .set = nullptr}},
        {u"priorities"_s, {.get = [](const auto &args) -> QJsonValue {
                                      const QVector<BitTorrent::DownloadPriority> prios = args.tor.filePriorities();
                                      QJsonArray result{};
                                      for (const BitTorrent::DownloadPriority p : prios)
                                          result.push_back(convertToTransmissionFilePriority(p));
                                      return result;
                                  }, .set = nullptr}},
        // guesswork anyway and qBt doesn't do it at all - return what Transmission returns when unknown
        {u"primary-mime-type"_s, {.get = [](const auto &) -> QJsonValue {
                                             return u"application/octet-stream"_s;
                                         }, .set = nullptr}},
        {u"queuePosition"_s, {.get = [](const auto &args) -> QJsonValue {
                                         return args.tor.queuePosition();
                                     }, .set = nullptr}},
        {u"rateDownload"_s, {.get = [](const auto &args) -> QJsonValue {
                                        return args.tor.downloadPayloadRate();
                                    }, .set = nullptr}},
        {u"rateUpload"_s, {.get = [](const auto &args) -> QJsonValue {
                                      return args.tor.uploadPayloadRate();
                                  }, .set = nullptr}},
        {u"recheckProgress"_s, {.get = [](const auto &args) -> QJsonValue {
                                           return args.tor.isChecking() ? args.tor.progress() : 0.0;
                                       }, .set = nullptr}},
        {u"secondsDownloading"_s, {.get = [](const auto &args) -> QJsonValue {
                                              return args.tor.activeTime() - args.tor.finishedTime();
                                          }, .set = nullptr}},
        {u"secondsSeeding"_s, {.get = [](const auto &args) -> QJsonValue {
                                          return args.tor.finishedTime();
                                      }, .set = nullptr}},
        {u"seedIdleLimit"_s, {.get = [](const auto &args) -> QJsonValue {
                                         return args.tor.inactiveSeedingTimeLimit();
                                     }, .set = nullptr}},
        // TODO?
        {u"seedIdleMode"_s, {.get = [](const auto &) -> QJsonValue {
                                        return 0;
                                    }, .set = nullptr}},
        {u"seedRatioLimit"_s, {.get = [](const auto &args) -> QJsonValue {
                                          return args.tor.ratioLimit();
                                      }, .set = nullptr}},
        // TODO?
        {u"seedRatioMode"_s, {.get = [](const auto &) -> QJsonValue {
                                         return 0;
                                     }, .set = nullptr}},
        {u"sequentialDownload"_s, {.get = [](const auto &args) -> QJsonValue {
                                              return args.tor.isSequentialDownload();
                                          }, .set = nullptr}},
        {u"sizeWhenDone"_s, {.get = [](const auto &args) -> QJsonValue {
                                        return args.tor.wantedSize();
                                    }, .set = nullptr}},
        // FIXME? doesn't look exposed by lbt, so just use addedDate instead
        {u"startDate"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return Utils::DateTime::toSecsSinceEpoch(args.tor.addedTime());
                                 }, .set = nullptr}},
        {u"status"_s, {.get = [](const auto &args) -> QJsonValue {
                                  return ::torrentGetStatus(args.tor);
                              }, .set = nullptr}},
        {u"trackers"_s, {.get = [](const auto &args) -> QJsonValue {
                                    return ::torrentGetTrackers(args.tor);
                                }, .set = nullptr}},
        {u"trackerList"_s, {.get = [](const auto &args) -> QJsonValue {
                                       const QVector<BitTorrent::TrackerEntry> trackers = args.tor.trackers();
                                       QMap<int, QVector<QString>> trackersByTier;
                                       for (const BitTorrent::TrackerEntry & trk : trackers)
                                           trackersByTier[trk.tier] << trk.url;
                                       QString result;
                                       for (auto it = trackersByTier.keyValueBegin(); it != trackersByTier.keyValueEnd() ; ++it){
                                           for (const QString &url : it->second){
                                               result += url;
                                               result += u'\n';
                                           }
                                           result += u'\n';
                                       }
                                       return result;
                                   }, .set = nullptr}},
        {u"trackerStats"_s, {.get = [](const auto &args) -> QJsonValue {
                                        return ::torrentGetTrackerStats(args.tor);
                                    }, .set = nullptr}},
        {u"totalSize"_s, {.get = [](const auto &args) -> QJsonValue {
                                     return args.tor.totalSize();
                                 }, .set = nullptr}},
        // FIXME not available?
        {u"torrentFile"_s, {.get = [](const auto &) -> QJsonValue {
                                       return u"foobar.torrent"_s;
                                   }, .set = nullptr}},
        {u"uploadedEver"_s, {.get = [](const auto &args) -> QJsonValue {
                                        return args.tor.totalUpload();
                                    }, .set = nullptr}},
        {u"uploadLimit"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return args.tor.uploadLimit();
                                   }, .set = nullptr} },
        {u"uploadLimited"_s, {.get = [](const auto &args) -> QJsonValue {
                                         return args.tor.uploadLimit() != 0;
                                     }, .set = nullptr}},
        {u"uploadRatio"_s, {.get = [](const auto &args) -> QJsonValue {
                                       return static_cast<qreal>(args.tor.totalUpload()) / args.tor.wantedSize();
                                   }, .set = nullptr}},
        {u"wanted"_s, {.get = [](const auto &args) -> QJsonValue {
                                  const QVector<BitTorrent::DownloadPriority> prios = args.tor.filePriorities();
                                  QJsonArray result{};
                                  for (const BitTorrent::DownloadPriority p : prios)
                                      result.push_back(p != BitTorrent::DownloadPriority::Ignored);
                                  return result;
                              }, .set = nullptr}},
        {u"webseeds"_s, {.get = [](const auto &args) -> QJsonValue {
                                    const QVector<QUrl> webSeeds = args.tor.urlSeeds();
                                    QJsonArray result;
                                    for (const QUrl &ws : webSeeds)
                                        result << ws.toString();
                                    return result;
                                }, .set = nullptr}},
        // FIXME
        {u"webseedsSendingToUs"_s, {.get = [](const auto &) -> QJsonValue {
                                               return 0;
                                           }, .set = nullptr}}
    };

    std::optional<QJsonValue> torrentGetSingleField(const BitTorrent::Torrent &tor, int transmissionTorId, const QString &fld)
    {
        const TorrentGetFieldFuncs::GetArgs args = { tor, tor.peers(), transmissionTorId };
        const TorrentGetFieldFuncs funcs = fieldValueFuncHash.value(fld);
        if (funcs.get)
            return funcs.get(args);
        else
            return std::nullopt;
    }

    QJsonObject torrentGetObject(const BitTorrent::Torrent &tor, int transmissionTorId, const QSet<QString> &fields)
    {
        QJsonObject result{};
        for (const QString &f : fields)
            if (auto val = torrentGetSingleField(tor, transmissionTorId, f))
                result[f] = *val;
        return result;
    }

    QJsonArray torrentGetTable(const BitTorrent::Torrent &tor, int transmissionTorId, const QVector<TorrentGetFieldFuncs::GetFunc> &getFieldFuncs)
    {
        QJsonArray result{};
        const TorrentGetFieldFuncs::GetArgs args = { tor, tor.peers(), transmissionTorId };
        for (const TorrentGetFieldFuncs::GetFunc f : getFieldFuncs)
            result.push_back(f(args));
        return result;
    }

    QSet<QString> fieldsAsSet(const QJsonArray &fields)
    {
        QSet<QString> fieldsSet{};
        for (QJsonValue && f : fields){
            if (f.isString())
                fieldsSet.insert(f.toString());
            else
                throw TransmissionAPIError(APIErrorType::BadParams,
                                           u"'fields' value '%1' is not a string"_s.arg(f.toVariant().toString()));
        }
        return fieldsSet;
    }

    QJsonObject sessionGet(const QJsonObject &args, const QString &sid)
    {
        QJsonObject result{};
        const QJsonValue fields = args[u"fields"_s];
        if (!fields.isUndefined() && !fields.isArray())
            throw TransmissionAPIError(APIErrorType::BadParams, u"'fields' key in 'arguments' to session-get should be an array"_s);

        const struct FieldValueFuncArgs
        {
            const BitTorrent::Session *session;
            const Preferences *pref;
            QString sid;
        } func_args = {
            BitTorrent::Session::instance(),
            Preferences::instance(),
            sid
        };

        static const QHash<QString, QJsonValue (*)(const FieldValueFuncArgs &)> fieldValueFuncHash = {
            {u"alt-speed-down"_s, [](const auto &args) -> QJsonValue {
                 { return args.session->altGlobalDownloadSpeedLimit(); }
             }},
            {u"alt-speed-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isAltGlobalSpeedLimitEnabled();
             }},
            {u"alt-speed-time-begin"_s, [](const auto &args) -> QJsonValue {
                 return minutesAfterMidnight(args.pref->getSchedulerStartTime());
             }},
            {u"alt-speed-time-day"_s, [](const auto &args) -> QJsonValue {
                 return transmissionSchedulerDays(args.pref->getSchedulerDays());
             }},
            {u"alt-speed-time-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isBandwidthSchedulerEnabled();
             }},
            {u"alt-speed-time-end"_s, [](const auto &args) -> QJsonValue {
                 return minutesAfterMidnight(args.pref->getSchedulerEndTime());
             }},
            {u"alt-speed-up"_s, [](const auto &args) -> QJsonValue {
                 return args.session->altGlobalUploadSpeedLimit();
             }},
            {u"blocklist-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isIPFilteringEnabled();
             }},
            {u"blocklist-url"_s, [](const auto &args) -> QJsonValue {
                 return args.session->IPFilterFile().toString();
             }},
            {u"cache-size-mb"_s, [](const auto &args) -> QJsonValue {
                 return args.session->diskCacheSize() / 1024 / 1024;
             }},
            {u"config-dir"_s, [](const auto &) -> QJsonValue {
                 return Profile::instance()->location(SpecialFolder::Config).toString();
             }},
            {u"default-trackers"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isAddTrackersEnabled() ? args.session->additionalTrackers() : QString{};
             }},
            {u"dht-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isDHTEnabled();
             }},
            {u"download-dir"_s, [](const auto &args) -> QJsonValue {
                 return args.session->savePath().toString();
             }},
            {u"download-dir-free-space"_s, [](const auto &args) -> QJsonValue {
                 return static_cast<qint64>(std::filesystem::space(args.session->savePath().toStdFsPath()).available);
             }},
            {u"download-queue-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isQueueingSystemEnabled();
             }},
            {u"download-queue-size"_s, [](const auto &args) -> QJsonValue {
                 return args.session->maxActiveDownloads();
             }},
            {u"encryption"_s, [](const auto &args) -> QJsonValue {
                 auto const strings = std::array{u"preferred"_s, u"required"_s, u"tolerated"_s};
                 const int val = args.session->encryption();
                 Q_ASSERT(val < static_cast<int>(strings.size()));
                 return strings[val];
             }},
            {u"incomplete-dir-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isDownloadPathEnabled();
             }},
            {u"incomplete-dir"_s, [](const auto &args) -> QJsonValue {
                 return args.session->downloadPath().toString();
             }},
            {u"lpd-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isLSDEnabled();
             }},
            {u"peer-limit-global"_s, [](const auto &args) -> QJsonValue {
                 return args.session->maxConnections();
             }},
            {u"peer-limit-per-torrent"_s, [](const auto &args) -> QJsonValue {
                 return args.session->maxConnectionsPerTorrent();
             }},
            {u"peer-port"_s, [](const auto &args) -> QJsonValue {
                 return args.session->port();
             }},
            {u"pex-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isPeXEnabled();
             }},
            {u"port-forwarding-enabled"_s, [](const auto &) -> QJsonValue {
                 return Net::PortForwarder::instance()->isEnabled();
             }},
            {u"rename-partial-files"_s, [](const auto &args) -> QJsonValue {
                 return args.session->isAppendExtensionEnabled();
             }},
            {u"rpc-version-minimum"_s, [](const auto &) -> QJsonValue {
                 return 18;
             }},
            {u"rpc-version-semver"_s, [](const auto &) -> QJsonValue {
                 return u"5.4.0"_s;
             }},
            {u"rpc-version"_s, [](const auto &) -> QJsonValue {
                 return 18;
             }},
            {u"script-torrent-added-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.pref->isAutoRunOnTorrentAddedEnabled();
             }},
            {u"script-torrent-added-filename"_s, [](const auto &args) -> QJsonValue {
                 return args.pref->getAutoRunOnTorrentAddedProgram();
             }},
            {u"script-torrent-done-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.pref->isAutoRunOnTorrentFinishedEnabled();
             }},
            {u"script-torrent-done-filename"_s, [](const auto &args) -> QJsonValue {
                 return args.pref->getAutoRunOnTorrentFinishedProgram();
             }},
            {u"seedRatioLimit"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalMaxRatio();
             }},
            {u"seedRatioLimited"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalMaxRatio() >= 0.;
             }},
            {u"speed-limit-down-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalDownloadSpeedLimit() > 0;
             }},
            {u"speed-limit-down"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalDownloadSpeedLimit() / 1024;
             }},
            {u"speed-limit-up-enabled"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalUploadSpeedLimit() > 0;
             }},
            {u"speed-limit-up"_s, [](const auto &args) -> QJsonValue {
                 return args.session->globalUploadSpeedLimit() / 1024;
             }},
            {u"start-added-torrents"_s, [](const auto &args) -> QJsonValue {
                 return !args.session->isAddTorrentPaused();
             }},
            {u"trash-original-torrent-files"_s, [](const auto &) -> QJsonValue {
                 return TorrentFileGuard::autoDeleteMode() >= TorrentFileGuard::AutoDeleteMode::IfAdded;
             }},
            {u"units"_s, [](const auto &) -> QJsonValue {
                 return ::units();
             }},
            {u"utp-enabled"_s, [](const auto &args) -> QJsonValue {
                 using btp = BitTorrent::SessionSettingsEnums::BTProtocol; const btp proto = args.session->btProtocol(); return proto == btp::Both || proto == btp::UTP;
             }},
            {u"version"_s, [](const auto &) -> QJsonValue {
                 return u"qBt %1 Qt %2 lb %3"_s.arg(QStringLiteral(QBT_VERSION), QStringLiteral(QT_VERSION_STR), Utils::Misc::libtorrentVersionString());
             }},
            {u"session-id"_s, [](const auto &args) -> QJsonValue {
                 return args.sid;
             }},

            // functionality deprecated in qbt
            {u"peer-port-random-on-start"_s, [](const auto &) -> QJsonValue {
                 return false;
             }},

            // not supported in qbt
            {u"script-torrent-done-seeding-enabled"_s, [](const auto &) -> QJsonValue {
                 return false;
             }},
            {u"script-torrent-done-seeding-filename"_s, [](const auto &) -> QJsonValue {
                 return QString{};
             }},
            {u"seed-queue-enabled"_s, [](const auto &) -> QJsonValue {
                 return false;
             }},
            {u"seed-queue-size"_s, [](const auto &) -> QJsonValue {
                 return 0;
             }},

            // TODOs / need investigation
            {u"blocklist-size"_s, [](const auto &) -> QJsonValue {
                 return 0;
             }},
            {u"idle-seeding-limit-enabled"_s, [](const auto &) -> QJsonValue {
                 return false;
             }},
            {u"idle-seeding-limit"_s, [](const auto &) -> QJsonValue {
                 return 0;
             }},
            {u"queue-stalled-enabled"_s, [](const auto &) -> QJsonValue {
                 return false;
             }},
            {u"queue-stalled-minutes"_s, [](const auto &) -> QJsonValue {
                 return 0;
             }},
        };

        const QSet<QString> fieldsSet = fieldsAsSet(fields.toArray());
        if (!fieldsSet.empty()){
            for (const QString &key : fieldsSet){
                if (auto func = fieldValueFuncHash.value(key, nullptr))
                    result[key] = func(func_args);
                else
                    throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown arg '%1' to session-get"_s.arg(key));
            }
        }
        else {
            const auto it_end = fieldValueFuncHash.constKeyValueEnd();
            for (auto it = fieldValueFuncHash.constKeyValueBegin(); it != it_end ; ++it)
                result[it->first] = it->second(func_args);
        }

        return result;
    }

    QJsonObject freeSpace(const QJsonObject &args)
    {
        const QString path = args[u"path"_s].toString();
        if (path.isEmpty())
            throw TransmissionAPIError(APIErrorType::BadParams,
                                       u"Missing parameter 'path' missing or not a string : %1"_s.arg(args[u"path"_s].toVariant().toString()));
        const std::filesystem::space_info space = std::filesystem::space(Path{path}.toStdFsPath());
        return {
            {u"path"_s, path},
            {u"size-bytes"_s, static_cast<qint64>(space.available)},
            {u"total_size"_s, static_cast<qint64>(space.capacity)}
        };
    }
}

TransmissionRPCController::TransmissionRPCController(IApplication *app, WebSession *parent)
    : APIController(app, parent), m_sid(parent->id())
{
    using namespace BitTorrent;
    const Session *const session = Session::instance();
    QVector<Torrent *> const torrents = session->torrents();
    m_idToTorrent.reserve(torrents.size());
    m_torrentToId.reserve(torrents.size());
    for (Torrent *t : torrents)
        saveMapping(t);
    QObject::connect(session, &Session::torrentAdded, this, &TransmissionRPCController::saveMapping);
    QObject::connect(session, &Session::torrentAboutToBeRemoved, this, &TransmissionRPCController::removeMapping);
}

void TransmissionRPCController::rpcAction()
{
    const QJsonObject request = parseTopLevel(data()[QString()]);
    const QString method = request[u"method"_s].toString();
    const QJsonObject args = request[u"arguments"_s].toObject();
    QJsonObject result{};
    const QJsonValue tag = request[u"tag"_s];
    if (tag.isDouble())
        result[u"tag"_s] = tag.toInt();

    try {
        QJsonObject methodResult{};
        if (method == u"session-get"_s)
            methodResult = sessionGet(args, m_sid);
        else if (method == u"session-close"_s){
            QTimer::singleShot(std::chrono::milliseconds(100), Qt::CoarseTimer, qApp, []
            {
                QCoreApplication::exit();
            });
        }
        else if (method == u"torrent-get"_s)
            methodResult = torrentGet(args);
        else if (method == u"free-space"_s)
            methodResult = freeSpace(args);
        else
            throw TransmissionAPIError(APIErrorType::NotFound, u"Undefined method %1"_s.arg(method));
        result[u"arguments"_s] = methodResult;
        result[u"result"_s] = u"success"_s;
    }
    catch (const TransmissionAPIError &err)
    {
        result[u"result"_s] = err.message();
    }

    setResult(result);
}

void TransmissionRPCController::saveMapping(BitTorrent::Torrent *tor)
{
    const int this_id = m_lastId++;
    m_idToTorrent[this_id] = tor;
    m_torrentToId[tor] = this_id;
}

void TransmissionRPCController::removeMapping(BitTorrent::Torrent *tor)
{
    const int id = m_torrentToId.take(tor);
    m_idToTorrent.remove(id);
}

QJsonObject TransmissionRPCController::torrentGet(const QJsonObject &args) const
{
    const QJsonValue fields = args[u"fields"_s];
    if (!fields.isArray())
        throw TransmissionAPIError(APIErrorType::BadParams, u"'fields' key in 'arguments' to torrent-get must be an array"_s);
    const TorrentGetFormat format = torrentGetParseFormatArg(args[u"format"_s]);
    const QSet<QString> fieldsSet = fieldsAsSet(fields.toArray());
    const QVector<std::pair<int, BitTorrent::Torrent *>> requestedTorrents = collectTorrentsForRequest(args[u"ids"_s]);
    QJsonArray torrents;
    if (format == TorrentGetFormat::Object)
        for (auto [id, t] : requestedTorrents)
            torrents.push_back(torrentGetObject(*t, id, fieldsSet));
    else if (format == TorrentGetFormat::Table){
        QJsonArray fields;
        QVector<TorrentGetFieldFuncs::GetFunc> fieldGetFuncs;
        fieldGetFuncs.reserve(fieldsSet.size());
        for (const QString &f : fieldsSet){
            const TorrentGetFieldFuncs funcs = fieldValueFuncHash.value(f);
            if (funcs.get){
                fieldGetFuncs.push_back(funcs.get);
                fields.push_back(f);
            }
        }
        torrents.push_back(fields);
        for (auto [id, t] : requestedTorrents)
            torrents.push_back(torrentGetTable(*t, id, fieldGetFuncs));
    }

    return {{u"torrents"_s, torrents}};
}

QVector<std::pair<int, BitTorrent::Torrent *>> TransmissionRPCController::collectTorrentsForRequest(const QJsonValue &ids) const
{
    QVector<std::pair<int, BitTorrent::Torrent *>> rv{};
    const auto saveTorrent = [&](int id) {
                                 if (BitTorrent::Torrent *const tor = m_idToTorrent.value(id, nullptr))
                                     rv.push_back(std::make_pair(id, tor));
                                 else
                                     throw TransmissionAPIError(APIErrorType::BadParams, u"Undefined id '%1'"_s.arg(id));
                             };

    if (ids.isUndefined()){
        rv.reserve(m_idToTorrent.size());
        std::copy(m_idToTorrent.keyValueBegin(), m_idToTorrent.keyValueEnd(), std::back_inserter(rv));
        return rv;
    }
    else if (ids.isArray()){
        const QJsonArray idsArray = ids.toArray();
        rv.reserve(idsArray.size());
        for (const QJsonValueConstRef &idVal : idsArray){
            if (idVal.isDouble()){
                const int id = idVal.toInt();
                saveTorrent(id);
            }
        }
        return rv;
    }
    else if (ids.toString() == u"recently-active"_s)
        // FIXME "recently" means 60 seconds in Transmission
        return {};
    else if (ids.isDouble()){
        saveTorrent(ids.toInt());
        return rv;
    }
    else
        throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown type for 'ids'"_s);
}
