#include "transmissionrpccontroller.h"

#include <filesystem>
#include <iterator>

#include <QBitArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
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
    explicit TransmissionAPIError(APIErrorType type, const QString &message = {}):APIError(type, message) {}
};

QJsonObject parseTopLevel(const QByteArray &payload)
{
    QJsonParseError parseErr{};
    const QJsonDocument request = QJsonDocument::fromJson(payload, &parseErr);
    if (request.isNull())
    {
        throw APIError(APIErrorType::BadData, parseErr.errorString());
    }
    if (!request.isObject())
    {
        throw APIError(APIErrorType::BadData, u"Top-level JSON should be an object"_s);
    }

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
    switch(days)
    {
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
        throw TransmissionAPIError(APIErrorType::BadParams, u"'format' key in 'arguments' to torrent-get must be either 'object' or 'table' if present, got '%1'"_s.arg(actual));
    };
    TorrentGetFormat rv = TorrentGetFormat::Object; // default if not present
    if (format.isString())
    {
        const QString formatStr = format.toString();
        if (formatStr == u"object"_s)
        {
            rv = TorrentGetFormat::Object;
        }
        else if (formatStr == u"table"_s)
        {
            rv = TorrentGetFormat::Table;
        }
        else
        {
            onError(formatStr);
        }
    }
    else if (!format.isUndefined())
    {
        onError(format.toVariant().toString());
    }
    return rv;
}

QJsonValue torrentGetFiles(const BitTorrent::Torrent &tor)
{
    QJsonArray result{};
    const BitTorrent::TorrentInfo info = tor.info();
    const int filesCount = info.filesCount();
    const QVector<qreal> filesProgress = tor.filesProgress();
    const PathList filePaths = tor.filePaths();
    for(int i = 0 ; i < filesCount ; ++i)
    {
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

qint64 convertToTransmissionFilePriority(BitTorrent::DownloadPriority priority) {
    using dp = BitTorrent::DownloadPriority;
    switch(priority) {
    case dp::Normal:
        return 0; // TR_PRI_NORMAL
    case dp::High:
    case dp::Maximum:
        return 1; // TR_PRI_HIGH, doesn't map too nicely :(
    default:
        return -1; // TR_PRI_LOW as there's no other choice
    }
}

QJsonValue torrentGetFileStats(const BitTorrent::Torrent &tor)
{
    QJsonArray result{};
    const BitTorrent::TorrentInfo info = tor.info();
    const int filesCount = info.filesCount();
    const QVector<qreal> filesProgress = tor.filesProgress();
    using dp = BitTorrent::DownloadPriority;
    const QVector<dp> filePriorities = tor.filePriorities();
    for(int i = 0 ; i < filesCount ; ++i)
    {
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

QJsonValue torrentGetSingleField(const BitTorrent::Torrent &tor, int transmissionTorId, const QString &fld)
{
    const struct FieldValueFuncArgs
    {
        const BitTorrent::Torrent &tor;
        int id;
    } args = { tor, transmissionTorId };

    static const QHash<QString, QJsonValue (*)(const FieldValueFuncArgs&)> fieldValueFuncHash = {
    {u"activityDate"_s, [](const auto &args) -> QJsonValue {
        const qlonglong timeSinceActivity = args.tor.timeSinceActivity();
        return (timeSinceActivity < 0)
            ? Utils::DateTime::toSecsSinceEpoch(args.tor.addedTime())
            : (QDateTime::currentDateTime().toSecsSinceEpoch() - timeSinceActivity);
    }},
    {u"addedDate"_s, [](const auto &args) -> QJsonValue { return Utils::DateTime::toSecsSinceEpoch(args.tor.addedTime()); }},
    {u"availability"_s, [](const auto &args) -> QJsonValue {
        const QVector<int> peersPerPiece = args.tor.pieceAvailability();
        const QBitArray ourPieces = args.tor.pieces();
        Q_ASSERT(peersPerPiece.size() == ourPieces.size());
        QJsonArray result = {};
        for(qsizetype i = 0 ; i < ourPieces.size() ; ++i)
        {
            result.push_back(ourPieces[i] ? -1 : peersPerPiece[i]);
        }
        return result;
    }},
    {u"bandwidthPriority"_s, [](const auto &) -> QJsonValue {
        // qBt doesn't appear to have torrent priorities so just report everything as NORMAL
        return 0;
    }},
    {u"comment"_s, [](const auto &args) -> QJsonValue { return args.tor.comment(); }},
    {u"corruptEver"_s, [](const auto &args) -> QJsonValue {
        // not really "ever" - resets on pause in qBt, while Transmission saves this info to resume files
        return args.tor.wastedSize();
    }},
    {u"creator"_s, [](const auto &args) -> QJsonValue { return args.tor.creator(); }},
    {u"dateCreated"_s, [](const auto &args) -> QJsonValue { return Utils::DateTime::toSecsSinceEpoch(args.tor.creationDate()); }},
    {u"desiredAvailable"_s, [](const auto &args) -> QJsonValue
    {
        const QVector<int> peersPerPiece = args.tor.pieceAvailability();
        const QBitArray ourPieces = args.tor.pieces();
        Q_ASSERT(peersPerPiece.size() == ourPieces.size());
        qint64 result = 0;
        const auto singlePieceSize = args.tor.pieceLength();
        for(qsizetype i = 0 ; i < ourPieces.size() ; ++i)
        {
            if (!ourPieces[i] && peersPerPiece[i] >= 1)
            {
                result += singlePieceSize;
            }
        }
        return result;
    }},
    {u"doneDate"_s, [](const auto &args) -> QJsonValue { return Utils::DateTime::toSecsSinceEpoch(args.tor.completedTime()); }},
    {u"downloadDir"_s, [](const auto &args) -> QJsonValue { return args.tor.savePath().toString(); }},
    {u"downloadedEver"_s, [](const auto &args) -> QJsonValue { return args.tor.totalDownload(); }},
    {u"downloadLimit"_s, [](const auto &args) -> QJsonValue { return args.tor.downloadLimit(); } },
    {u"downloadLimited"_s, [](const auto &args) -> QJsonValue { return args.tor.downloadLimit() != 0; }},
    {u"editDate"_s, [](const auto &) -> QJsonValue { return 0; }}, // unsupported, so return Transmission's default value.
    {u"error"_s, [](const auto &args) -> QJsonValue {
        if (args.tor.state() == BitTorrent::TorrentState::MissingFiles ||
                args.tor.state() == BitTorrent::TorrentState::Error)
        {
            return 3; // TR_STAT_LOCAL_ERROR - "local trouble"
        }

        const QVector<BitTorrent::TrackerEntry> trackers = args.tor.trackers();
        const bool trackerHasError = std::any_of(trackers.cbegin(), trackers.cend(), [](const BitTorrent::TrackerEntry &entry){
            return entry.status == BitTorrent::TrackerEntryStatus::TrackerError;
        });
        return trackerHasError ? 2 /* TR_STAT_TRACKER_ERROR */ : 0 /* TR_STAT_OK */;
    }},
    {u"errorString"_s, [](const auto &args) -> QJsonValue { return args.tor.error(); }},
    {u"eta"_s, [](const auto &args) -> QJsonValue { return args.tor.eta(); }},
    {u"etaIdle"_s, [](const auto &args) -> QJsonValue {
        const int maxInactiveSeedingTimeValue = args.tor.maxInactiveSeedingTime();
        if (maxInactiveSeedingTimeValue >= 0)
        {
            const qint64 inactiveSeedingTimeEta = (maxInactiveSeedingTimeValue * 60) - args.tor.timeSinceActivity();
            return std::max(inactiveSeedingTimeEta, qint64{0});
        }
        else
        {
            return 0;
        }
    }},
    {u"file-count"_s, [](const auto &args) -> QJsonValue { return args.tor.info().filesCount(); }},
    {u"files"_s, [](const auto &args) -> QJsonValue { return ::torrentGetFiles(args.tor); }},
    {u"fileStats"_s, [](const auto &args) -> QJsonValue { return ::torrentGetFileStats(args.tor); }},
    {u"group"_s, [](const auto&) -> QJsonValue { return QString{}; }}, // TODO not supported in qbt?
    {u"hashString"_s, [](const auto &args) -> QJsonValue { return args.tor.infoHash().v1().toString(); }},
    {u"haveUnchecked"_s, [](const auto &) -> QJsonValue { return 0; }}, // looks like this isn't exposed by lbt or I couldn't find it
    {u"haveValid"_s, [](const auto &args) -> QJsonValue { return args.tor.completedSize(); }},
    {u"honorsSessionLimits"_s, [](const auto &) -> QJsonValue { return false; }}, // TODO
    {u"id"_s, [](const auto &args) -> QJsonValue { return args.id; }}, // FIXME
    };

    if (auto func = fieldValueFuncHash.value(fld, nullptr))
    {
        return func(args);
    }
    else
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown arg '%1' to torrent-get"_s.arg(fld));
    }
}

QJsonObject torrentGetObject(const BitTorrent::Torrent &tor, int transmissionTorId, const QSet<QString> &fields)
{
    QJsonObject result{};
    for(const QString &f : fields)
    {
        result[f] = torrentGetSingleField(tor, transmissionTorId, f);
    }
    return result;
}

QJsonArray torrentGetTable(const BitTorrent::Torrent &tor, int transmissionTorId, const QSet<QString> &fields)
{
    QJsonArray result{};
    for(const QString &f : fields)
    {
        result.push_back(torrentGetSingleField(tor, transmissionTorId, f));
    }
    return result;
}

QSet<QString> fieldsAsSet(const QJsonArray &fields)
{
    QSet<QString> fieldsSet{};
    for (QJsonValue &&f : fields)
    {
        if (f.isString())
        {
            fieldsSet.insert(f.toString());
        }
        else
        {
            throw TransmissionAPIError(APIErrorType::BadParams,
                                       u"'fields' value '%1' is not a string"_s.arg(f.toVariant().toString()));
        }
    }
    return fieldsSet;
}

QJsonObject sessionGet(const QJsonObject &args, const QString &sid)
{
    QJsonObject result{};
    const QJsonValue fields = args[u"fields"_s];
    if (!fields.isUndefined() && !fields.isArray())
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"'fields' key in 'arguments' to session-get should be an array"_s);
    }

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

    static const QHash<QString, QJsonValue (*)(const FieldValueFuncArgs&)> fieldValueFuncHash = {
        {u"alt-speed-down"_s, [](const auto& args) -> QJsonValue { { return args.session->altGlobalDownloadSpeedLimit(); } }},
        {u"alt-speed-enabled"_s, [](const auto& args) -> QJsonValue { return args.session->isAltGlobalSpeedLimitEnabled(); }},
        {u"alt-speed-time-begin"_s, [](const auto& args) -> QJsonValue { return minutesAfterMidnight(args.pref->getSchedulerStartTime());}},
    {u"alt-speed-time-day"_s, [](const auto &args) -> QJsonValue { return transmissionSchedulerDays(args.pref->getSchedulerDays()); }},
    {u"alt-speed-time-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isBandwidthSchedulerEnabled(); }},
    {u"alt-speed-time-end"_s, [](const auto &args) -> QJsonValue { return minutesAfterMidnight(args.pref->getSchedulerEndTime()); }},
    {u"alt-speed-up"_s, [](const auto &args) -> QJsonValue { return args.session->altGlobalUploadSpeedLimit(); }},
    {u"blocklist-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isIPFilteringEnabled(); }},
    {u"blocklist-url"_s, [](const auto &args) -> QJsonValue { return args.session->IPFilterFile().toString(); }},
    {u"cache-size-mb"_s, [](const auto &args) -> QJsonValue { return args.session->diskCacheSize() / 1024 / 1024; }},
    {u"config-dir"_s, [](const auto &) -> QJsonValue { return Profile::instance()->location(SpecialFolder::Config).toString(); }},
    {u"default-trackers"_s, [](const auto &args) -> QJsonValue { return args.session->isAddTrackersEnabled() ? args.session->additionalTrackers() : QString{}; }},
    {u"dht-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isDHTEnabled(); }},
    {u"download-dir"_s, [](const auto &args) -> QJsonValue { return args.session->savePath().toString(); }},
    {u"download-dir-free-space"_s, [](const auto &args) -> QJsonValue { return static_cast<qint64>(std::filesystem::space(args.session->savePath().toStdFsPath()).available); }},
    {u"download-queue-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isQueueingSystemEnabled(); }},
    {u"download-queue-size"_s, [](const auto &args) -> QJsonValue { return args.session->maxActiveDownloads(); }},
    {u"encryption"_s, [](const auto &args) -> QJsonValue {
        auto const strings = std::array{u"preferred"_s, u"required"_s, u"tolerated"_s};
        const int val = args.session->encryption();
        Q_ASSERT(val < static_cast<int>(strings.size()));
        return strings[val];}},
    {u"incomplete-dir-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isDownloadPathEnabled(); }},
    {u"incomplete-dir"_s, [](const auto &args) -> QJsonValue { return args.session->downloadPath().toString(); }},
    {u"lpd-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isLSDEnabled(); }},
    {u"peer-limit-global"_s, [](const auto &args) -> QJsonValue { return args.session->maxConnections(); }},
    {u"peer-limit-per-torrent"_s, [](const auto &args) -> QJsonValue { return args.session->maxConnectionsPerTorrent(); }},
    {u"peer-port"_s, [](const auto &args) -> QJsonValue { return args.session->port(); }},
    {u"pex-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->isPeXEnabled(); }},
    {u"port-forwarding-enabled"_s, [](const auto &) -> QJsonValue { return Net::PortForwarder::instance()->isEnabled(); }},
    {u"rename-partial-files"_s, [](const auto &args) -> QJsonValue { return args.session->isAppendExtensionEnabled(); }},
    {u"rpc-version-minimum"_s, [](const auto &) -> QJsonValue { return 18; }},
    {u"rpc-version-semver"_s, [](const auto &) -> QJsonValue { return u"5.4.0"_s; }},
    {u"rpc-version"_s, [](const auto &) -> QJsonValue { return 18; }},
    {u"script-torrent-added-enabled"_s, [](const auto &args) -> QJsonValue { return args.pref->isAutoRunOnTorrentAddedEnabled(); }},
    {u"script-torrent-added-filename"_s, [](const auto &args) -> QJsonValue { return args.pref->getAutoRunOnTorrentAddedProgram(); }},
    {u"script-torrent-done-enabled"_s, [](const auto &args) -> QJsonValue { return args.pref->isAutoRunOnTorrentFinishedEnabled(); }},
    {u"script-torrent-done-filename"_s, [](const auto &args) -> QJsonValue { return args.pref->getAutoRunOnTorrentFinishedProgram(); }},
    {u"seedRatioLimit"_s, [](const auto &args) -> QJsonValue { return args.session->globalMaxRatio(); }},
    {u"seedRatioLimited"_s, [](const auto &args) -> QJsonValue { return args.session->globalMaxRatio() >= 0.; }},
    {u"speed-limit-down-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->globalDownloadSpeedLimit() > 0; }},
    {u"speed-limit-down"_s, [](const auto &args) -> QJsonValue { return args.session->globalDownloadSpeedLimit() / 1024; }},
    {u"speed-limit-up-enabled"_s, [](const auto &args) -> QJsonValue { return args.session->globalUploadSpeedLimit() > 0; }},
    {u"speed-limit-up"_s, [](const auto &args) -> QJsonValue { return args.session->globalUploadSpeedLimit() / 1024; }},
    {u"start-added-torrents"_s, [](const auto &args) -> QJsonValue { return !args.session->isAddTorrentPaused(); }},
    {u"trash-original-torrent-files"_s, [](const auto &) -> QJsonValue { return TorrentFileGuard::autoDeleteMode() >= TorrentFileGuard::AutoDeleteMode::IfAdded; }},
    {u"units"_s, [](const auto&) -> QJsonValue { return ::units(); }},
    {u"utp-enabled"_s, [](const auto &args) -> QJsonValue{using btp = BitTorrent::SessionSettingsEnums::BTProtocol; const btp proto = args.session->btProtocol(); return proto == btp::Both || proto == btp::UTP; }},
    {u"version"_s, [](const auto &) -> QJsonValue{return u"qBt %1 Qt %2 lb %3"_s.arg(QStringLiteral(QBT_VERSION), QStringLiteral(QT_VERSION_STR), Utils::Misc::libtorrentVersionString());}},
    {u"session-id"_s, [](const auto &args) -> QJsonValue { return args.sid; }}, // not applicable, maybe use cookie?

    {u"peer-port-random-on-start"_s, [](const auto &) -> QJsonValue { return false; }}, // functionality deprecated in qbt
    {u"script-torrent-done-seeding-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // not supported in qbt
    {u"script-torrent-done-seeding-filename"_s, [](const auto&) -> QJsonValue { return QString{}; }}, // not supported in qbt
    {u"seed-queue-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // not supported?
    {u"seed-queue-size"_s, [](const auto&) -> QJsonValue { return 0; }},

    {u"blocklist-size"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    {u"idle-seeding-limit-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // TODO
    {u"idle-seeding-limit"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    {u"queue-stalled-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // TODO
    {u"queue-stalled-minutes"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    };

    const QSet<QString> fieldsSet = fieldsAsSet(fields.toArray());
    if (!fieldsSet.empty())
    {
        for(const QString &key : fieldsSet)
        {
            if (auto func = fieldValueFuncHash.value(key, nullptr))
            {
                result[key] = func(func_args);
            }
            else
            {
                throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown arg '%1' to session-get"_s.arg(key));
            }
        }
    }
    else
    {
        const auto it_end = fieldValueFuncHash.constKeyValueEnd();
        for(auto it = fieldValueFuncHash.constKeyValueBegin(); it != it_end ; ++it)
        {
            result[it->first] = it->second(func_args);
        }
    }

    return result;
}

QJsonObject freeSpace(const QJsonObject &args)
{
    const QString path = args[u"path"_s].toString();
    if (path.isEmpty())
    {
        throw TransmissionAPIError(APIErrorType::BadParams,
                                   u"Missing parameter 'path' missing or not a string : %1"_s.arg(args[u"path"_s].toVariant().toString()));
    }
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
    QVector<Torrent*> const torrents = session->torrents();
    m_idToTorrent.reserve(torrents.size());
    m_torrentToId.reserve(torrents.size());
    for(Torrent *t : torrents)
    {
        saveMapping(t);
    }
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
    {
        result[u"tag"_s] = tag.toInt();
    }

    try
    {
        QJsonObject methodResult{};
        if (method == u"session-get"_s)
        {
            methodResult = sessionGet(args, m_sid);
        }
        else if (method == u"session-close"_s)
        {
            QTimer::singleShot(std::chrono::milliseconds(100), Qt::CoarseTimer, qApp, []
            {
                QCoreApplication::exit();
            });
        }
        else if (method == u"torrent-get"_s)
        {
            methodResult = torrentGet(args);
        }
        else if (method == u"free-space"_s)
        {
            methodResult = freeSpace(args);
        }
        else
        {
            throw TransmissionAPIError(APIErrorType::NotFound, u"Undefined method %1"_s.arg(method));
        }
        result[u"arguments"_s] = methodResult;
        result[u"result"_s] = u"success"_s;
    }
    catch(const TransmissionAPIError &err)
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
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"'fields' key in 'arguments' to torrent-get must be an array"_s);
    }
    const TorrentGetFormat format = torrentGetParseFormatArg(args[u"format"_s]);
    const QSet<QString> fieldsSet = fieldsAsSet(fields.toArray());
    QVector<std::pair<int, BitTorrent::Torrent*>> requestedTorrents = collectTorrentsForRequest(args[u"ids"_s]);
    QJsonArray torrents;
    if (format == TorrentGetFormat::Object)
    {
        for (auto [id, t] : requestedTorrents)
        {
            torrents.push_back(torrentGetObject(*t, id, fieldsSet));
        }
    }
    else if (format == TorrentGetFormat::Table)
    {
        QJsonArray fields;
        for (const QString &f : fieldsSet)
        {
            fields.push_back(f);
        }
        torrents.push_back(fields);
        for (auto [id, t] : requestedTorrents)
        {
            torrents.push_back(torrentGetTable(*t, id, fieldsSet));
        }
    }

    return {{u"torrents"_s, torrents}};
}

QVector<std::pair<int, BitTorrent::Torrent *> > TransmissionRPCController::collectTorrentsForRequest(const QJsonValue& ids) const
{
    QVector<std::pair<int, BitTorrent::Torrent*>> rv{};
    const auto saveTorrent = [&](int id) {
        if (BitTorrent::Torrent *const tor = m_idToTorrent.value(id, nullptr))
        {
            rv.push_back(std::make_pair(id, tor));
        }
        else
        {
            throw TransmissionAPIError(APIErrorType::BadParams, u"Undefined id '%1'"_s.arg(id));
        }
    };

    if (ids.isUndefined())
    {
        rv.reserve(m_idToTorrent.size());
        std::copy(m_idToTorrent.keyValueBegin(), m_idToTorrent.keyValueEnd(), std::back_inserter(rv));
        return rv;
    }
    else if (ids.isArray())
    {
        const QJsonArray idsArray = ids.toArray();
        rv.reserve(idsArray.size());
        for (const QJsonValueConstRef &idVal : idsArray)
        {
            if (idVal.isDouble())
            {
                const int id = idVal.toInt();
                saveTorrent(id);
            }
        }
        return rv;
    }
    else if (ids.toString() == u"recently-active"_s)
    {
        // FIXME "recently" means 60 seconds in Transmission
        return {};
    }
    else if (ids.isDouble())
    {
        saveTorrent(ids.toInt());
        return rv;
    }
    else
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown type for 'ids'"_s);
    }
}
