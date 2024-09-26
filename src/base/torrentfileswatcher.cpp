/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christian Kandeler, Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentfileswatcher.h"

#include <chrono>

#include <QtAssert>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QVariant>

#include "base/algorithm.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/tagset.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"

using namespace std::chrono_literals;

const std::chrono::seconds WATCH_INTERVAL {10};
const int MAX_FAILED_RETRIES = 5;
const QString CONF_FILE_NAME = u"watched_folders.json"_s;

const QString OPTION_ADDTORRENTPARAMS = u"add_torrent_params"_s;
const QString OPTION_RECURSIVE = u"recursive"_s;

namespace
{
    TorrentFilesWatcher::WatchedFolderOptions parseWatchedFolderOptions(const QJsonObject &jsonObj)
    {
        TorrentFilesWatcher::WatchedFolderOptions options;
        options.addTorrentParams = BitTorrent::parseAddTorrentParams(jsonObj.value(OPTION_ADDTORRENTPARAMS).toObject());
        options.recursive = jsonObj.value(OPTION_RECURSIVE).toBool();

        return options;
    }

    QJsonObject serializeWatchedFolderOptions(const TorrentFilesWatcher::WatchedFolderOptions &options)
    {
        return {{OPTION_ADDTORRENTPARAMS, BitTorrent::serializeAddTorrentParams(options.addTorrentParams)},
                {OPTION_RECURSIVE, options.recursive}};
    }
}

class TorrentFilesWatcher::Worker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Worker)

public:
    Worker(QFileSystemWatcher *watcher);

public slots:
    void setWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options);
    void removeWatchedFolder(const Path &path);

signals:
    void torrentFound(const BitTorrent::TorrentDescriptor &torrentDescr, const BitTorrent::AddTorrentParams &addTorrentParams);

private:
    void onTimeout();
    void scheduleWatchedFolderProcessing(const Path &path);
    void processWatchedFolder(const Path &path);
    void processFolder(const Path &path, const Path &watchedFolderPath, const TorrentFilesWatcher::WatchedFolderOptions &options);
    void processFailedTorrents();
    void addWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options);
    void updateWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options);

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_watchTimer = nullptr;
    QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> m_watchedFolders;
    QSet<Path> m_watchedByTimeoutFolders;

    // Failed torrents
    QTimer *m_retryTorrentTimer = nullptr;
    QHash<Path, QHash<Path, int>> m_failedTorrents;
};

TorrentFilesWatcher *TorrentFilesWatcher::m_instance = nullptr;

void TorrentFilesWatcher::initInstance()
{
    if (!m_instance)
        m_instance = new TorrentFilesWatcher;
}

void TorrentFilesWatcher::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

TorrentFilesWatcher *TorrentFilesWatcher::instance()
{
    return m_instance;
}

TorrentFilesWatcher::TorrentFilesWatcher(QObject *parent)
    : QObject(parent)
    , m_ioThread {new QThread}
    , m_asyncWorker {new TorrentFilesWatcher::Worker(new QFileSystemWatcher(this))}
{
    connect(m_asyncWorker, &TorrentFilesWatcher::Worker::torrentFound, this, &TorrentFilesWatcher::onTorrentFound);

    m_asyncWorker->moveToThread(m_ioThread.get());
    connect(m_ioThread.get(), &QThread::finished, m_asyncWorker, &QObject::deleteLater);
    m_ioThread->setObjectName("TorrentFilesWatcher m_ioThread");
    m_ioThread->start();

    load();
}

void TorrentFilesWatcher::load()
{
    const int fileMaxSize = 10 * 1024 * 1024;
    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);

    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        if (readResult.error().status == Utils::IO::ReadError::NotExist)
        {
            loadLegacy();
            return;
        }

        LogMsg(tr("Failed to load Watched Folders configuration. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse Watched Folders configuration from %1. Error: \"%2\"")
            .arg(path.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load Watched Folders configuration from %1. Error: \"Invalid data format.\"")
            .arg(path.toString()), Log::WARNING);
        return;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        const Path watchedFolder {it.key()};
        const WatchedFolderOptions options = parseWatchedFolderOptions(it.value().toObject());
        try
        {
            doSetWatchedFolder(watchedFolder, options);
        }
        catch (const InvalidArgument &err)
        {
            LogMsg(err.message(), Log::WARNING);
        }
    }
}

void TorrentFilesWatcher::loadLegacy()
{
    const auto dirs = SettingsStorage::instance()->loadValue<QVariantHash>(u"Preferences/Downloads/ScanDirsV2"_s);

    for (auto it = dirs.cbegin(); it != dirs.cend(); ++it)
    {
        const Path watchedFolder {it.key()};
        BitTorrent::AddTorrentParams params;
        if (it.value().userType() == QMetaType::Int)
        {
            if (it.value().toInt() == 0)
            {
                params.savePath = watchedFolder;
                params.useAutoTMM = false;
            }
        }
        else
        {
            const Path customSavePath {it.value().toString()};
            params.savePath = customSavePath;
            params.useAutoTMM = false;
        }

        try
        {
            doSetWatchedFolder(watchedFolder, {params, false});
        }
        catch (const InvalidArgument &err)
        {
            LogMsg(err.message(), Log::WARNING);
        }
    }

    store();
    SettingsStorage::instance()->removeValue(u"Preferences/Downloads/ScanDirsV2"_s);
}

void TorrentFilesWatcher::store() const
{
    QJsonObject jsonObj;
    for (auto it = m_watchedFolders.cbegin(); it != m_watchedFolders.cend(); ++it)
    {
        const Path &watchedFolder = it.key();
        const WatchedFolderOptions &options = it.value();
        jsonObj[watchedFolder.data()] = serializeWatchedFolderOptions(options);
    }

    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);
    const QByteArray data = QJsonDocument(jsonObj).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
    if (!result)
    {
        LogMsg(tr("Couldn't store Watched Folders configuration to %1. Error: %2")
            .arg(path.toString(), result.error()), Log::WARNING);
    }
}

QHash<Path, TorrentFilesWatcher::WatchedFolderOptions> TorrentFilesWatcher::folders() const
{
    return m_watchedFolders;
}

void TorrentFilesWatcher::setWatchedFolder(const Path &path, const WatchedFolderOptions &options)
{
    doSetWatchedFolder(path, options);
    store();
}

void TorrentFilesWatcher::doSetWatchedFolder(const Path &path, const WatchedFolderOptions &options)
{
    if (path.isEmpty())
        throw InvalidArgument(tr("Watched folder Path cannot be empty."));

    if (path.isRelative())
        throw InvalidArgument(tr("Watched folder Path cannot be relative."));

    m_watchedFolders[path] = options;

    QMetaObject::invokeMethod(m_asyncWorker, [this, path, options]
    {
        m_asyncWorker->setWatchedFolder(path, options);
    });

    emit watchedFolderSet(path, options);
}

void TorrentFilesWatcher::removeWatchedFolder(const Path &path)
{
    if (m_watchedFolders.remove(path))
    {
        if (m_asyncWorker)
        {
            QMetaObject::invokeMethod(m_asyncWorker, [this, path]()
            {
                m_asyncWorker->removeWatchedFolder(path);
            });
        }

        emit watchedFolderRemoved(path);

        store();
    }
}

void TorrentFilesWatcher::onTorrentFound(const BitTorrent::TorrentDescriptor &torrentDescr
        , const BitTorrent::AddTorrentParams &addTorrentParams)
{
    BitTorrent::Session::instance()->addTorrent(torrentDescr, addTorrentParams);
}

TorrentFilesWatcher::Worker::Worker(QFileSystemWatcher *watcher)
    : m_watcher {watcher}
    , m_watchTimer {new QTimer(this)}
    , m_retryTorrentTimer {new QTimer(this)}
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path)
    {
        scheduleWatchedFolderProcessing(Path(path));
    });
    connect(m_watchTimer, &QTimer::timeout, this, &Worker::onTimeout);

    connect(m_retryTorrentTimer, &QTimer::timeout, this, &Worker::processFailedTorrents);
}

void TorrentFilesWatcher::Worker::onTimeout()
{
    for (const Path &path : asConst(m_watchedByTimeoutFolders))
        processWatchedFolder(path);
}

void TorrentFilesWatcher::Worker::setWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    if (m_watchedFolders.contains(path))
        updateWatchedFolder(path, options);
    else
        addWatchedFolder(path, options);
}

void TorrentFilesWatcher::Worker::removeWatchedFolder(const Path &path)
{
    m_watchedFolders.remove(path);

    m_watcher->removePath(path.data());
    m_watchedByTimeoutFolders.remove(path);
    if (m_watchedByTimeoutFolders.isEmpty())
        m_watchTimer->stop();

    m_failedTorrents.remove(path);
    if (m_failedTorrents.isEmpty())
        m_retryTorrentTimer->stop();
}

void TorrentFilesWatcher::Worker::scheduleWatchedFolderProcessing(const Path &path)
{
    QTimer::singleShot(2s, Qt::CoarseTimer, this, [this, path]
    {
        processWatchedFolder(path);
    });
}

void TorrentFilesWatcher::Worker::processWatchedFolder(const Path &path)
{
    const TorrentFilesWatcher::WatchedFolderOptions options = m_watchedFolders.value(path);
    processFolder(path, path, options);

    if (!m_failedTorrents.empty() && !m_retryTorrentTimer->isActive())
        m_retryTorrentTimer->start(WATCH_INTERVAL);
}

void TorrentFilesWatcher::Worker::processFolder(const Path &path, const Path &watchedFolderPath
                                              , const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    QDirIterator dirIter {path.data(), {u"*.torrent"_s, u"*.magnet"_s}, QDir::Files};
    while (dirIter.hasNext())
    {
        const Path filePath {dirIter.next()};
        BitTorrent::AddTorrentParams addTorrentParams = options.addTorrentParams;
        if (path != watchedFolderPath)
        {
            const Path subdirPath = watchedFolderPath.relativePathOf(path);
            const bool useAutoTMM = addTorrentParams.useAutoTMM.value_or(!BitTorrent::Session::instance()->isAutoTMMDisabledByDefault());
            if (useAutoTMM)
            {
                addTorrentParams.category = addTorrentParams.category.isEmpty()
                        ? subdirPath.data() : (addTorrentParams.category + u'/' + subdirPath.data());
            }
            else
            {
                addTorrentParams.savePath = addTorrentParams.savePath / subdirPath;
            }
        }

        if (filePath.hasExtension(u".magnet"_s))
        {
            const int fileMaxSize = 100 * 1024 * 1024;

            QFile file {filePath.data()};
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                if (file.size() <= fileMaxSize)
                {
                    while (!file.atEnd())
                    {
                        const auto line = QString::fromLatin1(file.readLine()).trimmed();
                        if (const auto parseResult = BitTorrent::TorrentDescriptor::parse(line))
                            emit torrentFound(parseResult.value(), addTorrentParams);
                        else
                            LogMsg(tr("Invalid Magnet URI. URI: %1. Reason: %2").arg(line, parseResult.error()), Log::WARNING);
                    }

                    file.close();
                    Utils::Fs::removeFile(filePath);
                }
                else
                {
                    LogMsg(tr("Magnet file too big. File: %1").arg(file.errorString()), Log::WARNING);
                }
            }
            else
            {
                LogMsg(tr("Failed to open magnet file: %1").arg(file.errorString()));
            }
        }
        else
        {
            if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(filePath))
            {
                emit torrentFound(loadResult.value(), addTorrentParams);
                Utils::Fs::removeFile(filePath);
            }
            else
            {
                if (!m_failedTorrents.value(path).contains(filePath))
                {
                    m_failedTorrents[path][filePath] = 0;
                }
            }
        }
    }

    if (options.recursive)
    {
        QDirIterator iter {path.data(), (QDir::Dirs | QDir::NoDotAndDotDot)};
        while (iter.hasNext())
        {
            const Path folderPath {iter.next()};
            // Skip processing of subdirectory that is explicitly set as watched folder
            if (!m_watchedFolders.contains(folderPath))
                processFolder(folderPath, watchedFolderPath, options);
        }
    }
}

void TorrentFilesWatcher::Worker::processFailedTorrents()
{
    // Check which torrents are still partial
    Algorithm::removeIf(m_failedTorrents, [this](const Path &watchedFolderPath, QHash<Path, int> &partialTorrents)
    {
        const TorrentFilesWatcher::WatchedFolderOptions options = m_watchedFolders.value(watchedFolderPath);
        Algorithm::removeIf(partialTorrents, [this, &watchedFolderPath, &options](const Path &torrentPath, int &value)
        {
            if (!torrentPath.exists())
                return true;

            if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(torrentPath))
            {
                BitTorrent::AddTorrentParams addTorrentParams = options.addTorrentParams;
                if (torrentPath != watchedFolderPath)
                {
                    const Path subdirPath = watchedFolderPath.relativePathOf(torrentPath);
                    const bool useAutoTMM = addTorrentParams.useAutoTMM.value_or(!BitTorrent::Session::instance()->isAutoTMMDisabledByDefault());
                    if (useAutoTMM)
                    {
                        addTorrentParams.category = addTorrentParams.category.isEmpty()
                                ? subdirPath.data() : (addTorrentParams.category + u'/' + subdirPath.data());
                    }
                    else
                    {
                        addTorrentParams.savePath = addTorrentParams.savePath / subdirPath;
                    }
                }

                emit torrentFound(loadResult.value(), addTorrentParams);
                Utils::Fs::removeFile(torrentPath);

                return true;
            }

            if (value >= MAX_FAILED_RETRIES)
            {
                LogMsg(tr("Rejecting failed torrent file: %1").arg(torrentPath.toString()));
                Utils::Fs::renameFile(torrentPath, (torrentPath + u".qbt_rejected"));
                return true;
            }

            ++value;
            return false;
        });

        if (partialTorrents.isEmpty())
            return true;

        return false;
    });

    // Stop the partial timer if necessary
    if (m_failedTorrents.empty())
        m_retryTorrentTimer->stop();
    else
        m_retryTorrentTimer->start(WATCH_INTERVAL);
}

void TorrentFilesWatcher::Worker::addWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    // Check if the `path` points to a network file system or not
    if (Utils::Fs::isNetworkFileSystem(path) || options.recursive)
    {
        m_watchedByTimeoutFolders.insert(path);
        if (!m_watchTimer->isActive())
            m_watchTimer->start(WATCH_INTERVAL);
    }
    else
    {
        m_watcher->addPath(path.data());
        scheduleWatchedFolderProcessing(path);
    }

    m_watchedFolders[path] = options;

    LogMsg(tr("Watching folder: \"%1\"").arg(path.toString()));
}

void TorrentFilesWatcher::Worker::updateWatchedFolder(const Path &path, const TorrentFilesWatcher::WatchedFolderOptions &options)
{
    const bool recursiveModeChanged = (m_watchedFolders[path].recursive != options.recursive);
    if (recursiveModeChanged && !Utils::Fs::isNetworkFileSystem(path))
    {
        if (options.recursive)
        {
            m_watcher->removePath(path.data());

            m_watchedByTimeoutFolders.insert(path);
            if (!m_watchTimer->isActive())
                m_watchTimer->start(WATCH_INTERVAL);
        }
        else
        {
            m_watchedByTimeoutFolders.remove(path);
            if (m_watchedByTimeoutFolders.isEmpty())
                m_watchTimer->stop();

            m_watcher->addPath(path.data());
            scheduleWatchedFolderProcessing(path);
        }
    }

    m_watchedFolders[path] = options;
}

#include "torrentfileswatcher.moc"
