/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "base/iconprovider.h"
#include "base/bittorrent/session.h"
#include "rssmanager.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssfolder.h"

using namespace Rss;

Folder::Folder(Folder *parent, const QString &name)
    : m_parent(parent)
    , m_name(name)
{
}

Folder::~Folder() {}

Folder *Folder::parent() const
{
    return m_parent;
}

void Folder::setParent(Folder *parent)
{
    m_parent = parent;
}

uint Folder::unreadCount() const
{
    uint nbUnread = 0;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it)
        nbUnread += it.value()->unreadCount();

    return nbUnread;
}

void Folder::removeChild(const QString &childId)
{
    if (m_children.contains(childId)) {
        FilePtr child = m_children.take(childId);
        child->removeAllSettings();
    }
}

FolderPtr Folder::addFolder(const QString &name)
{
    FolderPtr subfolder;
    if (!m_children.contains(name)) {
        subfolder = FolderPtr(new Folder(this, name));
        m_children[name] = subfolder;
    }
    else {
        subfolder = qSharedPointerDynamicCast<Folder>(m_children.value(name));
    }
    return subfolder;
}

FeedPtr Folder::addStream(Manager *manager, const QString &url)
{
    qDebug() << Q_FUNC_INFO << manager << url;
    FeedPtr stream(new Feed(manager, this, url));
    Q_ASSERT(stream);
    qDebug() << "Stream URL is " << stream->url();
    Q_ASSERT(!m_children.contains(stream->url()));
    m_children[stream->url()] = stream;
    stream->refresh();
    return stream;
}

// Refresh All Children
bool Folder::refresh()
{
    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    bool refreshed = false;
    for ( ; it != itend; ++it) {
        if (it.value()->refresh())
            refreshed = true;
    }
    return refreshed;
}

ArticleList Folder::articleListByDateDesc() const
{
    ArticleList news;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        int n = news.size();
        news << it.value()->articleListByDateDesc();
        std::inplace_merge(news.begin(), news.begin() + n, news.end(), articleDateRecentThan);
    }
    return news;
}

ArticleList Folder::unreadArticleListByDateDesc() const
{
    ArticleList unreadNews;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        int n = unreadNews.size();
        unreadNews << it.value()->unreadArticleListByDateDesc();
        std::inplace_merge(unreadNews.begin(), unreadNews.begin() + n, unreadNews.end(), articleDateRecentThan);
    }
    return unreadNews;
}

FileList Folder::getContent() const
{
    return m_children.values();
}

uint Folder::getNbFeeds() const
{
    uint nbFeeds = 0;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(it.value()))
            nbFeeds += folder->getNbFeeds();
        else
            ++nbFeeds; // Feed
    }
    return nbFeeds;
}

QString Folder::displayName() const
{
    return m_name;
}

void Folder::rename(const QString &newName)
{
    if (m_name == newName) return;

    Q_ASSERT(!m_parent->hasChild(newName));
    if (!m_parent->hasChild(newName)) {
        // Update parent
        m_parent->renameChildFolder(m_name, newName);
        // Actually rename
        m_name = newName;
    }
}

void Folder::markAsRead()
{
    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        it.value()->markAsRead();
    }
}

FeedList Folder::getAllFeeds() const
{
    FeedList streams;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        if (FeedPtr feed = qSharedPointerDynamicCast<Feed>(it.value()))
            streams << feed;
        else if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(it.value()))
            streams << folder->getAllFeeds();
    }
    return streams;
}

QHash<QString, FeedPtr> Folder::getAllFeedsAsHash() const
{
    QHash<QString, FeedPtr> ret;

    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it) {
        if (FeedPtr feed = qSharedPointerDynamicCast<Feed>(it.value())) {
            qDebug() << Q_FUNC_INFO << feed->url();
            ret[feed->url()] = feed;
        }
        else if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(it.value())) {
            ret.unite(folder->getAllFeedsAsHash());
        }
    }
    return ret;
}

void Folder::addFile(const FilePtr &item)
{
    if (FeedPtr feed = qSharedPointerDynamicCast<Feed>(item)) {
        Q_ASSERT(!m_children.contains(feed->url()));
        m_children[feed->url()] = item;
        qDebug("Added feed %s to folder ./%s", qPrintable(feed->url()), qPrintable(m_name));
    }
    else if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(item)) {
        Q_ASSERT(!m_children.contains(folder->displayName()));
        m_children[folder->displayName()] = item;
        qDebug("Added folder %s to folder ./%s", qPrintable(folder->displayName()), qPrintable(m_name));
    }
    // Update parent
    item->setParent(this);
}

void Folder::removeAllItems()
{
    m_children.clear();
}

void Folder::removeAllSettings()
{
    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it)
        it.value()->removeAllSettings();
}

void Folder::saveItemsToDisk()
{
    foreach (const FilePtr &child, m_children.values())
        child->saveItemsToDisk();
}

QString Folder::id() const
{
    return m_name;
}

QString Folder::iconPath() const
{
    return IconProvider::instance()->getIconPath("inode-directory");
}

bool Folder::hasChild(const QString &childId)
{
    return m_children.contains(childId);
}

void Folder::renameChildFolder(const QString &oldName, const QString &newName)
{
    Q_ASSERT(m_children.contains(oldName));
    FilePtr folder = m_children.take(oldName);
    m_children[newName] = folder;
}

FilePtr Folder::takeChild(const QString &childId)
{
    return m_children.take(childId);
}

void Folder::recheckRssItemsForDownload()
{
    FileHash::ConstIterator it = m_children.begin();
    FileHash::ConstIterator itend = m_children.end();
    for ( ; it != itend; ++it)
        it.value()->recheckRssItemsForDownload();
}
