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

const QString Str_Id(QStringLiteral("id"));
const QString Str_Date(QStringLiteral("date"));
const QString Str_Title(QStringLiteral("title"));
const QString Str_Author(QStringLiteral("author"));
const QString Str_Description(QStringLiteral("description"));
const QString Str_TorrentURL(QStringLiteral("torrentURL"));
const QString Str_Torrent_Url(QStringLiteral("torrent_url"));
const QString Str_Link(QStringLiteral("link"));
const QString Str_News_Link(QStringLiteral("news_link"));
const QString Str_IsRead(QStringLiteral("isRead"));
const QString Str_Read(QStringLiteral("read"));

using namespace RSS;

Article::Article(Feed *feed, QString guid, QDateTime date, QString title, QString author
                 , QString description, QString torrentUrl, QString link, bool isRead)
    : QObject(feed)
    , m_feed(feed)
    , m_guid(guid)
    , m_date(date)
    , m_title(title)
    , m_author(author)
    , m_description(description)
    , m_torrentURL(torrentUrl)
    , m_link(link)
    , m_isRead(isRead)
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

void Article::markAsRead()
{
    if (!m_isRead) {
        m_isRead = true;
        emit read(this);
    }
}

QJsonObject Article::toJsonObject() const
{
    return {
        {Str_Id, m_guid},
        {Str_Date, m_date.toString(Qt::RFC2822Date)},
        {Str_Title, m_title},
        {Str_Author, m_author},
        {Str_Description, m_description},
        {Str_TorrentURL, m_torrentURL},
        {Str_Link, m_link},
        {Str_IsRead, m_isRead}
    };
}

bool Article::articleDateRecentThan(Article *article, const QDateTime &date)
{
    return article->date() > date;
}

Article *Article::fromJsonObject(Feed *feed, const QJsonObject &jsonObj)
{
    QString guid = jsonObj.value(Str_Id).toString();
    // If item does not have a guid, fall back to some other identifier
    if (guid.isEmpty())
        guid = jsonObj.value(Str_Torrent_Url).toString();
    if (guid.isEmpty())
        guid = jsonObj.value(Str_Title).toString();
    if (guid.isEmpty()) return nullptr;

    return new Article(
                feed, guid
                , QDateTime::fromString(jsonObj.value(Str_Date).toString(), Qt::RFC2822Date)
                , jsonObj.value(Str_Title).toString()
                , jsonObj.value(Str_Author).toString()
                , jsonObj.value(Str_Description).toString()
                , jsonObj.value(Str_TorrentURL).toString()
                , jsonObj.value(Str_Link).toString()
                , jsonObj.value(Str_IsRead).toBool(false));
}

Article *Article::fromVariantHash(Feed *feed, const QVariantHash &varHash)
{
    QString guid = varHash[Str_Id].toString();
    // If item does not have a guid, fall back to some other identifier
    if (guid.isEmpty())
        guid = varHash.value(Str_Torrent_Url).toString();
    if (guid.isEmpty())
        guid = varHash.value(Str_Title).toString();
    if (guid.isEmpty()) return nullptr;

    return new Article(feed, guid
                       , varHash.value(Str_Date).toDateTime()
                       , varHash.value(Str_Title).toString()
                       , varHash.value(Str_Author).toString()
                       , varHash.value(Str_Description).toString()
                       , varHash.value(Str_Torrent_Url).toString()
                       , varHash.value(Str_News_Link).toString()
                       , varHash.value(Str_Read, false).toBool());
}

Feed *Article::feed() const
{
    return m_feed;
}
