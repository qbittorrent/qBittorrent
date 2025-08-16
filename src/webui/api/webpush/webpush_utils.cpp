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

#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

#include "base/logger.h"
#include "base/utils/string.h"

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
        EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
        if (!ctx)
            return nullptr;
        if (EVP_PKEY_fromdata_init(ctx) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        EVP_PKEY *pkey = nullptr;

        OSSL_PARAM params[3];
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>("prime256v1"), 0);
        auto data = static_cast<const void*>(publicKeyBytes.data());
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, const_cast<void*>(data), publicKeyBytes.size());
        params[2] = OSSL_PARAM_construct_end();

        if (EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
        {
            EVP_PKEY_CTX_free(ctx);
            return nullptr;
        }

        EVP_PKEY_CTX_free(ctx);
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

        return { cek, nonce };
    }

    QByteArray aes128gcmEncrypt(const QByteArray& cek, const QByteArray& nonce, const QByteArray& plaintext)
    {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            return {};

        int len;
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
        if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) <= 0)
        {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }

        EVP_CIPHER_CTX_free(ctx);
        return ciphertext + tag;
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
}

EVP_PKEY *generateECDHKeypair()
{
    return EVP_EC_gen(const_cast<char*>("prime256v1"));
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

QPair<QByteArray, QByteArray> buildWebPushPayload(const QString& p256dh, const QString& auth, const QByteArray& payload)
{
    const auto salt = generateSalt();
    const auto receiverPublicKey = createPublicKeyFromBytes(base64UrlDecode(p256dh.toLatin1()));
    const auto authSecret = base64UrlDecode(auth.toLatin1());

    const auto senderPrivateKey = generateECDHKeypair();
    const auto senderPublicKeyOctets = getECPublicOctets(senderPrivateKey);

    const auto [cek, nonce] = deriveWebPushKeys(salt, senderPrivateKey, senderPublicKeyOctets, receiverPublicKey, authSecret);

    // Build header (salt || record size || pubKeyLen || pubKey)
    auto header = QByteArray(salt);
    header.append("\x00\x00\x10\x00", 4); // Use 4096 as record size
    const auto idLen = static_cast<quint8>(senderPublicKeyOctets.size());
    header.append(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
    header.append(senderPublicKeyOctets);

    // Append padding delimiter octet (0x02) to the payload and encrypt
    const auto encryptedPayload = aes128gcmEncrypt(cek, nonce, payload + "\x02");

    return { header + encryptedPayload, base64UrlEncode(senderPublicKeyOctets) };
}

// https://datatracker.ietf.org/doc/html/rfc8292#section-2
QString createVapidJWT(EVP_PKEY *privateKey, const QString& endpoint)
{
    const auto now = QDateTime::currentSecsSinceEpoch();
    const QJsonObject header{ {u"alg"_s, u"ES256"_s}, {u"typ"_s, u"JWT"_s} };
    const QJsonObject payload
    {
        {u"aud"_s, getAudienceFromEndpoint(endpoint)},
        // Limiting this to 24 hours balances the need for reuse
        // against the potential cost and likelihood of theft of a valid token.
        {u"exp"_s, now + 60  *60  *24}, // 24 hours
        {u"sub"_s, u"https://qbittorrent.org"_s}
    };

    const auto headerJson = QJsonDocument(header).toJson(QJsonDocument::Compact);
    const auto payloadJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QByteArray signingInput = base64UrlEncode(headerJson) + "." + base64UrlEncode(payloadJson);
    QByteArray signature = base64UrlEncode(ecdsaSign(privateKey, signingInput));

    return QString::fromLatin1(signingInput + "." + signature);
}

QString getVapidPublicKeyString(EVP_PKEY *privateKey)
{
    const auto publicKeyOctets = getECPublicOctets(privateKey);
    return QString::fromLatin1(base64UrlEncode(publicKeyOctets));
}
