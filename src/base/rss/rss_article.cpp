/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#include "rss_article.h"

#include <QJsonObject>
#include <QVariant>

#include "rss_feed.h"

using namespace RSS;

namespace
{
    QVariantHash articleDataFromJSON(const QJsonObject &jsonObj)
    {
        auto varHash = jsonObj.toVariantHash();
        // JSON object store DateTime as string so we need to convert it
        varHash[Article::KeyDate] =
                QDateTime::fromString(jsonObj.value(Article::KeyDate).toString(), Qt::RFC2822Date);

        return varHash;
    }
}

const QString Article::KeyId(QStringLiteral("id"));
const QString Article::KeyDate(QStringLiteral("date"));
const QString Article::KeyTitle(QStringLiteral("title"));
const QString Article::KeyAuthor(QStringLiteral("author"));
const QString Article::KeyDescription(QStringLiteral("description"));
const QString Article::KeyTorrentURL(QStringLiteral("torrentURL"));
const QString Article::KeyLink(QStringLiteral("link"));
const QString Article::KeyIsRead(QStringLiteral("isRead"));

Article::Article(Feed *feed, const QVariantHash &varHash)
    : QObject(feed)
    , m_feed(feed)
    , m_guid(varHash.value(KeyId).toString())
    , m_date(varHash.value(KeyDate).toDateTime())
    , m_title(varHash.value(KeyTitle).toString())
    , m_author(varHash.value(KeyAuthor).toString())
    , m_description(varHash.value(KeyDescription).toString())
    , m_torrentURL(varHash.value(KeyTorrentURL).toString())
    , m_link(varHash.value(KeyLink).toString())
    , m_isRead(varHash.value(KeyIsRead, false).toBool())
    , m_data(varHash)
{
}

Article::Article(Feed *feed, const QJsonObject &jsonObj)
    : Article(feed, articleDataFromJSON(jsonObj))
{
}

QString Article::guid() const
{
    return m_guid;
}

QDateTime Article::date() const
{
    return m_date;
}

QString Article::title() const
{
    return m_title;
}

QString Article::author() const
{
    return m_author;
}

QString Article::description() const
{
    return m_description;
}

QString Article::torrentUrl() const
{
    return (m_torrentURL.isEmpty() ? m_link : m_torrentURL);
}

QString Article::link() const
{
    return m_link;
}

bool Article::isRead() const
{
    return m_isRead;
}

QVariantHash Article::data() const
{
    return m_data;
}

void Article::markAsRead()
{
    if (!m_isRead)
    {
        m_isRead = true;
        m_data[KeyIsRead] = m_isRead;
        emit read(this);
    }
}

QJsonObject Article::toJsonObject() const
{
    auto jsonObj = QJsonObject::fromVariantHash(m_data);
    // JSON object doesn't support DateTime so we need to convert it
    jsonObj[KeyDate] = m_date.toString(Qt::RFC2822Date);

    return jsonObj;
}

bool Article::articleDateRecentThan(const Article *article, const QDateTime &date)
{
    return article->date() > date;
}

Feed *Article::feed() const
{
    return m_feed;
}
