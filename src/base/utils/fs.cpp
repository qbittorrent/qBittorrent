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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QDirIterator>
#include <QCoreApplication>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#endif

#ifndef Q_OS_WIN
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(Q_OS_HAIKU)
#include <kernel/fs_info.h>
#else
#include <sys/vfs.h>
#endif
#else
#include <shlobj.h>
#include <winbase.h>
#endif

#include <QStandardPaths>


#include "misc.h"
#include "fs.h"

/**
 * Converts a path to a string suitable for display.
 * This function makes sure the directory separator used is consistent
 * with the OS being run.
 */
QString Utils::Fs::toNativePath(const QString& path)
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
    QString ext = QString(filename).remove(".!qB");
    const int point_index = ext.lastIndexOf(".");
    return (point_index >= 0) ? ext.mid(point_index + 1) : QString();
}

QString Utils::Fs::fileName(const QString& file_path)
{
    QString path = fromNativePath(file_path);
    const int slash_index = path.lastIndexOf("/");
    if (slash_index == -1)
        return path;
    return path.mid(slash_index + 1);
}

QString Utils::Fs::folderName(const QString& file_path)
{
    QString path = fromNativePath(file_path);
    const int slash_index = path.lastIndexOf("/");
    if (slash_index == -1)
        return path;
    return path.left(slash_index);
}

/**
 * This function will first remove system cache files, e.g. `Thumbs.db`,
 * `.DS_Store`. Then will try to remove the whole tree if the tree consist
 * only of folders
 */
bool Utils::Fs::smartRemoveEmptyFolderTree(const QString& path)
{
    if (!QDir(path).exists())
        return false;

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
    std::sort(dirList.begin(), dirList.end(), [](const QString &l, const QString &r) { return l.count("/") > r.count("/"); });  // sort descending by directory depth

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
bool Utils::Fs::forceRemove(const QString& file_path)
{
    QFile f(file_path);
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
void Utils::Fs::removeDirRecursive(const QString& dirName)
{
    QDir(dirName).removeRecursively();
}

/**
 * Returns the size of a file.
 * If the file is a folder, it will compute its size based on its content.
 *
 * Returns -1 in case of error.
 */
qint64 Utils::Fs::computePathSize(const QString& path)
{
    // Check if it is a file
    QFileInfo fi(path);
    if (!fi.exists()) return -1;
    if (fi.isFile()) return fi.size();
    // Compute folder size based on its content
    qint64 size = 0;
    foreach (const QFileInfo &subfi, QDir(path).entryInfoList(QDir::Dirs|QDir::Files)) {
        if (subfi.fileName().startsWith(".")) continue;
        if (subfi.isDir())
            size += computePathSize(subfi.absoluteFilePath());
        else
            size += subfi.size();
    }
    return size;
}

/**
 * Makes deep comparison of two files to make sure they are identical.
 */
bool Utils::Fs::sameFiles(const QString& path1, const QString& path2)
{
    QFile f1(path1), f2(path2);
    if (!f1.exists() || !f2.exists()) return false;
    if (f1.size() != f2.size()) return false;
    if (!f1.open(QIODevice::ReadOnly)) return false;
    if (!f2.open(QIODevice::ReadOnly)) {
        f1.close();
        return false;
    }
    bool same = true;
    while(!f1.atEnd() && !f2.atEnd()) {
        if (f1.read(1024) != f2.read(1024)) {
            same = false;
            break;
        }
    }
    f1.close(); f2.close();
    return same;
}

QString Utils::Fs::toValidFileSystemName(const QString &name, bool allowSeparators)
{
    QRegExp regex(allowSeparators ? "[:?\"*<>|]+" : "[\\\\/:?\"*<>|]+");

    QString validName = name.trimmed();
    validName.replace(regex, " ");
    qDebug() << "toValidFileSystemName:" << name << "=>" << validName;

    return validName;
}

bool Utils::Fs::isValidFileSystemName(const QString &name, bool allowSeparators)
{
    if (name.isEmpty()) return false;

    QRegExp regex(allowSeparators ? "[:?\"*<>|]" : "[\\\\/:?\"*<>|]");
    return !name.contains(regex);
}

qulonglong Utils::Fs::freeDiskSpaceOnPath(const QString &path)
{
    if (path.isEmpty()) return 0;

    QDir dirPath(path);
    if (!dirPath.exists()) {
        QStringList parts = path.split("/");
        while (parts.size() > 1 && !QDir(parts.join("/")).exists())
            parts.removeLast();

        dirPath = QDir(parts.join("/"));
        if (!dirPath.exists()) return 0;
    }
    Q_ASSERT(dirPath.exists());

#if defined(Q_OS_WIN)
    ULARGE_INTEGER bytesFree;
    LPCWSTR nativePath = reinterpret_cast<LPCWSTR>((toNativePath(dirPath.path())).utf16());
    if (GetDiskFreeSpaceExW(nativePath, &bytesFree, NULL, NULL) == 0)
        return 0;
    return bytesFree.QuadPart;
#elif defined(Q_OS_HAIKU)
    const QString statfsPath = dirPath.path() + "/.";
    dev_t device = dev_for_path(qPrintable(statfsPath));
    if (device < 0)
        return 0;
    fs_info info;
    if (fs_stat_dev(device, &info) != B_OK)
        return 0;
    return ((qulonglong) info.free_blocks * (qulonglong) info.block_size);
#else
    struct statfs stats;
    const QString statfsPath = dirPath.path() + "/.";
    const int ret = statfs(qPrintable(statfsPath), &stats);
    if (ret != 0)
        return 0;
    return ((qulonglong) stats.f_bavail * (qulonglong) stats.f_bsize);
#endif
}

QString Utils::Fs::branchPath(const QString& file_path, QString* removed)
{
    QString ret = fromNativePath(file_path);
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
    QString ret = fromNativePath(path.trimmed());
    if (ret.isEmpty())
        return ret;

    return QDir::cleanPath(ret);
}

QString Utils::Fs::expandPathAbs(const QString& path)
{
    QString ret = expandPath(path);

    if (!QDir::isAbsolutePath(ret))
        ret = QDir(ret).absolutePath();

    return ret;
}

QString Utils::Fs::QDesktopServicesDataLocation()
{
    QString result;
#if defined(Q_OS_WIN)
    wchar_t path[MAX_PATH + 1] = {L'\0'};
    if (SHGetSpecialFolderPathW(0, path, CSIDL_LOCAL_APPDATA, FALSE))
        result = fromNativePath(QString::fromWCharArray(path));
    if (!QCoreApplication::applicationName().isEmpty())
        result += QLatin1String("/") + qApp->applicationName();
#elif defined(Q_OS_MAC)
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kApplicationSupportFolderType, false, &ref);
    if (err)
        return QString();
    QByteArray ba(2048, 0);
    if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
        result = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
    result += QLatin1Char('/') + qApp->applicationName();
#else
    QString xdgDataHome = QLatin1String(qgetenv("XDG_DATA_HOME"));
    if (xdgDataHome.isEmpty())
        xdgDataHome = QDir::homePath() + QLatin1String("/.local/share");
    xdgDataHome += QLatin1String("/data/")
            + qApp->applicationName();
    result = xdgDataHome;
#endif
    if (!result.endsWith("/"))
        result += "/";
    return result;
}

QString Utils::Fs::QDesktopServicesCacheLocation()
{
    QString result;
#if defined(Q_OS_WIN) || defined(Q_OS_OS2)
    result = QDesktopServicesDataLocation() + QLatin1String("cache");
#else
#ifdef Q_OS_MAC
    // http://developer.apple.com/documentation/Carbon/Reference/Folder_Manager/Reference/reference.html
    FSRef ref;
    OSErr err = FSFindFolder(kUserDomain, kCachedDataFolderType, false, &ref);
    if (err)
        return QString();
    QByteArray ba(2048, 0);
    if (FSRefMakePath(&ref, reinterpret_cast<UInt8 *>(ba.data()), ba.size()) == noErr)
        result = QString::fromUtf8(ba).normalized(QString::NormalizationForm_C);
    result += QLatin1Char('/') + qApp->applicationName();
#else
    QString xdgCacheHome = QLatin1String(qgetenv("XDG_CACHE_HOME"));
    if (xdgCacheHome.isEmpty())
        xdgCacheHome = QDir::homePath() + QLatin1String("/.cache");
    xdgCacheHome += QLatin1Char('/') + QCoreApplication::applicationName();
    result = xdgCacheHome;
#endif
#endif
    if (!result.endsWith("/"))
        result += "/";
    return result;
}

QString Utils::Fs::QDesktopServicesDownloadLocation()
{
#if defined(Q_OS_WIN)
    if (QSysInfo::windowsVersion() <= QSysInfo::WV_XP)  // Windows XP
        return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absoluteFilePath(
                    QCoreApplication::translate("fsutils", "Downloads"));
#endif
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

QString Utils::Fs::cacheLocation()
{
    QString location = expandPathAbs(QDesktopServicesCacheLocation());
    QDir locationDir(location);
    if (!locationDir.exists())
        locationDir.mkpath(locationDir.absolutePath());
    return location;
}

QString Utils::Fs::tempPath()
{
    static const QString path = QDir::tempPath() + "/.qBittorrent/";
    QDir().mkdir(path);
    return path;
}
