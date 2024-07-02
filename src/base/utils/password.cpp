/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
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

#include "password.h"

#include <array>

#include <openssl/evp.h>

#include <QByteArray>
#include <QList>
#include <QString>

#include "base/global.h"
#include "bytearray.h"
#include "random.h"

namespace Utils
{
    namespace Password
    {
        namespace PBKDF2
        {
            const int hashIterations = 100000;
            const auto hashMethod = EVP_sha512();
        }
    }
}

// Implements constant-time comparison to protect against timing attacks
// Taken from https://crackstation.net/hashing-security.htm
bool Utils::Password::slowEquals(const QByteArray &a, const QByteArray &b)
{
    const int lengthA = a.length();
    const int lengthB = b.length();

    int diff = lengthA ^ lengthB;
    for (int i = 0; (i < lengthA) && (i < lengthB); ++i)
        diff |= a[i] ^ b[i];

    return (diff == 0);
}

QString Utils::Password::generate()
{
    const QString alphanum = u"23456789ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz"_s;
    const int passwordLength = 9;
    QString pass;
    pass.reserve(passwordLength);
    while (pass.length() < passwordLength)
    {
        const auto num = Utils::Random::rand(0, (alphanum.size() - 1));
        pass.append(alphanum[num]);
    }

    return pass;
}

QByteArray Utils::Password::PBKDF2::generate(const QString &password)
{
    return generate(password.toUtf8());
}

QByteArray Utils::Password::PBKDF2::generate(const QByteArray &password)
{
    const std::array<uint32_t, 4> salt {
        {Random::rand(), Random::rand(), Random::rand(), Random::rand()}};

    std::array<unsigned char, 64> outBuf {};
    const int hmacResult = PKCS5_PBKDF2_HMAC(password.constData(), password.size()
        , reinterpret_cast<const unsigned char *>(salt.data()), static_cast<int>(sizeof(salt[0]) * salt.size())
        , hashIterations, hashMethod
        , static_cast<int>(outBuf.size()), outBuf.data());
    if (hmacResult != 1)
        return {};

    const QByteArray saltView = QByteArray::fromRawData(
        reinterpret_cast<const char *>(salt.data()), static_cast<int>(sizeof(salt[0]) * salt.size()));
    const QByteArray outBufView = QByteArray::fromRawData(
        reinterpret_cast<const char *>(outBuf.data()), static_cast<int>(outBuf.size()));

    return (saltView.toBase64() + ':' + outBufView.toBase64());
}

bool Utils::Password::PBKDF2::verify(const QByteArray &secret, const QString &password)
{
    return verify(secret, password.toUtf8());
}

bool Utils::Password::PBKDF2::verify(const QByteArray &secret, const QByteArray &password)
{
    const QList<QByteArrayView> list = ByteArray::splitToViews(secret, ":", Qt::SkipEmptyParts);
    if (list.size() != 2)
        return false;

    const QByteArray salt = QByteArray::fromBase64(Utils::ByteArray::asQByteArray(list[0]));
    const QByteArray key = QByteArray::fromBase64(Utils::ByteArray::asQByteArray(list[1]));

    std::array<unsigned char, 64> outBuf {};
    const int hmacResult = PKCS5_PBKDF2_HMAC(password.constData(), password.size()
        , reinterpret_cast<const unsigned char *>(salt.constData()), salt.size()
        , hashIterations, hashMethod
        , static_cast<int>(outBuf.size()), outBuf.data());
    if (hmacResult != 1)
        return false;

    const QByteArray outBufView = QByteArray::fromRawData(
        reinterpret_cast<const char *>(outBuf.data()), static_cast<int>(outBuf.size()));
    return slowEquals(key, outBufView);
}
