/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018
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

#include "filesystemwatcher.h"

#include <QtGlobal>

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <cstring>
#include <sys/mount.h>
#include <sys/param.h>
#endif

#include "base/algorithm.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"

namespace
{
    const int WATCH_INTERVAL = 10000; // 10 sec
    const int MAX_PARTIAL_RETRIES = 5;
}

FileSystemWatcher::FileSystemWatcher(QObject *parent)
    : QFileSystemWatcher(parent)
{
    connect(this, &QFileSystemWatcher::directoryChanged, this, &FileSystemWatcher::scanLocalFolder);

    m_partialTorrentTimer.setSingleShot(true);
    connect(&m_partialTorrentTimer, &QTimer::timeout, this, &FileSystemWatcher::processPartialTorrents);

    connect(&m_watchTimer, &QTimer::timeout, this, &FileSystemWatcher::scanNetworkFolders);
}

QStringList FileSystemWatcher::directories() const
{
    QStringList dirs = QFileSystemWatcher::directories();
    for (const QDir &dir : asConst(m_watchedFolders))
        dirs << dir.canonicalPath();
    return dirs;
}

void FileSystemWatcher::addPath(const QString &path)
{
    if (path.isEmpty()) return;

#if !defined Q_OS_HAIKU
    const QDir dir(path);
    if (!dir.exists()) return;

    // Check if the path points to a network file system or not
    if (Utils::Fs::isNetworkFileSystem(path))
    {
        // Network mode
        LogMsg(tr("Watching remote folder: \"%1\"").arg(Utils::Fs::toNativePath(path)));
        m_watchedFolders << dir;

        m_watchTimer.start(WATCH_INTERVAL);
        return;
    }
#endif

    // Normal mode
    LogMsg(tr("Watching local folder: \"%1\"").arg(Utils::Fs::toNativePath(path)));
    QFileSystemWatcher::addPath(path);
    scanLocalFolder(path);
}

void FileSystemWatcher::removePath(const QString &path)
{
    if (m_watchedFolders.removeOne(path))
    {
        if (m_watchedFolders.isEmpty())
            m_watchTimer.stop();
        return;
    }

    // Normal mode
    QFileSystemWatcher::removePath(path);
}

void FileSystemWatcher::scanLocalFolder(const QString &path)
{
    QTimer::singleShot(2000, this, [this, path]() { processTorrentsInDir(path); });
}

void FileSystemWatcher::scanNetworkFolders()
{
    for (const QDir &dir : asConst(m_watchedFolders))
        processTorrentsInDir(dir);
}

void FileSystemWatcher::processPartialTorrents()
{
    QStringList noLongerPartial;

    // Check which torrents are still partial
    Algorithm::removeIf(m_partialTorrents, [&noLongerPartial](const QString &torrentPath, int &value)
    {
        if (!QFile::exists(torrentPath))
            return true;

        if (BitTorrent::TorrentInfo::loadFromFile(torrentPath).isValid())
        {
            noLongerPartial << torrentPath;
            return true;
        }

        if (value >= MAX_PARTIAL_RETRIES)
        {
            QFile::rename(torrentPath, torrentPath + ".qbt_rejected");
            return true;
        }

        ++value;
        return false;
    });

    // Stop the partial timer if necessary
    if (m_partialTorrents.empty())
    {
        m_partialTorrentTimer.stop();
        qDebug("No longer any partial torrent.");
    }
    else
    {
        qDebug("Still %d partial torrents after delayed processing.", m_partialTorrents.count());
        m_partialTorrentTimer.start(WATCH_INTERVAL);
    }

    // Notify of new torrents
    if (!noLongerPartial.isEmpty())
        emit torrentsAdded(noLongerPartial);
}

void FileSystemWatcher::processTorrentsInDir(const QDir &dir)
{
    QStringList torrents;
    const QStringList files = dir.entryList({"*.torrent", "*.magnet"}, QDir::Files);
    for (const QString &file : files)
    {
        const QString fileAbsPath = dir.absoluteFilePath(file);
        if (file.endsWith(".magnet", Qt::CaseInsensitive))
            torrents << fileAbsPath;
        else if (BitTorrent::TorrentInfo::loadFromFile(fileAbsPath).isValid())
            torrents << fileAbsPath;
        else if (!m_partialTorrents.contains(fileAbsPath))
            m_partialTorrents[fileAbsPath] = 0;
    }

    if (!torrents.empty())
        emit torrentsAdded(torrents);

    if (!m_partialTorrents.empty() && !m_partialTorrentTimer.isActive())
        m_partialTorrentTimer.start(WATCH_INTERVAL);
}
