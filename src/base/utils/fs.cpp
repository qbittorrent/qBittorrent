/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <filesystem>

#if defined(Q_OS_WIN)
#include <memory>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(Q_OS_HAIKU)
#include <kernel/fs_info.h>
#else
#include <sys/vfs.h>
#include <unistd.h>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>

#include "base/path.h"

/**
 * This function will first check if there are only system cache files, e.g. `Thumbs.db`,
 * `.DS_Store` and/or only temp files that end with '~', e.g. `filename~`.
 * If they are the only files it will try to remove them and delete the folder.
 * This action will be performed for each subfolder starting from the deepest folder.
 * There is an inherent race condition here. A file might appear after it is checked
 * that only the above mentioned "useless" files exist but before the whole folder is removed.
 * In this case, the folder will not be removed but the "useless" files will be deleted.
 */
bool Utils::Fs::smartRemoveEmptyFolderTree(const Path &path)
{
    if (!path.exists())
        return true;

    const QStringList deleteFilesList =
    {
        // Windows
        u"Thumbs.db"_s,
        u"desktop.ini"_s,
        // Linux
        u".directory"_s,
        // Mac OS
        u".DS_Store"_s
    };

    // travel from the deepest folder and remove anything unwanted on the way out.
    QStringList dirList(path.data() + u'/');  // get all sub directories paths
    QDirIterator iter {path.data(), (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories};
    while (iter.hasNext())
        dirList << iter.next() + u'/';
    // sort descending by directory depth
    std::sort(dirList.begin(), dirList.end()
              , [](const QString &l, const QString &r) { return l.count(u'/') > r.count(u'/'); });

    for (const QString &p : asConst(dirList))
    {
        const QDir dir {p};
        // A deeper folder may have not been removed in the previous iteration
        // so don't remove anything from this folder either.
        if (!dir.isEmpty(QDir::Dirs | QDir::NoDotAndDotDot))
            continue;

        const QStringList tmpFileList = dir.entryList(QDir::Files);

        // deleteFilesList contains unwanted files, usually created by the OS
        // temp files on linux usually end with '~', e.g. `filename~`
        const bool hasOtherFiles = std::any_of(tmpFileList.cbegin(), tmpFileList.cend(), [&deleteFilesList](const QString &f)
        {
            return (!f.endsWith(u'~') && !deleteFilesList.contains(f, Qt::CaseInsensitive));
        });
        if (hasOtherFiles)
            continue;

        for (const QString &f : tmpFileList)
            removeFile(Path(p + f));

        // remove directory if empty
        dir.rmdir(p);
    }

    return path.exists();
}

/**
 * Removes directory and its content recursively.
 */
void Utils::Fs::removeDirRecursively(const Path &path)
{
    if (!path.isEmpty())
        QDir(path.data()).removeRecursively();
}

/**
 * Returns the size of a file.
 * If the file is a folder, it will compute its size based on its content.
 *
 * Returns -1 in case of error.
 */
qint64 Utils::Fs::computePathSize(const Path &path)
{
    // Check if it is a file
    const QFileInfo fi {path.data()};
    if (!fi.exists()) return -1;
    if (fi.isFile()) return fi.size();

    // Compute folder size based on its content
    qint64 size = 0;
    QDirIterator iter {path.data(), (QDir::Files | QDir::Hidden | QDir::NoSymLinks), QDirIterator::Subdirectories};
    while (iter.hasNext())
    {
        const QFileInfo fileInfo = iter.nextFileInfo();
        size += fileInfo.size();
    }
    return size;
}

/**
 * Makes deep comparison of two files to make sure they are identical.
 * The point is about the file contents. If the files do not exist then
 * the paths refers to nothing and therefore we cannot say the files are same
 * (because there are no files!)
 */
bool Utils::Fs::sameFiles(const Path &path1, const Path &path2)
{
    QFile f1 {path1.data()};
    QFile f2 {path2.data()};

    if (!f1.exists() || !f2.exists())
        return false;
    if (path1 == path2)
        return true;
    if (f1.size() != f2.size())
        return false;
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly))
        return false;

    const int readSize = 1024 * 1024;  // 1 MiB
    while (!f1.atEnd() && !f2.atEnd())
    {
        if (f1.read(readSize) != f2.read(readSize))
            return false;
    }
    return true;
}

QString Utils::Fs::toValidFileName(const QString &name, const QString &pad)
{
    const QRegularExpression regex {u"[\\\\/:?\"*<>|]+"_s};

    QString validName = name.trimmed();
    validName.replace(regex, pad);

    return validName;
}

Path Utils::Fs::toValidPath(const QString &name, const QString &pad)
{
    const QRegularExpression regex {u"[:?\"*<>|]+"_s};

    QString validPathStr = name;
    validPathStr.replace(regex, pad);

    return Path(validPathStr);
}

qint64 Utils::Fs::freeDiskSpaceOnPath(const Path &path)
{
    return QStorageInfo(path.data()).bytesAvailable();
}

Path Utils::Fs::tempPath()
{
    static const Path path = Path(QDir::tempPath()) / Path(u".qBittorrent"_s);
    mkdir(path);
    return path;
}

bool Utils::Fs::isRegularFile(const Path &path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path.toStdFsPath(), ec);
}

bool Utils::Fs::isNetworkFileSystem(const Path &path)
{
#if defined Q_OS_HAIKU
    return false;
#elif defined(Q_OS_WIN)
    const std::wstring pathW = path.toString().toStdWString();
    auto volumePath = std::make_unique<wchar_t[]>(pathW.length() + 1);
    if (!::GetVolumePathNameW(pathW.c_str(), volumePath.get(), static_cast<DWORD>(pathW.length() + 1)))
        return false;
    return (::GetDriveTypeW(volumePath.get()) == DRIVE_REMOTE);
#else
    const QString file = (path.toString() + u"/.");
    struct statfs buf {};
    if (statfs(file.toLocal8Bit().constData(), &buf) != 0)
        return false;

#if defined(Q_OS_OPENBSD)
    return ((strncmp(buf.f_fstypename, "cifs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "nfs", sizeof(buf.f_fstypename)) == 0)
        || (strncmp(buf.f_fstypename, "smbfs", sizeof(buf.f_fstypename)) == 0));
#else
    // Magic number reference:
    // https://github.com/coreutils/coreutils/blob/master/src/stat.c
    switch (static_cast<quint32>(buf.f_type))
    {
    case 0x0000517B:  // SMB
    case 0x0000564C:  // NCP
    case 0x00006969:  // NFS
    case 0x00C36400:  // CEPH
    case 0x01161970:  // GFS
    case 0x013111A8:  // IBRIX
    case 0x0BD00BD0:  // LUSTRE
    case 0x19830326:  // FHGFS
    case 0x47504653:  // GPFS
    case 0x50495045:  // PIPEFS
    case 0x5346414F:  // AFS
    case 0x61636673:  // ACFS
    case 0x61756673:  // AUFS
    case 0x65735543:  // FUSECTL
    case 0x65735546:  // FUSEBLK
    case 0x6B414653:  // KAFS
    case 0x6E667364:  // NFSD
    case 0x73757245:  // CODA
    case 0x7461636F:  // OCFS2
    case 0x786F4256:  // VBOXSF
    case 0x794C7630:  // OVERLAYFS
    case 0x7C7C6673:  // PRL_FS
    case 0xA501FCF5:  // VXFS
    case 0xAAD7AAEA:  // OVERLAYFS
    case 0xBACBACBC:  // VMHGFS
    case 0xBEEFDEAD:  // SNFS
    case 0xFE534D42:  // SMB2
    case 0xFF534D42:  // CIFS
        return true;
    default:
        break;
    }

    return false;
#endif
#endif
}

bool Utils::Fs::copyFile(const Path &from, const Path &to)
{
    if (!from.exists())
        return false;

    if (!mkpath(to.parentPath()))
        return false;

    return QFile::copy(from.data(), to.data());
}

bool Utils::Fs::renameFile(const Path &from, const Path &to)
{
    return QFile::rename(from.data(), to.data());
}

/**
 * Removes the file with the given filePath.
 *
 * This function will try to fix the file permissions before removing it.
 */
nonstd::expected<void, QString> Utils::Fs::removeFile(const Path &path)
{
    QFile file {path.data()};
    if (file.remove())
        return {};

    if (!file.exists())
        return {};

    // Make sure we have read/write permissions
    file.setPermissions(file.permissions() | QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (file.remove())
        return {};

    return nonstd::make_unexpected(file.errorString());
}

nonstd::expected<void, QString> Utils::Fs::moveFileToTrash(const Path &path)
{
    QFile file {path.data()};
    if (file.moveToTrash())
        return {};

    if (!file.exists())
        return {};

    // Make sure we have read/write permissions
    file.setPermissions(file.permissions() | QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (file.moveToTrash())
        return {};

    const QString errorMessage = file.errorString();
    return nonstd::make_unexpected(!errorMessage.isEmpty() ? errorMessage : QCoreApplication::translate("fs", "Unknown error"));
}


bool Utils::Fs::isReadable(const Path &path)
{
    return QFileInfo(path.data()).isReadable();
}

bool Utils::Fs::isWritable(const Path &path)
{
    return QFileInfo(path.data()).isWritable();
}

QDateTime Utils::Fs::lastModified(const Path &path)
{
    return QFileInfo(path.data()).lastModified();
}

bool Utils::Fs::isDir(const Path &path)
{
    return QFileInfo(path.data()).isDir();
}

Path Utils::Fs::toAbsolutePath(const Path &path)
{
    return Path(QFileInfo(path.data()).absoluteFilePath());
}

Path Utils::Fs::toCanonicalPath(const Path &path)
{
    return Path(QFileInfo(path.data()).canonicalFilePath());
}

Path Utils::Fs::homePath()
{
    return Path(QDir::homePath());
}

bool Utils::Fs::mkdir(const Path &dirPath)
{
    return QDir().mkdir(dirPath.data());
}

bool Utils::Fs::mkpath(const Path &dirPath)
{
    return QDir().mkpath(dirPath.data());
}

bool Utils::Fs::rmdir(const Path &dirPath)
{
    return QDir().rmdir(dirPath.data());
}
