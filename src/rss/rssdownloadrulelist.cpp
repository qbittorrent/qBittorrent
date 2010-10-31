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

#include "rssdownloadrulelist.h"
#include "qinisettings.h"

RssDownloadRuleList* RssDownloadRuleList::m_instance = 0;

RssDownloadRuleList::RssDownloadRuleList(){
  loadRulesFromStorage();
}

RssDownloadRuleList* RssDownloadRuleList::instance()
{
  if(!m_instance)
    m_instance = new RssDownloadRuleList;
  return m_instance;
}

void RssDownloadRuleList::drop()
{
  if(m_instance)
    delete m_instance;
}

const RssDownloadRule * RssDownloadRuleList::findMatchingRule(const QString &feed_url, const QString &article_title) const
{
  const QList<RssDownloadRule*> rules = feedRules(feed_url);
  foreach(const RssDownloadRule* rule, rules) {
    if(rule->matches(article_title)) return rule;
  }
  return 0;
}

void RssDownloadRuleList::loadRulesFromStorage()
{
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  if(qBTRSS.contains("feed_filters")) {
    importRulesInOldFormat(qBTRSS.value("feed_filters").toHash());
  }
}

void RssDownloadRuleList::importRulesInOldFormat(const QHash<QString, QVariant> &rules)
{
  foreach(const QString &feed_url, rules.keys()) {
    const QHash<QString, QVariant> feed_rules = rules.value(feed_url).toHash();
    foreach(const QString &rule_name, feed_rules.keys()) {
      const RssDownloadRule rule = RssDownloadRule::fromOldFormat(feed_rules.value(rule_name).toHash(), feed_url, rule_name);
      append(rule);
    }
  }
}

void RssDownloadRuleList::append(const RssDownloadRule &rule)
{
  Q_ASSERT(!contains(rule));
  QList<RssDownloadRule>::append(rule);
  foreach(const QString &feed_url, rule.rssFeeds()) {
    m_feedRules[feed_url].append(&last());
  }
}
