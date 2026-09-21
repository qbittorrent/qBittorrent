/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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
#include "base/global.h"

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

    qsizetype countInDir(const Path &dirPath, const PathList &fileNames, const qsizetype mustExceed)
    {
        qsizetype count = 0;
        qsizetype remaining = fileNames.size();
        for (const Path &fileName : fileNames)
        {
            if ((dirPath / fileName).exists() || (dirPath / (fileName + QB_EXT)).exists())
                ++count;
            --remaining;
            if (count + remaining <= mustExceed)
                return count;
        }
        return count;
    }

    bool isContainedName(const Path &name)
    {
        if (name.isEmpty() || !name.isRelative())
            return false;
        const QString rootItem = name.rootItem().data();
        return (rootItem != u"."_s) && (rootItem != u".."_s);
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

void FileSearcher::searchRoots(const PathList &originalFileNames, const Path &savePath
        , const Path &downloadPath, const PathList &candidates, const bool forceAppendExt, QPromise<SearchRootsResult> &promise)
{
    enum class Winner { None, SavePath, DownloadPath, Candidate };

    qsizetype bestCount = 0;
    Winner winner = Winner::None;
    Path winnerPath;

    if (!savePath.isEmpty() && savePath.exists())
    {
        const qsizetype count = countInDir(savePath, originalFileNames, bestCount);
        if (count > bestCount)
        {
            bestCount = count;
            winner = Winner::SavePath;
        }
    }

    if ((bestCount < originalFileNames.size()) && !downloadPath.isEmpty() && downloadPath.exists())
    {
        const qsizetype count = countInDir(downloadPath, originalFileNames, bestCount);
        if (count > bestCount)
        {
            bestCount = count;
            winner = Winner::DownloadPath;
        }
    }

    const bool searchedCandidates = !candidates.isEmpty() && (bestCount < originalFileNames.size());

    if (searchedCandidates)
    {
        for (const Path &candidate : candidates)
        {
            if (bestCount == originalFileNames.size())
                break;
            if (candidate.isEmpty() || !candidate.exists())
                continue;
            const qsizetype count = countInDir(candidate, originalFileNames, bestCount);
            if (count > bestCount)
            {
                bestCount = count;
                winner = Winner::Candidate;
                winnerPath = candidate;
            }
        }
    }

    if (winner == Winner::SavePath)
    {
        PathList names = originalFileNames;
        findInDir(savePath, names, (forceAppendExt && downloadPath.isEmpty()));
        promise.addResult(SearchRootsResult {.savePath = savePath, .fileNames = names
                , .matchCount = bestCount, .searchedCandidates = searchedCandidates, .foundAtOwnPath = true});
        return;
    }

    if (winner == Winner::DownloadPath)
    {
        PathList names = originalFileNames;
        findInDir(downloadPath, names, forceAppendExt);
        promise.addResult(SearchRootsResult {.savePath = downloadPath, .fileNames = names
                , .matchCount = bestCount, .searchedCandidates = searchedCandidates, .foundAtOwnPath = true});
        return;
    }

    if (winner == Winner::Candidate)
    {
        PathList names = originalFileNames;
        findInDir(winnerPath, names, false);
        promise.addResult(SearchRootsResult {.savePath = winnerPath, .fileNames = names
                , .matchCount = bestCount, .searchedCandidates = true});
        return;
    }

    PathList names = originalFileNames;
    findInDir(savePath, names, (forceAppendExt && downloadPath.isEmpty()));
    if (!downloadPath.isEmpty())
        findInDir(downloadPath, names, forceAppendExt);
    promise.addResult(SearchRootsResult {.savePath = (downloadPath.isEmpty() ? savePath : downloadPath)
            , .fileNames = names, .searchedCandidates = searchedCandidates});
}

PathList candidateRoots(const Path &savePath, const Path &downloadPath, const PathList &searchRoots
        , const Path &defaultSavePath, const QString &torrentName, const QString &sourceFileName)
{
    PathList result;
    const auto tryAppend = [&](const Path &form)
    {
        if ((form == savePath) || (form == downloadPath) || result.contains(form))
            return;
        result.append(form);
    };

    const Path nameForm = Path(torrentName);
    const bool hasName = isContainedName(nameForm);
    const Path sourceStem = Path(sourceFileName).removedExtension(TORRENT_FILE_EXTENSION);
    const bool hasSource = isContainedName(sourceStem);

    for (const Path &root : searchRoots)
    {
        Path effectiveRoot = root;
        if (effectiveRoot.isEmpty())
            effectiveRoot = defaultSavePath;
        if (effectiveRoot.isEmpty())
            continue;

        tryAppend(effectiveRoot);
        if (hasName)
            tryAppend(effectiveRoot / nameForm);
        if (hasSource)
            tryAppend(effectiveRoot / sourceStem);
    }

    return result;
}
