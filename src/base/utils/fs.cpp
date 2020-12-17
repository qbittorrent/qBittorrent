/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "fs.h"

#include <cerrno>
#include <cstring>

#if defined(Q_OS_WIN)
#include <memory>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(Q_OS_HAIKU)
#include <kernel/fs_info.h>
#else
#include <sys/vfs.h>
#include <unistd.h>
#endif

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QRegularExpression>

#include "base/bittorrent/common.h"
#include "base/global.h"

QString Utils::Fs::toNativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

QString Utils::Fs::toUniformPath(const QString &path)
{
    return QDir::fromNativeSeparators(path);
}

/**
 * Returns the file extension part of a file name.
 */
QString Utils::Fs::fileExtension(const QString &filename)
{
    const QString ext = QString(filename).remove(QB_EXT);
    const int pointIndex = ext.lastIndexOf('.');
    return (pointIndex >= 0) ? ext.mid(pointIndex + 1) : QString();
}

QString Utils::Fs::fileName(const QString &filePath)
{
    const QString path = toUniformPath(filePath);
    const int slashIndex = path.lastIndexOf('/');
    if (slashIndex == -1)
        return path;
    return path.mid(slashIndex + 1);
}

QString Utils::Fs::folderName(const QString &filePath)
{
    const QString path = toUniformPath(filePath);
    const int slashIndex = path.lastIndexOf('/');
    if (slashIndex == -1)
        return {};
    return path.left(slashIndex);
}

/**
 * This function will first check if there are only system cache files, e.g. `Thumbs.db`,
 * `.DS_Store` and/or only temp files that end with '~', e.g. `filename~`.
 * If they are the only files it will try to remove them and delete the folder.
 * This action will be performed for each subfolder starting from the deepest folder.
 * There is an inherent race condition here. A file might appear after it is checked
 * that only the above mentioned "useless" files exist but before the whole folder is removed.
 * In this case, the folder will not be removed but the "useless" files will be deleted.
 */
bool Utils::Fs::smartRemoveEmptyFolderTree(const QString &path)
{
    if (path.isEmpty() || !QDir(path).exists())
        return true;

    const QStringList deleteFilesList =
    {
        // Windows
        QLatin1String("Thumbs.db"),
        QLatin1String("desktop.ini"),
        // Linux
        QLatin1String(".directory"),
        // Mac OS
        QLatin1String(".DS_Store")
    };

    // travel from the deepest folder and remove anything unwanted on the way out.
    QStringList dirList(path + '/');  // get all sub directories paths
    QDirIterator iter(path, (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories);
    while (iter.hasNext())
        dirList << iter.next() + '/';
    // sort descending by directory depth
    std::sort(dirList.begin(), dirList.end()
              , [](const QString &l, const QString &r) { return l.count('/') > r.count('/'); });

    for (const QString &p : asConst(dirList))
    {
        const QDir dir(p);
        // A deeper folder may have not been removed in the previous iteration
        // so don't remove anything from this folder either.
        if (!dir.isEmpty(QDir::Dirs | QDir::NoDotAndDotDot))
            continue;

        const QStringList tmpFileList = dir.entryList(QDir::Files);

        // deleteFilesList contains unwanted files, usually created by the OS
        // temp files on linux usually end with '~', e.g. `filename~`
        const bool hasOtherFiles = std::any_of(tmpFileList.cbegin(), tmpFileList.cend(), [&deleteFilesList](const QString &f)
        {
            return (!f.endsWith('~') && !deleteFilesList.contains(f, Qt::CaseInsensitive));
        });
        if (hasOtherFiles)
            continue;

        for (const QString &f : tmpFileList)
            forceRemove(p + f);

        // remove directory if empty
        dir.rmdir(p);
    }

    return QDir(path).exists();
}

/**
 * Removes the file with the given filePath.
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
    const QFileInfo fi(path);
    if (!fi.exists()) return -1;
    if (fi.isFile()) return fi.size();

    // Compute folder size based on its content
    qint64 size = 0;
    QDirIterator iter(path, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iter.hasNext())
    {
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
    while (!f1.atEnd() && !f2.atEnd())
    {
        if (f1.read(readSize) != f2.read(readSize))
            return false;
    }
    return true;
}

QString Utils::Fs::toValidFileSystemName(const QString &name, const bool allowSeparators, const QString &pad)
{
    const QRegularExpression regex(allowSeparators ? "[:?\"*<>|]+" : "[\\\\/:?\"*<>|]+");

    QString validName = name.trimmed();
    validName.replace(regex, pad);
    qDebug() << "toValidFileSystemName:" << name << "=>" << validName;

    return validName;
}

bool Utils::Fs::isValidFileSystemName(const QString &name, const bool allowSeparators)
{
    if (name.isEmpty()) return false;

#if defined(Q_OS_WIN)
    const QRegularExpression regex
    {allowSeparators
        ? QLatin1String("[:?\"*<>|]")
        : QLatin1String("[\\\\/:?\"*<>|]")};
#elif defined(Q_OS_MACOS)
    const QRegularExpression regex
    {allowSeparators
        ? QLatin1String("[\\0:]")
        : QLatin1String("[\\0/:]")};
#else
    const QRegularExpression regex
    {allowSeparators
        ? QLatin1String("[\\0]")
        : QLatin1String("[\\0/]")};
#endif
    return !name.contains(regex);
}

qint64 Utils::Fs::freeDiskSpaceOnPath(const QString &path)
{
    if (path.isEmpty()) return -1;

    return QStorageInfo(path).bytesAvailable();
}

QString Utils::Fs::branchPath(const QString &filePath, QString *removed)
{
    QString ret = toUniformPath(filePath);
    if (ret.endsWith('/'))
        ret.chop(1);
    const int slashIndex = ret.lastIndexOf('/');
    if (slashIndex >= 0)
    {
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
    const QString ret = path.trimmed();
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

bool Utils::Fs::isRegularFile(const QString &path)
{
    struct ::stat st;
    if (::stat(path.toUtf8().constData(), &st) != 0)
    {
        //  analyse erno and log the error
        const auto err = errno;
        qDebug("Could not get file stats for path '%s'. Error: %s"
               , qUtf8Printable(path), qUtf8Printable(strerror(err)));
        return false;
    }

    return (st.st_mode & S_IFMT) == S_IFREG;
}

#if !defined Q_OS_HAIKU
bool Utils::Fs::isNetworkFileSystem(const QString &path)
{
#if defined(Q_OS_WIN)
    const std::wstring pathW {path.toStdWString()};
    auto volumePath = std::make_unique<wchar_t[]>(path.length() + 1);
    if (!::GetVolumePathNameW(pathW.c_str(), volumePath.get(), (path.length() + 1)))
        return false;

    return (::GetDriveTypeW(volumePath.get()) == DRIVE_REMOTE);
#elif defined(Q_OS_MACOS) || defined(Q_OS_OPENBSD)
    QString file = path;
    if (!file.endsWith('/'))
        file += '/';
    file += '.';

    struct statfs buf {};
    if (statfs(file.toLocal8Bit().constData(), &buf) != 0)
        return false;

    // XXX: should we make sure HAVE_STRUCT_FSSTAT_F_FSTYPENAME is defined?
    return ((strncmp(buf.f_fstypename, "cifs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "nfs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "smbfs", sizeof(buf.f_fstypename)) == 0));
#else // Q_OS_WIN
    QString file = path;
    if (!file.endsWith('/'))
        file += '/';
    file += '.';

    struct statfs buf {};
    if (statfs(file.toLocal8Bit().constData(), &buf) != 0)
        return false;

    // Magic number references:
    // 1. /usr/include/linux/magic.h
    // 2. https://github.com/coreutils/coreutils/blob/master/src/stat.c
    switch (static_cast<unsigned int>(buf.f_type))
    {
    case 0xFF534D42:  // CIFS_MAGIC_NUMBER
    case 0x6969:  // NFS_SUPER_MAGIC
    case 0x517B:  // SMB_SUPER_MAGIC
    case 0xFE534D42:  // S_MAGIC_SMB2
        return true;
    default:
        return false;
    }
#endif // Q_OS_WIN
}
#endif // Q_OS_HAIKU
