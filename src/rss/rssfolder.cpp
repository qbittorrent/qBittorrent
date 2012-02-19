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

#include "rssfolder.h"
#include "rssarticle.h"
#include "qbtsession.h"
#include "rssmanager.h"
#include "rssfeed.h"

RssFolder::RssFolder(RssFolder *parent, const QString &name): m_parent(parent), m_name(name) {
}

RssFolder::~RssFolder() {
  qDebug("Deleting a RSS folder, removing elements");
  qDeleteAll(m_children.values());
}

unsigned int RssFolder::unreadCount() const {
  unsigned int nb_unread = 0;
  foreach(const IRssFile *file, m_children.values()) {
    nb_unread += file->unreadCount();
  }
  return nb_unread;
}

IRssFile::FileType RssFolder::type() const {
  return IRssFile::FOLDER;
}

void RssFolder::removeChild(const QString &childId) {
  if(m_children.contains(childId)) {
    IRssFile* child = m_children.take(childId);
    child->removeAllSettings();
    delete child;
  }
}

RssFolder* RssFolder::addFolder(const QString &name) {
  RssFolder *subfolder;
  if(!m_children.contains(name)) {
    subfolder = new RssFolder(this, name);
    m_children[name] = subfolder;
  } else {
    subfolder = dynamic_cast<RssFolder*>(m_children.value(name));
  }
  return subfolder;
}

RssFeed* RssFolder::addStream(const QString &url) {
  RssFeed* stream = new RssFeed(this, url);
  Q_ASSERT(!m_children.contains(stream->url()));
  m_children[stream->url()] = stream;
  stream->refresh();
  return stream;
}

// Refresh All Children
void RssFolder::refresh() {
  foreach(IRssFile *child, m_children.values()) {
      child->refresh();
  }
}

const QList<RssArticle> RssFolder::articleList() const {
  QList<RssArticle> news;
  foreach(const IRssFile *child, m_children.values()) {
    news << child->articleList();
  }
  return news;
}

const QList<RssArticle> RssFolder::unreadArticleList() const {
  QList<RssArticle> unread_news;
  foreach(const IRssFile *child, m_children.values()) {
    unread_news << child->unreadArticleList();
  }
  return unread_news;
}

QList<IRssFile*> RssFolder::getContent() const {
  return m_children.values();
}

unsigned int RssFolder::getNbFeeds() const {
  unsigned int nbFeeds = 0;
  foreach(IRssFile* item, m_children.values()) {
    if(item->type() == IRssFile::FOLDER)
      nbFeeds += ((RssFolder*)item)->getNbFeeds();
    else
      nbFeeds += 1;
  }
  return nbFeeds;
}

QString RssFolder::displayName() const {
  return m_name;
}

void RssFolder::rename(const QString &new_name) {
  if(m_name == new_name) return;
  Q_ASSERT(!m_parent->hasChild(new_name));
  if(!m_parent->hasChild(new_name)) {
    // Update parent
    m_parent->renameChildFolder(m_name, new_name);
    // Actually rename
    m_name = new_name;
  }
}

void RssFolder::markAsRead() {
  foreach(IRssFile *item, m_children.values()) {
    item->markAsRead();
  }
}

QList<RssFeed*> RssFolder::getAllFeeds() const {
  QList<RssFeed*> streams;
  foreach(IRssFile *item, m_children.values()) {
    if(item->type() == IRssFile::FEED) {
      streams << static_cast<RssFeed*>(item);
    } else {
      streams << static_cast<RssFolder*>(item)->getAllFeeds();
    }
  }
  return streams;
}

QHash<QString, RssFeed*> RssFolder::getAllFeedsAsHash() const {
  QHash<QString, RssFeed*> ret;
  foreach(IRssFile *item, m_children.values()) {
    if(item->type() == IRssFile::FEED) {
      RssFeed* feed = dynamic_cast<RssFeed*>(item);
      Q_ASSERT(feed);
      qDebug() << Q_FUNC_INFO << feed->url();
      ret[feed->url()] = feed;
    } else {
      ret.unite(static_cast<RssFolder*>(item)->getAllFeedsAsHash());
    }
  }
  return ret;
}

void RssFolder::addFile(IRssFile * item) {
  if(item->type() == IRssFile::FEED) {
    RssFeed* feedItem = dynamic_cast<RssFeed*>(item);
    Q_ASSERT(!m_children.contains(feedItem->url()));
    m_children[feedItem->url()] = item;
    qDebug("Added feed %s to folder ./%s", qPrintable(feedItem->url()), qPrintable(m_name));
  } else {
    RssFolder* folderItem = dynamic_cast<RssFolder*>(item);
    Q_ASSERT(!m_children.contains(folderItem->displayName()));
    m_children[folderItem->displayName()] = item;
    qDebug("Added folder %s to folder ./%s", qPrintable(folderItem->displayName()), qPrintable(m_name));
  }
  // Update parent
  item->setParent(this);
}

void RssFolder::removeAllItems() {
  qDeleteAll(m_children.values());
  m_children.clear();
}

void RssFolder::removeAllSettings() {
  foreach(IRssFile* child, m_children.values()) {
    child->removeAllSettings();
  }
}

void RssFolder::saveItemsToDisk()
{
  foreach(IRssFile* child, m_children.values()) {
    child->saveItemsToDisk();
  }
}

QString RssFolder::id() const {
  return m_name;
}

bool RssFolder::hasChild(const QString &childId) {
  return m_children.contains(childId);
}

void RssFolder::renameChildFolder(const QString &old_name, const QString &new_name)
{
  Q_ASSERT(m_children.contains(old_name));
  IRssFile *folder = m_children.take(old_name);
  m_children[new_name] = folder;
}

IRssFile * RssFolder::takeChild(const QString &childId)
{
  return m_children.take(childId);
}
