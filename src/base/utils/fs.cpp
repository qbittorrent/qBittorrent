/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#include "fs.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QCoreApplication>
#include <QStorageInfo>

#if defined(Q_OS_WIN)
#include <Windows.h>
#elif defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(Q_OS_HAIKU)
#include <kernel/fs_info.h>
#else
#include <sys/vfs.h>
#endif

#include "base/bittorrent/torrenthandle.h"

/**
 * Converts a path to a string suitable for display.
 * This function makes sure the directory separator used is consistent
 * with the OS being run.
 */
QString Utils::Fs::toNativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

QString Utils::Fs::fromNativePath(const QString &path)
{
    return QDir::fromNativeSeparators(path);
}

/**
 * Returns the file extension part of a file name.
 */
QString Utils::Fs::fileExtension(const QString &filename)
{
    QString ext = QString(filename).remove(QB_EXT);
    const int pointIndex = ext.lastIndexOf(".");
    return (pointIndex >= 0) ? ext.mid(pointIndex + 1) : QString();
}

QString Utils::Fs::fileName(const QString &filePath)
{
    QString path = fromNativePath(filePath);
    const int slashIndex = path.lastIndexOf("/");
    if (slashIndex == -1)
        return path;
    return path.mid(slashIndex + 1);
}

QString Utils::Fs::folderName(const QString &filePath)
{
    QString path = fromNativePath(filePath);
    const int slashIndex = path.lastIndexOf("/");
    if (slashIndex == -1)
        return path;
    return path.left(slashIndex);
}

/**
 * This function will first remove system cache files, e.g. `Thumbs.db`,
 * `.DS_Store`. Then will try to remove the whole tree if the tree consist
 * only of folders
 */
bool Utils::Fs::smartRemoveEmptyFolderTree(const QString &path)
{
    if (path.isEmpty() || !QDir(path).exists())
        return true;

    static const QStringList deleteFilesList = {
        // Windows
        "Thumbs.db",
        "desktop.ini",
        // Linux
        ".directory",
        // Mac OS
        ".DS_Store"
    };

    // travel from the deepest folder and remove anything unwanted on the way out.
    QStringList dirList(path + "/");  // get all sub directories paths
    QDirIterator iter(path, (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories);
    while (iter.hasNext())
        dirList << iter.next() + "/";
    // sort descending by directory depth
    std::sort(dirList.begin(), dirList.end()
              , [](const QString &l, const QString &r) { return l.count("/") > r.count("/"); });

    for (const QString &p : dirList) {
        // remove unwanted files
        for (const QString &f : deleteFilesList) {
            forceRemove(p + f);
        }

        // remove temp files on linux (file ends with '~'), e.g. `filename~`
        QDir dir(p);
        QStringList tmpFileList = dir.entryList(QDir::Files);
        for (const QString &f : tmpFileList) {
            if (f.endsWith("~"))
                forceRemove(p + f);
        }

        // remove directory if empty
        dir.rmdir(p);
    }

    return QDir(path).exists();
}

/**
 * Removes the file with the given file_path.
 *
 * This function will try to fix the file permissions before removing it.
 */
bool Utils::Fs::forceRemove(const QString &filePath)
{
    QFile f(filePath);
    if (!f.exists())
        return true;
    // Make sure we have read/write permissions
    f.setPermissions(f.permissions() | QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    // Remove the file
    return f.remove();
}

/**
 * Removes directory and its content recursively.
 *
 */
void Utils::Fs::removeDirRecursive(const QString &path)
{
    if (!path.isEmpty())
        QDir(path).removeRecursively();
}

/**
 * Returns the size of a file.
 * If the file is a folder, it will compute its size based on its content.
 *
 * Returns -1 in case of error.
 */
qint64 Utils::Fs::computePathSize(const QString &path)
{
    // Check if it is a file
    QFileInfo fi(path);
    if (!fi.exists()) return -1;
    if (fi.isFile()) return fi.size();

    // Compute folder size based on its content
    qint64 size = 0;
    QDirIterator iter(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        iter.next();
        size += iter.fileInfo().size();
    }
    return size;
}

/**
 * Makes deep comparison of two files to make sure they are identical.
 */
bool Utils::Fs::sameFiles(const QString &path1, const QString &path2)
{
    QFile f1(path1), f2(path2);
    if (!f1.exists() || !f2.exists()) return false;
    if (f1.size() != f2.size()) return false;
    if (!f1.open(QIODevice::ReadOnly)) return false;
    if (!f2.open(QIODevice::ReadOnly)) return false;

    const int readSize = 1024 * 1024;  // 1 MiB
    while(!f1.atEnd() && !f2.atEnd()) {
        if (f1.read(readSize) != f2.read(readSize))
            return false;
    }
    return true;
}

QString Utils::Fs::toValidFileSystemName(const QString &name, bool allowSeparators, const QString &pad)
{
    QRegExp regex(allowSeparators ? "[:?\"*<>|]+" : "[\\\\/:?\"*<>|]+");

    QString validName = name.trimmed();
    validName.replace(regex, pad);
    qDebug() << "toValidFileSystemName:" << name << "=>" << validName;

    return validName;
}

bool Utils::Fs::isValidFileSystemName(const QString &name, bool allowSeparators)
{
    if (name.isEmpty()) return false;

    QRegExp regex(allowSeparators ? "[:?\"*<>|]" : "[\\\\/:?\"*<>|]");
    return !name.contains(regex);
}

qint64 Utils::Fs::freeDiskSpaceOnPath(const QString &path)
{
    if (path.isEmpty()) return -1;

    return QStorageInfo(path).bytesAvailable();
}

QString Utils::Fs::branchPath(const QString &filePath, QString *removed)
{
    QString ret = fromNativePath(filePath);
    if (ret.endsWith("/"))
        ret.chop(1);
    const int slashIndex = ret.lastIndexOf("/");
    if (slashIndex >= 0) {
        if (removed)
            *removed = ret.mid(slashIndex + 1);
        ret = ret.left(slashIndex);
    }
    return ret;
}

bool Utils::Fs::sameFileNames(const QString &first, const QString &second)
{
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
    return QString::compare(first, second, Qt::CaseSensitive) == 0;
#else
    return QString::compare(first, second, Qt::CaseInsensitive) == 0;
#endif
}

QString Utils::Fs::expandPath(const QString &path)
{
    QString ret = path.trimmed();
    if (ret.isEmpty())
        return ret;

    return QDir::cleanPath(ret);
}

QString Utils::Fs::expandPathAbs(const QString &path)
{
    return QDir(expandPath(path)).absolutePath();
}

QString Utils::Fs::tempPath()
{
    static const QString path = QDir::tempPath() + "/.qBittorrent/";
    QDir().mkdir(path);
    return path;
}
