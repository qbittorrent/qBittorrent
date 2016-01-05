/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "base/preferences.h"
#include "base/qinisettings.h"
#include "base/logger.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/magneturi.h"
#include "base/utils/misc.h"
#include "base/utils/fs.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "private/rssparser.h"
#include "rssdownloadrulelist.h"
#include "rssarticle.h"
#include "rssfolder.h"
#include "rssmanager.h"
#include "rssfeed.h"

namespace Rss
{
    bool articleDateRecentThan(const ArticlePtr &left, const ArticlePtr &right)
    {
        return left->date() > right->date();
    }
}

using namespace Rss;

Feed::Feed(const QString &url, Manager *manager)
    : m_manager(manager)
    , m_url (QUrl::fromEncoded(url.toUtf8()).toString())
    , m_icon(":/icons/oxygen/application-rss+xml.png")
    , m_unreadCount(0)
    , m_dirty(false)
    , m_inErrorState(false)
    , m_loading(false)
{
    qDebug() << Q_FUNC_INFO << m_url;
    m_parser = new Private::Parser;
    m_parser->moveToThread(m_manager->workingThread());
    connect(this, SIGNAL(destroyed()), m_parser, SLOT(deleteLater()));
    // Listen for new RSS downloads
    connect(m_parser, SIGNAL(feedTitle(QString)), SLOT(handleFeedTitle(QString)));
    connect(m_parser, SIGNAL(newArticle(QVariantHash)), SLOT(handleNewArticle(QVariantHash)));
    connect(m_parser, SIGNAL(finished(QString)), SLOT(handleParsingFinished(QString)));

    // Download the RSS Feed icon
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(iconUrl(), true);
    connect(handler, SIGNAL(downloadFinished(QString, QString)), this, SLOT(handleIconDownloadFinished(QString, QString)));

    // Load old RSS articles
    loadItemsFromDisk();

    refresh();
}

Feed::~Feed()
{
    if (!m_icon.startsWith(":/") && QFile::exists(m_icon))
        Utils::Fs::forceRemove(m_icon);
}

void Feed::saveItemsToDisk()
{
    qDebug() << Q_FUNC_INFO << m_url;
    if (!m_dirty) return;

    m_dirty = false;

    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QVariantList oldItems;

    ArticleHash::ConstIterator it = m_articles.begin();
    ArticleHash::ConstIterator itend = m_articles.end();
    for ( ; it != itend; ++it) {
        oldItems << it.value()->toHash();
    }
    qDebug("Saving %d old items for feed %s", oldItems.size(), qPrintable(displayName()));
    QHash<QString, QVariant> allOldItems = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
    allOldItems[m_url] = oldItems;
    qBTRSS.setValue("old_items", allOldItems);
}

void Feed::loadItemsFromDisk()
{
    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QHash<QString, QVariant> allOldItems = qBTRSS.value("old_items", QHash<QString, QVariant>()).toHash();
    const QVariantList oldItems = allOldItems.value(m_url, QVariantList()).toList();
    qDebug("Loading %d old items for feed %s", oldItems.size(), qPrintable(displayName()));

    foreach (const QVariant &var_it, oldItems) {
        QVariantHash item = var_it.toHash();
        ArticlePtr rssItem = Article::fromHash(this, item);
        if (rssItem)
            addArticle(rssItem);
    }
}

void Feed::addArticle(const ArticlePtr &article)
{
    int maxArticles = Preferences::instance()->getRSSMaxArticlesPerFeed();

    if (!m_articles.contains(article->guid())) {
        m_dirty = true;

        // Update unreadCount
        if (!article->isRead())
            ++m_unreadCount;
        // Insert in hash table
        m_articles[article->guid()] = article;
        if (!article->isRead()) // Optimization
            connect(article.data(), SIGNAL(articleWasRead()), SLOT(handleArticleRead()), Qt::UniqueConnection);
        // Insertion sort
        ArticleList::Iterator lowerBound = qLowerBound(m_articlesByDate.begin(), m_articlesByDate.end(), article, articleDateRecentThan);
        m_articlesByDate.insert(lowerBound, article);
        int lbIndex = m_articlesByDate.indexOf(article);
        if (m_articlesByDate.size() > maxArticles) {
            ArticlePtr oldestArticle = m_articlesByDate.takeLast();
            m_articles.remove(oldestArticle->guid());
            // Update unreadCount
            if (!oldestArticle->isRead())
                --m_unreadCount;
        }

        // Check if article was inserted at the end of the list and will break max_articles limit
        if (Preferences::instance()->isRssDownloadingEnabled()) {
            if ((lbIndex < maxArticles) && !article->isRead())
                downloadArticleTorrentIfMatching(article);
        }
    }
    else {
        // m_articles.contains(article->guid())
        // Try to download skipped articles
        if (Preferences::instance()->isRssDownloadingEnabled()) {
            ArticlePtr skipped = m_articles.value(article->guid(), ArticlePtr());
            if (skipped) {
                if (!skipped->isRead())
                    downloadArticleTorrentIfMatching(skipped);
            }
        }
    }
}

bool Feed::refresh()
{
    if (m_loading) {
        qWarning() << Q_FUNC_INFO << "Feed" << displayName() << "is already being refreshed, ignoring request";
        return false;
    }
    m_loading = true;
    // Download the RSS again
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(m_url);
    connect(handler, SIGNAL(downloadFinished(QString, QByteArray)), this, SLOT(handleRssDownloadFinished(QString, QByteArray)));
    connect(handler, SIGNAL(downloadFailed(QString, QString)), this, SLOT(handleRssDownloadFailed(QString, QString)));
    return true;
}

QString Feed::id() const
{
    return m_url;
}

void Feed::removeAllSettings()
{
    qDebug() << "Removing all settings / history for feed: " << m_url;
    QIniSettings qBTRSS("qBittorrent", "qBittorrent-rss");
    QVariantHash feedsWDownloader = qBTRSS.value("downloader_on", QVariantHash()).toHash();
    if (feedsWDownloader.contains(m_url)) {
        feedsWDownloader.remove(m_url);
        qBTRSS.setValue("downloader_on", feedsWDownloader);
    }
    QVariantHash allFeedsFilters = qBTRSS.value("feed_filters", QVariantHash()).toHash();
    if (allFeedsFilters.contains(m_url)) {
        allFeedsFilters.remove(m_url);
        qBTRSS.setValue("feed_filters", allFeedsFilters);
    }
    QVariantHash allOldItems = qBTRSS.value("old_items", QVariantHash()).toHash();
    if (allOldItems.contains(m_url)) {
        allOldItems.remove(m_url);
        qBTRSS.setValue("old_items", allOldItems);
    }
}

bool Feed::isLoading() const
{
    return m_loading;
}

QString Feed::title() const
{
    return m_title;
}

void Feed::rename(const QString &newName)
{
    qDebug() << "Renaming stream to" << newName;
    m_alias = newName;
}

// Return the alias if the stream has one, the url if it has no alias
QString Feed::displayName() const
{
    if (!m_alias.isEmpty())
        return m_alias;
    if (!m_title.isEmpty())
        return m_title;
    return m_url;
}

QString Feed::url() const
{
    return m_url;
}

QString Feed::iconPath() const
{
    if (m_inErrorState)
        return QLatin1String(":/icons/oxygen/unavailable.png");

    return m_icon;
}

bool Feed::hasCustomIcon() const
{
    return !m_icon.startsWith(":/");
}

void Feed::setIconPath(const QString &path)
{
    if (!path.isEmpty() && QFile::exists(path))
        m_icon = path;
}

ArticlePtr Feed::getItem(const QString &guid) const
{
    return m_articles.value(guid);
}

uint Feed::count() const
{
    return m_articles.size();
}

void Feed::markAsRead()
{
    ArticleHash::ConstIterator it = m_articles.begin();
    ArticleHash::ConstIterator itend = m_articles.end();
    for ( ; it != itend; ++it) {
        it.value()->markAsRead();
    }
    m_unreadCount = 0;
    m_manager->forwardFeedInfosChanged(m_url, displayName(), 0);
}

uint Feed::unreadCount() const
{
    return m_unreadCount;
}

ArticleList Feed::articleListByDateDesc() const
{
    return m_articlesByDate;
}

const ArticleHash &Feed::articleHash() const
{
    return m_articles;
}

ArticleList Feed::unreadArticleListByDateDesc() const
{
    ArticleList unreadNews;

    ArticleList::ConstIterator it = m_articlesByDate.begin();
    ArticleList::ConstIterator itend = m_articlesByDate.end();
    for ( ; it != itend; ++it) {
        if (!(*it)->isRead())
            unreadNews << *it;
    }
    return unreadNews;
}

// download the icon from the address
QString Feed::iconUrl() const
{
    // XXX: This works for most sites but it is not perfect
    return QString("http://%1/favicon.ico").arg(QUrl(m_url).host());
}

void Feed::handleIconDownloadFinished(const QString &url, const QString &filePath)
{
    Q_UNUSED(url);
    m_icon = filePath;
    qDebug() << Q_FUNC_INFO << "icon path:" << m_icon;
    m_manager->forwardFeedIconChanged(m_url, m_icon);
}

void Feed::handleRssDownloadFinished(const QString &url, const QByteArray &data)
{
    Q_UNUSED(url);
    qDebug() << Q_FUNC_INFO << "Successfully downloaded RSS feed at" << m_url;
    // Parse the download RSS
    QMetaObject::invokeMethod(m_parser, "parse", Qt::QueuedConnection, Q_ARG(QByteArray, data));
}

void Feed::handleRssDownloadFailed(const QString &url, const QString &error)
{
    Q_UNUSED(url);
    m_inErrorState = true;
    m_loading = false;
    m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
    qWarning() << "Failed to download RSS feed at" << m_url;
    qWarning() << "Reason:" << error;
}

void Feed::handleFeedTitle(const QString &title)
{
    if (m_title == title) return;

    m_title = title;

    // Notify that we now have something better than a URL to display
    if (m_alias.isEmpty())
        m_manager->forwardFeedInfosChanged(m_url, title, m_unreadCount);
}

void Feed::downloadArticleTorrentIfMatching(const ArticlePtr &article)
{
    Q_ASSERT(Preferences::instance()->isRssDownloadingEnabled());
    DownloadRuleList *rules = m_manager->downloadRules();
    DownloadRulePtr matchingRule = rules->findMatchingRule(m_url, article->title());
    if (!matchingRule) return;

    if (matchingRule->ignoreDays() > 0) {
        QDateTime lastMatch = matchingRule->lastMatch();
        if (lastMatch.isValid()) {
            if (QDateTime::currentDateTime() < lastMatch.addDays(matchingRule->ignoreDays())) {
                article->markAsRead();
                return;
            }
        }
    }

    matchingRule->setLastMatch(QDateTime::currentDateTime());
    rules->saveRulesToStorage();
    // Download the torrent
    const QString &torrentUrl = article->torrentUrl();
    if (torrentUrl.isEmpty()) {
        Logger::instance()->addMessage(tr("Automatic download of '%1' from '%2' RSS feed failed because it doesn't contain a torrent or a magnet link...").arg(article->title()).arg(displayName()), Log::WARNING);
        article->markAsRead();
        return;
    }

    Logger::instance()->addMessage(tr("Automatically downloading '%1' torrent from '%2' RSS feed...").arg(article->title()).arg(displayName()));
    if (BitTorrent::MagnetUri(torrentUrl).isValid())
        article->markAsRead();
    else
        connect(BitTorrent::Session::instance(), SIGNAL(downloadFromUrlFinished(QString)), article.data(), SLOT(handleTorrentDownloadSuccess(const QString&)), Qt::UniqueConnection);

    BitTorrent::AddTorrentParams params;
    params.savePath = matchingRule->savePath();
    params.label = matchingRule->label();
    if (matchingRule->addPaused() == DownloadRule::ALWAYS_PAUSED)
        params.addPaused = TriStateBool::True;
    else if (matchingRule->addPaused() == DownloadRule::NEVER_PAUSED)
        params.addPaused = TriStateBool::False;
    BitTorrent::Session::instance()->addTorrent(torrentUrl, params);
}

void Feed::recheckRssItemsForDownload()
{
    Q_ASSERT(Preferences::instance()->isRssDownloadingEnabled());
    foreach (const ArticlePtr &article, m_articlesByDate) {
        if (!article->isRead())
            downloadArticleTorrentIfMatching(article);
    }
}

void Feed::handleNewArticle(const QVariantHash &articleData)
{
    ArticlePtr article = Article::fromHash(this, articleData);
    if (article.isNull()) {
        qDebug() << "Article hash corrupted or guid is uncomputable; feed url: " << m_url;
        return;
    }
    Q_ASSERT(article);
    addArticle(article);

    m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
    // FIXME: We should forward the information here but this would seriously decrease
    // performance with current design.
    //m_manager->forwardFeedContentChanged(m_url);
}

void Feed::handleParsingFinished(const QString &error)
{
    if (!error.isEmpty()) {
        qWarning() << "Failed to parse RSS feed at" << m_url;
        qWarning() << "Reason:" << error;
    }

    m_loading = false;
    m_inErrorState = !error.isEmpty();

    m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
    // XXX: Would not be needed if we did this in handleNewArticle() instead
    m_manager->forwardFeedContentChanged(m_url);

    saveItemsToDisk();
}

void Feed::handleArticleRead()
{
    --m_unreadCount;
    m_dirty = true;
    m_manager->forwardFeedInfosChanged(m_url, displayName(), m_unreadCount);
}
