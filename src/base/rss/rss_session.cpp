/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rss_session.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QThread>

#include "../asyncfilestorage.h"
#include "../global.h"
#include "../logger.h"
#include "../profile.h"
#include "../settingsstorage.h"
#include "../utils/fs.h"
#include "rss_article.h"
#include "rss_feed.h"
#include "rss_folder.h"
#include "rss_item.h"

const int MsecsPerMin = 60000;
const QString CONF_FOLDER_NAME(QStringLiteral("rss"));
const QString DATA_FOLDER_NAME(QStringLiteral("rss/articles"));
const QString FEEDS_FILE_NAME(QStringLiteral("feeds.json"));

using namespace RSS;

QPointer<Session> Session::m_instance = nullptr;

Session::Session()
    : m_storeProcessingEnabled("RSS/Session/EnableProcessing")
    , m_storeRefreshInterval("RSS/Session/RefreshInterval", 30)
    , m_storeMaxArticlesPerFeed("RSS/Session/MaxArticlesPerFeed", 50)
    , m_workingThread(new QThread(this))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_confFileStorage = new AsyncFileStorage(specialFolderLocation(SpecialFolder::Config) / Path(CONF_FOLDER_NAME));
    m_confFileStorage->moveToThread(m_workingThread);
    connect(m_workingThread, &QThread::finished, m_confFileStorage, &AsyncFileStorage::deleteLater);
    connect(m_confFileStorage, &AsyncFileStorage::failed, [](const Path &fileName, const QString &errorString)
    {
        LogMsg(tr("Couldn't save RSS Session configuration in %1. Error: %2")
               .arg(fileName.toString(), errorString), Log::WARNING);
    });

    m_dataFileStorage = new AsyncFileStorage(specialFolderLocation(SpecialFolder::Data) / Path(DATA_FOLDER_NAME));
    m_dataFileStorage->moveToThread(m_workingThread);
    connect(m_workingThread, &QThread::finished, m_dataFileStorage, &AsyncFileStorage::deleteLater);
    connect(m_dataFileStorage, &AsyncFileStorage::failed, [](const Path &fileName, const QString &errorString)
    {
        LogMsg(tr("Couldn't save RSS Session data in %1. Error: %2")
               .arg(fileName.toString(), errorString), Log::WARNING);
    });

    m_itemsByPath.insert("", new Folder); // root folder

    m_workingThread->start();
    load();

    connect(&m_refreshTimer, &QTimer::timeout, this, &Session::refresh);
    if (isProcessingEnabled())
    {
        m_refreshTimer.start(refreshInterval() * MsecsPerMin);
        refresh();
    }

    // Remove legacy/corrupted settings
    // (at least on Windows, QSettings is case-insensitive and it can get
    // confused when asked about settings that differ only in their case)
    auto settingsStorage = SettingsStorage::instance();
    settingsStorage->removeValue("Rss/streamList");
    settingsStorage->removeValue("Rss/streamAlias");
    settingsStorage->removeValue("Rss/open_folders");
    settingsStorage->removeValue("Rss/qt5/splitter_h");
    settingsStorage->removeValue("Rss/qt5/splitterMain");
    settingsStorage->removeValue("Rss/hosts_cookies");
    settingsStorage->removeValue("RSS/streamList");
    settingsStorage->removeValue("RSS/streamAlias");
    settingsStorage->removeValue("RSS/open_folders");
    settingsStorage->removeValue("RSS/qt5/splitter_h");
    settingsStorage->removeValue("RSS/qt5/splitterMain");
    settingsStorage->removeValue("RSS/hosts_cookies");
    settingsStorage->removeValue("Rss/Session/EnableProcessing");
    settingsStorage->removeValue("Rss/Session/RefreshInterval");
    settingsStorage->removeValue("Rss/Session/MaxArticlesPerFeed");
    settingsStorage->removeValue("Rss/AutoDownloader/EnableProcessing");
}

Session::~Session()
{
    qDebug() << "Deleting RSS Session...";

    m_workingThread->quit();
    m_workingThread->wait();

    //store();
    delete m_itemsByPath[""]; // deleting root folder

    qDebug() << "RSS Session deleted.";
}

Session *Session::instance()
{
    return m_instance;
}

nonstd::expected<void, QString> Session::addFolder(const QString &path)
{
    const nonstd::expected<Folder *, QString> result = prepareItemDest(path);
    if (!result)
        return result.get_unexpected();

    const auto destFolder = result.value();
    addItem(new Folder(path), destFolder);
    store();
    return {};
}

nonstd::expected<void, QString> Session::addFeed(const QString &url, const QString &path)
{
    if (m_feedsByURL.contains(url))
        return nonstd::make_unexpected(tr("RSS feed with given URL already exists: %1.").arg(url));

    const nonstd::expected<Folder *, QString> result = prepareItemDest(path);
    if (!result)
        return result.get_unexpected();

    const auto destFolder = result.value();
    addItem(new Feed(generateUID(), url, path, this), destFolder);
    store();
    if (isProcessingEnabled())
        feedByURL(url)->refresh();

    return {};
}

nonstd::expected<void, QString> Session::moveItem(const QString &itemPath, const QString &destPath)
{
    if (itemPath.isEmpty())
        return nonstd::make_unexpected(tr("Cannot move root folder."));

    auto item = m_itemsByPath.value(itemPath);
    if (!item)
        return nonstd::make_unexpected(tr("Item doesn't exist: %1.").arg(itemPath));

    return moveItem(item, destPath);
}

nonstd::expected<void, QString> Session::moveItem(Item *item, const QString &destPath)
{
    Q_ASSERT(item);
    Q_ASSERT(item != rootFolder());

    const nonstd::expected<Folder *, QString> result = prepareItemDest(destPath);
    if (!result)
        return result.get_unexpected();

    auto srcFolder = static_cast<Folder *>(m_itemsByPath.value(Item::parentPath(item->path())));
    const auto destFolder = result.value();
    if (srcFolder != destFolder)
    {
        srcFolder->removeItem(item);
        destFolder->addItem(item);
    }
    m_itemsByPath.insert(destPath, m_itemsByPath.take(item->path()));
    item->setPath(destPath);
    store();
    return {};
}

nonstd::expected<void, QString> Session::removeItem(const QString &itemPath)
{
    if (itemPath.isEmpty())
        return nonstd::make_unexpected(tr("Cannot delete root folder."));

    auto *item = m_itemsByPath.value(itemPath);
    if (!item)
        return nonstd::make_unexpected(tr("Item doesn't exist: %1.").arg(itemPath));

    emit itemAboutToBeRemoved(item);
    item->cleanup();

    auto folder = static_cast<Folder *>(m_itemsByPath.value(Item::parentPath(item->path())));
    folder->removeItem(item);
    delete item;
    store();
    return {};
}

QList<Item *> Session::items() const
{
    return m_itemsByPath.values();
}

Item *Session::itemByPath(const QString &path) const
{
    return m_itemsByPath.value(path);
}

void Session::load()
{
    QFile itemsFile {(m_confFileStorage->storageDir() / Path(FEEDS_FILE_NAME)).data()};
    if (!itemsFile.exists())
    {
        loadLegacy();
        return;
    }

    if (!itemsFile.open(QFile::ReadOnly))
    {
        Logger::instance()->addMessage(
                    QString("Couldn't read RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName(), itemsFile.errorString()), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(itemsFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        Logger::instance()->addMessage(
                    QString("Couldn't parse RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        Logger::instance()->addMessage(
                    QString("Couldn't load RSS Session data from %1. Invalid data format.")
                    .arg(itemsFile.fileName()), Log::WARNING);
        return;
    }

    loadFolder(jsonDoc.object(), rootFolder());
}

void Session::loadFolder(const QJsonObject &jsonObj, Folder *folder)
{
    bool updated = false;
    for (const QString &key : asConst(jsonObj.keys()))
    {
        const QJsonValue val {jsonObj[key]};
        if (val.isString())
        {
            // previous format (reduced form) doesn't contain UID
            QString url = val.toString();
            if (url.isEmpty())
                url = key;
            addFeedToFolder(generateUID(), url, key, folder);
            updated = true;
        }
        else if (val.isObject())
        {
            const QJsonObject valObj {val.toObject()};
            if (valObj.contains("url"))
            {
                if (!valObj["url"].isString())
                {
                    LogMsg(tr("Couldn't load RSS Feed '%1'. URL is required.")
                           .arg(QString("%1\\%2").arg(folder->path(), key)), Log::WARNING);
                    continue;
                }

                QUuid uid;
                if (valObj.contains("uid"))
                {
                    uid = QUuid {valObj["uid"].toString()};
                    if (uid.isNull())
                    {
                        LogMsg(tr("Couldn't load RSS Feed '%1'. UID is invalid.")
                               .arg(QString("%1\\%2").arg(folder->path(), key)), Log::WARNING);
                        continue;
                    }

                    if (m_feedsByUID.contains(uid))
                    {
                        LogMsg(tr("Duplicate RSS Feed UID: %1. Configuration seems to be corrupted.")
                               .arg(uid.toString()), Log::WARNING);
                        continue;
                    }
                }
                else
                {
                    // previous format doesn't contain UID
                    uid = generateUID();
                    updated = true;
                }

                addFeedToFolder(uid, valObj["url"].toString(), key, folder);
            }
            else
            {
                loadFolder(valObj, addSubfolder(key, folder));
            }
        }
        else
        {
            LogMsg(tr("Couldn't load RSS Item '%1'. Invalid data format.")
                   .arg(QString::fromLatin1("%1\\%2").arg(folder->path(), key)), Log::WARNING);
        }
    }

    if (updated)
        store(); // convert to updated format
}

void Session::loadLegacy()
{
    const auto legacyFeedPaths = SettingsStorage::instance()->loadValue<QStringList>("Rss/streamList");
    const auto feedAliases = SettingsStorage::instance()->loadValue<QStringList>("Rss/streamAlias");
    if (legacyFeedPaths.size() != feedAliases.size())
    {
        Logger::instance()->addMessage("Corrupted RSS list, not loading it.", Log::WARNING);
        return;
    }

    uint i = 0;
    for (QString legacyPath : legacyFeedPaths)
    {
        if (Item::PathSeparator == QString(legacyPath[0]))
            legacyPath.remove(0, 1);
        const QString parentFolderPath = Item::parentPath(legacyPath);
        const QString feedUrl = Item::relativeName(legacyPath);

        for (const QString &folderPath : asConst(Item::expandPath(parentFolderPath)))
            addFolder(folderPath);

        const QString feedPath = feedAliases[i].isEmpty()
                ? legacyPath
                : Item::joinPath(parentFolderPath, feedAliases[i]);
        addFeed(feedUrl, feedPath);
        ++i;
    }

    store(); // convert to new format
}

void Session::store()
{
    m_confFileStorage->store(Path(FEEDS_FILE_NAME)
                             , QJsonDocument(rootFolder()->toJsonValue().toObject()).toJson());
}

nonstd::expected<Folder *, QString> Session::prepareItemDest(const QString &path)
{
    if (!Item::isValidPath(path))
        return nonstd::make_unexpected(tr("Incorrect RSS Item path: %1.").arg(path));

    if (m_itemsByPath.contains(path))
        return nonstd::make_unexpected(tr("RSS item with given path already exists: %1.").arg(path));

    const QString destFolderPath = Item::parentPath(path);
    const auto destFolder = qobject_cast<Folder *>(m_itemsByPath.value(destFolderPath));
    if (!destFolder)
        return nonstd::make_unexpected(tr("Parent folder doesn't exist: %1.").arg(destFolderPath));

    return destFolder;
}

Folder *Session::addSubfolder(const QString &name, Folder *parentFolder)
{
    auto folder = new Folder(Item::joinPath(parentFolder->path(), name));
    addItem(folder, parentFolder);
    return folder;
}

Feed *Session::addFeedToFolder(const QUuid &uid, const QString &url, const QString &name, Folder *parentFolder)
{
    auto feed = new Feed(uid, url, Item::joinPath(parentFolder->path(), name), this);
    addItem(feed, parentFolder);
    return feed;
}

void Session::addItem(Item *item, Folder *destFolder)
{
    if (auto feed = qobject_cast<Feed *>(item))
    {
        connect(feed, &Feed::titleChanged, this, &Session::handleFeedTitleChanged);
        connect(feed, &Feed::iconLoaded, this, &Session::feedIconLoaded);
        connect(feed, &Feed::stateChanged, this, &Session::feedStateChanged);
        m_feedsByUID[feed->uid()] = feed;
        m_feedsByURL[feed->url()] = feed;
    }

    connect(item, &Item::pathChanged, this, &Session::itemPathChanged);
    connect(item, &Item::aboutToBeDestroyed, this, &Session::handleItemAboutToBeDestroyed);
    m_itemsByPath[item->path()] = item;
    destFolder->addItem(item);
    emit itemAdded(item);
}

bool Session::isProcessingEnabled() const
{
    return m_storeProcessingEnabled;
}

void Session::setProcessingEnabled(const bool enabled)
{
    if (m_storeProcessingEnabled != enabled)
    {
        m_storeProcessingEnabled = enabled;
        if (enabled)
        {
            m_refreshTimer.start(refreshInterval() * MsecsPerMin);
            refresh();
        }
        else
        {
            m_refreshTimer.stop();
        }

        emit processingStateChanged(enabled);
    }
}

AsyncFileStorage *Session::confFileStorage() const
{
    return m_confFileStorage;
}

AsyncFileStorage *Session::dataFileStorage() const
{
    return m_dataFileStorage;
}

Folder *Session::rootFolder() const
{
    return static_cast<Folder *>(m_itemsByPath.value(""));
}

QList<Feed *> Session::feeds() const
{
    return m_feedsByURL.values();
}

Feed *Session::feedByURL(const QString &url) const
{
    return m_feedsByURL.value(url);
}

int Session::refreshInterval() const
{
    return m_storeRefreshInterval;
}

void Session::setRefreshInterval(const int refreshInterval)
{
    if (m_storeRefreshInterval != refreshInterval)
    {
        m_storeRefreshInterval = refreshInterval;
        m_refreshTimer.start(m_storeRefreshInterval * MsecsPerMin);
    }
}

QThread *Session::workingThread() const
{
    return m_workingThread;
}

void Session::handleItemAboutToBeDestroyed(Item *item)
{
    m_itemsByPath.remove(item->path());
    auto feed = qobject_cast<Feed *>(item);
    if (feed)
    {
        m_feedsByUID.remove(feed->uid());
        m_feedsByURL.remove(feed->url());
    }
}

void Session::handleFeedTitleChanged(Feed *feed)
{
    if (feed->name() == feed->url())
        // Now we have something better than a URL.
        // Trying to rename feed...
        moveItem(feed, Item::joinPath(Item::parentPath(feed->path()), feed->title()));
}

QUuid Session::generateUID() const
{
    QUuid uid = QUuid::createUuid();
    while (m_feedsByUID.contains(uid))
        uid = QUuid::createUuid();

    return uid;
}

int Session::maxArticlesPerFeed() const
{
    return m_storeMaxArticlesPerFeed;
}

void Session::setMaxArticlesPerFeed(const int n)
{
    if (m_storeMaxArticlesPerFeed != n)
    {
        m_storeMaxArticlesPerFeed = n;
        emit maxArticlesPerFeedChanged(n);
    }
}

void Session::refresh()
{
    // NOTE: Should we allow manually refreshing for disabled session?
    rootFolder()->refresh();
}
