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

#include "rssfile.h"

class RssManager;

class RssFeed: public RssFile, public QHash<QString, RssArticle> {
  Q_OBJECT

public:
  RssFeed(RssFolder* parent, QString _url);
  ~RssFeed();
  RssFolder* getParent() const { return parent; }
  void setParent(RssFolder* _parent) { parent = _parent; }
  FileType getType() const;
  void refresh();
  QString getID() const { return url; }
  void removeAllSettings();
  bool itemAlreadyExists(QString hash);
  void setLoading(bool val);
  bool isLoading();
  QString getTitle() const;
  void rename(QString _alias);
  QString getName() const;
  QString getLink() const;
  QString getUrl() const;
  QString getDescription() const;
  QString getImage() const;
  QString getFilePath() const;
  QString getIconPath() const;
  bool hasCustomIcon() const;
  void setIconPath(QString path);
  RssArticle& getItem(QString name);
  unsigned int getNbNews() const;
  void markAllAsRead();
  unsigned int getNbUnRead() const;
  QList<RssArticle> getNewsList() const;
  QList<RssArticle> getUnreadNewsList() const;
  QString getIconUrl();

public slots:
  void processDownloadedFile(QString file_path);
  void setDownloadFailed();

private:
  short readDoc(QIODevice* device);
  void resizeList();
  short openRss();

private:
  RssFolder *parent;
  QString title;
  QString link;
  QString description;
  QString image;
  QString url;
  QString alias;
  QString filePath;
  QString iconPath;
  bool read;
  bool refreshed;
  bool downloadFailure;
  bool currently_loading;

};


#endif // RSSFEED_H
