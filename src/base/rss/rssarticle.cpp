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
Article::Article(Feed *parent, const QString &guid)
    : m_parent(parent)
    , m_guid(guid)
    , m_read(false)
{
}

bool Article::hasAttachment() const
{
    return !m_torrentUrl.isEmpty();
}

QVariantHash Article::toHash() const
{
    QVariantHash item;
    item["title"] = m_title;
    item["id"] = m_guid;
    item["torrent_url"] = m_torrentUrl;
    item["news_link"] = m_link;
    item["description"] = m_description;
    item["date"] = m_date;
    item["author"] = m_author;
    item["read"] = m_read;
    return item;
}

ArticlePtr Article::fromHash(Feed *parent, const QVariantHash &h)
{
    const QString guid = h.value("id").toString();
    if (guid.isEmpty())
        return ArticlePtr();

    ArticlePtr art(new Article(parent, guid));
    art->m_title = h.value("title", "").toString();
    art->m_torrentUrl = h.value("torrent_url", "").toString();
    art->m_link = h.value("news_link", "").toString();
    art->m_description = h.value("description").toString();
    art->m_date = h.value("date").toDateTime();
    art->m_author = h.value("author").toString();
    art->m_read = h.value("read", false).toBool();

    return art;
}

Feed *Article::parent() const
{
    return m_parent;
}

const QString &Article::author() const
{
    return m_author;
}

const QString &Article::torrentUrl() const
{
    return m_torrentUrl;
}

const QString &Article::link() const
{
    return m_link;
}

QString Article::description() const
{
    return m_description.isNull() ? "" : m_description;
}

const QDateTime &Article::date() const
{
    return m_date;
}

bool Article::isRead() const
{
    return m_read;
}

void Article::markAsRead()
{
    if (!m_read) {
        m_read = true;
        emit articleWasRead();
    }
}

const QString &Article::guid() const
{
    return m_guid;
}

const QString &Article::title() const
{
    return m_title;
}

void Article::handleTorrentDownloadSuccess(const QString &url)
{
    if (url == m_torrentUrl)
        markAsRead();
}
