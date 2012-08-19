/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez, Arnaud Demaiziere
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

#ifndef RSSARTICLE_H
#define RSSARTICLE_H

#include <QXmlStreamReader>
#include <QDateTime>
#include <QVariantHash>
#include <QSharedPointer>

class RssFeed;
class RssArticle;

typedef QSharedPointer<RssArticle> RssArticlePtr;

// Item of a rss stream, single information
class RssArticle {

public:
  RssArticle(RssFeed* parent, const QString& guid);
  // Accessors
  bool hasAttachment() const;
  const QString& guid() const;
  RssFeed* parent() const;
  const QString& title() const;
  const QString& author() const;
  const QString& torrentUrl() const;
  const QString& link() const;
  const QString& description() const;
  const QDateTime& date() const;
  bool isRead() const;
  // Setters
  void markAsRead();
  // Serialization
  QVariantHash toHash() const;

  friend RssArticlePtr hashToRssArticle(RssFeed* parent, const QVariantHash& hash);

private:
  RssFeed* m_parent;
  QString m_guid;
  QString m_title;
  QString m_torrentUrl;
  QString m_link;
  QString m_description;
  QDateTime m_date;
  QString m_author;
  bool m_read;
};

RssArticlePtr hashToRssArticle(RssFeed* parent, const QVariantHash& hash);

#endif // RSSARTICLE_H
