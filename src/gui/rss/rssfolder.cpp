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

#include "guiiconprovider.h"
#include "rssfolder.h"
#include "rssarticle.h"
#include "core/bittorrent/session.h"
#include "rssmanager.h"
#include "rssfeed.h"

RssFolder::RssFolder(RssFolder *parent, const QString &name): m_parent(parent), m_name(name) {
}

RssFolder::~RssFolder() {
}

unsigned int RssFolder::unreadCount() const {
  uint nb_unread = 0;
  
  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    nb_unread += it.value()->unreadCount();
  }
  return nb_unread;
}

void RssFolder::removeChild(const QString &childId) {
  if (m_children.contains(childId)) {
    RssFilePtr child = m_children.take(childId);
    child->removeAllSettings();
  }
}

RssFolderPtr RssFolder::addFolder(const QString &name) {
  RssFolderPtr subfolder;
  if (!m_children.contains(name)) {
    subfolder = RssFolderPtr(new RssFolder(this, name));
    m_children[name] = subfolder;
  } else {
    subfolder = qSharedPointerDynamicCast<RssFolder>(m_children.value(name));
  }
  return subfolder;
}

RssFeedPtr RssFolder::addStream(RssManager* manager, const QString &url) {
  qDebug() << Q_FUNC_INFO << manager << url;
  RssFeedPtr stream(new RssFeed(manager, this, url));
  Q_ASSERT(stream);
  qDebug() << "Stream URL is " << stream->url();
  Q_ASSERT(!m_children.contains(stream->url()));
  m_children[stream->url()] = stream;
  stream->refresh();
  return stream;
}

// Refresh All Children
bool RssFolder::refresh() {
  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  bool refreshed = false;
  for ( ; it != itend; ++it) {
    if (it.value()->refresh())
      refreshed = true;
  }
  return refreshed;
}

RssArticleList RssFolder::articleListByDateDesc() const {
  RssArticleList news;

  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    int n = news.size();
    news << it.value()->articleListByDateDesc();
    std::inplace_merge(news.begin(), news.begin() + n, news.end(), rssArticleDateRecentThan);
  }
  return news;
}

RssArticleList RssFolder::unreadArticleListByDateDesc() const {
  RssArticleList unread_news;

  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    int n = unread_news.size();
    unread_news << it.value()->unreadArticleListByDateDesc();
    std::inplace_merge(unread_news.begin(), unread_news.begin() + n, unread_news.end(), rssArticleDateRecentThan);
  }
  return unread_news;
}

RssFileList RssFolder::getContent() const {
  return m_children.values();
}

unsigned int RssFolder::getNbFeeds() const {
  uint nbFeeds = 0;

  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    if (RssFolderPtr folder = qSharedPointerDynamicCast<RssFolder>(it.value()))
      nbFeeds += folder->getNbFeeds();
    else
      ++nbFeeds; // Feed
  }
  return nbFeeds;
}

QString RssFolder::displayName() const {
  return m_name;
}

void RssFolder::rename(const QString &new_name) {
  if (m_name == new_name) return;
  Q_ASSERT(!m_parent->hasChild(new_name));
  if (!m_parent->hasChild(new_name)) {
    // Update parent
    m_parent->renameChildFolder(m_name, new_name);
    // Actually rename
    m_name = new_name;
  }
}

void RssFolder::markAsRead() {
  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    it.value()->markAsRead();
  }
}

RssFeedList RssFolder::getAllFeeds() const {
  RssFeedList streams;

  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    if (RssFeedPtr feed = qSharedPointerDynamicCast<RssFeed>(it.value())) {
      streams << feed;
    } else if (RssFolderPtr folder = qSharedPointerDynamicCast<RssFolder>(it.value())) {
      streams << folder->getAllFeeds();
    }
  }
  return streams;
}

QHash<QString, RssFeedPtr> RssFolder::getAllFeedsAsHash() const {
  QHash<QString, RssFeedPtr> ret;

  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    if (RssFeedPtr feed = qSharedPointerDynamicCast<RssFeed>(it.value())) {
      qDebug() << Q_FUNC_INFO << feed->url();
      ret[feed->url()] = feed;
    } else if (RssFolderPtr folder = qSharedPointerDynamicCast<RssFolder>(it.value())) {
      ret.unite(folder->getAllFeedsAsHash());
    }
  }
  return ret;
}

void RssFolder::addFile(const RssFilePtr& item) {
  if (RssFeedPtr feed = qSharedPointerDynamicCast<RssFeed>(item)) {
    Q_ASSERT(!m_children.contains(feed->url()));
    m_children[feed->url()] = item;
    qDebug("Added feed %s to folder ./%s", qPrintable(feed->url()), qPrintable(m_name));
  } else if (RssFolderPtr folder = qSharedPointerDynamicCast<RssFolder>(item)) {
    Q_ASSERT(!m_children.contains(folder->displayName()));
    m_children[folder->displayName()] = item;
    qDebug("Added folder %s to folder ./%s", qPrintable(folder->displayName()), qPrintable(m_name));
  }
  // Update parent
  item->setParent(this);
}

void RssFolder::removeAllItems() {
  m_children.clear();
}

void RssFolder::removeAllSettings() {
  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    it.value()->removeAllSettings();
  }
}

void RssFolder::saveItemsToDisk()
{
  foreach (const RssFilePtr& child, m_children.values()) {
    child->saveItemsToDisk();
  }
}

QString RssFolder::id() const
{
  return m_name;
}

QIcon RssFolder::icon() const
{
  return GuiIconProvider::instance()->getIcon("inode-directory");
}

bool RssFolder::hasChild(const QString &childId) {
  return m_children.contains(childId);
}

void RssFolder::renameChildFolder(const QString &old_name, const QString &new_name)
{
  Q_ASSERT(m_children.contains(old_name));
  RssFilePtr folder = m_children.take(old_name);
  m_children[new_name] = folder;
}

RssFilePtr RssFolder::takeChild(const QString &childId)
{
  return m_children.take(childId);
}

void RssFolder::recheckRssItemsForDownload()
{
  RssFileHash::ConstIterator it = m_children.begin();
  RssFileHash::ConstIterator itend = m_children.end();
  for ( ; it != itend; ++it) {
    it.value()->recheckRssItemsForDownload();
  }
}
