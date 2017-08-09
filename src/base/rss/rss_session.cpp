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
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QString>
#include <QThread>
#include <QVariantHash>

#include "../asyncfilestorage.h"
#include "../logger.h"
#include "../profile.h"
#include "../settingsstorage.h"
#include "../utils/fs.h"
#include "rss_article.h"
#include "rss_feed.h"
#include "rss_item.h"
#include "rss_folder.h"

const int MsecsPerMin = 60000;
const QString ConfFolderName(QStringLiteral("rss"));
const QString DataFolderName(QStringLiteral("rss/articles"));
const QString FeedsFileName(QStringLiteral("feeds.json"));

const QString SettingsKey_ProcessingEnabled(QStringLiteral("RSS/Session/EnableProcessing"));
const QString SettingsKey_RefreshInterval(QStringLiteral("RSS/Session/RefreshInterval"));
const QString SettingsKey_MaxArticlesPerFeed(QStringLiteral("RSS/Session/MaxArticlesPerFeed"));

using namespace RSS;

QPointer<Session> Session::m_instance = nullptr;

Session::Session()
    : m_processingEnabled(SettingsStorage::instance()->loadValue(SettingsKey_ProcessingEnabled, false).toBool())
    , m_workingThread(new QThread(this))
    , m_refreshInterval(SettingsStorage::instance()->loadValue(SettingsKey_RefreshInterval, 30).toUInt())
    , m_maxArticlesPerFeed(SettingsStorage::instance()->loadValue(SettingsKey_MaxArticlesPerFeed, 50).toInt())
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    m_confFileStorage = new AsyncFileStorage(
                Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Config) + ConfFolderName));
    m_confFileStorage->moveToThread(m_workingThread);
    connect(m_workingThread, &QThread::finished, m_confFileStorage, &AsyncFileStorage::deleteLater);
    connect(m_confFileStorage, &AsyncFileStorage::failed, [](const QString &fileName, const QString &errorString)
    {
        Logger::instance()->addMessage(QString("Couldn't save RSS Session configuration in %1. Error: %2")
                                       .arg(fileName).arg(errorString), Log::WARNING);
    });

    m_dataFileStorage = new AsyncFileStorage(
                Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + DataFolderName));
    m_dataFileStorage->moveToThread(m_workingThread);
    connect(m_workingThread, &QThread::finished, m_dataFileStorage, &AsyncFileStorage::deleteLater);
    connect(m_dataFileStorage, &AsyncFileStorage::failed, [](const QString &fileName, const QString &errorString)
    {
        Logger::instance()->addMessage(QString("Couldn't save RSS Session data in %1. Error: %2")
                                       .arg(fileName).arg(errorString), Log::WARNING);
    });

    m_itemsByPath.insert("", new Folder); // root folder

    m_workingThread->start();
    load();

    connect(&m_refreshTimer, &QTimer::timeout, this, &Session::refresh);
    if (m_processingEnabled) {
        m_refreshTimer.start(m_refreshInterval * MsecsPerMin);
        refresh();
    }
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

bool Session::addFolder(const QString &path, QString *error)
{
    Folder *destFolder = prepareItemDest(path, error);
    if (!destFolder)
        return false;

    addItem(new Folder(path), destFolder);
    store();
    return true;
}

bool Session::addFeed(const QString &url, const QString &path, QString *error)
{
    if (m_feedsByURL.contains(url)) {
        if (error)
            *error = tr("RSS feed with given URL already exists: %1.").arg(url);
        return false;
    }

    Folder *destFolder = prepareItemDest(path, error);
    if (!destFolder)
        return false;

    addItem(new Feed(url, path, this), destFolder);
    store();
    if (m_processingEnabled)
        feedByURL(url)->refresh();
    return true;
}

bool Session::moveItem(const QString &itemPath, const QString &destPath, QString *error)
{
    if (itemPath.isEmpty()) {
        if (error)
            *error = tr("Cannot move root folder.");
        return false;
    }

    auto item = m_itemsByPath.value(itemPath);
    if (!item) {
        if (error)
            *error = tr("Item doesn't exists: %1.").arg(itemPath);
        return false;
    }

    return moveItem(item, destPath, error);
}

bool Session::moveItem(Item *item, const QString &destPath, QString *error)
{
    Q_ASSERT(item);
    Q_ASSERT(item != rootFolder());

    Folder *destFolder = prepareItemDest(destPath, error);
    if (!destFolder)
        return false;

    auto srcFolder = static_cast<Folder *>(m_itemsByPath.value(Item::parentPath(item->path())));
    if (srcFolder != destFolder) {
        srcFolder->removeItem(item);
        destFolder->addItem(item);
    }
    m_itemsByPath.insert(destPath, m_itemsByPath.take(item->path()));
    item->setPath(destPath);
    store();
    return true;
}

bool Session::removeItem(const QString &itemPath, QString *error)
{
    if (itemPath.isEmpty()) {
        if (error)
            *error = tr("Cannot delete root folder.");
        return false;
    }

    auto item = m_itemsByPath.value(itemPath);
    if (!item) {
        if (error)
            *error = tr("Item doesn't exists: %1.").arg(itemPath);
        return false;
    }

    emit itemAboutToBeRemoved(item);
    item->cleanup();

    auto folder = static_cast<Folder *>(m_itemsByPath.value(Item::parentPath(item->path())));
    folder->removeItem(item);
    delete item;
    store();
    return true;
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
    QFile itemsFile(m_confFileStorage->storageDir().absoluteFilePath(FeedsFileName));
    if (!itemsFile.exists()) {
        loadLegacy();
        return;
    }

    if (!itemsFile.open(QFile::ReadOnly)) {
        Logger::instance()->addMessage(
                    QString("Couldn't read RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName()).arg(itemsFile.errorString()), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(itemsFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        Logger::instance()->addMessage(
                    QString("Couldn't parse RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName()).arg(jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject()) {
        Logger::instance()->addMessage(
                    QString("Couldn't load RSS Session data from %1. Invalid data format.")
                    .arg(itemsFile.fileName()), Log::WARNING);
        return;
    }

    loadFolder(jsonDoc.object(), rootFolder());
}

void Session::loadFolder(const QJsonObject &jsonObj, Folder *folder)
{
    foreach (const QString &key, jsonObj.keys()) {
        QJsonValue val = jsonObj[key];
        if (val.isString()) {
            QString url = val.toString();
            if (url.isEmpty())
                url = key;
            addFeedToFolder(url, key, folder);
        }
        else if (!val.isObject()) {
            Logger::instance()->addMessage(
                        QString("Couldn't load RSS Item '%1'. Invalid data format.")
                        .arg(QString("%1\\%2").arg(folder->path()).arg(key)), Log::WARNING);
        }
        else {
            QJsonObject valObj = val.toObject();
            if (valObj.contains("url")) {
                if (!valObj["url"].isString()) {
                    Logger::instance()->addMessage(
                                QString("Couldn't load RSS Feed '%1'. URL is required.")
                                .arg(QString("%1\\%2").arg(folder->path()).arg(key)), Log::WARNING);
                    continue;
                }

                addFeedToFolder(valObj["url"].toString(), key, folder);
            }
            else {
                loadFolder(valObj, addSubfolder(key, folder));
            }
        }
    }
}

void Session::loadLegacy()
{
    struct LegacySettingsDeleter
    {
        ~LegacySettingsDeleter()
        {
            auto settingsStorage = SettingsStorage::instance();
            settingsStorage->removeValue("Rss/streamList");
            settingsStorage->removeValue("Rss/streamAlias");
            settingsStorage->removeValue("Rss/open_folders");
            settingsStorage->removeValue("Rss/qt5/splitter_h");
            settingsStorage->removeValue("Rss/qt5/splitterMain");
            settingsStorage->removeValue("Rss/hosts_cookies");
        }
    } legacySettingsDeleter;

    const QStringList legacyFeedPaths = SettingsStorage::instance()->loadValue("Rss/streamList").toStringList();
    const QStringList feedAliases = SettingsStorage::instance()->loadValue("Rss/streamAlias").toStringList();
    if (legacyFeedPaths.size() != feedAliases.size()) {
        Logger::instance()->addMessage("Corrupted RSS list, not loading it.", Log::WARNING);
        return;
    }

    uint i = 0;
    foreach (QString legacyPath, legacyFeedPaths) {
        if (Item::PathSeparator == QString(legacyPath[0]))
            legacyPath.remove(0, 1);
        const QString parentFolderPath = Item::parentPath(legacyPath);
        const QString feedUrl = Item::relativeName(legacyPath);

        foreach (const QString &folderPath, Item::expandPath(parentFolderPath))
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
    m_confFileStorage->store(FeedsFileName, QJsonDocument(rootFolder()->toJsonValue().toObject()).toJson());
}

Folder *Session::prepareItemDest(const QString &path, QString *error)
{
    if (!Item::isValidPath(path)) {
        if (error)
            *error = tr("Incorrect RSS Item path: %1.").arg(path);
        return nullptr;
    }

    if (m_itemsByPath.contains(path)) {
        if (error)
            *error = tr("RSS item with given path already exists: %1.").arg(path);
        return nullptr;
    }

    const QString destFolderPath = Item::parentPath(path);
    auto destFolder = qobject_cast<Folder *>(m_itemsByPath.value(destFolderPath));
    if (!destFolder) {
        if (error)
            *error = tr("Parent folder doesn't exist: %1.").arg(destFolderPath);
        return nullptr;
    }

    return destFolder;
}

Folder *Session::addSubfolder(const QString &name, Folder *parentFolder)
{
    auto folder = new Folder(Item::joinPath(parentFolder->path(), name));
    addItem(folder, parentFolder);
    return folder;
}

Feed *Session::addFeedToFolder(const QString &url, const QString &name, Folder *parentFolder)
{
    auto feed = new Feed(url, Item::joinPath(parentFolder->path(), name), this);
    addItem(feed, parentFolder);
    return feed;
}

void Session::addItem(Item *item, Folder *destFolder)
{
    if (auto feed = qobject_cast<Feed *>(item)) {
        connect(feed, &Feed::titleChanged, this, &Session::handleFeedTitleChanged);
        connect(feed, &Feed::iconLoaded, this, &Session::feedIconLoaded);
        connect(feed, &Feed::stateChanged, this, &Session::feedStateChanged);
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
    return m_processingEnabled;
}

void Session::setProcessingEnabled(bool enabled)
{
    if (m_processingEnabled != enabled) {
        m_processingEnabled = enabled;
        SettingsStorage::instance()->storeValue(SettingsKey_ProcessingEnabled, m_processingEnabled);
        if (m_processingEnabled) {
            m_refreshTimer.start(m_refreshInterval * MsecsPerMin);
            refresh();
        }
        else {
            m_refreshTimer.stop();
        }

        emit processingStateChanged(m_processingEnabled);
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

uint Session::refreshInterval() const
{
    return m_refreshInterval;
}

void Session::setRefreshInterval(uint refreshInterval)
{
    if (m_refreshInterval != refreshInterval) {
        SettingsStorage::instance()->storeValue(SettingsKey_RefreshInterval, refreshInterval);
        m_refreshInterval = refreshInterval;
        m_refreshTimer.start(m_refreshInterval * MsecsPerMin);
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
        m_feedsByURL.remove(feed->url());
}

void Session::handleFeedTitleChanged(Feed *feed)
{
    if (feed->name() == feed->url())
        // Now we have something better than a URL.
        // Trying to rename feed...
        moveItem(feed, Item::joinPath(Item::parentPath(feed->path()), feed->title()));
}

int Session::maxArticlesPerFeed() const
{
    return m_maxArticlesPerFeed;
}

void Session::setMaxArticlesPerFeed(int n)
{
    if (m_maxArticlesPerFeed != n) {
        m_maxArticlesPerFeed = n;
        SettingsStorage::instance()->storeValue(SettingsKey_MaxArticlesPerFeed, n);
        emit maxArticlesPerFeedChanged(n);
    }
}

void Session::refresh()
{
    // NOTE: Should we allow manually refreshing for disabled session?
    rootFolder()->refresh();
}
