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
#include <QList>

#include "base/exceptions.h"
#include "base/path.h"
#include "base/utils/fs.h"

void BitTorrent::AbstractFileStorage::renameFile(const Path &oldPath, const Path &newPath)
{
    if (!oldPath.isValid())
        throw RuntimeError(tr("The old path is invalid: '%1'.").arg(oldPath.toString()));
    if (!newPath.isValid())
        throw RuntimeError(tr("The new path is invalid: '%1'.").arg(newPath.toString()));
    if (newPath.isAbsolute())
        throw RuntimeError(tr("Absolute path isn't allowed: '%1'.").arg(newPath.toString()));

    int renamingFileIndex = -1;
    for (int i = 0; i < filesCount(); ++i)
    {
        const Path path = filePath(i);

        if ((renamingFileIndex < 0) && (path == oldPath))
            renamingFileIndex = i;
        else if (path == newPath)
            throw RuntimeError(tr("The file already exists: '%1'.").arg(newPath.toString()));
    }

    if (renamingFileIndex < 0)
        throw RuntimeError(tr("No such file: '%1'.").arg(oldPath.toString()));

    renameFile(renamingFileIndex, newPath);
}

void BitTorrent::AbstractFileStorage::renameFolder(const Path &oldFolderPath, const Path &newFolderPath)
{
    if (!oldFolderPath.isValid())
        throw RuntimeError(tr("The old path is invalid: '%1'.").arg(oldFolderPath.toString()));
    if (!newFolderPath.isValid())
        throw RuntimeError(tr("The new path is invalid: '%1'.").arg(newFolderPath.toString()));
    if (newFolderPath.isAbsolute())
        throw RuntimeError(tr("Absolute path isn't allowed: '%1'.").arg(newFolderPath.toString()));

    QList<int> renamingFileIndexes;
    renamingFileIndexes.reserve(filesCount());

    for (int i = 0; i < filesCount(); ++i)
    {
        const Path path = filePath(i);

        if (path.hasAncestor(oldFolderPath))
            renamingFileIndexes.append(i);
        else if (path.hasAncestor(newFolderPath))
            throw RuntimeError(tr("The folder already exists: '%1'.").arg(newFolderPath.toString()));
    }

    if (renamingFileIndexes.isEmpty())
        throw RuntimeError(tr("No such folder: '%1'.").arg(oldFolderPath.toString()));

    for (const int index : renamingFileIndexes)
    {
        const Path newFilePath = newFolderPath / oldFolderPath.relativePathOf(filePath(index));
        renameFile(index, newFilePath);
    }
}
