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

#include <QFile>
#include <QDataStream>
#include <QDebug>

#include "rssdownloadrulelist.h"
#include "core/preferences.h"
#include "core/qinisettings.h"

RssDownloadRuleList::RssDownloadRuleList()
{
  loadRulesFromStorage();
}

RssDownloadRulePtr RssDownloadRuleList::findMatchingRule(const QString &feed_url, const QString &article_title) const
{
  Q_ASSERT(Preferences::instance()->isRssDownloadingEnabled());
  QStringList rule_names = m_feedRules.value(feed_url);
  foreach (const QString &rule_name, rule_names) {
    RssDownloadRulePtr rule = m_rules[rule_name];
    if (rule->isEnabled() && rule->matches(article_title)) return rule;
  }
  return RssDownloadRulePtr();
}

void RssDownloadRuleList::replace(RssDownloadRuleList *other) {
  m_rules.clear();
  m_feedRules.clear();
  foreach (const QString& name, other->ruleNames()) {
    saveRule(other->getRule(name));
  }
}

void RssDownloadRuleList::saveRulesToStorage()
{
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  qBTRSS.setValue("download_rules", toVariantHash());
}

void RssDownloadRuleList::loadRulesFromStorage()
{
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  loadRulesFromVariantHash(qBTRSS.value("download_rules").toHash());
}

QVariantHash RssDownloadRuleList::toVariantHash() const
{
  QVariantHash ret;
  foreach (const RssDownloadRulePtr &rule, m_rules.values()) {
    ret.insert(rule->name(), rule->toVariantHash());
  }
  return ret;
}

void RssDownloadRuleList::loadRulesFromVariantHash(const QVariantHash &h)
{
  QVariantHash::ConstIterator it = h.begin();
  QVariantHash::ConstIterator itend = h.end();
  for ( ; it != itend; ++it) {
    RssDownloadRulePtr rule = RssDownloadRule::fromVariantHash(it.value().toHash());
    if (rule && !rule->name().isEmpty())
      saveRule(rule);
  }
}

void RssDownloadRuleList::saveRule(const RssDownloadRulePtr &rule)
{
  qDebug() << Q_FUNC_INFO << rule->name();
  Q_ASSERT(rule);
  if (m_rules.contains(rule->name())) {
    qDebug("This is an update, removing old rule first");
    removeRule(rule->name());
  }
  m_rules.insert(rule->name(), rule);
  // Update feedRules hashtable
  foreach (const QString &feed_url, rule->rssFeeds()) {
    m_feedRules[feed_url].append(rule->name());
  }
  qDebug() << Q_FUNC_INFO << "EXIT";
}

void RssDownloadRuleList::removeRule(const QString &name)
{
  qDebug() << Q_FUNC_INFO << name;
  if (!m_rules.contains(name)) return;
  RssDownloadRulePtr rule = m_rules.take(name);
  // Update feedRules hashtable
  foreach (const QString &feed_url, rule->rssFeeds()) {
    m_feedRules[feed_url].removeOne(rule->name());
  }
}

void RssDownloadRuleList::renameRule(const QString &old_name, const QString &new_name)
{
  if (!m_rules.contains(old_name)) return;
  RssDownloadRulePtr rule = m_rules.take(old_name);
  rule->setName(new_name);
  m_rules.insert(new_name, rule);
  // Update feedRules hashtable
  foreach (const QString &feed_url, rule->rssFeeds()) {
    m_feedRules[feed_url].replace(m_feedRules[feed_url].indexOf(old_name), new_name);
  }
}

RssDownloadRulePtr RssDownloadRuleList::getRule(const QString &name) const
{
  return m_rules.value(name);
}

bool RssDownloadRuleList::serialize(const QString& path)
{
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) {
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_4_5);
    out << toVariantHash();
    f.close();
    return true;
  } else {
    return false;
  }
}

bool RssDownloadRuleList::unserialize(const QString &path)
{
  QFile f(path);
  if (f.open(QIODevice::ReadOnly)) {
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_5);
    QVariantHash tmp;
    in >> tmp;
    f.close();
    if (tmp.isEmpty())
      return false;
    qDebug("Processing was successful!");
    loadRulesFromVariantHash(tmp);
    return true;
  } else {
    qDebug("Error: could not open file at %s", qPrintable(path));
    return false;
  }
}

