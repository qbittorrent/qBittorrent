/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  tehcneko
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

#include "pushcontroller.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

#include "base/addtorrentmanager.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/interfaces/iapplication.h"
#include "base/logger.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "apierror.h"
#include "webpush/webpush_utils.h"

const QString KEY_VAPID_PUBLIC_KEY = u"vapidPublicKey"_s;
const QString KEY_VAPID_PRIVATE_KEY = u"vapidPrivateKey"_s;
const QString KEY_SUBSCRIPTIONS = u"subscriptions"_s;
const QString KEY_ENDPOINT = u"endpoint"_s;
const QString KEY_KEYS = u"keys"_s;
const QString KEY_P256DH = u"p256dh"_s;
const QString KEY_AUTH = u"auth"_s;

const QString KEY_PAYLOAD = u"payload"_s;

const QString KEY_TORRENT_ID = u"torrent_id"_s;
const QString KEY_TORRENT_NAME = u"torrent_name"_s;
const QString KEY_TORRENT_CATEGORY = u"torrent_category"_s;
const QString KEY_TORRENT_TAGS = u"torrent_tags"_s;
const QString KEY_SOURCE = u"source"_s;
const QString KEY_REASON = u"reason"_s;
const QString KEY_MESSAGE = u"message"_s;

const QString EVENT_TEST = u"test"_s;
const QString EVENT_TORRENT_ADDED = u"torrent_added"_s;
const QString EVENT_TORRENT_FINISHED = u"torrent_finished"_s;
const QString EVENT_FULL_DISK_ERROR = u"full_disk_error"_s;
const QString EVENT_ADD_TORRENT_FAILED = u"add_torrent_failed"_s;

const QString PUSH_CONFIG_FILE_NAME = u"web_push.json"_s;

PushController::PushController(IApplication *app, QObject *parent)
    : APIController(app, parent)
    , m_networkManager {new QNetworkAccessManager(this)}
    , m_registeredSubscriptions {QList<PushSubscription>()}
{
    m_configFilePath = specialFolderLocation(SpecialFolder::Config) / Path(PUSH_CONFIG_FILE_NAME);
    if (m_configFilePath.exists())
    {
        const auto readResult = Utils::IO::readFile(m_configFilePath, -1);
        if (!readResult)
        {
            LogMsg(tr("Failed to load push config. %1").arg(readResult.error().message), Log::WARNING);
            return;
        }

        QJsonParseError jsonError;
        const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
        {
            LogMsg(tr("Failed to parse push config. File: \"%1\". Error: \"%2\"")
                .arg(m_configFilePath.toString(), jsonError.errorString()), Log::WARNING);
            return;
        }

        if (!jsonDoc.isObject())
        {
            LogMsg(tr("Failed to load push config. File: \"%1\". Error: \"Invalid data format\"")
                .arg(m_configFilePath.toString()), Log::WARNING);
            return;
        }

        const auto jsonObject = jsonDoc.object();

        m_vapidPrivateKey = createPrivateKeyFromPemString(jsonObject.value(KEY_VAPID_PRIVATE_KEY).toString());
        m_vapidPublicKeyString = getVapidPublicKeyString(m_vapidPrivateKey);

        const auto subscriptionsArray = jsonObject.value(KEY_SUBSCRIPTIONS).toArray();
        for (const auto& subscription : subscriptionsArray)
        {
            PushSubscription sub = PushSubscription::fromJson(subscription.toObject());
            m_registeredSubscriptions.append(sub);
        }
    }
    else
    {
        // Generate new VAPID keypair and save if the file does not exist
        const auto privateKey = generateECDHKeypair();
        m_vapidPublicKeyString = getVapidPublicKeyString(privateKey);
        m_vapidPrivateKey = privateKey;

        saveSubscriptions();
    }

    // Apply proxy settings
    connect(Net::ProxyConfigurationManager::instance(), &Net::ProxyConfigurationManager::proxyConfigurationChanged
        , this, &PushController::applyProxySettings);
    connect(Preferences::instance(), &Preferences::changed, this, &PushController::applyProxySettings);
    applyProxySettings();

    // Connect to signals
    const auto *btSession = BitTorrent::Session::instance();
    connect(btSession, &BitTorrent::Session::fullDiskError, this
        , [this](const BitTorrent::Torrent *torrent, const QString& msg)
        {
            QJsonObject payload;
            payload[KEY_TORRENT_ID] = torrent->id().toString();
            payload[KEY_TORRENT_NAME] = torrent->name();
            payload[KEY_TORRENT_CATEGORY] = torrent->category();
            payload[KEY_TORRENT_TAGS] = Utils::String::joinIntoString(torrent->tags(), u","_s);
            payload[KEY_MESSAGE] = msg;
            sendPushNotification(EVENT_FULL_DISK_ERROR, payload);
        });
    connect(btSession, &BitTorrent::Session::torrentFinished, this
        , [this](const BitTorrent::Torrent *torrent)
        {
            QJsonObject payload;
            payload[KEY_TORRENT_ID] = torrent->id().toString();
            payload[KEY_TORRENT_NAME] = torrent->name();
            payload[KEY_TORRENT_CATEGORY] = torrent->category();
            payload[KEY_TORRENT_TAGS] = Utils::String::joinIntoString(torrent->tags(), u","_s);
            sendPushNotification(EVENT_TORRENT_FINISHED, payload);
        });
    const auto *addTorrentManager = app->addTorrentManager();
    connect(addTorrentManager, &AddTorrentManager::torrentAdded, this
        , [this]([[maybe_unused]] const QString& source, const BitTorrent::Torrent *torrent)
        {

            QJsonObject payload;
            payload[KEY_TORRENT_ID] = torrent->id().toString();
            payload[KEY_TORRENT_NAME] = torrent->name();
            payload[KEY_TORRENT_CATEGORY] = torrent->category();
            payload[KEY_TORRENT_TAGS] = Utils::String::joinIntoString(torrent->tags(), u","_s);
            payload[KEY_SOURCE] = source;
            sendPushNotification(EVENT_TORRENT_ADDED, payload);
        });
    connect(addTorrentManager, &AddTorrentManager::addTorrentFailed, this
        , [this](const QString& source, const BitTorrent::AddTorrentError& reason)
        {
            QJsonObject payload;
            payload[KEY_SOURCE] = source;
            payload[KEY_REASON] = reason.message;
            sendPushNotification(EVENT_ADD_TORRENT_FAILED, payload);
        });
}

void PushController::applyProxySettings()
{
    const auto *proxyManager = Net::ProxyConfigurationManager::instance();
    const Net::ProxyConfiguration proxyConfig = proxyManager->proxyConfiguration();

    QNetworkProxy proxy;

    switch (proxyConfig.type)
    {
    case Net::ProxyType::None:
    case Net::ProxyType::SOCKS4:
        proxy = QNetworkProxy(QNetworkProxy::NoProxy);
        break;

    case Net::ProxyType::HTTP:
        proxy = QNetworkProxy(
            QNetworkProxy::HttpProxy,
            proxyConfig.ip,
            proxyConfig.port,
            proxyConfig.authEnabled ? proxyConfig.username : QString(),
            proxyConfig.authEnabled ? proxyConfig.password : QString());
        proxy.setCapabilities(proxyConfig.hostnameLookupEnabled
            ? (proxy.capabilities() | QNetworkProxy::HostNameLookupCapability)
            : (proxy.capabilities() & ~QNetworkProxy::HostNameLookupCapability));
        break;

    case Net::ProxyType::SOCKS5:
        proxy = QNetworkProxy(
            QNetworkProxy::Socks5Proxy,
            proxyConfig.ip,
            proxyConfig.port,
            proxyConfig.authEnabled ? proxyConfig.username : QString(),
            proxyConfig.authEnabled ? proxyConfig.password : QString());
        proxy.setCapabilities(proxyConfig.hostnameLookupEnabled
            ? (proxy.capabilities() | QNetworkProxy::HostNameLookupCapability)
            : (proxy.capabilities() & ~QNetworkProxy::HostNameLookupCapability));
        break;
    }

    m_networkManager->setProxy(proxy);
}

void PushController::subscribeAction()
{
    requireParams({u"subscription"_s});
    QJsonParseError jsonError;
    const auto subscriptionJsonDocument = QJsonDocument::fromJson(params()[u"subscription"_s].toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
        throw APIError(APIErrorType::BadParams, jsonError.errorString());
    if (!subscriptionJsonDocument.isObject())
        throw APIError(APIErrorType::BadParams, tr("subscription must be an object"));

    const auto newSubscription = PushSubscription::fromJson(subscriptionJsonDocument.object());

    if (newSubscription.endpoint.isEmpty() || newSubscription.p256dh.isEmpty() || newSubscription.auth.isEmpty())
        throw APIError(APIErrorType::BadParams, tr("invalid subscription"));

    if (m_registeredSubscriptions.contains(newSubscription))
        throw APIError(APIErrorType::BadData, tr("duplicated subscription"));

    m_registeredSubscriptions.append(newSubscription);

    saveSubscriptions();

    setResult(QString());
}

void PushController::subscriptionsAction()
{
    QJsonArray subscriptionsArray;
    for (const auto& sub : m_registeredSubscriptions)
    {
        subscriptionsArray.append(sub.toJson());
    }
    QJsonObject jsonObject;
    jsonObject[KEY_SUBSCRIPTIONS] = subscriptionsArray;
    setResult(jsonObject);
}

void PushController::testAction()
{
    sendPushNotification(EVENT_TEST, {});

    setResult(QString());
}

void PushController::unsubscribeAction()
{
    requireParams({u"endpoint"_s});

    const auto endpoint = params()[u"endpoint"_s];

    auto subscription = std::find_if(m_registeredSubscriptions.begin(), m_registeredSubscriptions.end(),
        [&](const PushSubscription& sub) { return sub.endpoint == endpoint; });

    if (subscription == m_registeredSubscriptions.end())
        throw APIError(APIErrorType::NotFound, tr("subscription not found"));

    m_registeredSubscriptions.erase(subscription);

    saveSubscriptions();

    setResult(QString());
}

void PushController::vapidPublicKeyAction()
{
    QJsonObject jsonObject;
    jsonObject[KEY_VAPID_PUBLIC_KEY] = m_vapidPublicKeyString;
    setResult(jsonObject);
}

void PushController::saveSubscriptions()
{
    QJsonObject jsonObject;
    jsonObject[KEY_VAPID_PRIVATE_KEY] = savePrivateKeyToPemString(m_vapidPrivateKey);
    QJsonArray jsonArray;
    for (const auto& sub : m_registeredSubscriptions)
    {
        jsonArray.append(sub.toJson());
    }
    jsonObject[KEY_SUBSCRIPTIONS] = jsonArray;
    const QByteArray data = QJsonDocument(jsonObject).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(m_configFilePath, data);
    if (!result)
    {
        LogMsg(tr("Failed to save push registration subscriptions. Error: \"%1\"").arg(result.error()), Log::WARNING);
    }
}

void PushController::sendPushNotification(const QString event, const QJsonObject& payload)
{
    QJsonObject jsonPayload;
    jsonPayload[u"event"_s] = event;
    jsonPayload[KEY_PAYLOAD] = payload;
    const QByteArray payloadData = QJsonDocument(jsonPayload).toJson(QJsonDocument::Compact);
    // Absent header (86 octets), padding(minimum 1 octet), and expansion for AEAD_AES_128_GCM(16 octets),
    // this equates to, at most, 3993 octetsof plaintext.
    // https://datatracker.ietf.org/doc/html/rfc8291#section-4
    if (payloadData.size() >= 3993)
    {
        LogMsg(tr("Push notification payload is too large (%1 bytes).").arg(payloadData.size()), Log::WARNING);
        return;
    }
    for (const auto& subscription : m_registeredSubscriptions)
    {
        sendWebPushNotificationToSubscription(subscription, payloadData);
    }
}

void PushController::sendWebPushNotificationToSubscription(const PushController::PushSubscription& subscription, const QByteArray& payload)
{

    QNetworkRequest request(QUrl(subscription.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/octet-stream"_s);
    request.setRawHeader("Content-Encoding", "aes128gcm");
    request.setRawHeader("TTL", "3600");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qBittorrent/" QBT_VERSION_2));

    const auto vapidJWT = createVapidJWT(m_vapidPrivateKey, subscription.endpoint);
    request.setRawHeader("Authorization", "vapid t=" + vapidJWT.toLatin1() + ",k=" + m_vapidPublicKeyString.toLatin1());

    const auto [encryptedPayload, p256ecdsa] = buildWebPushPayload(subscription.p256dh, subscription.auth, payload);
    if (encryptedPayload.isEmpty())
    {
        LogMsg(tr("Failed to build web push payload for subscription: %1").arg(subscription.endpoint), Log::CRITICAL);
        return;
    }
    request.setRawHeader("Crypto-Key", "p256ecdsa=" + p256ecdsa);

    const auto reply = m_networkManager->post(request, encryptedPayload);

    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        if (reply->error() != QNetworkReply::NoError)
        {
            LogMsg(tr("Failed to send web push notification: %1").arg(reply->errorString()), Log::CRITICAL);
        }
        reply->deleteLater();
    });
}

QJsonObject PushController::PushSubscription::toJson() const
{
    QJsonObject jsonObject;
    jsonObject[KEY_ENDPOINT] = endpoint;

    if (!p256dh.isEmpty() && !auth.isEmpty())
    {
        QJsonObject keys;
        keys[KEY_P256DH] = p256dh;
        keys[KEY_AUTH] = auth;
        jsonObject[KEY_KEYS] = keys;
    }

    return jsonObject;
}

PushController::PushSubscription PushController::PushSubscription::fromJson(const QJsonObject& jsonObj)
{
    PushSubscription subscription;

    subscription.endpoint = jsonObj.value(KEY_ENDPOINT).toString();

    if (jsonObj.contains(KEY_KEYS) && jsonObj[KEY_KEYS].isObject())
    {
        const auto keys = jsonObj[KEY_KEYS].toObject();
        subscription.p256dh = keys.value(KEY_P256DH).toString();
        subscription.auth = keys.value(KEY_AUTH).toString();
    }

    return subscription;
}

bool PushController::PushSubscription::operator==(const PushSubscription& other) const
{
    return endpoint == other.endpoint;
}
