/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Vladimir Golovnev <glassez@yandex.ru>
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

#include "abstractfilestorage.h"

#include <QDir>
#include <QHash>
#include <QVector>

#include "base/bittorrent/common.h"
#include "base/exceptions.h"
#include "base/utils/fs.h"

#if defined(Q_OS_WIN)
const Qt::CaseSensitivity CASE_SENSITIVITY {Qt::CaseInsensitive};
#else
const Qt::CaseSensitivity CASE_SENSITIVITY {Qt::CaseSensitive};
#endif

namespace
{
    bool areSameFileNames(QString first, QString second)
    {
        if (first.endsWith(QB_EXT, Qt::CaseInsensitive))
            first.chop(QB_EXT.size());
        if (second.endsWith(QB_EXT, Qt::CaseInsensitive))
            second.chop(QB_EXT.size());
        return QString::compare(first, second, CASE_SENSITIVITY) == 0;
    }
}

void BitTorrent::AbstractFileStorage::renameFile(const QString &oldPath, const QString &newPath)
{
    if (!Utils::Fs::isValidFileSystemName(oldPath, true))
        throw RuntimeError {tr("The old path is invalid: '%1'.").arg(oldPath)};
    if (!Utils::Fs::isValidFileSystemName(newPath, true))
        throw RuntimeError {tr("The new path is invalid: '%1'.").arg(newPath)};

    const QString oldFilePath = Utils::Fs::toUniformPath(oldPath);
    if (oldFilePath.endsWith(QLatin1Char {'/'}))
        throw RuntimeError {tr("Invalid file path: '%1'.").arg(oldFilePath)};

    const QString newFilePath = Utils::Fs::toUniformPath(newPath);
    if (newFilePath.endsWith(QLatin1Char {'/'}))
        throw RuntimeError {tr("Invalid file path: '%1'.").arg(newFilePath)};
    if (QDir().isAbsolutePath(newFilePath))
        throw RuntimeError {tr("Absolute path isn't allowed: '%1'.").arg(newFilePath)};

    int renamingFileIndex = -1;
    for (int i = 0; i < filesCount(); ++i)
    {
        const QString path = filePath(i);

        if ((renamingFileIndex < 0) && areSameFileNames(path, oldFilePath))
            renamingFileIndex = i;

        if (areSameFileNames(path, newFilePath))
            throw RuntimeError {tr("The file already exists: '%1'.").arg(newFilePath)};
    }

    if (renamingFileIndex < 0)
        throw RuntimeError {tr("No such file: '%1'.").arg(oldFilePath)};

    const auto extAdjusted = [](const QString &path, const bool needExt) -> QString
    {
        if (path.endsWith(QB_EXT, Qt::CaseInsensitive) == needExt)
            return path;

        return (needExt ? (path + QB_EXT) : (path.left(path.size() - QB_EXT.size())));
    };

    renameFile(renamingFileIndex, extAdjusted(newFilePath, filePath(renamingFileIndex).endsWith(QB_EXT, Qt::CaseInsensitive)));
}

void BitTorrent::AbstractFileStorage::renameFolder(const QString &oldPath, const QString &newPath)
{
    if (!Utils::Fs::isValidFileSystemName(oldPath, true))
        throw RuntimeError {tr("The old path is invalid: '%1'.").arg(oldPath)};
    if (!Utils::Fs::isValidFileSystemName(newPath, true))
        throw RuntimeError {tr("The new path is invalid: '%1'.").arg(newPath)};

    const auto cleanFolderPath = [](const QString &path) -> QString
    {
        const QString uniformPath = Utils::Fs::toUniformPath(path);
        return (uniformPath.endsWith(QLatin1Char {'/'}) ? uniformPath : uniformPath + QLatin1Char {'/'});
    };

    const QString oldFolderPath = cleanFolderPath(oldPath);
    const QString newFolderPath = cleanFolderPath(newPath);
    if (QDir().isAbsolutePath(newFolderPath))
        throw RuntimeError {tr("Absolute path isn't allowed: '%1'.").arg(newFolderPath)};

    QVector<int> renamingFileIndexes;
    renamingFileIndexes.reserve(filesCount());

    for (int i = 0; i < filesCount(); ++i)
    {
        const QString path = filePath(i);

        if (path.startsWith(oldFolderPath, CASE_SENSITIVITY))
            renamingFileIndexes.append(i);

        if (path.startsWith(newFolderPath, CASE_SENSITIVITY))
            throw RuntimeError {tr("The folder already exists: '%1'.").arg(newFolderPath)};
    }

    if (renamingFileIndexes.isEmpty())
        throw RuntimeError {tr("No such folder: '%1'.").arg(oldFolderPath)};

    for (const int index : renamingFileIndexes)
    {
        const QString newFilePath = newFolderPath + filePath(index).mid(oldFolderPath.size());
        renameFile(index, newFilePath);
    }
}
