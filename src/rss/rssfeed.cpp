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

#include "rssfeed.h"
#include "rssmanager.h"
#include "qbtsession.h"
#include "rssfolder.h"
#include "rsssettings.h"
#include "rssarticle.h"
#include "misc.h"
#include "rssdownloadrulelist.h"

RssFeed::RssFeed(RssFolder* parent, QString _url): parent(parent), alias(""), iconPath(":/Icons/oxygen/application-rss+xml.png"), refreshed(false), downloadFailure(false), currently_loading(false) {
  qDebug("RSSStream constructed");
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  url = QUrl(_url).toString();
  QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
  const QVariantList old_items = all_old_items.value(url, QVariantList()).toList();
  qDebug("Loading %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
  foreach(const QVariant &var_it, old_items) {
    QHash<QString, QVariant> item = var_it.toHash();
    RssArticle *rss_item = hashToRssArticle(this, item);
    if(rss_item) {
      insert(rss_item->guid(), rss_item);
    }
  }
}

RssFeed::~RssFeed(){
  qDebug("Deleting a RSS stream: %s", getName().toLocal8Bit().data());
  if(refreshed) {
    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QVariantList old_items;
    foreach(RssArticle *item, this->values()) {
      old_items << item->toHash();
    }
    qDebug("Saving %d old items for feed %s", old_items.size(), getName().toLocal8Bit().data());
    QHash<QString, QVariant> all_old_items = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
    all_old_items[url] = old_items;
    qBTRSS.setValue("old_items", all_old_items);
  }
  qDebug("Removing all item from feed");
  removeAllItems();
  qDebug("All items were removed");
  if(QFile::exists(filePath))
    misc::safeRemove(filePath);
  if(QFile::exists(iconPath) && !iconPath.startsWith(":/"))
    misc::safeRemove(iconPath);
}

RssFile::FileType RssFeed::getType() const {
  return RssFile::FEED;
}

void RssFeed::refresh() {
  parent->refreshStream(url);
}

// delete all the items saved
void RssFeed::removeAllItems() {
  qDeleteAll(this->values());
  this->clear();
}

void RssFeed::removeAllSettings() {
  QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
  QHash<QString, QVariant> feeds_w_downloader = qBTRSS.value("downloader_on", QHash<QString, QVariant>()).toHash();
  if(feeds_w_downloader.contains(url)) {
    feeds_w_downloader.remove(url);
    qBTRSS.setValue("downloader_on", feeds_w_downloader);
  }
  QHash<QString, QVariant> all_feeds_filters = qBTRSS.value("feed_filters", QHash<QString, QVariant>()).toHash();
  if(all_feeds_filters.contains(url)) {
    all_feeds_filters.remove(url);
    qBTRSS.setValue("feed_filters", all_feeds_filters);
  }
}

bool RssFeed::itemAlreadyExists(QString name) {
  return this->contains(name);
}

void RssFeed::setLoading(bool val) {
  currently_loading = val;
}

bool RssFeed::isLoading() {
  return currently_loading;
}

QString RssFeed::getTitle() const{
  return title;
}

void RssFeed::rename(QString new_name){
  qDebug("Renaming stream to %s", new_name.toLocal8Bit().data());
  alias = new_name;
}

// Return the alias if the stream has one, the url if it has no alias
QString RssFeed::getName() const{
  if(!alias.isEmpty()) {
    //qDebug("getName() returned alias: %s", (const char*)alias.toLocal8Bit());
    return alias;
  }
  if(!title.isEmpty()) {
    //qDebug("getName() returned title: %s", (const char*)title.toLocal8Bit());
    return title;
  }
  //qDebug("getName() returned url: %s", (const char*)url.toLocal8Bit());
  return url;
}

QString RssFeed::getLink() const{
  return link;
}

QString RssFeed::getUrl() const{
  return url;
}

QString RssFeed::getDescription() const{
  return description;
}

QString RssFeed::getImage() const{
  return image;
}

QString RssFeed::getFilePath() const{
  return filePath;
}

QString RssFeed::getIconPath() const{
  if(downloadFailure)
    return ":/Icons/oxygen/unavailable.png";
  return iconPath;
}

bool RssFeed::hasCustomIcon() const{
  return !iconPath.startsWith(":/");
}

void RssFeed::setIconPath(QString path) {
  iconPath = path;
}

RssArticle* RssFeed::getItem(QString id) const{
  return this->value(id);
}

unsigned int RssFeed::getNbNews() const{
  return this->size();
}

void RssFeed::markAllAsRead() {
  foreach(RssArticle *item, this->values()){
    item->markAsRead();
  }
  RssManager::instance()->forwardFeedInfosChanged(url, getName(), 0);
}

unsigned int RssFeed::getNbUnRead() const{
  unsigned int nbUnread=0;
  foreach(RssArticle *item, this->values()) {
    if(!item->isRead())
      ++nbUnread;
  }
  return nbUnread;
}

QList<RssArticle*> RssFeed::getNewsList() const{
  return this->values();
}

QList<RssArticle*> RssFeed::getUnreadNewsList() const {
  QList<RssArticle*> unread_news;
  foreach(RssArticle *item, this->values()) {
    if(!item->isRead())
      unread_news << item;
  }
  return unread_news;
}

// download the icon from the adress
QString RssFeed::getIconUrl() {
  QUrl siteUrl(url);
  return QString::fromUtf8("http://")+siteUrl.host()+QString::fromUtf8("/favicon.ico");
}

// read and create items from a rss document
short RssFeed::readDoc(QIODevice* device) {
  qDebug("Parsing RSS file...");
  QXmlStreamReader xml(device);
  // is it a rss file ?
  if (xml.atEnd()) {
    qDebug("ERROR: Could not parse RSS file");
    return -1;
  }
  while (!xml.atEnd()) {
    xml.readNext();
    if(xml.isStartElement()) {
      if(xml.name() != "rss") {
        qDebug("ERROR: this is not a rss file, root tag is <%s>", qPrintable(xml.name().toString()));
        return -1;
      } else {
        break;
      }
    }
  }
  // Read channels
  while(!xml.atEnd()) {
    xml.readNext();

    if(xml.isEndElement())
      break;

    if(xml.isStartElement()) {
      //qDebug("xml.name() == %s", qPrintable(xml.name().toString()));
      if(xml.name() == "channel") {
        qDebug("in channel");

        // Parse channel content
        while(!xml.atEnd()) {
          xml.readNext();

          if(xml.isEndElement() && xml.name() == "channel") {
            break;
          }

          if(xml.isStartElement()) {
            //qDebug("xml.name() == %s", qPrintable(xml.name().toString()));
            if(xml.name() == "title") {
              title = xml.readElementText();
              if(alias == getUrl())
                rename(title);
            }
            else if(xml.name() == "link") {
              link = xml.readElementText();
            }
            else if(xml.name() == "description") {
              description = xml.readElementText();
            }
            else if(xml.name() == "image") {
              image = xml.attributes().value("url").toString();
            }
            else if(xml.name() == "item") {
              RssArticle * item = new RssArticle(this, xml);
              if(item->isValid() && !itemAlreadyExists(item->guid())) {
                this->insert(item->guid(), item);
              } else {
                delete item;
              }
            }
          }
        }
      }
    }
  }

  resizeList();

  // RSS Feed Downloader
  if(RssSettings().isRssDownloadingEnabled()) {
    foreach(RssArticle* item, values()) {
      if(item->isRead()) continue;
      QString torrent_url;
      if(item->hasAttachment())
        torrent_url = item->torrentUrl();
      else
        torrent_url = item->link();
      // Check if the item should be automatically downloaded
      const RssDownloadRule matching_rule = RssDownloadRuleList::instance()->findMatchingRule(url, item->title());
      if(matching_rule.isValid()) {
        // Download the torrent
        QBtSession::instance()->addConsoleMessage(tr("Automatically downloading %1 torrent from %2 RSS feed...").arg(item->title()).arg(getName()));
        QBtSession::instance()->downloadUrlAndSkipDialog(torrent_url, matching_rule.savePath(), matching_rule.label());
        // Item was downloaded, consider it as Read
        item->markAsRead();
      }
    }
  }
  return 0;
}

void RssFeed::resizeList() {
  const unsigned int max_articles = RssSettings().getRSSMaxArticlesPerFeed();
  const unsigned int nb_articles = this->size();
  if(nb_articles > max_articles) {
    QList<RssArticle*> listItem = RssManager::sortNewsList(this->values());
    const int excess = nb_articles - max_articles;
    for(int i=0; i<excess; ++i){
      RssArticle *lastItem = listItem.takeLast();
      delete this->take(lastItem->guid());
    }
  }
}

// existing and opening test after download
short RssFeed::openRss(){
  qDebug("openRss() called");
  QFile fileRss(filePath);
  if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qDebug("openRss error: open failed, no file or locked, %s", (const char*)filePath.toLocal8Bit());
    if(QFile::exists(filePath)) {
      fileRss.remove();
    }
    return -1;
  }

  // start reading the xml
  short return_lecture = readDoc(&fileRss);
  fileRss.close();
  if(QFile::exists(filePath)) {
    fileRss.remove();
  }
  return return_lecture;
}

// read and store the downloaded rss' informations
void RssFeed::processDownloadedFile(QString file_path) {
  filePath = file_path;
  downloadFailure = false;
  if(openRss() >= 0) {
    refreshed = true;
  } else {
    qDebug("OpenRss: Feed update Failed");
  }
}

void RssFeed::setDownloadFailed(){
  downloadFailure = true;
}
