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

#ifndef RSSDOWNLOADERFILTERS_H
#define RSSDOWNLOADERFILTERS_H

#include <QHash>
#include <QStringList>
#include <QVariant>

class RssFilter: public QHash<QString, QVariant> {
private:
  bool valid;
public:
  RssFilter();
  RssFilter(bool valid);
  RssFilter(QHash<QString, QVariant> filter);
  bool matches(QString s);
  bool isValid() const;
  QStringList getMatchingTokens() const;
  QString getMatchingTokens_str() const;
  void setMatchingTokens(QString tokens);
  QStringList getNotMatchingTokens() const;
  QString getNotMatchingTokens_str() const;
  void setNotMatchingTokens(QString tokens);
  QString getSavePath() const;
  void setSavePath(QString save_path);
};

class RssFilters : public QHash<QString, QVariant> {

public:
  RssFilters();
  RssFilters(QString feed_url, QHash<QString, QVariant> filters);
  bool hasFilter(QString name) const;
  RssFilter* matches(QString s);
  QStringList names() const;
  RssFilter getFilter(QString name) const;
  void setFilter(QString name, RssFilter f);
  bool isDownloadingEnabled() const;
  void setDownloadingEnabled(bool enabled);
  static RssFilters getFeedFilters(QString url);
  void rename(QString old_name, QString new_name);
  bool serialize(QString path);
  bool unserialize(QString path);
  void save();

private:
  QString feed_url;

};

#endif // RSSDOWNLOADERFILTERS_H
