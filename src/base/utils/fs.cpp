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
#include <QMimeDatabase>
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
    return QMimeDatabase().suffixForFileName(Utils::Fs::stripQbExtension(filename));
}

bool Utils::Fs::hasQbExtension(const QString &filename)
{
    return filename.endsWith(QB_EXT, Qt::CaseInsensitive);
}

QString Utils::Fs::stripQbExtension(const QString &filename)
{
    return Utils::Fs::hasQbExtension(filename)
        ? filename.chopped(QB_EXT.length())
        : filename;
}

QString Utils::Fs::ensureQbExtension(const QString &filename)
{
    return Utils::Fs::hasQbExtension(filename)
        ? filename
        : filename + QB_EXT;
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

QVector<QString> Utils::Fs::parentFolders(const QString &filePath)
{
    QStringList parts = Utils::Fs::folderName(filePath).split(QLatin1Char{'/'}, Qt::SkipEmptyParts);
    QVector<QString> result;
    for (int i = 0; i < parts.size(); i++)
    {
        QString curFolder = filePath.startsWith("/") ? "/" : "";
        for (int k = 0; k <= i; k++)
        {
            curFolder += parts[k] + QLatin1Char{'/'};
        }
        curFolder.chop(1); // eliminate trailing slash
        result.push_back(curFolder);
    }
    return result;
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

QString Utils::Fs::combinePaths(const QString &p1, const QString &p2)
{
    if (p1.isEmpty() || p2.isEmpty())
        return p1 + p2;

    QString left = p1.endsWith("/") ? p1.chopped(1) : p1;
    QString right = p2.startsWith("/") ? p2.mid(1) : p2;
    return left + QLatin1Char{'/'} + right;
}

QString Utils::Fs::renamePath(const QString &oldPathWithExtension
                              , const std::function<QString (const QString &)> &transformer
                              , bool paths
                              , bool *ok)
{
    bool unusedOk;
    if (ok == nullptr)
        ok = &unusedOk;
    QString oldPath = Utils::Fs::stripQbExtension(oldPathWithExtension);

    QString newPath;
    if (paths)
    {
        newPath = transformer(oldPath);
    }
    else
    {
        QString newName = transformer(Utils::Fs::fileName(oldPath));
        *ok = Utils::Fs::isValidFileSystemName(newName, false);
        if (!*ok)
            return "";
        newPath = Utils::Fs::combinePaths(Utils::Fs::folderName(oldPath), newName);
    }

    if (Utils::Fs::hasQbExtension(oldPathWithExtension))
        newPath = Utils::Fs::ensureQbExtension(newPath);

    *ok = Utils::Fs::isValidFileSystemName(newPath, true);
    if (!*ok)
        return "";

    return newPath;
}

bool Utils::Fs::absolutePathsStartWithSlash()
{
    return Utils::Fs::toUniformPath(QDir::rootPath()).startsWith("/");
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
#else
    QString file = path;
    if (!file.endsWith('/'))
        file += '/';
    file += '.';

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
#endif // Q_OS_HAIKU

QString Utils::Fs::findRootFolder(const QStringList &filePaths)
{
    QString rootFolder;
    for (const QString &filePath : filePaths)
    {
        // when filePath is absolute (file relocated), then filePathElements.(0) is empty, which
        // will force this function to return empty, which is correct.
        const auto filePathElements = QStringView(filePath).split(u'/');
        // if at least one file has no root folder, no common root folder exists
        if (filePathElements.count() <= 1)
            return {};

        if (rootFolder.isEmpty())
            rootFolder = filePathElements.at(0).toString();
        else if (rootFolder != filePathElements.at(0))
            return {};
    }

    return rootFolder;
}

QStringList Utils::Fs::stripRootFolder(const QStringList &filePaths)
{
    const QString commonRootFolder = findRootFolder(filePaths);
    if (commonRootFolder.isEmpty())
        return filePaths;

    QStringList result;
    for (const QString &filePath : filePaths)
        result.push_back(filePath.mid(commonRootFolder.size() + 1));
    return result;
}

QStringList Utils::Fs::addRootFolder(const QStringList &filePaths, const QString &rootFolder)
{
    Q_ASSERT(!rootFolder.isEmpty());

    QStringList result;
    for (const QString &filePath : filePaths)
    {
        if (QDir::isAbsolutePath(filePath))
            return QStringList(); // no-op
        else
            result.push_back(Utils::Fs::combinePaths(rootFolder, filePath));
    }
    return result;
}

Utils::Fs::RenameList Utils::Fs::stringListToRenameList(const QStringList &filePaths)
{
    Utils::Fs::RenameList result;
    for (int idx = 0; idx < filePaths.size(); idx++)
    {
        result.insert(idx, filePaths[idx]);
    }
    return result;
}
