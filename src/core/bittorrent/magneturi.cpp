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

#include <libtorrent/bencode.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>

#include "core/utils/string.h"
#include "magneturi.h"

namespace libt = libtorrent;
using namespace BitTorrent;

MagnetUri::MagnetUri(const QString &url)
    : m_valid(false)
    , m_url(url)
{
    if (url.isEmpty()) return;

    libt::error_code ec;
    libt::parse_magnet_uri(url.toUtf8().constData(), m_addTorrentParams, ec);
    if (ec) return;

    m_valid = true;
    m_hash = m_addTorrentParams.info_hash;
    m_name = Utils::String::fromStdString(m_addTorrentParams.name);

    foreach (const std::string &tracker, m_addTorrentParams.trackers)
        m_trackers.append(Utils::String::fromStdString(tracker));

#if LIBTORRENT_VERSION_NUM >= 10000
    foreach (const std::string &urlSeed, m_addTorrentParams.url_seeds)
        m_urlSeeds.append(QUrl(urlSeed.c_str()));
#endif
}

bool MagnetUri::isValid() const
{
    return m_valid;
}

InfoHash MagnetUri::hash() const
{
    return m_hash;
}

QString MagnetUri::name() const
{
    return m_name;
}

QList<TrackerEntry> MagnetUri::trackers() const
{
    return m_trackers;
}

QList<QUrl> MagnetUri::urlSeeds() const
{
    return m_urlSeeds;
}

QString MagnetUri::url() const
{
    return m_url;
}

libtorrent::add_torrent_params MagnetUri::addTorrentParams() const
{
    return m_addTorrentParams;
}
