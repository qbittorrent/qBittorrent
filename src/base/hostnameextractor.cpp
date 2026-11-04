/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "hostnameextractor.h"

#include <QHash>
#include <QUrl>

QAtomicInt HostnameExtractor::m_refCount {0};
QReadWriteLock HostnameExtractor::m_cacheLock;
QHash<QString, QString> HostnameExtractor::m_cache;

HostnameExtractor::HostnameExtractor()
{
    m_refCount.ref();
}

HostnameExtractor::HostnameExtractor(const HostnameExtractor &)
    : HostnameExtractor()
{
}

HostnameExtractor::~HostnameExtractor()
{
    const QWriteLocker cacheWriteLocker {&m_cacheLock};
    if (!m_refCount.deref())
        m_cache.clear();
}

QString HostnameExtractor::extractHostname(const QString &url) const
{
    {
        const QReadLocker cacheReadLocker {&m_cacheLock};
        if (const auto iter = m_cache.constFind(url); iter != m_cache.constEnd())
            return iter.value();
    }

    // We want the hostname.
    const QString host = QUrl(url).host();
    // If failed to parse the domain, original input should be returned
    const QString result = !host.isEmpty() ? host : url;

    const QWriteLocker cacheWriteLocker {&m_cacheLock};
    m_cache[url] = result;

    return result;
}
