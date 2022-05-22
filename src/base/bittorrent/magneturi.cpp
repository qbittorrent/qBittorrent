/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "magneturi.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/sha1_hash.hpp>

#include <QRegularExpression>

#include "base/global.h"
#include "infohash.h"

namespace
{
    // BEP9 Extension for Peers to Send Metadata Files

    bool isV1Hash(const QString &string)
    {
        // There are 2 representations for BitTorrent v1 info hash:
        // 1. 40 chars hex-encoded string
        //      == 20 (SHA-1 length in bytes) * 2 (each byte maps to 2 hex characters)
        // 2. 32 chars Base32 encoded string
        //      == 20 (SHA-1 length in bytes) * 1.6 (the efficiency of Base32 encoding)
        const int V1_HEX_SIZE = SHA1Hash::length() * 2;
        const int V1_BASE32_SIZE = SHA1Hash::length() * 1.6;

        return ((((string.size() == V1_HEX_SIZE))
                && !string.contains(QRegularExpression(u"[^0-9A-Fa-f]"_qs)))
            || ((string.size() == V1_BASE32_SIZE)
                && !string.contains(QRegularExpression(u"[^2-7A-Za-z]"_qs))));
    }

    bool isV2Hash(const QString &string)
    {
        // There are 1 representation for BitTorrent v2 info hash:
        // 1. 64 chars hex-encoded string
        //      == 32 (SHA-2 256 length in bytes) * 2 (each byte maps to 2 hex characters)
        const int V2_HEX_SIZE = SHA256Hash::length() * 2;

        return (string.size() == V2_HEX_SIZE)
                && !string.contains(QRegularExpression(u"[^0-9A-Fa-f]"_qs));
    }
}

using namespace BitTorrent;

const int magnetUriId = qRegisterMetaType<MagnetUri>();

MagnetUri::MagnetUri(const QString &source)
    : m_valid(false)
    , m_url(source)
{
    if (source.isEmpty()) return;

    if (isV2Hash(source))
        m_url = u"magnet:?xt=urn:btmh:1220" + source; // 0x12 0x20 is the "multihash format" tag for the SHA-256 hashing scheme.
    else if (isV1Hash(source))
        m_url = u"magnet:?xt=urn:btih:" + source;

    lt::error_code ec;
    lt::parse_magnet_uri(m_url.toStdString(), m_addTorrentParams, ec);
    if (ec) return;

    m_valid = true;

#ifdef QBT_USES_LIBTORRENT2
    m_infoHash = m_addTorrentParams.info_hashes;
#else
    m_infoHash = m_addTorrentParams.info_hash;
#endif

    m_name = QString::fromStdString(m_addTorrentParams.name);

    m_trackers.reserve(static_cast<decltype(m_trackers)::size_type>(m_addTorrentParams.trackers.size()));
    int tier = 0;
    auto tierIter = m_addTorrentParams.tracker_tiers.cbegin();
    for (const std::string &url : m_addTorrentParams.trackers)
    {
        if (tierIter != m_addTorrentParams.tracker_tiers.cend())
            tier = *tierIter++;

        m_trackers.append({QString::fromStdString(url), tier});
    }

    m_urlSeeds.reserve(static_cast<decltype(m_urlSeeds)::size_type>(m_addTorrentParams.url_seeds.size()));
    for (const std::string &urlSeed : m_addTorrentParams.url_seeds)
        m_urlSeeds.append(QString::fromStdString(urlSeed));
}

bool MagnetUri::isValid() const
{
    return m_valid;
}

InfoHash MagnetUri::infoHash() const
{
    return m_infoHash;
}

QString MagnetUri::name() const
{
    return m_name;
}

QVector<TrackerEntry> MagnetUri::trackers() const
{
    return m_trackers;
}

QVector<QUrl> MagnetUri::urlSeeds() const
{
    return m_urlSeeds;
}

QString MagnetUri::url() const
{
    return m_url;
}

lt::add_torrent_params MagnetUri::addTorrentParams() const
{
    return m_addTorrentParams;
}
