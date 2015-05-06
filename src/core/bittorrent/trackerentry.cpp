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

#include <QString>

#include "core/utils/misc.h"
#include "core/utils/string.h"
#include "trackerentry.h"

using namespace BitTorrent;

TrackerEntry::TrackerEntry(const QString &url)
    : m_nativeEntry(libtorrent::announce_entry(Utils::String::toStdString(url)))
{
}

TrackerEntry::TrackerEntry(const libtorrent::announce_entry &nativeEntry)
    : m_nativeEntry(nativeEntry)
{
}

TrackerEntry::TrackerEntry(const TrackerEntry &other)
    : m_nativeEntry(other.m_nativeEntry)
{
}

QString TrackerEntry::url() const
{
    return Utils::String::fromStdString(m_nativeEntry.url);
}

int TrackerEntry::tier() const
{
    return m_nativeEntry.tier;
}

TrackerEntry::Status TrackerEntry::status() const
{
    if (m_nativeEntry.verified)
        return Working;
    else if ((m_nativeEntry.fails == 0) && m_nativeEntry.updating)
        return Updating;
    else if (m_nativeEntry.fails == 0)
        return NotContacted;
    else
        return NotWorking;
}

void TrackerEntry::setTier(int value)
{
    m_nativeEntry.tier = value;
}

TrackerEntry &TrackerEntry::operator=(const TrackerEntry &other)
{
    this->m_nativeEntry = other.m_nativeEntry;
    return *this;
}

bool TrackerEntry::operator==(const TrackerEntry &other)
{
    return (QUrl(url()) == QUrl(other.url()));
}

libtorrent::announce_entry TrackerEntry::nativeEntry() const
{
    return m_nativeEntry;
}
