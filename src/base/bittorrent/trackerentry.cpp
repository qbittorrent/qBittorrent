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
    : m_nativeEntry(url.toStdString())
{
}

TrackerEntry::TrackerEntry(const libtorrent::announce_entry &nativeEntry)
    : m_nativeEntry(nativeEntry)
{
}

QString TrackerEntry::url() const
{
    return QString::fromStdString(m_nativeEntry.url);
}

bool TrackerEntry::isWorking() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return m_nativeEntry.is_working();
#else
    return std::any_of(m_nativeEntry.endpoints.begin(), m_nativeEntry.endpoints.end()
                       , [](const lt::announce_endpoint &endpoint)
    {
        return endpoint.is_working();
    });
#endif
}

int TrackerEntry::tier() const
{
    return m_nativeEntry.tier;
}

TrackerEntry::Status TrackerEntry::status() const
{
    // libtorrent::announce_entry::is_working() returns
    // true when the tracker hasn't been tried yet.
    if (m_nativeEntry.verified && isWorking())
        return Working;
    if ((m_nativeEntry.fails == 0) && m_nativeEntry.updating)
        return Updating;
    if (m_nativeEntry.fails == 0)
        return NotContacted;

    return NotWorking;
}

void TrackerEntry::setTier(const int value)
{
    m_nativeEntry.tier = value;
}

int TrackerEntry::numSeeds() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return nativeEntry().scrape_complete;
#else
    // FIXME: Handle all possible endpoints.
    return nativeEntry().endpoints.empty() ? -1 : nativeEntry().endpoints[0].scrape_complete;
#endif
}

int TrackerEntry::numLeeches() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return nativeEntry().scrape_incomplete;
#else
    // FIXME: Handle all possible endpoints.
    return nativeEntry().endpoints.empty() ? -1 : nativeEntry().endpoints[0].scrape_incomplete;
#endif
}

int TrackerEntry::numDownloaded() const
{
#if (LIBTORRENT_VERSION_NUM < 10200)
    return nativeEntry().scrape_downloaded;
#else
    // FIXME: Handle all possible endpoints.
    return nativeEntry().endpoints.empty() ? -1 : nativeEntry().endpoints[0].scrape_downloaded;
#endif
}

libtorrent::announce_entry TrackerEntry::nativeEntry() const
{
    return m_nativeEntry;
}

bool BitTorrent::operator==(const TrackerEntry &left, const TrackerEntry &right)
{
    return (QUrl(left.url()) == QUrl(right.url()));
}
