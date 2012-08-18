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

#ifndef RSSFEED_H
#define RSSFEED_H

#include <QHash>
#include <QSharedPointer>
#include <QVariantHash>
#include <QXmlStreamReader>

#include "rssfile.h"

class RssFeed;
class RssManager;

typedef QHash<QString, RssArticlePtr> RssArticleHash;
typedef QSharedPointer<RssFeed> RssFeedPtr;
typedef QList<RssFeedPtr> RssFeedList;

class RssFeed: public QObject, public RssFile {
  Q_OBJECT

public:
  RssFeed(RssManager* manager, RssFolder* m_parent, const QString &url);
  virtual ~RssFeed();
  virtual RssFolder* parent() const { return m_parent; }
  virtual void setParent(RssFolder* parent) { m_parent = parent; }
  void refresh();
  virtual QString id() const { return m_url; }
  virtual void removeAllSettings();
  virtual void saveItemsToDisk();
  void setLoading(bool val);
  bool isLoading() const;
  QString title() const;
  virtual void rename(const QString &alias);
  virtual QString displayName() const;
  QString url() const;
  QString icon() const;
  bool hasCustomIcon() const;
  void setIconPath(const QString &pathHierarchy);
  RssArticlePtr getItem(const QString &guid) const;
  uint count() const;
  virtual void markAsRead();
  virtual uint unreadCount() const;
  virtual RssArticleList articleList() const;
  const RssArticleHash& articleHash() const { return m_articles; }
  virtual RssArticleList unreadArticleList() const;

private slots:
  void handleFinishedDownload(const QString& url, const QString &file_path);
  void handleDownloadFailure(const QString &url, const QString& error);
  void handleFeedTitle(const QString& feedUrl, const QString& title);
  void handleNewArticle(const QString& feedUrl, const QVariantHash& article);
  void handleFeedParsingFinished(const QString& feedUrl, const QString& error);

private:
  void removeOldArticles();
  QString iconUrl() const;
  void loadItemsFromDisk();

private:
  RssManager* m_manager;
  RssArticleHash m_articles;
  RssFolder *m_parent;
  QString m_title;
  QString m_url;
  QString m_alias;
  QString m_icon;
  QString m_iconUrl;
  bool m_read;
  bool m_refreshed;
  bool m_inErrorState;
  bool m_loading;

};


#endif // RSSFEED_H
