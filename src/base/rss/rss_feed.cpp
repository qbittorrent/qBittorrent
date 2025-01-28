/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Jonathan Ketchker
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#include "rss_feed.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QUrl>

#include "base/asyncfilestorage.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "feed_serializer.h"
#include "rss_article.h"
#include "rss_parser.h"
#include "rss_session.h"

const QString KEY_UID = u"uid"_s;
const QString KEY_URL = u"url"_s;
const QString KEY_TITLE = u"title"_s;
const QString KEY_LASTBUILDDATE = u"lastBuildDate"_s;
const QString KEY_ISLOADING = u"isLoading"_s;
const QString KEY_HASERROR = u"hasError"_s;
const QString KEY_ARTICLES = u"articles"_s;

using namespace RSS;

Feed::Feed(const QUuid &uid, const QString &url, const QString &path, Session *session)
    : Item(path)
    , m_session(session)
    , m_uid(uid)
    , m_url(url)
{
    const auto uidHex = QString::fromLatin1(m_uid.toRfc4122().toHex());
    m_dataFileName = Path(uidHex + u".json");

    // Move to new file naming scheme (since v4.1.2)
    const QString legacyFilename = Utils::Fs::toValidFileName(m_url, u"_"_s) + u".json";
    const Path storageDir = m_session->dataFileStorage()->storageDir();
    const Path dataFilePath = storageDir / m_dataFileName;
    if (!dataFilePath.exists())
        Utils::Fs::renameFile((storageDir / Path(legacyFilename)), dataFilePath);

    m_iconPath = storageDir / Path(uidHex + u".ico");

    m_serializer = new Private::FeedSerializer;
    m_serializer->moveToThread(m_session->workingThread());
    connect(this, &Feed::destroyed, m_serializer, &Private::FeedSerializer::deleteLater);
    connect(m_serializer, &Private::FeedSerializer::loadingFinished, this, &Feed::handleArticleLoadFinished);

    m_parser = new Private::Parser(m_lastBuildDate);
    m_parser->moveToThread(m_session->workingThread());
    connect(this, &Feed::destroyed, m_parser, &Private::Parser::deleteLater);
    connect(m_parser, &Private::Parser::finished, this, &Feed::handleParsingFinished);

    connect(m_session, &Session::maxArticlesPerFeedChanged, this, &Feed::handleMaxArticlesPerFeedChanged);

    if (m_session->isProcessingEnabled())
        downloadIcon();
    else
        connect(m_session, &Session::processingStateChanged, this, &Feed::handleSessionProcessingEnabledChanged);

    Net::DownloadManager::instance()->registerSequentialService(Net::ServiceID::fromURL(m_url), m_session->fetchDelay());

    load();
}

Feed::~Feed()
{
    store();
    emit aboutToBeDestroyed(this);
}

QList<Article *> Feed::articles() const
{
    return m_articlesByDate;
}

void Feed::markAsRead()
{
    const int oldUnreadCount = m_unreadCount;
    for (Article *article : asConst(m_articles))
    {
        if (!article->isRead())
        {
            article->disconnect(this);
            article->markAsRead();
            --m_unreadCount;
            emit articleRead(article);
        }
    }

    if (m_unreadCount != oldUnreadCount)
    {
        m_dirty = true;
        store();
        emit unreadCountChanged(this);
    }
}

void Feed::refresh()
{
    if (!m_isInitialized)
    {
        m_pendingRefresh = true;
        return;
    }

    if (m_downloadHandler)
        m_downloadHandler->cancel();

    // NOTE: Should we allow manually refreshing for disabled session?

    m_downloadHandler = Net::DownloadManager::instance()->download(m_url, Preferences::instance()->useProxyForRSS());
    connect(m_downloadHandler, &Net::DownloadHandler::finished, this, &Feed::handleDownloadFinished);

    if (!m_iconPath.exists())
        downloadIcon();

    m_isLoading = true;
    emit stateChanged(this);
}

void Feed::updateFetchDelay()
{
    // Update delay values for registered sequential services
    Net::DownloadManager::instance()->registerSequentialService(Net::ServiceID::fromURL(m_url), m_session->fetchDelay());
}

QUuid Feed::uid() const
{
    return m_uid;
}

QString Feed::url() const
{
    return m_url;
}

QString Feed::title() const
{
    return m_title;
}

bool Feed::isLoading() const
{
    return m_isLoading || !m_isInitialized;
}

QString Feed::lastBuildDate() const
{
    return m_lastBuildDate;
}

int Feed::unreadCount() const
{
    return m_unreadCount;
}

Article *Feed::articleByGUID(const QString &guid) const
{
    return m_articles.value(guid);
}

void Feed::handleMaxArticlesPerFeedChanged(const int n)
{
    while (m_articlesByDate.size() > n)
        removeOldestArticle();
    // We don't need store articles here
}

void Feed::handleIconDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status == Net::DownloadStatus::Success)
    {
        emit iconLoaded(this);
    }
}

bool Feed::hasError() const
{
    return m_hasError;
}

void Feed::handleDownloadFinished(const Net::DownloadResult &result)
{
    m_downloadHandler = nullptr; // will be deleted by DownloadManager later

    if (result.status == Net::DownloadStatus::Success)
    {
        LogMsg(tr("RSS feed at '%1' is successfully downloaded. Starting to parse it.")
                .arg(result.url));
        // Parse the download RSS
        QMetaObject::invokeMethod(m_parser, [this, data = result.data]()
        {
            m_parser->parse(data);
        });
    }
    else
    {
        m_isLoading = false;
        m_hasError = true;

        LogMsg(tr("Failed to download RSS feed at '%1'. Reason: %2")
               .arg(result.url, result.errorString), Log::WARNING);

        emit stateChanged(this);
    }
}

void Feed::handleParsingFinished(const RSS::Private::ParsingResult &result)
{
    m_hasError = !result.error.isEmpty();

    if (!result.title.isEmpty() && (title() != result.title))
    {
        m_title = result.title;
        m_dirty = true;
        emit titleChanged(this);
    }

    if (!result.lastBuildDate.isEmpty())
    {
        m_lastBuildDate = result.lastBuildDate;
        m_dirty = true;
    }

    // For some reason, the RSS feed may contain malformed XML data and it may not be
    // successfully parsed by the XML parser. We are still trying to load as many articles
    // as possible until we encounter corrupted data. So we can have some articles here
    // even in case of parsing error.
    const int newArticlesCount = updateArticles(result.articles);
    store();

    if (m_hasError)
    {
        LogMsg(tr("Failed to parse RSS feed at '%1'. Reason: %2").arg(m_url, result.error)
               , Log::WARNING);
    }
    LogMsg(tr("RSS feed at '%1' updated. Added %2 new articles.")
           .arg(url(), QString::number(newArticlesCount)));

    m_isLoading = false;
    emit stateChanged(this);
}

void Feed::load()
{
    QMetaObject::invokeMethod(m_serializer
            , [serializer = m_serializer, url = m_url
                , path = (m_session->dataFileStorage()->storageDir() / m_dataFileName)]
    {
        serializer->load(path, url);
    });
}

void Feed::store()
{
    if (!m_dirty)
        return;

    m_dirty = false;
    m_savingTimer.stop();

    QList<QVariantHash> articlesData;
    articlesData.reserve(m_articles.size());

    for (Article *article :asConst(m_articles))
        articlesData.push_back(article->data());

    QMetaObject::invokeMethod(m_serializer
            , [articlesData, serializer = m_serializer
                , path = (m_session->dataFileStorage()->storageDir() / m_dataFileName)]
    {
        serializer->store(path, articlesData);
    });
}

void Feed::storeDeferred()
{
    if (!m_savingTimer.isActive())
        m_savingTimer.start(5 * 1000, this);
}

bool Feed::addArticle(const QVariantHash &articleData)
{
    Q_ASSERT(!m_articles.contains(articleData.value(Article::KeyId).toString()));

    // Insertion sort
    const int maxArticles = m_session->maxArticlesPerFeed();
    const auto lowerBound = std::lower_bound(m_articlesByDate.cbegin(), m_articlesByDate.cend()
        , articleData.value(Article::KeyDate).toDateTime(), Article::articleDateRecentThan);
    if ((lowerBound - m_articlesByDate.cbegin()) >= maxArticles)
        return false; // we reach max articles

    auto *article = new Article(this, articleData);
    m_articles[article->guid()] = article;
    m_articlesByDate.insert(lowerBound, article);
    if (!article->isRead())
    {
        increaseUnreadCount();
        connect(article, &Article::read, this, &Feed::handleArticleRead);
    }

    m_dirty = true;
    emit newArticle(article);

    if (m_articlesByDate.size() > maxArticles)
        removeOldestArticle();

    return true;
}

void Feed::removeOldestArticle()
{
    auto *oldestArticle = m_articlesByDate.last();
    emit articleAboutToBeRemoved(oldestArticle);

    m_articles.remove(oldestArticle->guid());
    m_articlesByDate.removeLast();
    const bool isRead = oldestArticle->isRead();
    delete oldestArticle;

    if (!isRead)
        decreaseUnreadCount();
}

void Feed::increaseUnreadCount()
{
    ++m_unreadCount;
    emit unreadCountChanged(this);
}

void Feed::decreaseUnreadCount()
{
    Q_ASSERT(m_unreadCount > 0);

    --m_unreadCount;
    emit unreadCountChanged(this);
}

void Feed::downloadIcon()
{
    // Download the RSS Feed icon
    // XXX: This works for most sites but it is not perfect
    const QUrl url(m_url);
    const auto iconUrl = u"%1://%2/favicon.ico"_s.arg(url.scheme(), url.host());
    Net::DownloadManager::instance()->download(
            Net::DownloadRequest(iconUrl).saveToFile(true).destFileName(m_iconPath)
            , Preferences::instance()->useProxyForRSS(), this, &Feed::handleIconDownloadFinished);
}

int Feed::updateArticles(const QList<QVariantHash> &loadedArticles)
{
    if (loadedArticles.empty())
        return 0;

    QDateTime dummyPubDate {QDateTime::currentDateTime()};
    QList<QVariantHash> newArticles;
    newArticles.reserve(loadedArticles.size());
    for (QVariantHash article : loadedArticles)
    {
        // If article has no publication date we use feed update time as a fallback.
        // To prevent processing of "out-of-limit" articles we must not assign dates
        // that are earlier than the dates of existing articles.
        const Article *existingArticle = articleByGUID(article[Article::KeyId].toString());
        if (existingArticle)
        {
            dummyPubDate = existingArticle->date().addMSecs(-1);
            continue;
        }

        QVariant &articleDate = article[Article::KeyDate];
        if (!articleDate.toDateTime().isValid())
            articleDate = dummyPubDate;

        newArticles.append(article);
    }

    if (newArticles.empty())
        return 0;

    using ArticleSortAdaptor = std::pair<QDateTime, const QVariantHash *>;
    std::vector<ArticleSortAdaptor> sortData;
    const QList<Article *> existingArticles = articles();
    sortData.reserve(existingArticles.size() + newArticles.size());
    std::transform(existingArticles.begin(), existingArticles.end(), std::back_inserter(sortData)
                   , [](const Article *article)
    {
        return std::make_pair(article->date(), nullptr);
    });
    std::transform(newArticles.begin(), newArticles.end(), std::back_inserter(sortData)
                   , [](const QVariantHash &article)
    {
        return std::make_pair(article[Article::KeyDate].toDateTime(), &article);
    });

    // Sort article list in reverse chronological order
    std::sort(sortData.begin(), sortData.end()
              , [](const ArticleSortAdaptor &a1, const ArticleSortAdaptor &a2)
    {
        return (a1.first > a2.first);
    });

    if (sortData.size() > static_cast<uint>(m_session->maxArticlesPerFeed()))
        sortData.resize(m_session->maxArticlesPerFeed());

    int newArticlesCount = 0;
    std::for_each(sortData.crbegin(), sortData.crend(), [this, &newArticlesCount](const ArticleSortAdaptor &a)
    {
        if (a.second)
        {
            addArticle(*a.second);
            ++newArticlesCount;
        }
    });

    return newArticlesCount;
}

Path Feed::iconPath() const
{
    return m_iconPath;
}

void Feed::setURL(const QString &url)
{
    const QString oldURL = m_url;
    m_url = url;
    emit urlChanged(oldURL);
}

QJsonValue Feed::toJsonValue(const bool withData) const
{
    QJsonObject jsonObj;
    jsonObj.insert(KEY_UID, uid().toString());
    jsonObj.insert(KEY_URL, url());

    if (withData)
    {
        jsonObj.insert(KEY_TITLE, title());
        jsonObj.insert(KEY_LASTBUILDDATE, lastBuildDate());
        jsonObj.insert(KEY_ISLOADING, isLoading());
        jsonObj.insert(KEY_HASERROR, hasError());

        QJsonArray jsonArr;
        for (Article *article : asConst(m_articles))
        {
            auto articleObj = QJsonObject::fromVariantHash(article->data());
            // JSON object doesn't support DateTime so we need to convert it
            articleObj[Article::KeyDate] = article->date().toString(Qt::RFC2822Date);
            jsonArr.append(articleObj);
        }
        jsonObj.insert(KEY_ARTICLES, jsonArr);
    }

    return jsonObj;
}

void Feed::handleSessionProcessingEnabledChanged(const bool enabled)
{
    if (enabled)
    {
        downloadIcon();
        disconnect(m_session, &Session::processingStateChanged
                   , this, &Feed::handleSessionProcessingEnabledChanged);
    }
}

void Feed::handleArticleRead(Article *article)
{
    article->disconnect(this);
    decreaseUnreadCount();
    emit articleRead(article);
    // will be stored deferred
    m_dirty = true;
    storeDeferred();
}

void Feed::handleArticleLoadFinished(QList<QVariantHash> articles)
{
    Q_ASSERT(m_articles.isEmpty());
    Q_ASSERT(m_unreadCount == 0);

    const int maxArticles = m_session->maxArticlesPerFeed();
    if (articles.size() > maxArticles)
        articles.resize(maxArticles);

    m_articles.reserve(articles.size());
    m_articlesByDate.reserve(articles.size());

    for (const QVariantHash &articleData : articles)
    {
        const auto articleID = articleData.value(Article::KeyId).toString();
        if (m_articles.contains(articleID)) [[unlikely]]
            continue;

        auto *article = new Article(this, articleData);
        m_articles[articleID] = article;
        m_articlesByDate.append(article);
        if (!article->isRead())
        {
            ++m_unreadCount;
            connect(article, &Article::read, this, &Feed::handleArticleRead);
        }

        emit newArticle(article);
    }

    if (m_unreadCount > 0)
        emit unreadCountChanged(this);

    m_isInitialized = true;
    emit stateChanged(this);

    if (m_pendingRefresh)
    {
        m_pendingRefresh = false;
        refresh();
    }
}

void Feed::cleanup()
{
    m_dirty = false;
    m_savingTimer.stop();
    Utils::Fs::removeFile(m_session->dataFileStorage()->storageDir() / m_dataFileName);
    Utils::Fs::removeFile(m_iconPath);
}

void Feed::timerEvent([[maybe_unused]] QTimerEvent *event)
{
    store();
}
