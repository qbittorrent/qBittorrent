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

#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
#include <cstring>
#include <sys/mount.h>
#include <sys/param.h>
#elif !defined Q_OS_WIN && !defined Q_OS_HAIKU
#include <sys/vfs.h>
#endif

#include "algorithm.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/preferences.h"

namespace
{
    // usually defined in /usr/include/linux/magic.h
    const unsigned long CIFS_MAGIC_NUMBER = 0xFF534D42;
    const unsigned long NFS_SUPER_MAGIC = 0x6969;
    const unsigned long SMB_SUPER_MAGIC = 0x517B;

    const int WATCH_INTERVAL = 10000; // 10 sec
    const int MAX_PARTIAL_RETRIES = 5;
}

FileSystemWatcher::FileSystemWatcher(QObject *parent)
    : QFileSystemWatcher(parent)
{
    connect(this, &QFileSystemWatcher::directoryChanged, this, &FileSystemWatcher::scanLocalFolder);
}

FileSystemWatcher::~FileSystemWatcher()
{
#ifndef Q_OS_WIN
    delete m_watchTimer;
#endif
    delete m_partialTorrentTimer;
}

QStringList FileSystemWatcher::directories() const
{
    QStringList dirs = QFileSystemWatcher::directories();
#ifndef Q_OS_WIN
    for (const QDir &dir : qAsConst(m_watchedFolders))
        dirs << dir.canonicalPath();
#endif
    return dirs;
}

void FileSystemWatcher::addPath(const QString &path)
{
    if (path.isEmpty()) return;

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const QDir dir(path);
    if (!dir.exists()) return;

    // Check if the path points to a network file system or not
    if (isNetworkFileSystem(path)) {
        // Network mode
        qDebug("Network folder detected: %s", qUtf8Printable(path));
        qDebug("Using file polling mode instead of inotify...");
        m_watchedFolders << dir;
        // Set up the watch timer
        if (!m_watchTimer) {
            m_watchTimer = new QTimer(this);
            connect(m_watchTimer, &QTimer::timeout, this, &FileSystemWatcher::scanNetworkFolders);
            m_watchTimer->start(WATCH_INTERVAL);
        }
        return;
    }
#endif

    // Normal mode
    qDebug("FS Watching is watching %s in normal mode", qUtf8Printable(path));
    QFileSystemWatcher::addPath(path);
    scanLocalFolder(path);
}

void FileSystemWatcher::removePath(const QString &path)
{
#ifndef Q_OS_WIN
    if (m_watchedFolders.removeOne(path)) {
        if (m_watchedFolders.isEmpty())
            delete m_watchTimer;
        return;
    }
#endif
    // Normal mode
    QFileSystemWatcher::removePath(path);
}

void FileSystemWatcher::scanLocalFolder(const QString &path)
{
    processTorrentsInDir(path);
}

#ifndef Q_OS_WIN
void FileSystemWatcher::scanNetworkFolders()
{
    for (const QDir &dir : qAsConst(m_watchedFolders))
        processTorrentsInDir(dir);
}
#endif

void FileSystemWatcher::processPartialTorrents()
{
    QStringList noLongerPartial;

    // Check which torrents are still partial
    Dict::removeIf(m_partialTorrents, [&noLongerPartial](const QString &torrentPath, int &value)
    {
        if (!QFile::exists(torrentPath))
            return true;

        if (BitTorrent::TorrentInfo::loadFromFile(torrentPath).isValid()) {
            noLongerPartial << torrentPath;
            return true;
        }

        if (value >= MAX_PARTIAL_RETRIES) {
            QFile::rename(torrentPath, torrentPath + ".qbt_rejected");
            return true;
        }

        ++value;
        return false;
    });

    // Stop the partial timer if necessary
    if (m_partialTorrents.empty()) {
        m_partialTorrentTimer->stop();
        m_partialTorrentTimer->deleteLater();
        qDebug("No longer any partial torrent.");
    }
    else {
        qDebug("Still %d partial torrents after delayed processing.", m_partialTorrents.count());
        m_partialTorrentTimer->start(WATCH_INTERVAL);
    }

    // Notify of new torrents
    if (!noLongerPartial.isEmpty())
        emit torrentsAdded(noLongerPartial);
}

void FileSystemWatcher::processTorrentsInDir(const QDir &dir)
{
    QStringList torrents;
    const QStringList files = dir.entryList({"*.torrent", "*.magnet"}, QDir::Files);
    for (const QString &file : files) {
        const QString fileAbsPath = dir.absoluteFilePath(file);
        if (file.endsWith(".magnet"))
            torrents << fileAbsPath;
        else if (BitTorrent::TorrentInfo::loadFromFile(fileAbsPath).isValid())
            torrents << fileAbsPath;
        else if (!m_partialTorrents.contains(fileAbsPath))
            m_partialTorrents[fileAbsPath] = 0;
    }

    if (!torrents.empty())
        emit torrentsAdded(torrents);

    if (!m_partialTorrents.empty() && !m_partialTorrentTimer) {
        m_partialTorrentTimer = new QTimer(this);
        connect(m_partialTorrentTimer, &QTimer::timeout, this, &FileSystemWatcher::processPartialTorrents);
        m_partialTorrentTimer->setSingleShot(true);
        m_partialTorrentTimer->start(WATCH_INTERVAL);
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
bool FileSystemWatcher::isNetworkFileSystem(const QString &path)
{
    QString file = path;
    if (!file.endsWith('/'))
        file += '/';
    file += '.';

    struct statfs buf {};
    if (statfs(file.toLocal8Bit().constData(), &buf) != 0)
        return false;

#ifdef Q_OS_MAC
        // XXX: should we make sure HAVE_STRUCT_FSSTAT_F_FSTYPENAME is defined?
    return ((strncmp(buf.f_fstypename, "nfs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "cifs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "smbfs", sizeof(buf.f_fstypename)) == 0));
#else
    return ((buf.f_type == CIFS_MAGIC_NUMBER)
        || (buf.f_type == NFS_SUPER_MAGIC)
        || (buf.f_type == SMB_SUPER_MAGIC));
#endif
}
#endif
