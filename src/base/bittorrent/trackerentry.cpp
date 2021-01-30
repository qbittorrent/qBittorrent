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

#include "trackerentry.h"

#include <algorithm>

#include <libtorrent/version.hpp>

#include <QString>
#include <QUrl>

using namespace BitTorrent;

TrackerEntry::TrackerEntry(const QString &url)
    : m_nativeEntry {url.toStdString()}
{
}

TrackerEntry::TrackerEntry(const lt::announce_entry &nativeEntry)
    : m_nativeEntry {nativeEntry}
{
}

QString TrackerEntry::url() const
{
    return QString::fromStdString(nativeEntry().url);
}

int TrackerEntry::tier() const
{
    return nativeEntry().tier;
}

TrackerEntry::Status TrackerEntry::status(const InfoHashFormats hashVersions) const
{
#if (LIBTORRENT_VERSION_NUM >= 20000)
    Q_ASSERT(hashVersions.testFlag(InfoHashFormat::V1) || hashVersions.testFlag(InfoHashFormat::V2));
#endif

    const auto &endpoints = nativeEntry().endpoints;

    const bool allFailed = !endpoints.empty() && std::all_of(endpoints.begin(), endpoints.end()
        , [hashVersions](const lt::announce_endpoint &endpoint) -> bool
    {
#if (LIBTORRENT_VERSION_NUM >= 20000)
        if (hashVersions.testFlag(InfoHashFormat::V1)
            && (endpoint.info_hashes[lt::protocol_version::V1].fails == 0))
        {
            return false;
        }
        if (hashVersions.testFlag(InfoHashFormat::V2)
            && (endpoint.info_hashes[lt::protocol_version::V2].fails == 0))
        {
            return false;
        }
        return true;
#else
        return (endpoint.fails > 0);
#endif
    });
    if (allFailed)
        return NotWorking;

    const bool isUpdating = std::any_of(endpoints.begin(), endpoints.end()
        , [hashVersions](const lt::announce_endpoint &endpoint) -> bool
    {
#if (LIBTORRENT_VERSION_NUM >= 20000)
        if (hashVersions.testFlag(InfoHashFormat::V1)
            && endpoint.info_hashes[lt::protocol_version::V1].updating)
        {
            return true;
        }
        if (hashVersions.testFlag(InfoHashFormat::V2)
            && endpoint.info_hashes[lt::protocol_version::V2].updating)
        {
            return true;
        }
        return false;
#else
        return endpoint.updating;
#endif
    });
    if (isUpdating)
        return Updating;

    if (!nativeEntry().verified)
        return NotContacted;

    return Working;
}

void TrackerEntry::setTier(const int value)
{
    m_nativeEntry.tier = value;
}

int TrackerEntry::numSeeds(const InfoHashFormats hashVersions) const
{
#if (LIBTORRENT_VERSION_NUM >= 20000)
    Q_ASSERT(hashVersions.testFlag(InfoHashFormat::V1) || hashVersions.testFlag(InfoHashFormat::V2));
#endif

    int value = -1;
    for (const lt::announce_endpoint &endpoint : nativeEntry().endpoints)
    {
#if (LIBTORRENT_VERSION_NUM >= 20000)
        if (hashVersions.testFlag(InfoHashFormat::V1))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V1].scrape_complete);
        if (hashVersions.testFlag(InfoHashFormat::V2))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V2].scrape_complete);
#else
        Q_UNUSED(hashVersions);
        value = std::max(value, endpoint.scrape_complete);
#endif
    }
    return value;
}

int TrackerEntry::numLeeches(const InfoHashFormats hashVersions) const
{
#if (LIBTORRENT_VERSION_NUM >= 20000)
    Q_ASSERT(hashVersions.testFlag(InfoHashFormat::V1) || hashVersions.testFlag(InfoHashFormat::V2));
#endif

    int value = -1;
    for (const lt::announce_endpoint &endpoint : nativeEntry().endpoints)
    {
#if (LIBTORRENT_VERSION_NUM >= 20000)
        if (hashVersions.testFlag(InfoHashFormat::V1))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V1].scrape_incomplete);
        if (hashVersions.testFlag(InfoHashFormat::V2))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V2].scrape_incomplete);
#else
        Q_UNUSED(hashVersions);
        value = std::max(value, endpoint.scrape_incomplete);
#endif
    }
    return value;
}

int TrackerEntry::numDownloaded(const InfoHashFormats hashVersions) const
{
#if (LIBTORRENT_VERSION_NUM >= 20000)
    Q_ASSERT(hashVersions.testFlag(InfoHashFormat::V1) || hashVersions.testFlag(InfoHashFormat::V2));
#endif

    int value = -1;
    for (const lt::announce_endpoint &endpoint : nativeEntry().endpoints)
    {
#if (LIBTORRENT_VERSION_NUM >= 20000)
        if (hashVersions.testFlag(InfoHashFormat::V1))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V1].scrape_downloaded);
        if (hashVersions.testFlag(InfoHashFormat::V2))
            value = std::max(value, endpoint.info_hashes[lt::protocol_version::V2].scrape_downloaded);
#else
        Q_UNUSED(hashVersions);
        value = std::max(value, endpoint.scrape_downloaded);
#endif
    }
    return value;
}

const lt::announce_entry &TrackerEntry::nativeEntry() const
{
    return m_nativeEntry;
}

bool BitTorrent::operator==(const TrackerEntry &left, const TrackerEntry &right)
{
    return ((left.tier() == right.tier())
        && QUrl(left.url()) == QUrl(right.url()));
}

uint BitTorrent::qHash(const TrackerEntry &key, const uint seed)
{
    return (::qHash(key.url(), seed) ^ ::qHash(key.tier()));
}
