/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2017  Vladimir Golovnev <glassez@yandex.ru>
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
#include <vector>

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QScopedPointer>
#include <QUrl>

#include "../asyncfilestorage.h"
#include "../global.h"
#include "../logger.h"
#include "../net/downloadhandler.h"
#include "../net/downloadmanager.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "private/rss_parser.h"
#include "rss_article.h"
#include "rss_session.h"

const QString KEY_UID(QStringLiteral("uid"));
const QString KEY_URL(QStringLiteral("url"));
const QString KEY_TITLE(QStringLiteral("title"));
const QString KEY_LASTBUILDDATE(QStringLiteral("lastBuildDate"));
const QString KEY_ISLOADING(QStringLiteral("isLoading"));
const QString KEY_HASERROR(QStringLiteral("hasError"));
const QString KEY_ARTICLES(QStringLiteral("articles"));

using namespace RSS;

Feed::Feed(const QUuid &uid, const QString &url, const QString &path, Session *session)
    : Item(path)
    , m_session(session)
    , m_uid(uid)
    , m_url(url)
{
    m_dataFileName = QString::fromLatin1(m_uid.toRfc4122().toHex()) + QLatin1String(".json");

    // Move to new file naming scheme (since v4.1.2)
    const QString legacyFilename {Utils::Fs::toValidFileSystemName(m_url, false, QLatin1String("_"))
                + QLatin1String(".json")};
    const QDir storageDir {m_session->dataFileStorage()->storageDir()};
    if (!QFile::exists(storageDir.absoluteFilePath(m_dataFileName)))
        QFile::rename(storageDir.absoluteFilePath(legacyFilename), storageDir.absoluteFilePath(m_dataFileName));

    m_parser = new Private::Parser(m_lastBuildDate);
    m_parser->moveToThread(m_session->workingThread());
    connect(this, &Feed::destroyed, m_parser, &Private::Parser::deleteLater);
    connect(m_parser, &Private::Parser::finished, this, &Feed::handleParsingFinished);

    connect(m_session, &Session::maxArticlesPerFeedChanged, this, &Feed::handleMaxArticlesPerFeedChanged);

    if (m_session->isProcessingEnabled())
        downloadIcon();
    else
        connect(m_session, &Session::processingStateChanged, this, &Feed::handleSessionProcessingEnabledChanged);

    Net::DownloadManager::instance()->registerSequentialService(Net::ServiceID::fromURL(m_url));

    load();
}

Feed::~Feed()
{
    emit aboutToBeDestroyed(this);
    Utils::Fs::forceRemove(m_iconPath);
}

QList<Article *> Feed::articles() const
{
    return m_articlesByDate;
}

void Feed::markAsRead()
{
    auto oldUnreadCount = m_unreadCount;
    for (Article *article : asConst(m_articles)) {
        if (!article->isRead()) {
            article->disconnect(this);
            article->markAsRead();
            --m_unreadCount;
            emit articleRead(article);
        }
    }

    if (m_unreadCount != oldUnreadCount) {
        m_dirty = true;
        store();
        emit unreadCountChanged(this);
    }
}

void Feed::refresh()
{
    if (isLoading()) return;

    // NOTE: Should we allow manually refreshing for disabled session?

    Net::DownloadHandler *handler = Net::DownloadManager::instance()->download({m_url});
    connect(handler
            , static_cast<void (Net::DownloadHandler::*)(const QString &, const QByteArray &)>(&Net::DownloadHandler::downloadFinished)
            , this, &Feed::handleDownloadFinished);
    connect(handler, &Net::DownloadHandler::downloadFailed, this, &Feed::handleDownloadFailed);

    m_isLoading = true;
    emit stateChanged(this);
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
    return m_isLoading;
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

void Feed::handleMaxArticlesPerFeedChanged(int n)
{
    while (m_articlesByDate.size() > n)
        removeOldestArticle();
    // We don't need store articles here
}

void Feed::handleIconDownloadFinished(const QString &url, const QString &filePath)
{
    Q_UNUSED(url);

    m_iconPath = Utils::Fs::fromNativePath(filePath);
    emit iconLoaded(this);
}

bool Feed::hasError() const
{
    return m_hasError;
}

void Feed::handleDownloadFinished(const QString &url, const QByteArray &data)
{
    qDebug() << "Successfully downloaded RSS feed at" << url;
    // Parse the download RSS
    m_parser->parse(data);
}

void Feed::handleDownloadFailed(const QString &url, const QString &error)
{
    m_isLoading = false;
    m_hasError = true;

    LogMsg(tr("Failed to download RSS feed at '%1'. Reason: %2").arg(url, error)
           , Log::WARNING);

    emit stateChanged(this);
}

void Feed::handleParsingFinished(const RSS::Private::ParsingResult &result)
{
    m_hasError = !result.error.isEmpty();

    if (!result.title.isEmpty() && (title() != result.title)) {
        m_title = result.title;
        m_dirty = true;
        emit titleChanged(this);
    }

    if (!result.lastBuildDate.isEmpty()) {
        m_lastBuildDate = result.lastBuildDate;
        m_dirty = true;
    }

    // For some reason, the RSS feed may contain malformed XML data and it may not be
    // successfully parsed by the XML parser. We are still trying to load as many articles
    // as possible until we encounter corrupted data. So we can have some articles here
    // even in case of parsing error.
    const int newArticlesCount = updateArticles(result.articles);
    store();

    if (m_hasError) {
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
    QFile file(m_session->dataFileStorage()->storageDir().absoluteFilePath(m_dataFileName));

    if (!file.exists()) {
        loadArticlesLegacy();
        m_dirty = true;
        store(); // convert to new format
    }
    else if (file.open(QFile::ReadOnly)) {
        loadArticles(file.readAll());
        file.close();
    }
    else {
        LogMsg(tr("Couldn't read RSS Session data from %1. Error: %2")
               .arg(m_dataFileName, file.errorString())
               , Log::WARNING);
    }
}

void Feed::loadArticles(const QByteArray &data)
{
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        LogMsg(tr("Couldn't parse RSS Session data. Error: %1").arg(jsonError.errorString())
               , Log::WARNING);
        return;
    }

    if (!jsonDoc.isArray()) {
        LogMsg(tr("Couldn't load RSS Session data. Invalid data format."), Log::WARNING);
        return;
    }

    const QJsonArray jsonArr = jsonDoc.array();
    int i = -1;
    for (const QJsonValue &jsonVal : jsonArr) {
        ++i;
        if (!jsonVal.isObject()) {
            LogMsg(tr("Couldn't load RSS article '%1#%2'. Invalid data format.").arg(m_url).arg(i)
                   , Log::WARNING);
            continue;
        }

        try {
            auto article = new Article(this, jsonVal.toObject());
            if (!addArticle(article))
                delete article;
        }
        catch (const std::runtime_error&) {}
    }
}

void Feed::loadArticlesLegacy()
{
    SettingsPtr qBTRSSFeeds = Profile::instance().applicationSettings(QStringLiteral("qBittorrent-rss-feeds"));
    const QVariantHash allOldItems = qBTRSSFeeds->value("old_items").toHash();

    for (const QVariant &var : asConst(allOldItems.value(m_url).toList())) {
        auto hash = var.toHash();
        // update legacy keys
        hash[Article::KeyLink] = hash.take(QLatin1String("news_link"));
        hash[Article::KeyTorrentURL] = hash.take(QLatin1String("torrent_url"));
        hash[Article::KeyIsRead] = hash.take(QLatin1String("read"));
        try {
            auto article = new Article(this, hash);
            if (!addArticle(article))
                delete article;
        }
        catch (const std::runtime_error&) {}
    }
}

void Feed::store()
{
    if (!m_dirty) return;

    m_dirty = false;
    m_savingTimer.stop();

    QJsonArray jsonArr;
    for (Article *article :asConst(m_articles))
        jsonArr << article->toJsonObject();

    m_session->dataFileStorage()->store(m_dataFileName, QJsonDocument(jsonArr).toJson());
}

void Feed::storeDeferred()
{
    if (!m_savingTimer.isActive())
        m_savingTimer.start(5 * 1000, this);
}

bool Feed::addArticle(Article *article)
{
    Q_ASSERT(article);
    Q_ASSERT(!m_articles.contains(article->guid()));

    // Insertion sort
    const int maxArticles = m_session->maxArticlesPerFeed();
    auto lowerBound = std::lower_bound(m_articlesByDate.begin(), m_articlesByDate.end()
                                       , article->date(), Article::articleDateRecentThan);
    if ((lowerBound - m_articlesByDate.begin()) >= maxArticles)
        return false; // we reach max articles

    m_articles[article->guid()] = article;
    m_articlesByDate.insert(lowerBound, article);
    if (!article->isRead()) {
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
    auto oldestArticle = m_articlesByDate.last();
    emit articleAboutToBeRemoved(oldestArticle);

    m_articles.remove(oldestArticle->guid());
    m_articlesByDate.removeLast();
    bool isRead = oldestArticle->isRead();
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
    auto iconUrl = QString("%1://%2/favicon.ico").arg(url.scheme(), url.host());
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->download(
            Net::DownloadRequest(iconUrl).saveToFile(true));
    connect(handler
            , static_cast<void (Net::DownloadHandler::*)(const QString &, const QString &)>(&Net::DownloadHandler::downloadFinished)
            , this, &Feed::handleIconDownloadFinished);
}

int Feed::updateArticles(const QList<QVariantHash> &loadedArticles)
{
    if (loadedArticles.empty())
        return 0;

    QDateTime dummyPubDate {QDateTime::currentDateTime()};
    QVector<QVariantHash> newArticles;
    newArticles.reserve(loadedArticles.size());
    for (QVariantHash article : loadedArticles) {
        QVariant &torrentURL = article[Article::KeyTorrentURL];
        if (torrentURL.toString().isEmpty())
            torrentURL = article[Article::KeyLink];

        // If item does not have an ID, fall back to some other identifier.
        QVariant &localId = article[Article::KeyId];
        if (localId.toString().isEmpty())
            localId = article.value(Article::KeyTorrentURL);
        if (localId.toString().isEmpty())
            localId = article.value(Article::KeyTitle);

        if (localId.toString().isEmpty())
            continue;

        // If article has no publication date we use feed update time as a fallback.
        // To prevent processing of "out-of-limit" articles we must not assign dates
        // that are earlier than the dates of existing articles.
        const Article *existingArticle = articleByGUID(localId.toString());
        if (existingArticle) {
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

    using ArticleSortAdaptor = QPair<QDateTime, const QVariantHash *>;
    std::vector<ArticleSortAdaptor> sortData;
    const QList<Article *> existingArticles = articles();
    sortData.reserve(existingArticles.size() + newArticles.size());
    std::transform(existingArticles.begin(), existingArticles.end(), std::back_inserter(sortData)
                   , [](const Article *article)
    {
        return qMakePair(article->date(), nullptr);
    });
    std::transform(newArticles.begin(), newArticles.end(), std::back_inserter(sortData)
                   , [](const QVariantHash &article)
    {
        return qMakePair(article[Article::KeyDate].toDateTime(), &article);
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
        if (a.second) {
            addArticle(new Article {this, *a.second});
            ++newArticlesCount;
        }
    });

    return newArticlesCount;
}

QString Feed::iconPath() const
{
    return m_iconPath;
}

QJsonValue Feed::toJsonValue(bool withData) const
{
    QJsonObject jsonObj;
    jsonObj.insert(KEY_UID, uid().toString());
    jsonObj.insert(KEY_URL, url());

    if (withData) {
        jsonObj.insert(KEY_TITLE, title());
        jsonObj.insert(KEY_LASTBUILDDATE, lastBuildDate());
        jsonObj.insert(KEY_ISLOADING, isLoading());
        jsonObj.insert(KEY_HASERROR, hasError());

        QJsonArray jsonArr;
        for (Article *article : asConst(m_articles))
            jsonArr << article->toJsonObject();
        jsonObj.insert(KEY_ARTICLES, jsonArr);
    }

    return jsonObj;
}

void Feed::handleSessionProcessingEnabledChanged(bool enabled)
{
    if (enabled) {
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

void Feed::cleanup()
{
    Utils::Fs::forceRemove(m_session->dataFileStorage()->storageDir().absoluteFilePath(m_dataFileName));
}

void Feed::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    store();
}
