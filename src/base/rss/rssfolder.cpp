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

Folder::Folder(const QString &name)
    : m_name(name)
{}

uint Folder::count() const
{
    uint total = 0;
    for (const FilePtr &child : m_children)
        total += child->count();
    return total;
}

uint Folder::unreadCount() const
{
    uint nbUnread = 0;
    for (const FilePtr &child : m_children)
        nbUnread += child->unreadCount();
    return nbUnread;
}

// Refresh All Children
bool Folder::refresh()
{
    bool refreshed = false;
    for (auto child : m_children)
        refreshed |= child->refresh();
    return refreshed;
}

ArticleList Folder::articleListByDateDesc() const
{
    ArticleList news;

    for (const FilePtr &child : m_children) {
        int n = news.size();
        news << child->articleListByDateDesc();
        std::inplace_merge(news.begin(), news.begin() + n, news.end(), articleDateRecentThan);
    }
    return news;
}

ArticleList Folder::unreadArticleListByDateDesc() const
{
    ArticleList unreadNews;

    for (const FilePtr &child : m_children) {
        int n = unreadNews.size();
        unreadNews << child->unreadArticleListByDateDesc();
        std::inplace_merge(unreadNews.begin(), unreadNews.begin() + n, unreadNews.end(), articleDateRecentThan);
    }
    return unreadNews;
}

FileList Folder::getContent() const
{
    return m_children.values();
}

QString Folder::displayName() const
{
    return m_name;
}

bool Folder::doRename(const QString &newName)
{
    if (m_name == newName || !parentFolder())
        return false;

    bool renamed = false;
    Folder *parent = parentFolder();
    if ((renamed = !parent->hasChild(newName))) {
        // Update parent
        FilePtr folder = parent->m_children.take(m_name);
        parent->m_children.insert(newName, folder);
        // Actually rename
        m_name = newName;
    }

    return renamed;
}

void Folder::markAsRead()
{
    if (m_children.size() == 0)
        return;

    for (const FilePtr &child : m_children) {
        child->disconnect(SIGNAL(unreadCountChanged()), this);
        child->markAsRead();
        this->connect(child.data(), SIGNAL(unreadCountChanged()), SIGNAL(unreadCountChanged()));
    }

    emit unreadCountChanged(0);
}

FeedList Folder::getAllFeeds() const
{
    FeedList streams;
    for (const FilePtr &child : m_children) {
        if (FeedPtr feed = qSharedPointerDynamicCast<Feed>(child))
            streams << feed;
        else if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(child))
            streams << folder->getAllFeeds();
    }
    return streams;
}

QHash<QString, FeedPtr> Folder::getAllFeedsAsHash() const
{
    QHash<QString, FeedPtr> ret;
    for (const FilePtr &child : m_children) {
        if (FeedPtr feed = qSharedPointerDynamicCast<Feed>(child)) {
            qDebug() << Q_FUNC_INFO << feed->url();
            ret[feed->url()] = feed;
        }
        else if (FolderPtr folder = qSharedPointerDynamicCast<Folder>(child)) {
            ret.unite(folder->getAllFeedsAsHash());
        }
    }
    return ret;
}

bool Folder::addFile(const FilePtr &item)
{
    if (!hasChild(item->id())) {
        m_children[item->id()] = item;
        becomeParent(item.data(), {});
        emit unreadCountChanged();
        emit countChanged();
        return true;
    }

    return false;
}

FilePtr Folder::child(const QString &childId)
{
    return m_children.value(childId);
}

void Folder::removeAllSettings()
{
    for (const FilePtr &child : m_children)
        child->removeAllSettings();
}

void Folder::saveItemsToDisk()
{
    for (const FilePtr &child : m_children)
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

bool Folder::hasChild(const QString &childId) const
{
    return m_children.contains(childId);
}

FilePtr Folder::takeChild(const QString &childId)
{
    FilePtr child = m_children.take(childId);
    if (child) {
        emit unreadCountChanged();
        emit countChanged();
    }
    return child;
}

void Folder::removeChild(const QString &childId)
{
    if (FilePtr child = takeChild(childId))
        child->removeAllSettings();
}

void Folder::recheckRssItemsForDownload()
{
    for (const FilePtr &child : m_children)
        child->recheckRssItemsForDownload();
}
