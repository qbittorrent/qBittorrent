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

#include "rssfolder.h"
#include "rssarticle.h"
#include "qbtsession.h"
#include "downloadthread.h"
#include "rssmanager.h"
#include "rssfeed.h"

RssFolder::RssFolder(RssFolder *parent, RssManager *rssmanager, Bittorrent *BTSession, QString name): parent(parent), rssmanager(rssmanager), BTSession(BTSession), name(name) {
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processFinishedDownload(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
}

RssFolder::~RssFolder() {
  qDebug("Deleting a RSS folder, removing elements");
  qDeleteAll(this->values());
  qDebug("Deleting downloader thread");
  delete downloader;
  qDebug("Downloader thread removed");
}

unsigned int RssFolder::getNbUnRead() const {
  unsigned int nb_unread = 0;
  foreach(RssFile *file, this->values()) {
    nb_unread += file->getNbUnRead();
  }
  return nb_unread;
}

RssFile::FileType RssFolder::getType() const {
  return RssFile::FOLDER;
}

void RssFolder::refreshAll(){
  qDebug("Refreshing all rss feeds");
  const QList<RssFile*> items = this->values();
  for(int i=0; i<items.size(); ++i) {
    //foreach(RssFile *item, *this){
    RssFile *item = items.at(i);
    if(item->getType() == RssFile::FEED) {
      RssFeed* stream = (RssFeed*) item;
      QString url = stream->getUrl();
      if(stream->isLoading()) return;
      stream->setLoading(true);
      downloader->downloadUrl(url);
      if(!stream->hasCustomIcon()){
        downloader->downloadUrl(stream->getIconUrl());
      }
    } else {
      RssFolder *folder = (RssFolder*)item;
      folder->refreshAll();
    }
  }
}

void RssFolder::removeFile(QString ID) {
  if(this->contains(ID)) {
    RssFile* child = this->take(ID);
    child->removeAllSettings();
    child->removeAllItems();
    delete child;
  }
}

RssFolder* RssFolder::addFolder(QString name) {
  RssFolder *subfolder;
  if(!this->contains(name)) {
    subfolder = new RssFolder(this, rssmanager, BTSession, name);
    (*this)[name] = subfolder;
  } else {
    subfolder = (RssFolder*)this->value(name);
  }
  return subfolder;
}

RssFeed* RssFolder::addStream(QString url) {
  RssFeed* stream = new RssFeed(this, rssmanager, BTSession, url);
  Q_ASSERT(!this->contains(stream->getUrl()));
  (*this)[stream->getUrl()] = stream;
  refreshStream(stream->getUrl());
  return stream;
}

// Refresh All Children
void RssFolder::refresh() {
  foreach(RssFile *child, this->values()) {
    // Little optimization child->refresh() would work too
    if(child->getType() == RssFile::FEED)
      refreshStream(child->getID());
    else
      child->refresh();
  }
}

QList<RssArticle*> RssFolder::getNewsList() const {
  QList<RssArticle*> news;
  foreach(RssFile *child, this->values()) {
    news << child->getNewsList();
  }
  return news;
}

QList<RssArticle*> RssFolder::getUnreadNewsList() const {
  QList<RssArticle*> unread_news;
  foreach(RssFile *child, this->values()) {
    unread_news << child->getUnreadNewsList();
  }
  return unread_news;
}

void RssFolder::refreshStream(QString url) {
  qDebug("Refreshing feed: %s", url.toLocal8Bit().data());
  Q_ASSERT(this->contains(url));
  RssFeed *stream = (RssFeed*)this->value(url);
  if(stream->isLoading()) {
    qDebug("Stream %s is already being loaded...", stream->getUrl().toLocal8Bit().data());
    return;
  }
  stream->setLoading(true);
  qDebug("stream %s : loaded=true", stream->getUrl().toLocal8Bit().data());
  downloader->downloadUrl(url);
  if(!stream->hasCustomIcon()){
    downloader->downloadUrl(stream->getIconUrl());
  }else{
    qDebug("No need to download this feed's icon, it was already downloaded");
  }
}

QList<RssFile*> RssFolder::getContent() const {
  return this->values();
}

unsigned int RssFolder::getNbFeeds() const {
  unsigned int nbFeeds = 0;
  foreach(RssFile* item, this->values()) {
    if(item->getType() == RssFile::FOLDER)
      nbFeeds += ((RssFolder*)item)->getNbFeeds();
    else
      nbFeeds += 1;
  }
  return nbFeeds;
}

void RssFolder::processFinishedDownload(QString url, QString path) {
  if(url.endsWith("favicon.ico")){
    // Icon downloaded
    QImage fileIcon;
    if(fileIcon.load(path)) {
      QList<RssFeed*> res = findFeedsWithIcon(url);
      RssFeed* stream;
      foreach(stream, res){
        stream->setIconPath(path);
        if(!stream->isLoading())
          rssmanager->forwardFeedIconChanged(stream->getUrl(), stream->getIconPath());
      }
    }else{
      qDebug("Unsupported icon format at %s", (const char*)url.toLocal8Bit());
    }
    return;
  }
  RssFeed *stream = (RssFeed*)this->value(url, 0);
  if(!stream){
    qDebug("This rss stream was deleted in the meantime, nothing to update");
    return;
  }
  stream->processDownloadedFile(path);
  stream->setLoading(false);
  qDebug("stream %s : loaded=false", stream->getUrl().toLocal8Bit().data());
  // If the feed has no alias, then we use the title as Alias
  // this is more user friendly
  if(stream->getName().isEmpty()){
    if(!stream->getTitle().isEmpty())
      stream->rename(stream->getTitle());
  }
  rssmanager->forwardFeedInfosChanged(url, stream->getName(), stream->getNbUnRead());
}

void RssFolder::handleDownloadFailure(QString url, QString reason) {
  if(url.endsWith("favicon.ico")){
    // Icon download failure
    qDebug("Could not download icon at %s, reason: %s", (const char*)url.toLocal8Bit(), (const char*)reason.toLocal8Bit());
    return;
  }
  RssFeed *stream = (RssFeed*)this->value(url, 0);
  if(!stream){
    qDebug("This rss stream was deleted in the meantime, nothing to update");
    return;
  }
  stream->setLoading(false);
  qDebug("Could not download Rss at %s, reason: %s", (const char*)url.toLocal8Bit(), (const char*)reason.toLocal8Bit());
  stream->setDownloadFailed();
  rssmanager->forwardFeedInfosChanged(url, stream->getName(), stream->getNbUnRead());
}

QList<RssFeed*> RssFolder::findFeedsWithIcon(QString icon_url) const {
  QList<RssFeed*> res;
  RssFile* item;
  foreach(item, this->values()){
    if(item->getType() == RssFile::FEED && ((RssFeed*)item)->getIconUrl() == icon_url)
      res << (RssFeed*)item;
  }
  return res;
}

QString RssFolder::getName() const {
  return name;
}

void RssFolder::rename(QString new_name) {
  Q_ASSERT(!parent->contains(new_name));
  if(!parent->contains(new_name)) {
    // Update parent
    (*parent)[new_name] = parent->take(name);
    // Actually rename
    name = new_name;
  }
}

void RssFolder::markAllAsRead() {
  foreach(RssFile *item, this->values()) {
    item->markAllAsRead();
  }
}

QList<RssFeed*> RssFolder::getAllFeeds() const {
  QList<RssFeed*> streams;
  foreach(RssFile *item, this->values()) {
    if(item->getType() == RssFile::FEED) {
      streams << ((RssFeed*)item);
    } else {
      foreach(RssFeed* stream, ((RssFolder*)item)->getAllFeeds()) {
        streams << stream;
      }
    }
  }
  return streams;
}

void RssFolder::addFile(RssFile * item) {
  if(item->getType() == RssFile::FEED) {
    Q_ASSERT(!this->contains(((RssFeed*)item)->getUrl()));
    (*this)[((RssFeed*)item)->getUrl()] = item;
    qDebug("Added feed %s to folder ./%s", ((RssFeed*)item)->getUrl().toLocal8Bit().data(), name.toLocal8Bit().data());
  } else {
    Q_ASSERT(!this->contains(((RssFolder*)item)->getName()));
    (*this)[((RssFolder*)item)->getName()] = item;
    qDebug("Added folder %s to folder ./%s", ((RssFolder*)item)->getName().toLocal8Bit().data(), name.toLocal8Bit().data());
  }
  // Update parent
  item->setParent(this);
}

void RssFolder::removeAllItems() {
  foreach(RssFile* child, values()) {
    child->removeAllItems();
  }
  qDeleteAll(values());
  clear();
}

void RssFolder::removeAllSettings() {
  foreach(RssFile* child, values()) {
    child->removeAllSettings();
  }
}

QString RssFolder::getID() const {
  return name;
}

bool RssFolder::hasChild(QString ID) {
  return this->contains(ID);
}
