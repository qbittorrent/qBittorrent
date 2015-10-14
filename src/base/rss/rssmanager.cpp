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

#include "base/logger.h"
#include "base/preferences.h"
#include "rssfeed.h"
#include "rssarticle.h"
#include "rssdownloadrulelist.h"
#include "rssparser.h"
#include "rssmanager.h"

static const int MSECS_PER_MIN = 60000;

RssManager::RssManager()
    : m_downloadRules(new RssDownloadRuleList)
    , m_rssParser(new RssParser(this))
{
    connect(&m_refreshTimer, SIGNAL(timeout()), SLOT(refresh()));
    m_refreshInterval = Preferences::instance()->getRSSRefreshInterval();
    m_refreshTimer.start(m_refreshInterval * MSECS_PER_MIN);
}

RssManager::~RssManager()
{
    qDebug("Deleting RSSManager...");
    delete m_downloadRules;
    delete m_rssParser;
    saveItemsToDisk();
    saveStreamList();
    qDebug("RSSManager deleted");
}

RssParser *RssManager::rssParser() const
{
    return m_rssParser;
}

void RssManager::updateRefreshInterval(uint val)
{
    if (m_refreshInterval != val) {
        m_refreshInterval = val;
        m_refreshTimer.start(m_refreshInterval*60000);
        qDebug("New RSS refresh interval is now every %dmin", m_refreshInterval);
    }
}

void RssManager::loadStreamList()
{
    const Preferences *const pref = Preferences::instance();
    const QStringList streamsUrl = pref->getRssFeedsUrls();
    const QStringList aliases =  pref->getRssFeedsAliases();
    if (streamsUrl.size() != aliases.size()) {
        Logger::instance()->addMessage("Corrupted RSS list, not loading it.", Log::WARNING);
        return;
    }

    uint i = 0;
    qDebug() << Q_FUNC_INFO << streamsUrl;
    foreach (QString s, streamsUrl) {
        QStringList path = s.split("\\", QString::SkipEmptyParts);
        if (path.empty()) continue;

        const QString feedUrl = path.takeLast();
        qDebug() << "Feed URL:" << feedUrl;
        // Create feed path (if it does not exists)
        RssFolder *feedParent = this;
        foreach (const QString &folderName, path) {
            qDebug() << "Adding parent folder:" << folderName;
            feedParent = feedParent->addFolder(folderName).data();
        }
        // Create feed
        qDebug() << "Adding feed to parent folder";
        RssFeedPtr stream = feedParent->addStream(this, feedUrl);
        const QString &alias = aliases[i];
        if (!alias.isEmpty())
            stream->rename(alias);
        ++i;
    }
    qDebug("NB RSS streams loaded: %d", streamsUrl.size());
}

void RssManager::forwardFeedContentChanged(const QString &url)
{
    emit feedContentChanged(url);
}

void RssManager::forwardFeedInfosChanged(const QString &url, const QString &displayName, uint unreadCount)
{
    emit feedInfosChanged(url, displayName, unreadCount);
}

void RssManager::forwardFeedIconChanged(const QString &url, const QString &iconPath)
{
    emit feedIconChanged(url, iconPath);
}

void RssManager::moveFile(const RssFilePtr &file, const RssFolderPtr &destinationFolder)
{
    RssFolder *srcFolder = file->parent();
    if (destinationFolder != srcFolder) {
        // Remove reference in old folder
        srcFolder->takeChild(file->id());
        // add to new Folder
        destinationFolder->addFile(file);
    }
    else {
        qDebug("Nothing to move, same destination folder");
    }
}

void RssManager::saveStreamList() const
{
    QStringList streamsUrl;
    QStringList aliases;
    RssFeedList streams = getAllFeeds();
    foreach (const RssFeedPtr &stream, streams) {
        // This backslash has nothing to do with path handling
        QString streamPath = stream->pathHierarchy().join("\\");
        if (streamPath.isNull())
            streamPath = "";
        qDebug("Saving stream path: %s", qPrintable(streamPath));
        streamsUrl << streamPath;
        aliases << stream->displayName();
    }
    Preferences *const pref = Preferences::instance();
    pref->setRssFeedsUrls(streamsUrl);
    pref->setRssFeedsAliases(aliases);
}

RssDownloadRuleList *RssManager::downloadRules() const
{
    Q_ASSERT(m_downloadRules);
    return m_downloadRules;
}
