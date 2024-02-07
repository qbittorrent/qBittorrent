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

QJsonValue torrentGetSingleField(const BitTorrent::Torrent &, const QString &)
{
    // TODO
    return QJsonValue{};
}

QJsonObject torrentGetObject(const BitTorrent::Torrent &tor, const QSet<QString> &fields)
{
    QJsonObject result{};
    for(const QString &f : fields)
    {
        result[f] = torrentGetSingleField(tor, f);
    }
    return result;
}

QJsonArray torrentGetTable(const BitTorrent::Torrent &tor, const QSet<QString> &fields)
{
    QJsonArray result{};
    for(const QString &f : fields)
    {
        result.push_back(torrentGetSingleField(tor, f));
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

QJsonObject sessionGet(const QJsonObject &args)
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
    } func_args = {
        BitTorrent::Session::instance(),
        Preferences::instance()
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
    {u"peer-port-random-on-start"_s, [](const auto &) -> QJsonValue { return false; }}, // functionality deprecated in qbt
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
    {u"version"_s, [](const auto &) -> QJsonValue{return u"qBitTorrent %1 with Qt %2, libtorrent %3"_s.arg(QStringLiteral(QBT_VERSION), QStringLiteral(QT_VERSION_STR), Utils::Misc::libtorrentVersionString());}},

    {u"blocklist-size"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    {u"idle-seeding-limit-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // TODO
    {u"idle-seeding-limit"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    {u"queue-stalled-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // TODO
    {u"queue-stalled-minutes"_s, [](const auto&) -> QJsonValue { return 0; }}, // TODO
    {u"script-torrent-done-seeding-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // not supported in qbt
    {u"script-torrent-done-seeding-filename"_s, [](const auto&) -> QJsonValue { return QString{}; }}, // not supported in qbt
    {u"seed-queue-enabled"_s, [](const auto&) -> QJsonValue { return false; }}, // TODO not supported?
    {u"seed-queue-size"_s, [](const auto&) -> QJsonValue { return 0; }},
    {u"session-id"_s, [](const auto&) -> QJsonValue { return u"foobar"_s; }}, // not applicable, maybe use cookie?
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

TransmissionRPCController::TransmissionRPCController(IApplication *app, QObject *parent)
    : APIController(app, parent)
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
            methodResult = sessionGet(args);
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
    QVector<BitTorrent::Torrent*> requestedTorrents = collectTorrentsForRequest(args[u"ids"_s]);
    QJsonArray torrents;
    if (format == TorrentGetFormat::Object)
    {
        for (BitTorrent::Torrent *t : requestedTorrents)
        {
            torrents.push_back(torrentGetObject(*t, fieldsSet));
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
        for (BitTorrent::Torrent *t : requestedTorrents)
        {
            torrents.push_back(torrentGetTable(*t, fieldsSet));
        }
    }

    return {{u"torrents"_s, torrents}};
}

QVector<BitTorrent::Torrent*> TransmissionRPCController::collectTorrentsForRequest(const QJsonValue& ids) const
{
    if (ids.isUndefined())
    {
        return BitTorrent::Session::instance()->torrents();
    }
    else if (ids.isArray())
    {
        const QJsonArray idsArray = ids.toArray();
        QVector<BitTorrent::Torrent*> rv{};
        rv.reserve(idsArray.size());
        for (const QJsonValueConstRef &idVal : idsArray)
        {
            if (idVal.isDouble())
            {
                rv.push_back(m_idToTorrent[idVal.toInt()]);
            }
        }
        return rv;
    }
    else if (ids.toString() == u"recently-active"_s)
    {
        // FIXME
        return {};
    }
    else if (ids.isDouble())
    {
        QVector<BitTorrent::Torrent*> rv{};
        rv.push_back(m_idToTorrent[ids.toInt()]);
        return rv;
    }
    else
    {
        throw TransmissionAPIError(APIErrorType::BadParams, u"Unknown type for 'ids'"_s);
    }
}
