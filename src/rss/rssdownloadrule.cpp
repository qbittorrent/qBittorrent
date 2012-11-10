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
#include <QDebug>

#include "rssdownloadrule.h"
#include "preferences.h"
#include "qinisettings.h"
#include "rssfeed.h"
#include "rssarticle.h"

RssDownloadRule::RssDownloadRule(): m_enabled(false), m_useRegex(false)
{
}

bool RssDownloadRule::matches(const QString &article_title) const
{
  foreach (const QString& token, m_mustContain) {
    if (!token.isEmpty()) {
      QRegExp reg(token, Qt::CaseInsensitive, m_useRegex ? QRegExp::RegExp : QRegExp::Wildcard);
      if (reg.indexIn(article_title) < 0)
        return false;
    }
  }
  qDebug("Checking not matching tokens");
  // Checking not matching
  foreach (const QString& token, m_mustNotContain) {
    if (!token.isEmpty()) {
      QRegExp reg(token, Qt::CaseInsensitive, m_useRegex ? QRegExp::RegExp : QRegExp::Wildcard);
      if (reg.indexIn(article_title) > -1)
        return false;
    }
  }
  return true;
}

void RssDownloadRule::setMustContain(const QString &tokens)
{
  if (m_useRegex)
    m_mustContain = QStringList() << tokens;
  else
    m_mustContain = tokens.split(" ");
}

void RssDownloadRule::setMustNotContain(const QString &tokens)
{
  if (m_useRegex)
    m_mustNotContain = QStringList() << tokens;
  else
    m_mustNotContain = tokens.split(QRegExp("[\\s|]"));
}

RssDownloadRulePtr RssDownloadRule::fromVariantHash(const QVariantHash &rule_hash)
{
  RssDownloadRulePtr rule(new RssDownloadRule);
  rule->setName(rule_hash.value("name").toString());
  rule->setUseRegex(rule_hash.value("use_regex", false).toBool());
  rule->setMustContain(rule_hash.value("must_contain").toString());
  rule->setMustNotContain(rule_hash.value("must_not_contain").toString());
  rule->setRssFeeds(rule_hash.value("affected_feeds").toStringList());
  rule->setEnabled(rule_hash.value("enabled", false).toBool());
  rule->setSavePath(rule_hash.value("save_path").toString());
  rule->setLabel(rule_hash.value("label_assigned").toString());
  return rule;
}

QVariantHash RssDownloadRule::toVariantHash() const
{
  QVariantHash hash;
  hash["name"] = m_name;
  hash["must_contain"] = m_mustContain.join(" ");
  hash["must_not_contain"] = m_mustNotContain.join(" ");
  hash["save_path"] = m_savePath;
  hash["affected_feeds"] = m_rssFeeds;
  hash["enabled"] = m_enabled;
  hash["label_assigned"] = m_label;
  hash["use_regex"] = m_useRegex;
  return hash;
}

bool RssDownloadRule::operator==(const RssDownloadRule &other) {
  return m_name == other.name();
}

void RssDownloadRule::setSavePath(const QString &save_path)
{
  if (!save_path.isEmpty() && QDir(save_path) != QDir(Preferences().getSavePath()))
    m_savePath = save_path;
  else
    m_savePath = QString();
}

QStringList RssDownloadRule::findMatchingArticles(const RssFeedPtr& feed) const
{
  QStringList ret;
  const RssArticleHash& feed_articles = feed->articleHash();

  RssArticleHash::ConstIterator artIt = feed_articles.begin();
  RssArticleHash::ConstIterator artItend = feed_articles.end();
  for ( ; artIt != artItend ; ++artIt) {
    const QString title = artIt.value()->title();
    if (matches(title))
      ret << title;
  }
  return ret;
}
