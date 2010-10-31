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

#include <QRegExp>

#include "rssdownloadrule.h"
#include "qinisettings.h"

RssDownloadRule::RssDownloadRule()
{
}

bool RssDownloadRule::matches(const QString &article_title) const
{
  foreach(const QString& token, m_mustContain) {
    if(token.isEmpty() || token == "")
      continue;
    QRegExp reg(token, Qt::CaseInsensitive, QRegExp::Wildcard);
    //reg.setMinimal(false);
    if(reg.indexIn(article_title) < 0) return false;
  }
  qDebug("Checking not matching tokens");
  // Checking not matching
  foreach(const QString& token, m_mustNotContain) {
    if(token.isEmpty()) continue;
    QRegExp reg(token, Qt::CaseInsensitive, QRegExp::Wildcard);
    if(reg.indexIn(article_title) > -1) return false;
  }
  return true;
}

void RssDownloadRule::setMustContain(const QString &tokens)
{
  m_mustContain = tokens.split(" ");
}

void RssDownloadRule::setMustNotContain(const QString &tokens)
{
  m_mustNotContain = tokens.split(QRegExp("[\\s|]"));
}

RssDownloadRule RssDownloadRule::fromOldFormat(const QHash<QString, QVariant> &rule_hash, const QString &feed_url, const QString &rule_name)
{
  RssDownloadRule rule;
  rule.setName(rule_name);
  rule.setMustContain(rule_hash.value("matches", "").toString());
  rule.setMustNotContain(rule_hash.value("not", "").toString());
  rule.setRssFeeds(QStringList() << feed_url);
  rule.setSavePath(rule_hash.value("save_path", "").toString());
  // Is enabled?
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  const QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on").toHash();
  rule.setEnabled(feeds_w_downloader.value(feed_url, false).toBool());
  // label was unsupported < 2.5.0
  return rule;
}

bool RssDownloadRule::operator==(const RssDownloadRule &other) {
  return m_name == other.name();
}
