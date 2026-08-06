/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  The qBittorrent project
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

#include "toplevelpayload.h"

#include <algorithm>

#include <QCoreApplication>
#include <QSet>

#include "base/global.h"
#include "base/utils/fs.h"

namespace
{
    using BitTorrent::TorrentContentLayout;

    constexpr int HASH_TAG_HEX_LEN = 12;

    PathList renameRootFolder(PathList filePaths, const QString &oldName, const QString &newName)
    {
        const Path oldRoot {oldName};
        const Path newRoot {newName};
        for (Path &filePath : filePaths)
        {
            if (filePath == oldRoot)
                filePath = newRoot;
            else if (filePath.hasAncestor(oldRoot))
                filePath = newRoot / oldRoot.relativePathOf(filePath);
        }
        return filePaths;
    }

    // Same limits as Utils::Fs::isValidFileName (Win: 255 chars, else 255 UTF-8 bytes).
    bool exceedsFileNameLengthLimit(const QString &name)
    {
#ifdef Q_OS_WIN
        return (name.length() > 255);
#else
        return (name.toUtf8().length() > 255);
#endif
    }

    // Always keep full tag at the end; shorten the name until base+tag fits the platform limit.
    QString taggedComponent(const QString &name, const QString &tag)
    {
        QString base = Utils::Fs::toValidFileName(name.trimmed());
        if (base.isEmpty())
            base = u"Torrent"_s;

        if (base.endsWith(tag))
            base.chop(tag.size());

        QString result = base + tag;
        while (exceedsFileNameLengthLimit(result) && !base.isEmpty())
        {
            base.chop(1);
#ifdef Q_OS_WIN
            while (base.endsWith(u'.') || base.endsWith(u' '))
                base.chop(1);
#endif
            result = base + tag;
        }

        if (base.isEmpty())
            result = u"Torrent"_s + tag;

        return result;
    }

    QString originalNameForUniqueDir(const PathList &filePaths, const QString &torrentName, const QString &tag)
    {
        QString base = torrentName.trimmed();
        if (base.isEmpty())
        {
            const Path root = Path::findRootFolder(filePaths);
            if (!root.isEmpty())
                base = root.toString();
            else if (!filePaths.isEmpty())
                base = filePaths.at(0).filename();
        }
        if (base.isEmpty())
            base = u"Torrent"_s;

        if (base.endsWith(tag))
            base.chop(tag.size());

        // Match Subfolder naming for single-file torrents: folder uses name without extension
        // (e.g. movie.mkv → "movie <hash>/movie.mkv"). Also keeps applyUniqueSubfolderLayout idempotent
        // when paths are already under that folder.
        if (filePaths.size() == 1)
            base = Path(base).removedExtension().toString();

        if (base.isEmpty())
            base = u"Torrent"_s;

        return base;
    }

    bool isUnderUniqueRoot(const Path &path, const Path &uniqueRoot)
    {
        return (path == uniqueRoot) || path.hasAncestor(uniqueRoot);
    }

    Path stripPathPrefix(const Path &path, const Path &prefix)
    {
        if (path == prefix)
            return {};

        const QString pathStr = path.data();
        const QString prefixStr = prefix.data();
        if (pathStr.startsWith(prefixStr + u'/'))
            return Path(pathStr.sliced(prefixStr.size() + 1));
        return path;
    }

    // Map paths into uniqueRoot.
    // - NoSubfolder: prepend uniqueRoot to the complete path.
    // - Original/Subfolder: among paths not already under uniqueRoot, require one shared
    //   top-level folder and replace it; single-component remaining paths are prepended.
    // - Paths already under uniqueRoot are left unchanged.
    // Returns empty PathList if Original/Subfolder remaining paths are nested but share no root.
    PathList mapPathsIntoUniqueRoot(const PathList &filePaths, const Path &uniqueRoot
            , const TorrentContentLayout layout, bool *const mappingOk = nullptr)
    {
        if (mappingOk)
            *mappingOk = true;

        PathList out = filePaths;
        QList<int> remainingIndexes;
        remainingIndexes.reserve(filePaths.size());

        for (int i = 0; i < filePaths.size(); ++i)
        {
            if (isUnderUniqueRoot(filePaths.at(i), uniqueRoot))
                out[i] = filePaths.at(i);
            else
                remainingIndexes.append(i);
        }

        if (remainingIndexes.isEmpty())
            return out;

        if (layout == TorrentContentLayout::NoSubfolder)
        {
            for (const int i : asConst(remainingIndexes))
                out[i] = uniqueRoot / filePaths.at(i);
            return out;
        }

        PathList remainingPaths;
        remainingPaths.reserve(remainingIndexes.size());
        for (const int i : asConst(remainingIndexes))
            remainingPaths.append(filePaths.at(i));

        const Path sharedRoot = Path::findRootFolder(remainingPaths);
        if (!sharedRoot.isEmpty())
        {
            for (const int i : asConst(remainingIndexes))
            {
                const Path relative = stripPathPrefix(filePaths.at(i), sharedRoot);
                out[i] = relative.isEmpty() ? uniqueRoot : (uniqueRoot / relative);
            }
            return out;
        }

        // No shared root: only allow single-component paths (e.g. lone "movie.mkv").
        // Nested paths with different first folders (Disc1/... + Disc2/...) are rejected.
        for (const int i : asConst(remainingIndexes))
        {
            if (filePaths.at(i).data().contains(u'/'))
            {
                if (mappingOk)
                    *mappingOk = false;
                return {};
            }
            out[i] = uniqueRoot / filePaths.at(i);
        }
        return out;
    }

    // True if the exact target path exists, or any parent component is a regular file.
    bool targetPathIsBlocked(const Path &targetAbs)
    {
        if (targetAbs.exists())
            return true;

        Path cursor = targetAbs.parentPath();
        while (!cursor.isEmpty())
        {
            if (cursor.exists() && !Utils::Fs::isDir(cursor))
                return true;

            const Path parent = cursor.parentPath();
            if (parent == cursor)
                break;
            cursor = parent;
        }
        return false;
    }
}

QString BitTorrent::uniqueSubfolderTag(const TorrentID &id)
{
    return u' ' + id.toString().left(HASH_TAG_HEX_LEN);
}

QString BitTorrent::uniqueSubfolderName(const TorrentID &id, const QString &originalName)
{
    return taggedComponent(originalName, uniqueSubfolderTag(id));
}

PathList BitTorrent::applyUniqueSubfolderLayout(PathList filePaths, const TorrentID &id, const QString &torrentName)
{
    if (filePaths.isEmpty())
        return filePaths;

    const QString tag = uniqueSubfolderTag(id);
    const QString folderName = uniqueSubfolderName(id, originalNameForUniqueDir(filePaths, torrentName, tag));
    const Path uniqueRoot {folderName};

    const Path rootFolder = Path::findRootFolder(filePaths);
    if (!rootFolder.isEmpty())
    {
        if (rootFolder == uniqueRoot)
            return filePaths;
        // Common top-level folder (Original/Subfolder style): rename the root only.
        return renameRootFolder(std::move(filePaths), rootFolder.toString(), folderName);
    }

    // Rootless multi-file or single file: prepend unique folder to the full path.
    // e.g. CD1/movie.mkv + CD2/movie.mkv → Unique/CD1/movie.mkv, Unique/CD2/movie.mkv
    return mapPathsIntoUniqueRoot(filePaths, uniqueRoot, TorrentContentLayout::NoSubfolder);
}

BitTorrent::UniqueSubfolderMigrationPlan BitTorrent::makeUniqueSubfolderMigrationPlan(
        const PathList &currentPaths, const TorrentID &id, const QString &torrentName
        , const Path &storageRoot, const TorrentContentLayout currentLayout)
{
    UniqueSubfolderMigrationPlan plan;
    if (currentPaths.isEmpty())
        return plan;

    if (storageRoot.isEmpty())
    {
        plan.blocked = true;
        plan.blockReason = QCoreApplication::translate("BitTorrent", "Storage location is unknown.");
        return plan;
    }

    const QString tag = uniqueSubfolderTag(id);
    const Path uniqueRoot {uniqueSubfolderName(id, originalNameForUniqueDir(currentPaths, torrentName, tag))};
    if (uniqueRoot.isEmpty())
    {
        plan.blocked = true;
        plan.blockReason = QCoreApplication::translate("BitTorrent", "Target unique subfolder is invalid.");
        return plan;
    }

    bool allAlreadyUnderUnique = true;
    for (const Path &path : currentPaths)
    {
        if (!isUnderUniqueRoot(path, uniqueRoot))
        {
            allAlreadyUnderUnique = false;
            break;
        }
    }

    if (allAlreadyUnderUnique)
    {
        // Paths already converted; only the stored layout may still need updating.
        if (currentLayout != TorrentContentLayout::UniqueSubfolder)
            plan.finalizeOnly = true;
        return plan;
    }

    bool mappingOk = true;
    const PathList targetPaths = mapPathsIntoUniqueRoot(currentPaths, uniqueRoot, currentLayout, &mappingOk);
    if (!mappingOk)
    {
        plan.blocked = true;
        plan.blockReason = QCoreApplication::translate("BitTorrent"
                , "Migration cannot continue: torrent files do not share one top-level folder.");
        return plan;
    }

    QSet<QString> plannedTargets;
    plannedTargets.reserve(targetPaths.size());

    for (int i = 0; i < targetPaths.size(); ++i)
    {
        if (targetPaths.at(i) == currentPaths.at(i))
            continue;

        const QString targetKey = targetPaths.at(i).data();
        if (plannedTargets.contains(targetKey))
        {
            plan.blocked = true;
            plan.blockReason = QCoreApplication::translate("BitTorrent"
                    , "Migration cannot continue: two files map to the same destination: \"%1\".")
                    .arg(targetPaths.at(i).toString());
            plan.renames.clear();
            return plan;
        }
        plannedTargets.insert(targetKey);

        const Path targetAbs = storageRoot / targetPaths.at(i);
        if (targetPathIsBlocked(targetAbs))
        {
            plan.blocked = true;
            plan.blockReason = QCoreApplication::translate("BitTorrent"
                    , "Migration cannot continue: destination path already exists or a parent is a file: \"%1\".")
                    .arg(targetPaths.at(i).toString());
            plan.renames.clear();
            return plan;
        }

        plan.renames.append({.fileIndex = i, .to = targetPaths.at(i)});
    }

    return plan;
}
