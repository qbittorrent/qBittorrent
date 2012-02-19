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

RssFeed::RssFeed(RssFolder* parent, const QString &url): m_parent(parent), m_icon(":/Icons/oxygen/application-rss+xml.png"),
  m_refreshed(false), m_downloadFailure(false), m_loading(false) {
  qDebug() << Q_FUNC_INFO << url;
  m_url = QUrl::fromEncoded(url.toUtf8()).toString();
  // Listen for new RSS downloads
  connect(RssManager::instance()->rssDownloader(), SIGNAL(downloadFinished(QString,QString)), SLOT(handleFinishedDownload(QString,QString)));
  connect(RssManager::instance()->rssDownloader(), SIGNAL(downloadFailure(QString,QString)), SLOT(handleDownloadFailure(QString,QString)));
  // Download the RSS Feed icon
  m_iconUrl = iconUrl();
  RssManager::instance()->rssDownloader()->downloadUrl(m_iconUrl);

  // Load old RSS articles
  loadItemsFromDisk();
}

RssFeed::~RssFeed(){
  // Saving current articles to hard disk
  if(m_refreshed) {
    saveItemsToDisk();
  }
  if(!m_icon.startsWith(":/") && QFile::exists(m_icon))
    QFile::remove(m_icon);
}

void RssFeed::saveItemsToDisk() {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QVariantList old_items;
  foreach(const RssArticle &item, m_articles.values()) {
    old_items << item.toHash();
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

  foreach(const QVariant &var_it, old_items) {
    QHash<QString, QVariant> item = var_it.toHash();
    const RssArticle rss_item = hashToRssArticle(this, item);
    if(rss_item.isValid()) {
      m_articles.insert(rss_item.guid(), rss_item);
    }
  }
}

IRssFile::FileType RssFeed::type() const {
  return IRssFile::FEED;
}

void RssFeed::refresh() {
  if(m_loading) {
    qWarning() << Q_FUNC_INFO << "Feed" << this->displayName() << "is already being refreshed, ignoring request";
    return;
  }
  m_loading = true;
  // Download the RSS again
  RssManager::instance()->rssDownloader()->downloadUrl(m_url);
}

void RssFeed::removeAllSettings() {
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

bool RssFeed::itemAlreadyExists(const QString &guid) const {
  return m_articles.contains(guid);
}

void RssFeed::setLoading(bool val) {
  m_loading = val;
}

bool RssFeed::isLoading() const {
  return m_loading;
}

QString RssFeed::title() const{
  return m_title;
}

void RssFeed::rename(const QString &new_name){
  qDebug() << "Renaming stream to" << new_name;
  m_alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssFeed::displayName() const {
  if(!m_alias.isEmpty()) {
    //qDebug("getName() returned alias: %s", (const char*)alias.toLocal8Bit());
    return m_alias;
  }
  if(!m_title.isEmpty()) {
    //qDebug("getName() returned title: %s", (const char*)title.toLocal8Bit());
    return m_title;
  }
  //qDebug("getName() returned url: %s", (const char*)url.toLocal8Bit());
  return m_url;
}

QString RssFeed::url() const{
  return m_url;
}

QString RssFeed::icon() const{
  if(m_downloadFailure)
    return ":/Icons/oxygen/unavailable.png";
  return m_icon;
}

bool RssFeed::hasCustomIcon() const{
  return !m_icon.startsWith(":/");
}

void RssFeed::setIconPath(const QString &path) {
  if(path.isEmpty() || !QFile::exists(path)) return;
  m_icon = path;
}

RssArticle& RssFeed::getItem(const QString &guid) {
  return m_articles[guid];
}

uint RssFeed::count() const{
  return m_articles.size();
}

void RssFeed::markAsRead() {
  QHash<QString, RssArticle>::iterator it;
  for(it = m_articles.begin(); it != m_articles.end(); it++) {
    it.value().markAsRead();
  }
  RssManager::instance()->forwardFeedInfosChanged(m_url, displayName(), 0);
}

uint RssFeed::unreadCount() const{
  uint nbUnread=0;
  foreach(const RssArticle &item, m_articles.values()) {
    if(!item.isRead())
      ++nbUnread;
  }
  return nbUnread;
}

const QList<RssArticle> RssFeed::articleList() const{
  return m_articles.values();
}

const QList<RssArticle> RssFeed::unreadArticleList() const {
  QList<RssArticle> unread_news;
  foreach(const RssArticle &item, m_articles.values()) {
    if(!item.isRead())
      unread_news << item;
  }
  return unread_news;
}

// download the icon from the adress
QString RssFeed::iconUrl() const {
  const QUrl siteUrl(m_url);
  // XXX: This works for most sites but it is not perfect
  return QString("http://")+siteUrl.host()+QString("/favicon.ico");
}

// read and create items from a rss document
bool RssFeed::parseRSS(QIODevice* device) {
  qDebug("Parsing RSS file...");
  QXmlStreamReader xml(device);
  // is it a rss file ?
  if (xml.atEnd()) {
    qDebug("ERROR: Could not parse RSS file");
    return false;
  }
  while (!xml.atEnd()) {
    xml.readNext();
    if(xml.isStartElement()) {
      if(xml.name() != "rss") {
        qDebug("ERROR: this is not a rss file, root tag is <%s>", qPrintable(xml.name().toString()));
        return false;
      } else {
        break;
      }
    }
  }
  // Read channels
  while(!xml.atEnd()) {
    xml.readNext();

    if(!xml.isStartElement())
      continue;

    if(xml.name() != "channel")
      continue;

    // Parse channel content
    while(!xml.atEnd()) {
      xml.readNext();

      if(xml.isEndElement() && xml.name() == "channel")
        break; // End of this channel, parse the next one

      if(!xml.isStartElement())
        continue;

      if(xml.name() == "title") {
        m_title = xml.readElementText();
        if(m_alias == url())
          rename(m_title);
      }
      else if(xml.name() == "image") {
        QString icon_path = xml.attributes().value("url").toString();
        if(!icon_path.isEmpty()) {
          m_iconUrl = icon_path;
          RssManager::instance()->rssDownloader()->downloadUrl(m_iconUrl);
        }
      }
      else if(xml.name() == "item") {
        RssArticle item(this, xml);
        if(item.isValid() && !itemAlreadyExists(item.guid())) {
          m_articles.insert(item.guid(), item);
        }
      }
    }
  }

  // RSS Feed Downloader
  if(RssSettings().isRssDownloadingEnabled())
    downloadMatchingArticleTorrents();

  // Make sure we limit the number of articles
  resizeList();

  // Save items to disk (for safety)
  saveItemsToDisk();

  return true;
}

void RssFeed::downloadMatchingArticleTorrents() {
  Q_ASSERT(RssSettings().isRssDownloadingEnabled());
  QHash<QString, RssArticle>::iterator it;
  for (it = m_articles.begin(); it != m_articles.end(); it++) {
    RssArticle &item = it.value();
    if(item.isRead()) continue;
    QString torrent_url;
    if(item.hasAttachment())
      torrent_url = item.torrentUrl();
    else
      torrent_url = item.link();
    // Check if the item should be automatically downloaded
    const RssDownloadRule matching_rule = RssDownloadRuleList::instance()->findMatchingRule(m_url, item.title());
    if(matching_rule.isValid()) {
      // Item was downloaded, consider it as Read
      item.markAsRead();
      // Download the torrent
      QBtSession::instance()->addConsoleMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(item.title()).arg(displayName()));
      QBtSession::instance()->downloadUrlAndSkipDialog(torrent_url, matching_rule.savePath(), matching_rule.label());
    }
  }
}

void RssFeed::resizeList() {
  const uint max_articles = RssSettings().getRSSMaxArticlesPerFeed();
  const uint nb_articles = m_articles.size();
  if(nb_articles > max_articles) {
    QList<RssArticle> listItems = m_articles.values();
    RssManager::sortNewsList(listItems);
    const int excess = nb_articles - max_articles;
    for(uint i=nb_articles-excess; i<nb_articles; ++i){
      m_articles.remove(listItems.at(i).guid());
    }
  }
}

// existing and opening test after download
bool RssFeed::parseXmlFile(const QString &file_path){
  qDebug("openRss() called");
  QFile fileRss(file_path);
  if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("openRss error: open failed, no file or locked, %s", qPrintable(file_path));
    if(QFile::exists(file_path)) {
      fileRss.remove();
    }
    return false;
  }

  // start reading the xml
  bool ret = parseRSS(&fileRss);
  fileRss.close();
  if(QFile::exists(file_path))
    fileRss.remove();
  return ret;
}

// read and store the downloaded rss' informations
void RssFeed::handleFinishedDownload(const QString& url, const QString &file_path) {
  if(url == m_url) {
    qDebug() << Q_FUNC_INFO << "Successfuly downloaded RSS feed at" << url;
    m_downloadFailure = false;
    m_loading = false;
    // Parse the download RSS
    if(parseXmlFile(file_path)) {
      m_refreshed = true;
      RssManager::instance()->forwardFeedInfosChanged(m_url, displayName(), unreadCount()); // XXX: Ugly
      qDebug() << Q_FUNC_INFO << "Feed parsed successfuly";
    }
  }
  else if(url == m_iconUrl) {
    m_icon = file_path;
    qDebug() << Q_FUNC_INFO << "icon path:" << m_icon;
    RssManager::instance()->forwardFeedIconChanged(m_url, m_icon); // XXX: Ugly
  }
}

void RssFeed::handleDownloadFailure(const QString &url, const QString& error) {
  if(url != m_url) return;
  m_downloadFailure = true;
  m_loading = false;
  RssManager::instance()->forwardFeedInfosChanged(m_url, displayName(), unreadCount()); // XXX: Ugly
  qWarning() << "Failed to download RSS feed at" << url;
  qWarning() << "Reason:" << error;
}
