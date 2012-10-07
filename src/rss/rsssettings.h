/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#ifndef RSSSETTINGS_H
#define RSSSETTINGS_H

#include <QStringList>
#include <QNetworkCookie>
#include "qinisettings.h"

class RssSettings: public QIniSettings{

public:
  RssSettings() : QIniSettings("qBittorrent", "qBittorrent") {}

  bool isRSSEnabled() const {
    return value(QString::fromUtf8("Preferences/RSS/RSSEnabled"), false).toBool();
  }

  void setRSSEnabled(bool enabled) {
    setValue(QString::fromUtf8("Preferences/RSS/RSSEnabled"), enabled);
  }

  unsigned int getRSSRefreshInterval() const {
    return value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toUInt();
  }

  void setRSSRefreshInterval(uint interval) {
    setValue(QString::fromUtf8("Preferences/RSS/RSSRefresh"), interval);
  }

  int getRSSMaxArticlesPerFeed() const {
    return value(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), 50).toInt();
  }

  void setRSSMaxArticlesPerFeed(int nb) {
    setValue(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), nb);
  }

  bool isRssDownloadingEnabled() const {
    return value("Preferences/RSS/RssDownloading", true).toBool();
  }

  void setRssDownloadingEnabled(bool b) {
    setValue("Preferences/RSS/RssDownloading", b);
  }

  QStringList getRssFeedsUrls() const {
    return value("Rss/streamList").toStringList();
  }

  void setRssFeedsUrls(const QStringList &rssFeeds) {
    setValue("Rss/streamList", rssFeeds);
  }

  QStringList getRssFeedsAliases() const {
    return value("Rss/streamAlias").toStringList();
  }

  void setRssFeedsAliases(const QStringList &rssAliases) {
    setValue("Rss/streamAlias", rssAliases);
  }

  QList<QByteArray> getHostNameCookies(const QString &host_name) const {
    QMap<QString, QVariant> hosts_table = value("Rss/hosts_cookies").toMap();
    if (!hosts_table.contains(host_name)) return QList<QByteArray>();
    QByteArray raw_cookies = hosts_table.value(host_name).toByteArray();
    return raw_cookies.split(':');
  }

  QList<QNetworkCookie> getHostNameQNetworkCookies(const QString& host_name) const {
    QList<QNetworkCookie> cookies;
    const QList<QByteArray> raw_cookies = getHostNameCookies(host_name);
    foreach (const QByteArray& raw_cookie, raw_cookies) {
      QList<QByteArray> cookie_parts = raw_cookie.split('=');
      if (cookie_parts.size() == 2) {
        qDebug("Loading cookie: %s = %s", cookie_parts.first().constData(), cookie_parts.last().constData());
        cookies << QNetworkCookie(cookie_parts.first(), cookie_parts.last());
      }
    }
    return cookies;
  }

  void setHostNameCookies(const QString &host_name, const QList<QByteArray> &cookies) {
    QMap<QString, QVariant> hosts_table = value("Rss/hosts_cookies").toMap();
    QByteArray raw_cookies = "";
    foreach (const QByteArray& cookie, cookies) {
      raw_cookies += cookie + ":";
    }
    if (raw_cookies.endsWith(":"))
      raw_cookies.chop(1);
    hosts_table.insert(host_name, raw_cookies);
    setValue("Rss/hosts_cookies", hosts_table);
  }
};

#endif // RSSSETTINGS_H
