#include "transmissionrpccontroller.h"

#include <filesystem>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "base/bittorrent/session.h"
#include "base/net/portforwarder.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/torrentfileguard.h"
#include "base/utils/misc.h"
#include "base/version.h"

#include "apierror.h"

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
    case Scheduler::Days::Weekend:
        return weekend;
    case Scheduler::Days::Weekday:
        return weekday;
    default:
        return getBitmask(days);
    }
}

QJsonObject sessionGet(const QJsonObject &args)
{
    QJsonObject result{};
    const QJsonValue fields = args[u"fields"_s];
    if (!fields.isUndefined() && !fields.isArray())
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"'fields' key in 'arguments' to session-get should be an array"_s);
    }
    const QJsonArray fieldsArray = fields.toArray();
    QSet<QString> fieldsSet{};
    for (QJsonValue &&f : fieldsArray)
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

    const auto insertIfRequested = [&](const QString &key, auto &&value_fn){
        if (fieldsSet.empty() || fieldsSet.contains(key))
        {
            result[key] = value_fn();
        }
    };
    const Preferences *const pref = Preferences::instance();
    const BitTorrent::Session *const session = BitTorrent::Session::instance();

    insertIfRequested(u"alt-speed-down"_s, [&] { return session->altGlobalDownloadSpeedLimit(); });
    insertIfRequested(u"alt-speed-enabled"_s, [&] { return session->isAltGlobalSpeedLimitEnabled(); });
    insertIfRequested(u"alt-speed-time-begin"_s, [&] { return minutesAfterMidnight(pref->getSchedulerStartTime());});
    insertIfRequested(u"alt-speed-time-day"_s, [&] { return transmissionSchedulerDays(pref->getSchedulerDays()); });
    insertIfRequested(u"alt-speed-time-enabled"_s, [&] { return session->isBandwidthSchedulerEnabled(); });
    insertIfRequested(u"alt-speed-time-end"_s, [&] { return minutesAfterMidnight(pref->getSchedulerEndTime()); });
    insertIfRequested(u"alt-speed-up"_s, [&] { return session->altGlobalUploadSpeedLimit(); });
    insertIfRequested(u"blocklist-enabled"_s, [&] { return session->isIPFilteringEnabled(); });
    insertIfRequested(u"blocklist-url"_s, [&] { return session->IPFilterFile().toString(); });
    insertIfRequested(u"cache-size-mb"_s, [&] { return session->diskCacheSize() / 1024 / 1024; });
    insertIfRequested(u"config-dir"_s, [&] { return Profile::instance()->location(SpecialFolder::Config).toString(); });
    insertIfRequested(u"default-trackers"_s, [&] { return session->isAddTrackersEnabled() ? session->additionalTrackers() : QString{}; });
    insertIfRequested(u"dht-enabled"_s, [&] { return session->isDHTEnabled(); });
    insertIfRequested(u"download-dir"_s, [&] { return session->savePath().toString(); });
    insertIfRequested(u"download-dir-free-space"_s, [&] { return static_cast<qint64>(std::filesystem::space(session->savePath().toStdFsPath()).available); }); // TODO
    insertIfRequested(u"download-queue-enabled"_s, [&] { return session->isQueueingSystemEnabled(); });
    insertIfRequested(u"download-queue-size"_s, [&] { return session->maxActiveDownloads(); });
    insertIfRequested(u"encryption"_s, [&] {
        auto const strings = std::array{u"preferred"_s, u"required"_s, u"tolerated"_s};
        const int val = session->encryption();
        Q_ASSERT(val < static_cast<int>(strings.size()));
        return strings[val];});
    insertIfRequested(u"incomplete-dir-enabled"_s, [&] { return session->isDownloadPathEnabled(); });
    insertIfRequested(u"incomplete-dir"_s, [&] { return session->downloadPath().toString(); });
    insertIfRequested(u"lpd-enabled"_s, [&] { return session->isLSDEnabled(); });
    insertIfRequested(u"peer-limit-global"_s, [&] { return session->maxConnections(); });
    insertIfRequested(u"peer-limit-per-torrent"_s, [&] { return session->maxConnectionsPerTorrent(); });
    insertIfRequested(u"peer-port-random-on-start"_s, [&] { return false; }); // functionality deprecated in qbt
    insertIfRequested(u"peer-port"_s, [&] { return session->port(); });
    insertIfRequested(u"pex-enabled"_s, [&] { return session->isPeXEnabled(); });
    insertIfRequested(u"port-forwarding-enabled"_s, [&] { return Net::PortForwarder::instance()->isEnabled(); });
    insertIfRequested(u"rename-partial-files"_s, [&] { return session->isAppendExtensionEnabled(); });
    insertIfRequested(u"rpc-version-minimum"_s, [&] { return 18; });
    insertIfRequested(u"rpc-version-semver"_s, [&] { return u"5.4.0"_s; });
    insertIfRequested(u"rpc-version"_s, [&] { return 18; });
    insertIfRequested(u"script-torrent-added-enabled"_s, [&] { return pref->isAutoRunOnTorrentAddedEnabled(); });
    insertIfRequested(u"script-torrent-added-filename"_s, [&] { return pref->getAutoRunOnTorrentAddedProgram(); });
    insertIfRequested(u"script-torrent-done-enabled"_s, [&] { return pref->isAutoRunOnTorrentFinishedEnabled(); });
    insertIfRequested(u"script-torrent-done-filename"_s, [&] { return pref->getAutoRunOnTorrentFinishedProgram(); });
    insertIfRequested(u"seedRatioLimit"_s, [&] { return session->globalMaxRatio(); });
    insertIfRequested(u"seedRatioLimited"_s, [&] { return session->globalMaxRatio() >= 0.; });
    insertIfRequested(u"speed-limit-down-enabled"_s, [&] { return session->globalDownloadSpeedLimit() > 0; });
    insertIfRequested(u"speed-limit-down"_s, [&] { return session->globalDownloadSpeedLimit() / 1024; });
    insertIfRequested(u"speed-limit-up-enabled"_s, [&] { return session->globalUploadSpeedLimit() > 0; });
    insertIfRequested(u"speed-limit-up"_s, [&] { return session->globalUploadSpeedLimit() / 1024; });
    insertIfRequested(u"start-added-torrents"_s, [&] { return !session->isAddTorrentPaused(); });
    insertIfRequested(u"trash-original-torrent-files"_s, [&] { return TorrentFileGuard::autoDeleteMode() >= TorrentFileGuard::AutoDeleteMode::IfAdded; });
    insertIfRequested(u"units"_s, ::units);
    insertIfRequested(u"utp-enabled"_s, [&]{using btp = BitTorrent::SessionSettingsEnums::BTProtocol; const btp proto = session->btProtocol(); return proto == btp::Both || proto == btp::UTP; });
    insertIfRequested(u"version"_s, [&]{return u"qBitTorrent %1 with Qt %2, libtorrent %3"_s.arg(QStringLiteral(QBT_VERSION), QStringLiteral(QT_VERSION_STR), Utils::Misc::libtorrentVersionString());});

    insertIfRequested(u"blocklist-size"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"idle-seeding-limit-enabled"_s, [&] { return false; }); // TODO
    insertIfRequested(u"idle-seeding-limit"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"queue-stalled-enabled"_s, [&] { return false; }); // TODO
    insertIfRequested(u"queue-stalled-minutes"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"script-torrent-done-seeding-enabled"_s, [&] { return false; }); // not supported in qbt
    insertIfRequested(u"script-torrent-done-seeding-filename"_s, [&] { return QString{}; }); // not supported in qbt
    insertIfRequested(u"seed-queue-enabled"_s, [&] { return false; }); // TODO not supported?
    insertIfRequested(u"seed-queue-size"_s, [&] { return 0; });
    insertIfRequested(u"session-id"_s, [&] { return u"foobar"_s; }); // not applicable, maybe use cookie?
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
    std::filesystem::space_info space = std::filesystem::space(Path{path}.toStdFsPath());
    return {
        {u"path"_s, path},
        {u"size-bytes"_s, static_cast<qint64>(space.available)},
        {u"total_size"_s, static_cast<qint64>(space.capacity)}
    };
}
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
            methodResult = sessionGet(args);
        }
        else if (method == u"session-close"_s)
        {
            QTimer::singleShot(std::chrono::milliseconds(100), Qt::CoarseTimer, qApp, []
            {
                QCoreApplication::exit();
            });
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
