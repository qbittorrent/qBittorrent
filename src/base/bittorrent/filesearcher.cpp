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

#include <QPromise>

#include "base/bittorrent/common.h"

namespace
{
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

void FileSearcher::search(const PathList &originalFileNames, const Path &savePath
        , const Path &downloadPath, const bool forceAppendExt, QPromise<FileSearchResult> &promise)
{
    Path usedPath = savePath;
    PathList adjustedFileNames = originalFileNames;
    const bool found = findInDir(usedPath, adjustedFileNames, (forceAppendExt && downloadPath.isEmpty()));
    if (!found && !downloadPath.isEmpty())
    {
        usedPath = downloadPath;
        findInDir(usedPath, adjustedFileNames, forceAppendExt);
    }

    promise.addResult(FileSearchResult {.savePath = usedPath, .fileNames = adjustedFileNames});
}
