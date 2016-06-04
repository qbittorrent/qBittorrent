/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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
 *
 * Contact: chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#include <QVariant>
#include <QDebug>
#include <iostream>

#include "rssfeed.h"
#include "rssarticle.h"

using namespace Rss;

// public constructor
Article::Article(Feed *parent)
    : m_parent(parent)
{
}

bool Article::hasAttachment() const
{
    return !torrentUrl().isEmpty();
}

QVariantHash Article::toHash()
{
    return m_hash;
}

ArticlePtr Article::fromHash(Feed *parent, const QVariantHash &h)
{
    const QString guid = h.value("id").toString();
    if (guid.isEmpty())
        return ArticlePtr();

    ArticlePtr art(new Article(parent));
    art->m_hash = h;

    return art;
}

Feed *Article::parent() const
{
    return m_parent;
}

const QString Article::author()
{
    return m_hash.value("author").toString();
}

const QString Article::torrentUrl()
{
    return m_hash.value("torrent_url", "").toString();
}

const QString Article::link()
{
    return m_hash.value("news_link", "").toString();
}

QString Article::description() const
{
    return m_hash.value("description", "").toString();
}

const QDateTime Article::date()
{
    return m_hash.value("date").toDateTime();
}

bool Article::isRead() const
{
    return m_hash.value("read", false).toBool();
}

void Article::markAsRead()
{
    if (!isRead()) {
        m_hash.insert("read", true);
        emit articleWasRead();
    }
}

const QString Article::guid()
{
    return m_hash.value("id").toString();
}

const QString Article::title()
{
    return m_hash.value("title", "").toString();
}

void Article::handleTorrentDownloadSuccess(const QString &url)
{
    if (url == torrentUrl())
        markAsRead();
}

const QString Article::getValue(const QString &name) const
{
    return m_hash.value(name, name).toString();
}
