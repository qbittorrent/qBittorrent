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
#include <QDir>

#include "rssdownloadrule.h"
#include "core/preferences.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "core/utils/fs.h"

RssDownloadRule::RssDownloadRule(): m_enabled(false), m_useRegex(false), m_apstate(USE_GLOBAL)
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
  if (!m_episodeFilter.isEmpty()) {
    qDebug("Checking episode filter");
    QRegExp f("(^\\d{1,4})x(.*;$)");
    int pos = f.indexIn(m_episodeFilter);
    if (pos < 0)
      return false;

    QString s = f.cap(1);
    QStringList eps = f.cap(2).split(";");
    QString expStr;
    expStr += "s0?" + s + "[ -_\\.]?" + "e0?";

    foreach (const QString& ep, eps) {
      if (ep.isEmpty())
        continue;

      if (ep.indexOf('-') != -1) { // Range detected
        QString partialPattern = "s0?" + s + "[ -_\\.]?" + "e(0?\\d{1,4})";
        QRegExp reg(partialPattern, Qt::CaseInsensitive);

        if (ep.endsWith('-')) { // Infinite range
          int epOurs = ep.left(ep.size() - 1).toInt();

          // Extract partial match from article and compare as digits
          pos = reg.indexIn(article_title);
          if (pos != -1) {
            int epTheirs = reg.cap(1).toInt();
            if (epTheirs >= epOurs)
              return true;
          }
        }
        else { // Normal range
          QStringList range = ep.split('-');
          Q_ASSERT(range.size() == 2);
          if (range.first().toInt() > range.last().toInt())
            continue; // Ignore this subrule completely

          int epOursFirst = range.first().toInt();
          int epOursLast = range.last().toInt();

          // Extract partial match from article and compare as digits
          pos = reg.indexIn(article_title);
          if (pos != -1) {
            int epTheirs = reg.cap(1).toInt();
            if (epOursFirst <= epTheirs && epOursLast >= epTheirs)
              return true;
          }
        }
      }
      else { // Single number
        QRegExp reg(expStr + ep + "\\D", Qt::CaseInsensitive);
        if (reg.indexIn(article_title) != -1)
          return true;
      }
    }
    return false;
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
    m_mustNotContain = tokens.split("|");
}

RssDownloadRulePtr RssDownloadRule::fromVariantHash(const QVariantHash &rule_hash)
{
  RssDownloadRulePtr rule(new RssDownloadRule);
  rule->setName(rule_hash.value("name").toString());
  rule->setUseRegex(rule_hash.value("use_regex", false).toBool());
  rule->setMustContain(rule_hash.value("must_contain").toString());
  rule->setMustNotContain(rule_hash.value("must_not_contain").toString());
  rule->setEpisodeFilter(rule_hash.value("episode_filter").toString());
  rule->setRssFeeds(rule_hash.value("affected_feeds").toStringList());
  rule->setEnabled(rule_hash.value("enabled", false).toBool());
  rule->setSavePath(rule_hash.value("save_path").toString());
  rule->setLabel(rule_hash.value("label_assigned").toString());
  rule->setAddPaused(AddPausedState(rule_hash.value("add_paused").toUInt()));
  rule->setLastMatch(rule_hash.value("last_match").toDateTime());
  rule->setIgnoreDays(rule_hash.value("ignore_days").toInt());
  return rule;
}

QVariantHash RssDownloadRule::toVariantHash() const
{
  QVariantHash hash;
  hash["name"] = m_name;
  hash["must_contain"] = m_mustContain.join(" ");
  hash["must_not_contain"] = m_mustNotContain.join("|");
  hash["save_path"] = m_savePath;
  hash["affected_feeds"] = m_rssFeeds;
  hash["enabled"] = m_enabled;
  hash["label_assigned"] = m_label;
  hash["use_regex"] = m_useRegex;
  hash["add_paused"] = m_apstate;
  hash["episode_filter"] = m_episodeFilter;
  hash["last_match"] = m_lastMatch;
  hash["ignore_days"] = m_ignoreDays;
  return hash;
}

bool RssDownloadRule::operator==(const RssDownloadRule &other) const {
  return m_name == other.name();
}

void RssDownloadRule::setSavePath(const QString &save_path)
{
  if (!save_path.isEmpty() && QDir(save_path) != QDir(Preferences::instance()->getSavePath()))
    m_savePath = Utils::Fs::fromNativePath(save_path);
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
