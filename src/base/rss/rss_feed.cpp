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

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QScopedPointer>
#include <QUrl>

#include "../asyncfilestorage.h"
#include "../logger.h"
#include "../net/downloadhandler.h"
#include "../net/downloadmanager.h"
#include "../profile.h"
#include "../utils/fs.h"
#include "private/rss_parser.h"
#include "rss_article.h"
#include "rss_session.h"

const QString Str_Url(QStringLiteral("url"));
const QString Str_Title(QStringLiteral("title"));
const QString Str_LastBuildDate(QStringLiteral("lastBuildDate"));
const QString Str_IsLoading(QStringLiteral("isLoading"));
const QString Str_HasError(QStringLiteral("hasError"));
const QString Str_Articles(QStringLiteral("articles"));

using namespace RSS;

Feed::Feed(const QString &url, const QString &path, Session *session)
    : Item(path)
    , m_session(session)
    , m_url(url)
{
    m_dataFileName = QString("%1.json").arg(Utils::Fs::toValidFileSystemName(m_url, false, QLatin1String("_")));

    m_parser = new Private::Parser(m_lastBuildDate);
    m_parser->moveToThread(m_session->workingThread());
    connect(this, &Feed::destroyed, m_parser, &Private::Parser::deleteLater);
    connect(m_parser, &Private::Parser::finished, this, &Feed::handleParsingFinished);

    connect(m_session, &Session::maxArticlesPerFeedChanged, this, &Feed::handleMaxArticlesPerFeedChanged);

    if (m_session->isProcessingEnabled())
        downloadIcon();
    else
        connect(m_session, &Session::processingStateChanged, this, &Feed::handleSessionProcessingEnabledChanged);

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
    foreach (Article *article, m_articles) {
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

    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(m_url);
    connect(handler
            , static_cast<void (Net::DownloadHandler::*)(const QString &, const QByteArray &)>(&Net::DownloadHandler::downloadFinished)
            , this, &Feed::handleDownloadFinished);
    connect(handler, &Net::DownloadHandler::downloadFailed, this, &Feed::handleDownloadFailed);

    m_isLoading = true;
    emit stateChanged(this);
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
    emit stateChanged(this);
    qWarning() << "Failed to download RSS feed at" << url;
    qWarning() << "Reason:" << error;
}

void Feed::handleParsingFinished(const RSS::Private::ParsingResult &result)
{
    if (!result.error.isEmpty()) {
        qWarning() << "Failed to parse RSS feed at" << m_url;
        qWarning() << "Reason:" << result.error;
    }
    else {
        if (title() != result.title) {
            m_title = result.title;
            emit titleChanged(this);
        }

        m_lastBuildDate = result.lastBuildDate;

        foreach (const QVariantHash &varHash, result.articles) {
            auto article = Article::fromVariantHash(this, varHash);
            if (article) {
                if (!addArticle(article))
                    delete article;
                else
                    m_dirty = true;
            }
        }

        store();
    }

    m_isLoading = false;
    m_hasError = false;
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
        Logger::instance()->addMessage(
                    QString("Couldn't read RSS AutoDownloader rules from %1. Error: %2")
                    .arg(m_dataFileName).arg(file.errorString()), Log::WARNING);
    }
}

void Feed::loadArticles(const QByteArray &data)
{
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        Logger::instance()->addMessage(
                    QString("Couldn't parse RSS Session data. Error: %1")
                    .arg(jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isArray()) {
        Logger::instance()->addMessage(
                    QString("Couldn't load RSS Session data. Invalid data format."), Log::WARNING);
        return;
    }

    QJsonArray jsonArr = jsonDoc.array();
    int i = -1;
    foreach (const QJsonValue &jsonVal, jsonArr) {
        ++i;
        if (!jsonVal.isObject()) {
            Logger::instance()->addMessage(
                        QString("Couldn't load RSS article '%1#%2'. Invalid data format.").arg(m_url).arg(i)
                        , Log::WARNING);
            continue;
        }

        auto article = Article::fromJsonObject(this, jsonVal.toObject());
        if (article && !addArticle(article))
            delete article;
    }
}

void Feed::loadArticlesLegacy()
{
    SettingsPtr qBTRSSFeeds = Profile::instance().applicationSettings(QStringLiteral("qBittorrent-rss-feeds"));
    QVariantHash allOldItems = qBTRSSFeeds->value("old_items").toHash();

    foreach (const QVariant &var, allOldItems.value(m_url).toList()) {
        auto article = Article::fromVariantHash(this, var.toHash());
        if (article && !addArticle(article))
            delete article;
    }
}

void Feed::store()
{
    if (!m_dirty) return;

    m_dirty = false;
    m_savingTimer.stop();

    QJsonArray jsonArr;
    foreach (Article *article, m_articles)
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

    if (m_articles.contains(article->guid()))
        return false;

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
    emit newArticle(article);

    if (m_articlesByDate.size() > maxArticles)
        removeOldestArticle();

    return true;
}

void Feed::removeOldestArticle()
{
    auto oldestArticle = m_articlesByDate.takeLast();
    m_articles.remove(oldestArticle->guid());
    emit articleAboutToBeRemoved(oldestArticle);
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
    auto iconUrl = QString("%1://%2/favicon.ico").arg(url.scheme()).arg(url.host());
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(iconUrl, true);
    connect(handler
            , static_cast<void (Net::DownloadHandler::*)(const QString &, const QString &)>(&Net::DownloadHandler::downloadFinished)
            , this, &Feed::handleIconDownloadFinished);
}

QString Feed::iconPath() const
{
    return m_iconPath;
}

QJsonValue Feed::toJsonValue(bool withData) const
{
    if (!withData) {
        // if feed alias is empty we create "reduced" JSON
        // value for it since its name is equal to its URL
        return (name() == url() ? "" : url());
        // if we'll need storing some more properties we should check
        // for its default values and produce JSON object instead of (if it's required)
    }

    QJsonArray jsonArr;
    foreach (Article *article, m_articles)
        jsonArr << article->toJsonObject();

    QJsonObject jsonObj;
    jsonObj.insert(Str_Url, url());
    jsonObj.insert(Str_Title, title());
    jsonObj.insert(Str_LastBuildDate, lastBuildDate());
    jsonObj.insert(Str_IsLoading, isLoading());
    jsonObj.insert(Str_HasError, hasError());
    jsonObj.insert(Str_Articles, jsonArr);

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
