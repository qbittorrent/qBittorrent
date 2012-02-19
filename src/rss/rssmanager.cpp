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
#include "rssmanager.h"
#include "rsssettings.h"
#include "qbtsession.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssdownloadrulelist.h"
#include "downloadthread.h"

RssManager* RssManager::m_instance = 0;

RssManager::RssManager(): RssFolder() {
  m_rssDownloader = new DownloadThread(this);
  connect(&m_refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));
  m_refreshInterval = RssSettings().getRSSRefreshInterval();
  m_refreshTimer.start(m_refreshInterval*60000);
}

RssManager::~RssManager(){
  qDebug("Deleting RSSManager");
  m_refreshTimer.stop();
  delete m_rssDownloader;
  RssDownloadRuleList::drop();
  saveStreamList();
  qDebug("RSSManager deleted");
}

void RssManager::updateRefreshInterval(uint val){
  if(m_refreshInterval != val) {
    m_refreshInterval = val;
    m_refreshTimer.start(m_refreshInterval*60000);
    qDebug("New RSS refresh interval is now every %dmin", m_refreshInterval);
  }
}

void RssManager::loadStreamList() {
  RssSettings settings;
  const QStringList streamsUrl = settings.getRssFeedsUrls();
  const QStringList aliases =  settings.getRssFeedsAliases();
  if(streamsUrl.size() != aliases.size()){
    std::cerr << "Corrupted Rss list, not loading it\n";
    return;
  }
  uint i = 0;
  qDebug() << Q_FUNC_INFO << streamsUrl;
  foreach(QString s, streamsUrl){
    QStringList path = s.split("\\", QString::SkipEmptyParts);
    if(path.empty()) continue;
    const QString feed_url = path.takeLast();
    qDebug() << "Feed URL:" << feed_url;
    // Create feed path (if it does not exists)
    RssFolder * feed_parent = this;
    foreach(const QString &folder_name, path) {
      qDebug() << "Adding parent folder:" << folder_name;
      feed_parent = feed_parent->addFolder(folder_name);
    }
    // Create feed
    qDebug() << "Adding feed to parent folder";
    RssFeed *stream = feed_parent->addStream(feed_url);
    const QString alias = aliases.at(i);
    if(!alias.isEmpty()) {
      stream->rename(alias);
    }
    ++i;
  }
  qDebug("NB RSS streams loaded: %d", streamsUrl.size());
}

void RssManager::forwardFeedInfosChanged(const QString &url, const QString &display_name, uint nbUnread) {
  emit feedInfosChanged(url, display_name, nbUnread);
}

void RssManager::forwardFeedIconChanged(const QString &url, const QString &icon_path) {
  emit feedIconChanged(url, icon_path);
}

void RssManager::moveFile(IRssFile* file, RssFolder* dest_folder) {
  RssFolder* src_folder = file->parent();
  if(dest_folder != src_folder) {
    // Remove reference in old folder
    src_folder->takeChild(file->id());
    // add to new Folder
    dest_folder->addFile(file);
  } else {
    qDebug("Nothing to move, same destination folder");
  }
}

void RssManager::saveStreamList() const {
  QStringList streamsUrl;
  QStringList aliases;
  const QList<RssFeed*> streams = getAllFeeds();
  foreach(const RssFeed *stream, streams) {
    QString stream_path = stream->pathHierarchy().join("\\");
    if(stream_path.isNull()) {
      stream_path = "";
    }
    qDebug("Saving stream path: %s", qPrintable(stream_path));
    streamsUrl << stream_path;
    aliases << stream->displayName();
  }
  RssSettings settings;
  settings.setRssFeedsUrls(streamsUrl);
  settings.setRssFeedsAliases(aliases);
}

static bool earlierItemDate(const RssArticle& a, const RssArticle& b)
{
  return (a.date() < b.date());
}

void RssManager::sortNewsList(QList<RssArticle>& news_list) {
  qSort(news_list.begin(), news_list.end(), earlierItemDate);
}

RssManager * RssManager::instance()
{
  if(!m_instance)
    m_instance = new RssManager;
  return m_instance;
}

void RssManager::drop()
{
  if(m_instance) {
    delete m_instance;
    m_instance = 0;
  }
}
