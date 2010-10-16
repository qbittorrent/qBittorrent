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
#include <QHash>

class RssFeed;

// Item of a rss stream, single information
class RssArticle: public QObject {
  Q_OBJECT

public:
  RssArticle(RssFeed* parent, QXmlStreamReader& xml);
  RssArticle(RssFeed* parent, QString _id, QString _title,
             QString _torrent_url, QString _news_link, QString _description,
             QDateTime _date, QString _author, bool _read);
  ~RssArticle();
  bool has_attachment() const;
  QString getId() const;
  QHash<QString, QVariant> toHash() const;
  static RssArticle* fromHash(RssFeed* parent, const QHash<QString, QVariant> &h);
  RssFeed* getParent() const;
  bool isValid() const;
  QString getTitle() const;
  QString getAuthor() const;
  QString getTorrentUrl() const;
  QString getLink() const;
  QString getDescription() const;
  QDateTime getDate() const;
  bool isRead() const;
  void setRead();

protected:
  QDateTime parseDate(const QString &string);

private:
  RssFeed* parent;
  QString id;
  QString title;
  QString torrent_url;
  QString news_link;
  QString description;
  QDateTime date;
  QString author;
  bool is_valid;
  bool read;
};


#endif // RSSARTICLE_H
