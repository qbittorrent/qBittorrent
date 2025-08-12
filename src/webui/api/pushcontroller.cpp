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

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>

#include "base/addtorrentmanager.h"
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

const QString KEY_VAPID_PUBLIC_KEY = u"vapidPublicKey"_s;
const QString KEY_VAPID_PRIVATE_KEY = u"vapidPrivateKey"_s;
const QString KEY_SUBSCRIPTIONS = u"subscriptions"_s;
const QString KEY_ENDPOINT = u"endpoint"_s;
const QString KEY_KEYS = u"keys"_s;
const QString KEY_P256DH = u"p256dh"_s;
const QString KEY_AUTH = u"auth"_s;

const QString KEY_EVENT = u"event"_s;
const QString KEY_PAYLOAD = u"payload"_s;

const QString KEY_TORRENT_NAME = u"torrent_name"_s;
const QString KEY_SOURCE = u"source"_s;
const QString KEY_REASON = u"reason"_s;
const QString KEY_MESSAGE = u"message"_s;

const QString EVENT_TEST = u"test"_s;
const QString EVENT_TORRENT_ADDED = u"torrent_added"_s;
const QString EVENT_TORRENT_FINISHED = u"torrent_finished"_s;
const QString EVENT_FULL_DISK_ERROR = u"full_disk_error"_s;
const QString EVENT_ADD_TORRENT_FAILED = u"add_torrent_failed"_s;

const QString PUSH_CONFIG_FILE_NAME = u"web_push.json"_s;

namespace
{
    QByteArray base64UrlDecode(const QByteArray& data)
    {
        return QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding);
    }

    QByteArray base64UrlEncode(const QByteArray& data)
    {
        return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    }

    QByteArray generateSalt()
    {
        QByteArray salt(16, 0);
        RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), salt.size());
        return salt;
    }

    QByteArray getECPublicOctets(EVP_PKEY *key)
    {
        size_t outLen = 0;
        if (EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &outLen) <= 0)
        {
            return {};
        }
        QByteArray out(outLen, 0);
        if (EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY,
            reinterpret_cast<unsigned char*>(out.data()), outLen, &outLen) <= 0)
        {
            return {};
        }
        return out;
    }

    EVP_PKEY *createPublicKeyFromBytes(const QByteArray& publicKeyBytes)
    {
        EVP_PKEY *pkey = nullptr;
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        if (!ctx)
            return nullptr;

        if (EVP_PKEY_keygen_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        EVP_PKEY_CTX_free(ctx);

        OSSL_PARAM params[2];
        params[0] = OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PUB_KEY,
            (void*)publicKeyBytes.data(),
            publicKeyBytes.size()
        );
        params[1] = OSSL_PARAM_construct_end();

        if (EVP_PKEY_set_params(pkey, params) <= 0)
        {
            EVP_PKEY_free(pkey);
            return nullptr;
        }

        return pkey;
    }

    QByteArray computeECDHSecret(EVP_PKEY *senderPrivateKey, EVP_PKEY *receiverPublicKey)
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(senderPrivateKey, nullptr);
        if (!ctx)
            return {};

        if (EVP_PKEY_derive_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_derive_set_peer(ctx, receiverPublicKey) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        size_t outLen = EVP_MAX_MD_SIZE;
        if (EVP_PKEY_derive(ctx, nullptr, &outLen) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        QByteArray out(outLen, 0);
        if (EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        out.resize(outLen);

        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    QByteArray hkdfExtract(const QByteArray& salt, const QByteArray& ikm)
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
        if (!ctx)
            return {};

        if (EVP_PKEY_derive_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set_hkdf_mode(ctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set1_hkdf_key(ctx, reinterpret_cast<const unsigned char*>(ikm.constData()), ikm.size()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set1_hkdf_salt(ctx, reinterpret_cast<const unsigned char*>(salt.constData()), salt.size()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        size_t outLen = EVP_MAX_MD_SIZE;
        if (EVP_PKEY_derive(ctx, nullptr, &outLen) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        QByteArray out(outLen, 0);
        if (EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    QByteArray hkdfExpand(const QByteArray& prk, const QByteArray& info, size_t length)
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
        if (!ctx)
            return {};

        if (EVP_PKEY_derive_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set_hkdf_mode(ctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set1_hkdf_key(ctx, reinterpret_cast<const unsigned char*>(prk.constData()), prk.size()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_add1_hkdf_info(ctx, reinterpret_cast<const unsigned char*>(info.constData()), info.size()) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        size_t outLen = EVP_MAX_MD_SIZE;
        QByteArray out(outLen, 0);
        if (EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        out.resize(length);

        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    // Derive keys for WebPush. Returns CEK (16), NONCE (12)
    // https://datatracker.ietf.org/doc/html/rfc8291#section-4
    QPair<QByteArray, QByteArray> deriveWebPushKeys(const QByteArray& salt,
        EVP_PKEY *senderPrivateKey, const QByteArray& senderPublicKeyOctets,
        EVP_PKEY *receiverPublicKey, const QByteArray& authSecret
    )
    {
        // ecdh_secret = ECDH(as_private, ua_public)
        const auto ecdhSecret = computeECDHSecret(senderPrivateKey, receiverPublicKey);

        // PRK_key = HKDF-Extract(salt=auth_secret, IKM=ecdh_secret)
        const auto prkKey = hkdfExtract(authSecret, ecdhSecret);

        const auto receiverPublicKeyOctets = getECPublicOctets(receiverPublicKey);

        // IKM = HKDF-Expand(PRK_key, key_info, L_key=32)
        auto keyInfo = QByteArray("WebPush: info");
        keyInfo.append('\0');
        keyInfo.append(receiverPublicKeyOctets);
        keyInfo.append(senderPublicKeyOctets);
        const auto ikm = hkdfExpand(prkKey, keyInfo, 32);

        // PRK = HKDF-Extract(salt, IKM)
        const auto prk = hkdfExtract(salt, ikm);

        // CEK = HKDF-Expand(PRK, "Content-Encoding: aes128gcm" || 0x00, L=16)
        const auto cekInfo = QByteArray("Content-Encoding: aes128gcm").append('\0');
        const auto cek = hkdfExpand(prk, cekInfo, 16);

        // NONCE = HKDF-Expand(PRK, "Content-Encoding: nonce" || 0x00, L=12)
        const auto nonceInfo = QByteArray("Content-Encoding: nonce").append('\0');
        const auto nonce = hkdfExpand(prk, nonceInfo, 12);

        return {cek, nonce};
    }

    QByteArray aes128gcmEncrypt(const QByteArray& cek, const QByteArray& nonce, const QByteArray& plaintext)
    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return {};

        int len;
        int ciphertextLen;
        QByteArray ciphertext(plaintext.size(), 0);
        QByteArray tag(16, 0);

        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr,
            reinterpret_cast<const unsigned char*>(cek.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        if (EVP_EncryptUpdate(ctx,
            reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
            reinterpret_cast<const unsigned char*>(plaintext.constData()), plaintext.size()) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        ciphertextLen = len;
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        ciphertextLen += len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }

        EVP_CIPHER_CTX_free(ctx);
        return ciphertext + tag;
    }

    EVP_PKEY *generateVapidKeypair()
    {
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        if (!ctx)
            return {};
        if (EVP_PKEY_keygen_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }

        EVP_PKEY *pkey = nullptr;
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return {};
        }
        EVP_PKEY_CTX_free(ctx);

        return pkey;
    }

    EVP_PKEY *createPrivateKeyFromPemString(const QString& pemString)
    {
        const auto pemBytes = pemString.toLatin1();
        BIO *bio = BIO_new_mem_buf(pemBytes.constData(), pemBytes.size());
        if (!bio)
        {
            return nullptr;
        }

        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        if (!pkey)
        {
            return nullptr;
        }

        return pkey;
    }

    QString savePrivateKeyToPemString(EVP_PKEY *pkey)
    {
        BIO *bio = BIO_new(BIO_s_mem());
        if (!bio)
            return {};

        if (!PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr))
        {
            BIO_free(bio);
            return {};
        }

        BUF_MEM *bptr = nullptr;
        BIO_get_mem_ptr(bio, &bptr);
        if (!bptr || !bptr->data)
        {
            BIO_free(bio);
            return {};
        }

        QString pemStr = QString::fromLatin1(bptr->data, bptr->length);
        BIO_free(bio);
        return pemStr;
    }

    QString getAudienceFromEndpoint(const QString& endpoint)
    {
        QUrl url(endpoint);
        QString audience = url.scheme() + u"://"_s + url.host();
        if (url.port() != -1)
        {
            audience += u":"_s + QString::number(url.port());
        }

        return audience;
    }

    QByteArray derSigToRaw(const QByteArray& derSig)
    {
        const unsigned char *ptr = reinterpret_cast<const unsigned char*>(derSig.constData());
        ECDSA_SIG *sig = d2i_ECDSA_SIG(nullptr, &ptr, derSig.size());
        if (!sig)
            return {};

        const BIGNUM *r;
        const BIGNUM *s;
        ECDSA_SIG_get0(sig, &r, &s);

        QByteArray rawSig;
        rawSig.resize(64);
        BN_bn2binpad(r, reinterpret_cast<unsigned char*>(rawSig.data()), 32);
        BN_bn2binpad(s, reinterpret_cast<unsigned char*>(rawSig.data() + 32), 32);

        ECDSA_SIG_free(sig);
        return rawSig;
    }

    QByteArray ecdsaSign(EVP_PKEY *pkey, const QByteArray& data)
    {
        QByteArray signature;
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
            return {};

        if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) <= 0)
        {
            EVP_MD_CTX_free(ctx);
            return {};
        }
        if (EVP_DigestSignUpdate(ctx, data.constData(), data.size()) <= 0)
        {
            EVP_MD_CTX_free(ctx);
            return {};
        }
        size_t sigLen = 0;
        if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) <= 0)
        {
            EVP_MD_CTX_free(ctx);
            return {};
        }
        signature.resize(sigLen);
        if (EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(signature.data()), &sigLen) <= 0)
        {
            EVP_MD_CTX_free(ctx);
            return {};
        }
        EVP_MD_CTX_free(ctx);
        signature.resize(sigLen);
        return derSigToRaw(signature);
    }

    // https://datatracker.ietf.org/doc/html/rfc8292#section-2
    QString createVapidJWT(EVP_PKEY *privateKey, const QString& audience)
    {
        const auto now = QDateTime::currentSecsSinceEpoch();
        const QJsonObject header{{u"alg"_s, u"ES256"_s}, {u"typ"_s, u"JWT"_s}};
        const QJsonObject payload
        {
            {u"aud"_s, audience},
            // Limiting this to 24 hours balances the need for reuse
            // against the potential cost and likelihood of theft of a valid token.
            {u"exp"_s, now + 60 * 60 * 24}, // 24 hours
            {u"sub"_s, u"https://qbittorrent.org"_s}
        };

        const auto headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
        const auto payloadJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QByteArray signingInput = base64UrlEncode(headerJson) + "." + base64UrlEncode(payloadJson);
        QByteArray signature = base64UrlEncode(ecdsaSign(privateKey, signingInput));

        return QString::fromLatin1(signingInput + "." + signature);
    }
}

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
        m_vapidPublicKeyOctets = getECPublicOctets(m_vapidPrivateKey);

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
        const auto privateKey = generateVapidKeypair();
        m_vapidPublicKeyOctets = getECPublicOctets(privateKey);
        m_vapidPrivateKey = privateKey;

        saveSubscriptions();
    }

    connect(Net::ProxyConfigurationManager::instance(), &Net::ProxyConfigurationManager::proxyConfigurationChanged
        , this, &PushController::applyProxySettings);
    connect(Preferences::instance(), &Preferences::changed, this, &PushController::applyProxySettings);
    applyProxySettings();

    const auto *btSession = BitTorrent::Session::instance();
    connect(btSession, &BitTorrent::Session::fullDiskError, this
        , [this](const BitTorrent::Torrent *torrent, const QString& msg)
        {
            QJsonObject payload;
            payload[KEY_TORRENT_NAME] = torrent->name();
            payload[KEY_MESSAGE] = msg;
            sendPushNotification(EVENT_FULL_DISK_ERROR, payload);
        });
    connect(btSession, &BitTorrent::Session::torrentFinished, this
        , [this](const BitTorrent::Torrent *torrent)
        {
            QJsonObject payload;
            payload[KEY_TORRENT_NAME] = torrent->name();
            sendPushNotification(EVENT_TORRENT_FINISHED, payload);
        });
    const auto *addTorrentManager = app->addTorrentManager();
    connect(addTorrentManager, &AddTorrentManager::torrentAdded, this
        , [this]([[maybe_unused]] const QString& source, const BitTorrent::Torrent *torrent)
        {

            QJsonObject payload;
            payload[KEY_TORRENT_NAME] = torrent->name();
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
    jsonObject[KEY_VAPID_PUBLIC_KEY] = QString::fromLatin1(base64UrlEncode(m_vapidPublicKeyOctets));
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
    jsonPayload[KEY_EVENT] = event;
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

    const auto senderPublicKeyBase64 = base64UrlEncode(m_vapidPublicKeyOctets);
    const auto audience = getAudienceFromEndpoint(subscription.endpoint);
    const auto vapidJWT = createVapidJWT(m_vapidPrivateKey, audience);
    request.setRawHeader("Authorization", "vapid t=" + vapidJWT.toLatin1() + ",k=" + senderPublicKeyBase64);
    request.setRawHeader("Crypto-Key", "p256ecdsa=" + senderPublicKeyBase64);

    const auto salt = generateSalt();
    const auto receiverPublicKey = createPublicKeyFromBytes(base64UrlDecode(subscription.p256dh.toLatin1()));
    const auto authSecret = base64UrlDecode(subscription.auth.toLatin1());
    if (!receiverPublicKey)
    {
        LogMsg(tr("Failed to create public key from subscription p256dh for endpoint: ")
            .arg(subscription.endpoint), Log::CRITICAL);
        return;
    }
    const auto [cek, nonce] = deriveWebPushKeys(salt, m_vapidPrivateKey, m_vapidPublicKeyOctets, receiverPublicKey, authSecret);
    if (cek.isEmpty() || nonce.isEmpty())
    {
        LogMsg(tr("Failed to derive keys for web push notification for endpoint: ")
            .arg(subscription.endpoint), Log::CRITICAL);
        return;
    }

    // Build header (salt || record size || pubKeyLen || pubKey)
    auto header = QByteArray(salt);
    header.append("\x00\x00\x10\x00", 4); // Use 4096 as record size
    const auto idLen = static_cast<quint8>(m_vapidPublicKeyOctets.size());
    header.append(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
    header.append(m_vapidPublicKeyOctets);

    // Append padding delimiter octet (0x02) to the payload and encrypt
    const auto encryptedPayload = aes128gcmEncrypt(cek, nonce, payload + "\x02");

    const auto reply = m_networkManager->post(request, header + encryptedPayload);

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
