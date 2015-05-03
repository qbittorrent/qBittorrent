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

#include <QDebug>
#include "rssfeed.h"
#include "rssmanager.h"
#include "core/bittorrent/session.h"
#include "rssfolder.h"
#include "core/preferences.h"
#include "core/qinisettings.h"
#include "rssarticle.h"
#include "rssparser.h"
#include "core/utils/misc.h"
#include "rssdownloadrulelist.h"
#include "core/net/downloadmanager.h"
#include "core/net/downloadhandler.h"
#include "core/utils/fs.h"
#include "core/logger.h"

bool rssArticleDateRecentThan(const RssArticlePtr& left, const RssArticlePtr& right)
{
  return left->date() > right->date();
}

RssFeed::RssFeed(RssManager* manager, RssFolder* parent, const QString& url):
  m_manager(manager),
  m_parent(parent),
  m_url (QUrl::fromEncoded(url.toUtf8()).toString()),
  m_icon(":/icons/oxygen/application-rss+xml.png"),
  m_unreadCount(0),
  m_dirty(false),
  m_inErrorState(false),
  m_loading(false)
{
  qDebug() << Q_FUNC_INFO << m_url;
  // Listen for new RSS downloads
  connect(manager->rssParser(), SIGNAL(feedTitle(QString,QString)), SLOT(handleFeedTitle(QString,QString)));
  connect(manager->rssParser(), SIGNAL(newArticle(QString,QVariantHash)), SLOT(handleNewArticle(QString,QVariantHash)));
  connect(manager->rssParser(), SIGNAL(feedParsingFinished(QString,QString)), SLOT(handleFeedParsingFinished(QString,QString)));

  // Download the RSS Feed icon
  Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(iconUrl());
  connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleFinishedDownload(QString, QString)));
  connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  m_iconUrl = handler->url();

  // Load old RSS articles
  loadItemsFromDisk();
}

RssFeed::~RssFeed()
{
  if (!m_icon.startsWith(":/") && QFile::exists(m_icon))
    Utils::Fs::forceRemove(m_icon);
}

void RssFeed::saveItemsToDisk()
{
  qDebug() << Q_FUNC_INFO << m_url;
  if (!m_dirty)
    return;
  markAsDirty(false);

  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QVariantList old_items;

  RssArticleHash::ConstIterator it = m_articles.begin();
  RssArticleHash::ConstIterator itend = m_articles.end();
  for ( ; it != itend; ++it) {
    old_items << it.value()->toHash();
  }
  qDebug("Saving %d old items for feed %s", old_items.size(), qPrintable(displayName()));
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  all_old_items[m_url] = old_items;
  qBTRSS.setValue("old_items", all_old_items);
}

void RssFeed::loadItemsFromDisk()
{
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  const QVariantList old_items = all_old_items.value(m_url, QVariantList()).toList();
  qDebug("Loading %d old items for feed %s", old_items.size(), qPrintable(displayName()));

  foreach (const QVariant& var_it, old_items) {
    QVariantHash item = var_it.toHash();
    RssArticlePtr rss_item = hashToRssArticle(this, item);
    if (rss_item)
      addArticle(rss_item);
  }
}

void RssFeed::addArticle(const RssArticlePtr& article) {
  int max_articles = Preferences::instance()->getRSSMaxArticlesPerFeed();

  if (!m_articles.contains(article->guid())) {
    markAsDirty();

    // Update unreadCount
    if (!article->isRead())
      ++m_unreadCount;
    // Insert in hash table
    m_articles[article->guid()] = article;
    // Insertion sort
    RssArticleList::Iterator lowerBound = qLowerBound(m_articlesByDate.begin(), m_articlesByDate.end(), article, rssArticleDateRecentThan);
    m_articlesByDate.insert(lowerBound, article);
    int lbIndex = m_articlesByDate.indexOf(article);
    if (m_articlesByDate.size() > max_articles) {
      RssArticlePtr oldestArticle = m_articlesByDate.takeLast();
      m_articles.remove(oldestArticle->guid());
      // Update unreadCount
      if (!oldestArticle->isRead())
        --m_unreadCount;
    }

    // Check if article was inserted at the end of the list and will break max_articles limit
    if (Preferences::instance()->isRssDownloadingEnabled()) {
      if (lbIndex < max_articles && !article->isRead())
        downloadArticleTorrentIfMatching(m_manager->downloadRules(), article);
    }
  }
  else {
    // m_articles.contains(article->guid())
    // Try to download skipped articles
    if (Preferences::instance()->isRssDownloadingEnabled()) {
      RssArticlePtr skipped = m_articles.value(article->guid(), RssArticlePtr());
      if (skipped) {
        if (!skipped->isRead())
          downloadArticleTorrentIfMatching(m_manager->downloadRules(), skipped);
      }
    }
  }
}

bool RssFeed::refresh()
{
  if (m_loading) {
    qWarning() << Q_FUNC_INFO << "Feed" << displayName() << "is already being refreshed, ignoring request";
    return false;
  }
  m_loading = true;
  // Download the RSS again
  Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(m_url);
  connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleFinishedDownload(QString, QString)));
  connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  m_url = handler->url(); // sync URL encoding
  return true;
}

void RssFeed::removeAllSettings()
{
  qDebug() << "Removing all settings / history for feed: " << m_url;
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QVariantHash feeds_w_downloader = qBTRSS.value("downloader_on", QVariantHash()).toHash();
  if (feeds_w_downloader.contains(m_url)) {
    feeds_w_downloader.remove(m_url);
    qBTRSS.setValue("downloader_on", feeds_w_downloader);
  }
  QVariantHash all_feeds_filters = qBTRSS.value("feed_filters", QVariantHash()).toHash();
  if (all_feeds_filters.contains(m_url)) {
    all_feeds_filters.remove(m_url);
    qBTRSS.setValue("feed_filters", all_feeds_filters);
  }
  QVariantHash all_old_items = qBTRSS.value("old_items", QVariantHash()).toHash();
  if (all_old_items.contains(m_url)) {
      all_old_items.remove(m_url);
      qBTRSS.setValue("old_items", all_old_items);
  }
}

bool RssFeed::isLoading() const
{
  return m_loading;
}

QString RssFeed::title() const
{
  return m_title;
}

void RssFeed::rename(const QString &new_name)
{
  qDebug() << "Renaming stream to" << new_name;
  m_alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssFeed::displayName() const
{
  if (!m_alias.isEmpty())
    return m_alias;
  if (!m_title.isEmpty())
    return m_title;
  return m_url;
}

QString RssFeed::url() const
{
  return m_url;
}

QIcon RssFeed::icon() const
{
  if (m_inErrorState)
    return QIcon(":/icons/oxygen/unavailable.png");

  return QIcon(m_icon);
}

bool RssFeed::hasCustomIcon() const
{
  return !m_icon.startsWith(":/");
}

void RssFeed::setIconPath(const QString& path)
{
  if (path.isEmpty() || !QFile::exists(path))
    return;

  m_icon = path;
}

RssArticlePtr RssFeed::getItem(const QString& guid) const
{
  return m_articles.value(guid);
}

uint RssFeed::count() const
{
  return m_articles.size();
}

void RssFeed::markAsRead()
{
  RssArticleHash::ConstIterator it = m_articles.begin();
  RssArticleHash::ConstIterator itend = m_articles.end();
  for ( ; it != itend; ++it) {
    it.value()->markAsRead();
  }
  m_unreadCount = 0;
  m_manager->forwardFeedInfosChanged(m_url, displayName(), 0);
}

void RssFeed::markAsDirty(bool dirty)
{
  m_dirty = dirty;
}

uint RssFeed::unreadCount() const
{
  return m_unreadCount;
}

RssArticleList RssFeed::articleListByDateDesc() const
{
  return m_articlesByDate;
}

RssArticleList RssFeed::unreadArticleListByDateDesc() const
{
  RssArticleList unread_news;

  RssArticleList::ConstIterator it = m_articlesByDate.begin();
  RssArticleList::ConstIterator itend = m_articlesByDate.end();
  for ( ; it != itend; ++it) {
    if (!(*it)->isRead())
      unread_news << *it;
  }
  return unread_news;
}

// download the icon from the address
QString RssFeed::iconUrl() const
{
  // XXX: This works for most sites but it is not perfect
  return QString("http://") + QUrl(m_url).host() + QString("/favicon.ico");
}

// read and store the downloaded rss' informations
void RssFeed::handleFinishedDownload(const QString &url, const QString &filePath)
{
  if (url == m_url) {
    qDebug() << Q_FUNC_INFO << "Successfully downloaded RSS feed at" << url;
    // Parse the download RSS
    m_manager->rssParser()->parseRssFile(m_url, filePath);
  } else if (url == m_iconUrl) {
    m_icon = filePath;
    qDebug() << Q_FUNC_INFO << "icon path:" << m_icon;
    m_manager->forwardFeedIconChanged(m_url, m_icon);
  }
}

void RssFeed::handleDownloadFailure(const QString &url, const QString &error)
{
  if (url != m_url) return;

  m_inErrorState = true;
  m_loading = false;
  m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
  qWarning() << "Failed to download RSS feed at" << url;
  qWarning() << "Reason:" << error;
}

void RssFeed::handleFeedTitle(const QString& feedUrl, const QString& title)
{
  if (feedUrl != m_url)
    return;

  if (m_title == title)
    return;

  m_title = title;

  // Notify that we now have something better than a URL to display
  if (m_alias.isEmpty())
    m_manager->forwardFeedInfosChanged(feedUrl, title, m_unreadCount);
}

void RssFeed::downloadArticleTorrentIfMatching(RssDownloadRuleList* rules, const RssArticlePtr& article)
{
  Q_ASSERT(Preferences::instance()->isRssDownloadingEnabled());
  RssDownloadRulePtr matching_rule = rules->findMatchingRule(m_url, article->title());
  if (!matching_rule)
    return;

  if (matching_rule->ignoreDays() > 0) {
    QDateTime lastMatch = matching_rule->lastMatch();
    if (lastMatch.isValid()) {
      if (QDateTime::currentDateTime() < lastMatch.addDays(matching_rule->ignoreDays())) {
        connect(article.data(), SIGNAL(articleWasRead()), SLOT(handleArticleStateChanged()), Qt::UniqueConnection);
        article->markAsRead();
        return;
      }
    }
  }
  matching_rule->setLastMatch(QDateTime::currentDateTime());
  rules->saveRulesToStorage();
  // Download the torrent
  const QString& torrent_url = article->torrentUrl();
  Logger::instance()->addMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(article->title()).arg(displayName()));
  connect(BitTorrent::Session::instance(), SIGNAL(downloadFromUrlFinished(QString)), article.data(), SLOT(handleTorrentDownloadSuccess(const QString&)), Qt::UniqueConnection);
  connect(article.data(), SIGNAL(articleWasRead()), SLOT(handleArticleStateChanged()), Qt::UniqueConnection);

  BitTorrent::AddTorrentParams params;
  params.savePath = matching_rule->savePath();
  params.label = matching_rule->label();
  if (matching_rule->addPaused() == RssDownloadRule::ALWAYS_PAUSED)
      params.addPaused = TriStateBool::True;
  else if (matching_rule->addPaused() == RssDownloadRule::NEVER_PAUSED)
      params.addPaused = TriStateBool::False;
  BitTorrent::Session::instance()->addTorrent(torrent_url, params);
}

void RssFeed::recheckRssItemsForDownload()
{
  Q_ASSERT(Preferences::instance()->isRssDownloadingEnabled());
  RssDownloadRuleList* rules = m_manager->downloadRules();
  foreach (const RssArticlePtr& article, m_articlesByDate) {
    if (!article->isRead())
      downloadArticleTorrentIfMatching(rules, article);
  }
}

void RssFeed::handleNewArticle(const QString& feedUrl, const QVariantHash& articleData)
{
  if (feedUrl != m_url)
    return;

  RssArticlePtr article = hashToRssArticle(this, articleData);
  if (article.isNull()) {
    qDebug() << "Article hash corrupted or guid is uncomputable; feed url: " << feedUrl;
    return;
  }
  Q_ASSERT(article);
  addArticle(article);

  m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
  // FIXME: We should forward the information here but this would seriously decrease
  // performance with current design.
  //m_manager->forwardFeedContentChanged(m_url);
}

void RssFeed::handleFeedParsingFinished(const QString& feedUrl, const QString& error)
{
  if (feedUrl != m_url)
    return;

  if (!error.isEmpty()) {
    qWarning() << "Failed to parse RSS feed at" << feedUrl;
    qWarning() << "Reason:" << error;
  }

  m_loading = false;
  m_inErrorState = !error.isEmpty();

  m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
  // XXX: Would not be needed if we did this in handleNewArticle() instead
  m_manager->forwardFeedContentChanged(m_url);

  saveItemsToDisk();
}

void RssFeed::handleArticleStateChanged()
{
  m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
}

void RssFeed::decrementUnreadCount()
{
  --m_unreadCount;
}
