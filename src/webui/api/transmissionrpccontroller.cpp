#include "transmissionrpccontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    insertIfRequested(u"alt-speed-time-begin"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"alt-speed-time-day"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"alt-speed-time-enabled"_s, [&] { return session->isBandwidthSchedulerEnabled(); });
    insertIfRequested(u"alt-speed-time-end"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"alt-speed-up"_s, [&] { return session->altGlobalUploadSpeedLimit(); });
    insertIfRequested(u"blocklist-enabled"_s, [&] { return session->isIPFilteringEnabled(); });
    insertIfRequested(u"blocklist-size"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"blocklist-url"_s, [&] { return session->IPFilterFile().toString(); });
    insertIfRequested(u"cache-size-mb"_s, [&] { return session->diskCacheSize() / 1024 / 1024; });
    insertIfRequested(u"config-dir"_s, [&] { return Profile::instance()->location(SpecialFolder::Config).toString(); });
    insertIfRequested(u"default-trackers"_s, [&] { return QString{}; }); // TODO
    insertIfRequested(u"dht-enabled"_s, [&] { return session->isDHTEnabled(); });
    insertIfRequested(u"download-dir"_s, [&] { return session->savePath().toString(); });
    insertIfRequested(u"download-dir-free-space"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"download-queue-enabled"_s, [&] { return session->isQueueingSystemEnabled(); });
    insertIfRequested(u"download-queue-size"_s, [&] { return session->maxActiveDownloads(); });
    insertIfRequested(u"encryption"_s, [&] {
        auto const strings = std::array{u"preferred"_s, u"required"_s, u"tolerated"_s};
        const int val = session->encryption();
        Q_ASSERT(val < static_cast<int>(strings.size()));
        return strings[val];});
    insertIfRequested(u"idle-seeding-limit-enabled"_s, [&] { return false; }); // TODO
    insertIfRequested(u"idle-seeding-limit"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"incomplete-dir-enabled"_s, [&] { return session->isDownloadPathEnabled(); });
    insertIfRequested(u"incomplete-dir"_s, [&] { return session->downloadPath().toString(); });
    insertIfRequested(u"lpd-enabled"_s, [&] { return session->isLSDEnabled(); });
    insertIfRequested(u"peer-limit-global"_s, [&] { return session->maxConnections(); });
    insertIfRequested(u"peer-limit-per-torrent"_s, [&] { return session->maxConnectionsPerTorrent(); });
    insertIfRequested(u"peer-port-random-on-start"_s, [&] { return false; }); // functionality deprecated in qbt
    insertIfRequested(u"peer-port"_s, [&] { return session->port(); });
    insertIfRequested(u"pex-enabled"_s, [&] { return session->isPeXEnabled(); });
    insertIfRequested(u"port-forwarding-enabled"_s, [&] { return Net::PortForwarder::instance()->isEnabled(); });
    insertIfRequested(u"queue-stalled-enabled"_s, [&] { return false; }); // TODO
    insertIfRequested(u"queue-stalled-minutes"_s, [&] { return 0; }); // TODO
    insertIfRequested(u"rename-partial-files"_s, [&] { return session->isAppendExtensionEnabled(); });
    insertIfRequested(u"rpc-version-minimum"_s, [&] { return 18; });
    insertIfRequested(u"rpc-version-semver"_s, [&] { return u"5.4.0"_s; });
    insertIfRequested(u"rpc-version"_s, [&] { return 18; });
    insertIfRequested(u"script-torrent-added-enabled"_s, [&] { return pref->isAutoRunOnTorrentAddedEnabled(); });
    insertIfRequested(u"script-torrent-added-filename"_s, [&] { return pref->getAutoRunOnTorrentAddedProgram(); });
    insertIfRequested(u"script-torrent-done-enabled"_s, [&] { return pref->isAutoRunOnTorrentFinishedEnabled(); });
    insertIfRequested(u"script-torrent-done-filename"_s, [&] { return pref->getAutoRunOnTorrentFinishedProgram(); });
    insertIfRequested(u"script-torrent-done-seeding-enabled"_s, [&] { return false; }); // not supported in qbt
    insertIfRequested(u"script-torrent-done-seeding-filename"_s, [&] { return QString{}; }); // not supported in qbt
    insertIfRequested(u"seed-queue-enabled"_s, [&] { return false; }); // TODO not supported?
    insertIfRequested(u"seed-queue-size"_s, [&] { return 0; });
    insertIfRequested(u"seedRatioLimit"_s, [&] { return session->globalMaxRatio(); });
    insertIfRequested(u"seedRatioLimited"_s, [&] { return session->globalMaxRatio() >= 0.; });
    insertIfRequested(u"session-id"_s, [&] { return u"foobar"_s; }); // not applicable
    insertIfRequested(u"speed-limit-down-enabled"_s, [&] { return session->globalDownloadSpeedLimit() > 0; });
    insertIfRequested(u"speed-limit-down"_s, [&] { return session->globalDownloadSpeedLimit(); });
    insertIfRequested(u"speed-limit-up-enabled"_s, [&] { return session->globalUploadSpeedLimit() > 0; });
    insertIfRequested(u"speed-limit-up"_s, [&] { return session->globalUploadSpeedLimit() > 0; });
    insertIfRequested(u"start-added-torrents"_s, [&] { return !session->isAddTorrentPaused(); });
    insertIfRequested(u"trash-original-torrent-files"_s, [&] { return TorrentFileGuard::autoDeleteMode() >= TorrentFileGuard::AutoDeleteMode::IfAdded; });
    insertIfRequested(u"units"_s, [&] { return QJsonObject{}; });
    insertIfRequested(u"utp-enabled"_s, [&]{return false;}); // not supported in qbt
    insertIfRequested(u"version"_s, [&]{return u"qBitTorrent %1 with Qt %2, libtorrent %3"_s.arg(QStringLiteral(QBT_VERSION), QStringLiteral(QT_VERSION_STR), Utils::Misc::libtorrentVersionString());});

    return result;
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
