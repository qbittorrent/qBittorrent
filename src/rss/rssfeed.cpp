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
#include "qbtsession.h"
#include "rssfolder.h"
#include "rsssettings.h"
#include "rssarticle.h"
#include "misc.h"
#include "rssdownloadrulelist.h"
#include "downloadthread.h"
#include "fs_utils.h"

RssFeed::RssFeed(RssManager* manager, RssFolder* parent, const QString &url):
  m_manager(manager), m_parent(parent), m_icon(":/Icons/oxygen/application-rss+xml.png"),
  m_refreshed(false), m_downloadFailure(false), m_loading(false) {
  qDebug() << Q_FUNC_INFO << url;
  m_url = QUrl::fromEncoded(url.toUtf8()).toString();
  // Listen for new RSS downloads
  connect(manager->rssDownloader(), SIGNAL(downloadFinished(QString,QString)), SLOT(handleFinishedDownload(QString,QString)));
  connect(manager->rssDownloader(), SIGNAL(downloadFailure(QString,QString)), SLOT(handleDownloadFailure(QString,QString)));
  // Download the RSS Feed icon
  m_iconUrl = iconUrl();
  manager->rssDownloader()->downloadUrl(m_iconUrl);

  // Load old RSS articles
  loadItemsFromDisk();
}

RssFeed::~RssFeed() {
  if (!m_icon.startsWith(":/") && QFile::exists(m_icon))
    fsutils::forceRemove(m_icon);
}

void RssFeed::saveItemsToDisk() {
  qDebug() << Q_FUNC_INFO << m_url;
  if (!m_refreshed)
    return;
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QVariantList old_items;

  RssArticleHash::ConstIterator it=m_articles.begin();
  RssArticleHash::ConstIterator itend=m_articles.end();
  for ( ; it != itend; ++it) {
    old_items << it.value()->toHash();
  }
  qDebug("Saving %d old items for feed %s", old_items.size(), displayName().toLocal8Bit().data());
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  all_old_items[m_url] = old_items;
  qBTRSS.setValue("old_items", all_old_items);
}

void RssFeed::loadItemsFromDisk() {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  const QVariantList old_items = all_old_items.value(m_url, QVariantList()).toList();
  qDebug("Loading %d old items for feed %s", old_items.size(), displayName().toLocal8Bit().data());

  foreach (const QVariant &var_it, old_items) {
    QHash<QString, QVariant> item = var_it.toHash();
    RssArticlePtr rss_item = hashToRssArticle(this, item);
    if (rss_item) {
      m_articles.insert(rss_item->guid(), rss_item);
    }
  }
}

QList<QNetworkCookie> RssFeed::feedCookies() const
{
  QString feed_hostname = QUrl::fromEncoded(m_url.toUtf8()).host();
  return RssSettings().getHostNameQNetworkCookies(feed_hostname);
}

void RssFeed::refresh() {
  if (m_loading) {
    qWarning() << Q_FUNC_INFO << "Feed" << this->displayName() << "is already being refreshed, ignoring request";
    return;
  }
  m_loading = true;
  // Download the RSS again
  m_manager->rssDownloader()->downloadUrl(m_url, feedCookies());
}

void RssFeed::removeAllSettings() {
  qDebug() << "Removing all settings / history for feed: " << m_url;
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
  if (feeds_w_downloader.contains(m_url)) {
    feeds_w_downloader.remove(m_url);
    qBTRSS.setValue("downloader_on", feeds_w_downloader);
  }
  QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
  if (all_feeds_filters.contains(m_url)) {
    all_feeds_filters.remove(m_url);
    qBTRSS.setValue("feed_filters", all_feeds_filters);
  }
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  if (all_old_items.contains(m_url)) {
      all_old_items.remove(m_url);
      qBTRSS.setValue("old_items", all_old_items);
  }
}

void RssFeed::setLoading(bool val) {
  m_loading = val;
}

bool RssFeed::isLoading() const {
  return m_loading;
}

QString RssFeed::title() const {
  return m_title;
}

void RssFeed::rename(const QString &new_name) {
  qDebug() << "Renaming stream to" << new_name;
  m_alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssFeed::displayName() const {
  if (!m_alias.isEmpty())
    return m_alias;
  if (!m_title.isEmpty())
    return m_title;
  return m_url;
}

QString RssFeed::url() const {
  return m_url;
}

QString RssFeed::icon() const {
  if (m_downloadFailure)
    return ":/Icons/oxygen/unavailable.png";
  return m_icon;
}

bool RssFeed::hasCustomIcon() const {
  return !m_icon.startsWith(":/");
}

void RssFeed::setIconPath(const QString &path) {
  if (path.isEmpty() || !QFile::exists(path)) return;
  m_icon = path;
}

RssArticlePtr RssFeed::getItem(const QString &guid) const {
  return m_articles.value(guid);
}

uint RssFeed::count() const {
  return m_articles.size();
}

void RssFeed::markAsRead() {
  RssArticleHash::ConstIterator it=m_articles.begin();
  RssArticleHash::ConstIterator itend=m_articles.end();
  for ( ; it != itend; ++it) {
    it.value()->markAsRead();
  }
  m_manager->forwardFeedInfosChanged(m_url, displayName(), 0);
}

uint RssFeed::unreadCount() const {
  uint nbUnread = 0;

  RssArticleHash::ConstIterator it=m_articles.begin();
  RssArticleHash::ConstIterator itend=m_articles.end();
  for ( ; it != itend; ++it) {
    if (!it.value()->isRead())
      ++nbUnread;
  }
  return nbUnread;
}

RssArticleList RssFeed::articleList() const {
  return m_articles.values();
}

RssArticleList RssFeed::unreadArticleList() const {
  RssArticleList unread_news;

  RssArticleHash::ConstIterator it = m_articles.begin();
  RssArticleHash::ConstIterator itend = m_articles.end();
  for ( ; it != itend; ++it) {
    if (!it.value()->isRead())
      unread_news << it.value();
  }
  return unread_news;
}

// download the icon from the adress
QString RssFeed::iconUrl() const {
  // XXX: This works for most sites but it is not perfect
  return QString("http://")+QUrl(m_url).host()+QString("/favicon.ico");
}

void RssFeed::parseRSSChannel(QXmlStreamReader& xml)
{
  qDebug() << Q_FUNC_INFO;
  Q_ASSERT(xml.isStartElement() && xml.name() == "channel");

  while(!xml.atEnd()) {
    xml.readNext();

    if (xml.isStartElement()) {
      if (xml.name() == "title") {
        m_title = xml.readElementText();
        if (m_alias == url())
          rename(m_title);
      }
      else if (xml.name() == "image") {
        QString icon_path = xml.attributes().value("url").toString();
        if (!icon_path.isEmpty()) {
          m_iconUrl = icon_path;
          m_manager->rssDownloader()->downloadUrl(m_iconUrl);
        }
      }
      else if (xml.name() == "item") {
        RssArticlePtr article = xmlToRssArticle(this, xml);
        qDebug() << "Found RSS Item, valid: " << (article ? "True" : "False");
        if (article) {
          QString guid = article->guid();
          if (m_articles.contains(guid) && m_articles[guid]->isRead())
            article->markAsRead();
          m_articles[guid] = article;
        }
      }
    }
  }
}

// read and create items from a rss document
bool RssFeed::parseRSS(QIODevice* device)
{
  qDebug("Parsing RSS file...");
  QXmlStreamReader xml(device);

  bool found_channel = false;
  while (xml.readNextStartElement()) {
    if (xml.name() == "rss") {
      // Find channels
      while (xml.readNextStartElement()) {
        if (xml.name() == "channel") {
          parseRSSChannel(xml);
          found_channel = true;
          break;
        } else {
          qDebug() << "Skip rss item: " << xml.name();
          xml.skipCurrentElement();
        }
      }
      break;
    } else {
      qDebug() << "Skip root item: " << xml.name();
      xml.skipCurrentElement();
    }
  }

  if (xml.hasError()) {
    qWarning() << "Error parsing RSS document: " << xml.errorString();
  }

  if (!found_channel) {
    qWarning() << m_url << " is not a valid RSS feed";
    return false;
  }

  // Make sure we limit the number of articles
  removeOldArticles();

  // RSS Feed Downloader
  if (RssSettings().isRssDownloadingEnabled())
    downloadMatchingArticleTorrents();

  // Save items to disk (for safety)
  saveItemsToDisk();

  return true;
}

void RssFeed::downloadMatchingArticleTorrents() {
  Q_ASSERT(RssSettings().isRssDownloadingEnabled());
  RssDownloadRuleList *download_rules = m_manager->downloadRules();

  RssArticleHash::ConstIterator it = m_articles.begin();
  RssArticleHash::ConstIterator itend = m_articles.end();
  for ( ; it != itend; ++it) {
    RssArticlePtr article = it.value();
    // Skip read articles
    if (article->isRead())
      continue;
    // Check if the item should be automatically downloaded
    RssDownloadRulePtr matching_rule = download_rules->findMatchingRule(m_url, article->title());
    if (matching_rule) {
      // Torrent was downloaded, consider article as read
      article->markAsRead();
      // Download the torrent
      QString torrent_url = article->hasAttachment() ? article->torrentUrl() : article->link();
      QBtSession::instance()->addConsoleMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(article->title()).arg(displayName()));
      if (torrent_url.startsWith("magnet:", Qt::CaseInsensitive))
        QBtSession::instance()->addMagnetSkipAddDlg(torrent_url, matching_rule->savePath(), matching_rule->label());
      else
        QBtSession::instance()->downloadUrlAndSkipDialog(torrent_url, matching_rule->savePath(), matching_rule->label());
    }
  }
}

void RssFeed::removeOldArticles() {
  const uint max_articles = RssSettings().getRSSMaxArticlesPerFeed();
  const uint nb_articles = m_articles.size();
  if (nb_articles > max_articles) {
    RssArticleList listItems = m_articles.values();
    RssManager::sortArticleListByDateDesc(listItems);
    const int excess = nb_articles - max_articles;
    for (uint i=nb_articles-excess; i<nb_articles; ++i) {
      m_articles.remove(listItems.at(i)->guid());
    }
  }
}

// existing and opening test after download
bool RssFeed::parseXmlFile(const QString &file_path) {
  qDebug("openRss() called");
  QFile fileRss(file_path);
  if (!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("openRss error: open failed, no file or locked, %s", qPrintable(file_path));
    if (QFile::exists(file_path))
      fileRss.remove();
    return false;
  }

  // start reading the xml
  bool ret = parseRSS(&fileRss);
  fileRss.close();
  if (QFile::exists(file_path))
    fileRss.remove();
  return ret;
}

// read and store the downloaded rss' informations
void RssFeed::handleFinishedDownload(const QString& url, const QString &file_path) {
  if (url == m_url) {
    qDebug() << Q_FUNC_INFO << "Successfully downloaded RSS feed at" << url;
    m_downloadFailure = false;
    m_loading = false;
    // Parse the download RSS
    if (parseXmlFile(file_path)) {
      m_refreshed = true;
      m_manager->forwardFeedInfosChanged(m_url, displayName(), unreadCount()); // XXX: Ugly
      qDebug() << Q_FUNC_INFO << "Feed parsed successfully";
    }
  }
  else if (url == m_iconUrl) {
    m_icon = file_path;
    qDebug() << Q_FUNC_INFO << "icon path:" << m_icon;
    m_manager->forwardFeedIconChanged(m_url, m_icon); // XXX: Ugly
  }
}

void RssFeed::handleDownloadFailure(const QString &url, const QString& error) {
  if (url != m_url) return;
  m_downloadFailure = true;
  m_loading = false;
  m_manager->forwardFeedInfosChanged(m_url, displayName(), unreadCount()); // XXX: Ugly
  qWarning() << "Failed to download RSS feed at" << url;
  qWarning() << "Reason:" << error;
}
