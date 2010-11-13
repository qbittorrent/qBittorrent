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

#include "rssmanager.h"
#include "rsssettings.h"
#include "qbtsession.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssdownloadrulelist.h"

RssManager::RssManager(QBtSession *BTSession): RssFolder(0, this, BTSession, QString::null) {
  loadStreamList();
  connect(&newsRefresher, SIGNAL(timeout()), this, SLOT(refreshAll()));
  refreshInterval = RssSettings::getRSSRefreshInterval();
  newsRefresher.start(refreshInterval*60000);
}

RssManager::~RssManager(){
  qDebug("Deleting RSSManager");
  RssDownloadRuleList::drop();
  saveStreamList();
  qDebug("RSSManager deleted");
}

void RssManager::updateRefreshInterval(unsigned int val){
  if(refreshInterval != val) {
    refreshInterval = val;
    newsRefresher.start(refreshInterval*60000);
    qDebug("New RSS refresh interval is now every %dmin", refreshInterval);
  }
}

void RssManager::loadStreamList() {
  const QStringList streamsUrl = RssSettings::getRssFeedsUrls();
  const QStringList aliases =  RssSettings::getRssFeedsAliases();
  if(streamsUrl.size() != aliases.size()){
    std::cerr << "Corrupted Rss list, not loading it\n";
    return;
  }
  unsigned int i = 0;
  foreach(QString s, streamsUrl){
    QStringList path = s.split("\\");
    if(path.empty()) continue;
    QString feed_url = path.takeLast();
    // Create feed path (if it does not exists)
    RssFolder * feed_parent = this;
    foreach(QString folder_name, path) {
      feed_parent = feed_parent->addFolder(folder_name);
    }
    // Create feed
    RssFeed *stream = feed_parent->addStream(feed_url);
    QString alias = aliases.at(i);
    if(!alias.isEmpty()) {
      stream->rename(alias);
    }
    ++i;
  }
  qDebug("NB RSS streams loaded: %d", streamsUrl.size());
}

void RssManager::forwardFeedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread) {
  emit feedInfosChanged(url, aliasOrUrl, nbUnread);
}

void RssManager::forwardFeedIconChanged(QString url, QString icon_path) {
  emit feedIconChanged(url, icon_path);
}

void RssManager::moveFile(RssFile* file, RssFolder* dest_folder) {
  RssFolder* src_folder = file->getParent();
  if(dest_folder != src_folder) {
    // Copy to new Folder
    dest_folder->addFile(file);
    // Remove reference in old folder
    src_folder->remove(file->getID());
  } else {
    qDebug("Nothing to move, same destination folder");
  }
}

void RssManager::saveStreamList(){
  QStringList streamsUrl;
  QStringList aliases;
  const QList<RssFeed*> streams = getAllFeeds();
  foreach(const RssFeed *stream, streams) {
    QString stream_path = stream->getPath().join("\\");
    if(stream_path.isNull()) {
      stream_path = "";
    }
    qDebug("Saving stream path: %s", qPrintable(stream_path));
    streamsUrl << stream_path;
    aliases << stream->getName();
  }
  RssSettings::setRssFeedsUrls(streamsUrl);
  RssSettings::setRssFeedsAliases(aliases);
}

void RssManager::insertSortElem(QList<RssArticle*> &list, RssArticle *item) {
  int i = 0;
  while(i < list.size() && item->getDate() < list.at(i)->getDate()) {
    ++i;
  }
  list.insert(i, item);
}

QList<RssArticle*> RssManager::sortNewsList(const QList<RssArticle*>& news_list) {
  QList<RssArticle*> new_list;
  foreach(RssArticle *item, news_list) {
    insertSortElem(new_list, item);
  }
  return new_list;
}
