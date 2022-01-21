/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <libtorrent/sha1_hash.hpp>

#include <QByteArray>
#include <QHash>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QString>

template <int N>
class Digest32
{
public:
    using UnderlyingType = lt::digest32<N>;

    Digest32() = default;
    Digest32(const Digest32 &other) = default;
    Digest32(Digest32 &&other) = default;

    Digest32(const UnderlyingType &nativeDigest)
    {
        m_dataPtr->valid = true;
        m_dataPtr->nativeDigest = nativeDigest;
        const QByteArray raw = QByteArray::fromRawData(nativeDigest.data(), length());
        m_dataPtr->hashString = QString::fromLatin1(raw.toHex());
    }

    static constexpr int length()
    {
        return UnderlyingType::size();
    }

    bool isValid() const
    {
        return m_dataPtr->valid;
    }

    Digest32 &operator=(const Digest32 &other) = default;
    Digest32 &operator=(Digest32 &&other) = default;

    operator UnderlyingType() const
    {
        return m_dataPtr->nativeDigest;
    }

    static Digest32 fromString(const QString &digestString)
    {
        if (digestString.size() != (length() * 2))
            return {};

        const QByteArray raw = QByteArray::fromHex(digestString.toLatin1());
        if (raw.size() != length())  // QByteArray::fromHex() will skip over invalid characters
            return {};

        Digest32 result;
        result.m_dataPtr->valid = true;
        result.m_dataPtr->hashString = digestString;
        result.m_dataPtr->nativeDigest.assign(raw.constData());

        return result;
    }

    QString toString() const
    {
        return m_dataPtr->hashString;
    }

private:
    struct Data : public QSharedData
    {
        bool valid = false;
        UnderlyingType nativeDigest;
        QString hashString;
    };

    QSharedDataPointer<Data> m_dataPtr {new Data};
};

template <int N>
bool operator==(const Digest32<N> &left, const Digest32<N> &right)
{
    return (static_cast<typename Digest32<N>::UnderlyingType>(left)
            == static_cast<typename Digest32<N>::UnderlyingType>(right));
}

template <int N>
bool operator!=(const Digest32<N> &left, const Digest32<N> &right)
{
    return !(left == right);
}

template <int N>
bool operator<(const Digest32<N> &left, const Digest32<N> &right)
{
    return static_cast<typename Digest32<N>::UnderlyingType>(left)
            < static_cast<typename Digest32<N>::UnderlyingType>(right);
}

template <int N>
uint qHash(const Digest32<N> &key, const uint seed)
{
    return ::qHash(std::hash<typename Digest32<N>::UnderlyingType>()(key), seed);
}
