/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "filesearcher.h"

#include <algorithm>

#include <QPromise>

#include "base/bittorrent/common.h"

namespace
{
    Path makeRootFolderVariant(const Path &rootFolder, const int suffix)
    {
        Q_ASSERT(suffix > 0);

        return Path(rootFolder.data() + u" (" + QString::number(suffix) + u")");
    }

    bool findInDir(const Path &dirPath, PathList &fileNames, const bool forceAppendExt)
    {
        bool found = false;
        for (Path &fileName : fileNames)
        {
            if ((dirPath / fileName).exists())
            {
                found = true;
            }
            else
            {
                const Path incompleteFilename = fileName + QB_EXT;
                if ((dirPath / incompleteFilename).exists())
                {
                    found = true;
                    fileName = incompleteFilename;
                }
                else if (forceAppendExt)
                {
                    fileName = incompleteFilename;
                }
            }
        }

        return found;
    }
}

Path FileSearcher::makeRootFolderUnique(PathList &fileNames, const Path &baseRootFolder, const QList<Path> &basePaths
        , const QSet<Path> &occupiedRootPaths)
{
    const Path rootFolder = Path::findRootFolder(fileNames);
    if (rootFolder.isEmpty())
        return {};

    const Path baseVariantRootFolder = (baseRootFolder.isEmpty() ? rootFolder : baseRootFolder);
    Path newRootFolder = rootFolder;
    for (int suffix = 1; ; ++suffix)
    {
        const auto iter = std::ranges::find_if(basePaths, [&occupiedRootPaths, &newRootFolder](const Path &basePath)
        {
            return !basePath.isEmpty() && occupiedRootPaths.contains(basePath / newRootFolder);
        });
        if (iter == basePaths.cend())
            break;

        newRootFolder = makeRootFolderVariant(baseVariantRootFolder, suffix);
    }

    if (newRootFolder != rootFolder)
    {
        Path::stripRootFolder(fileNames);
        Path::addRootFolder(fileNames, newRootFolder);
    }

    return newRootFolder;
}

void FileSearcher::search(const PathList &originalFileNames, const Path &savePath
        , const Path &downloadPath, const bool forceAppendExt, const QSet<Path> &occupiedRootPaths
        , QPromise<FileSearchResult> &promise)
{
    Path usedPath = savePath;
    PathList adjustedFileNames = originalFileNames;
    const Path rootFolder = Path::findRootFolder(originalFileNames);
    const bool found = findInDir(usedPath, adjustedFileNames, (forceAppendExt && downloadPath.isEmpty()));
    if (!found && !downloadPath.isEmpty())
    {
        usedPath = downloadPath;
        findInDir(usedPath, adjustedFileNames, forceAppendExt);
    }

    QList<Path> basePaths {usedPath};
    if (savePath != usedPath)
        basePaths.append(savePath);
    makeRootFolderUnique(adjustedFileNames, rootFolder, basePaths, occupiedRootPaths);

    promise.addResult(FileSearchResult {.savePath = usedPath, .fileNames = adjustedFileNames, .rootFolder = rootFolder});
}
