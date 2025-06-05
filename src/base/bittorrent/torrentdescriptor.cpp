/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentdescriptor.h"

#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/version.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <QByteArray>
#include <QRegularExpression>
#include <QUrl>

#include "base/global.h"
#include "base/preferences.h"
#include "base/utils/io.h"
#include "infohash.h"
#include "trackerentry.h"

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

        return ((((string.size() == V1_HEX_SIZE)) && !string.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s)))
                || ((string.size() == V1_BASE32_SIZE) && !string.contains(QRegularExpression(u"[^2-7A-Za-z]"_s))));
    }

    bool isV2Hash(const QString &string)
    {
        // There are 1 representation for BitTorrent v2 info hash:
        // 1. 64 chars hex-encoded string
        //      == 32 (SHA-2 256 length in bytes) * 2 (each byte maps to 2 hex characters)
        const int V2_HEX_SIZE = SHA256Hash::length() * 2;

        return (string.size() == V2_HEX_SIZE) && !string.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s));
    }

    lt::load_torrent_limits loadTorrentLimits()
    {
        const auto *pref = Preferences::instance();

        lt::load_torrent_limits limits;
        limits.max_buffer_size = static_cast<int>(pref->getTorrentFileSizeLimit());
        limits.max_decode_depth = pref->getBdecodeDepthLimit();
        limits.max_decode_tokens = pref->getBdecodeTokenLimit();

        return limits;
    }
}

const int TORRENTDESCRIPTOR_TYPEID = qRegisterMetaType<BitTorrent::TorrentDescriptor>();

nonstd::expected<BitTorrent::TorrentDescriptor, QString>
BitTorrent::TorrentDescriptor::load(const QByteArray &data) noexcept
try
{
    return TorrentDescriptor(lt::load_torrent_buffer(lt::span<const char>(data.data(), data.size()), loadTorrentLimits()));
}
catch (const lt::system_error &err)
{
    return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
}

nonstd::expected<BitTorrent::TorrentDescriptor, QString>
BitTorrent::TorrentDescriptor::loadFromFile(const Path &path) noexcept
try
{
    return TorrentDescriptor(lt::load_torrent_file(path.toString().toStdString(), loadTorrentLimits()));
}
catch (const lt::system_error &err)
{
    return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
}

nonstd::expected<BitTorrent::TorrentDescriptor, QString>
BitTorrent::TorrentDescriptor::parse(const QString &str) noexcept
try
{
    QString magnetURI = str;
    if (isV2Hash(str))
        magnetURI = u"magnet:?xt=urn:btmh:1220" + str; // 0x12 0x20 is the "multihash format" tag for the SHA-256 hashing scheme.
    else if (isV1Hash(str))
        magnetURI = u"magnet:?xt=urn:btih:" + str;

    return TorrentDescriptor(lt::parse_magnet_uri(magnetURI.toStdString()));
}
catch (const lt::system_error &err)
{
    return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
}

nonstd::expected<void, QString> BitTorrent::TorrentDescriptor::saveToFile(const Path &path) const
try
{
    const lt::entry torrentEntry = lt::write_torrent_file(m_ltAddTorrentParams);
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, torrentEntry);
    if (!result)
        return result.get_unexpected();

    return {};
}
catch (const lt::system_error &err)
{
    return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
}

BitTorrent::TorrentDescriptor::TorrentDescriptor(lt::add_torrent_params ltAddTorrentParams)
    : m_ltAddTorrentParams {std::move(ltAddTorrentParams)}
{
    if (m_ltAddTorrentParams.ti && m_ltAddTorrentParams.ti->is_valid())
    {
        m_info.emplace(*m_ltAddTorrentParams.ti);
        if (m_ltAddTorrentParams.ti->creation_date() > 0)
            m_creationDate = QDateTime::fromSecsSinceEpoch(m_ltAddTorrentParams.ti->creation_date());
        m_creator = QString::fromStdString(m_ltAddTorrentParams.ti->creator());
        m_comment = QString::fromStdString(m_ltAddTorrentParams.ti->comment());
    }
}

BitTorrent::InfoHash BitTorrent::TorrentDescriptor::infoHash() const
{
#ifdef QBT_USES_LIBTORRENT2
    return m_ltAddTorrentParams.info_hashes;
#else
    return m_ltAddTorrentParams.info_hash;
#endif
}

QString BitTorrent::TorrentDescriptor::name() const
{
    return m_info ? m_info->name() : QString::fromStdString(m_ltAddTorrentParams.name);
}

QDateTime BitTorrent::TorrentDescriptor::creationDate() const
{
    return m_creationDate;
}

QString BitTorrent::TorrentDescriptor::creator() const
{
    return m_creator;
}

QString BitTorrent::TorrentDescriptor::comment() const
{
    return m_comment;
}

const std::optional<BitTorrent::TorrentInfo> &BitTorrent::TorrentDescriptor::info() const
{
    return m_info;
}

void BitTorrent::TorrentDescriptor::setTorrentInfo(TorrentInfo torrentInfo)
{
    if (!torrentInfo.isValid())
    {
        m_info.reset();
        m_ltAddTorrentParams.ti.reset();
    }
    else
    {
        m_info = std::move(torrentInfo);
        m_ltAddTorrentParams.ti = m_info->nativeInfo();
#ifdef QBT_USES_LIBTORRENT2
        m_ltAddTorrentParams.info_hashes = m_ltAddTorrentParams.ti->info_hashes();
#else
        m_ltAddTorrentParams.info_hash = m_ltAddTorrentParams.ti->info_hash();
#endif
    }
}

QList<BitTorrent::TrackerEntry> BitTorrent::TorrentDescriptor::trackers() const
{
    QList<TrackerEntry> ret;
    ret.reserve(static_cast<decltype(ret)::size_type>(m_ltAddTorrentParams.trackers.size()));
    std::size_t i = 0;
    for (const std::string &tracker : m_ltAddTorrentParams.trackers)
        ret.append({QString::fromStdString(tracker), m_ltAddTorrentParams.tracker_tiers[i++]});

    return ret;
}

QList<QUrl> BitTorrent::TorrentDescriptor::urlSeeds() const
{
    QList<QUrl> urlSeeds;
    urlSeeds.reserve(static_cast<decltype(urlSeeds)::size_type>(m_ltAddTorrentParams.url_seeds.size()));

    for (const std::string &nativeURLSeed : m_ltAddTorrentParams.url_seeds)
        urlSeeds.append(QUrl(QString::fromStdString(nativeURLSeed)));

    return urlSeeds;
}

const libtorrent::add_torrent_params &BitTorrent::TorrentDescriptor::ltAddTorrentParams() const
{
    return m_ltAddTorrentParams;
}
