#include "transmissionrpccontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/bittorrent/session.h"
#include "base/profile.h"
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

QJsonObject sessionGet(const QJsonObject &args)
{
    QJsonObject result{};
    const QJsonValue fields = args[u"fields"_s];
    if (!fields.isNull() && !fields.isArray())
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
    const BitTorrent::Session *session = BitTorrent::Session::instance();

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
        result[u"arguments"_s] = methodResult;
        result[u"result"_s] = u"success"_s;
    }
    catch(const TransmissionAPIError &err)
    {
        result[u"result"_s] = err.message();
    }

    setResult(result);
}
