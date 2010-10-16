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

#include <QFile>

#include "preferences.h"
#include "rssfilters.h"

// RssFilter

RssFilter::RssFilter(): valid(true) {

}

RssFilter::RssFilter(bool valid): valid(valid) {

}

RssFilter::RssFilter(QHash<QString, QVariant> filter):
  QHash<QString, QVariant>(filter), valid(true) {

}

bool RssFilter::matches(QString s) {
  QStringList match_tokens = getMatchingTokens();
  foreach(const QString& token, match_tokens) {
    if(token.isEmpty() || token == "")
      continue;
    QRegExp reg(token, Qt::CaseInsensitive, QRegExp::Wildcard);
    //reg.setMinimal(false);
    if(reg.indexIn(s) < 0) return false;
  }
  qDebug("Checking not matching tokens");
  // Checking not matching
  QStringList notmatch_tokens = getNotMatchingTokens();
  foreach(const QString& token, notmatch_tokens) {
    if(token.isEmpty()) continue;
    QRegExp reg(token, Qt::CaseInsensitive, QRegExp::Wildcard);
    if(reg.indexIn(s) > -1) return false;
  }
  return true;
}

bool RssFilter::isValid() const {
  return valid;
}

QStringList RssFilter::getMatchingTokens() const {
  QString matches = this->value("matches", "").toString();
  return matches.split(" ");
}

QString RssFilter::getMatchingTokens_str() const {
  return this->value("matches", "").toString();
}

void RssFilter::setMatchingTokens(QString tokens) {
  tokens = tokens.trimmed();
  if(tokens.isEmpty())
    (*this)["matches"] = "";
  else
    (*this)["matches"] = tokens;
}

QStringList RssFilter::getNotMatchingTokens() const {
  QString notmatching = this->value("not", "").toString();
  return notmatching.split(QRegExp("[\\s|]"));
}

QString RssFilter::getNotMatchingTokens_str() const {
  return this->value("not", "").toString();
}

void RssFilter::setNotMatchingTokens(QString tokens) {
  (*this)["not"] = tokens.trimmed();
}

QString RssFilter::getSavePath() const {
  return this->value("save_path", "").toString();
}

void RssFilter::setSavePath(QString save_path) {
  (*this)["save_path"] = save_path;
}

// RssFilters

RssFilters::RssFilters() {

}

RssFilters::RssFilters(QString feed_url, QHash<QString, QVariant> filters):
  QHash<QString, QVariant>(filters), feed_url(feed_url) {

}

bool RssFilters::hasFilter(QString name) const {
  return this->contains(name);
}

RssFilter* RssFilters::matches(QString s) {
  if(!isDownloadingEnabled()) return 0;
  if(this->size() == 0) return new RssFilter(false);
  foreach(QVariant var_hash_filter, this->values()) {
    QHash<QString, QVariant> hash_filter = var_hash_filter.toHash();
    RssFilter *filter = new RssFilter(hash_filter);
    if(filter->matches(s))
      return filter;
    else
      delete filter;
  }
  return 0;
}

QStringList RssFilters::names() const {
  return this->keys();
}

RssFilter RssFilters::getFilter(QString name) const {
  if(this->contains(name))
    return RssFilter(this->value(name).toHash());
  return RssFilter();
}

void RssFilters::setFilter(QString name, RssFilter f) {
  (*this)[name] = f;
}

bool RssFilters::isDownloadingEnabled() const {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
  return feeds_w_downloader.value(feed_url, false).toBool();
}

void RssFilters::setDownloadingEnabled(bool enabled) {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
  feeds_w_downloader[feed_url] = enabled;
  qBTRSS.setValue("downloader_on", feeds_w_downloader);
}

RssFilters RssFilters::getFeedFilters(QString url) {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
  return RssFilters(url, all_feeds_filters.value(url, QHash<QString, QVariant>()).toHash());
}

void RssFilters::rename(QString old_name, QString new_name) {
  Q_ASSERT(this->contains(old_name));
  (*this)[new_name] = this->take(old_name);
}

bool RssFilters::serialize(QString path) {
  QFile f(path);
  if(f.open(QIODevice::WriteOnly)) {
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_4_3);
    out << (QHash<QString, QVariant>)(*this);
    f.close();
    return true;
  } else {
    return false;
  }
}

bool RssFilters::unserialize(QString path) {
  QFile f(path);
  if(f.open(QIODevice::ReadOnly)) {
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_3);
    QHash<QString, QVariant> tmp;
    in >> tmp;
    qDebug("Unserialized %d filters", tmp.size());
    foreach(const QString& key, tmp.keys()) {
      (*this)[key] = tmp[key];
    }
    return true;
  } else {
    return false;
  }
}

void RssFilters::save() {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
  qDebug("Saving filters for feed: %s (%d filters)", qPrintable(feed_url), (*this).size());
  all_feeds_filters[feed_url] = *this;
  qBTRSS.setValue("feed_filters", all_feeds_filters);
}
