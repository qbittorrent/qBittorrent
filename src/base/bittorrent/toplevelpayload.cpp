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
#include <ranges>

#include <QSet>

#include "base/global.h"

namespace
{
    constexpr int MAX_SUFFIX = 9999;

    QString withNumericSuffix(const QString &name, const bool isFile, const int n)
    {
        const QString mark = u" ("_s + QString::number(n) + u')';
        if (isFile)
        {
            const Path p {name};
            const QString ext = p.extension();
            if (!ext.isEmpty())
                return p.removedExtension().toString() + mark + ext;
        }
        return name + mark;
    }

    Path absClaim(const Path &storageRoot, const QString &topName)
    {
        return storageRoot.isEmpty() ? Path(topName) : (storageRoot / Path(topName));
    }

    QStringList rootlessTopNames(const PathList &filePaths)
    {
        QSet<QString> seen;
        QStringList names;
        for (const Path &filePath : filePaths)
        {
            const Path root = filePath.rootItem();
            if (root.isEmpty())
                continue;
            const QString name = root.toString();
            if (seen.contains(name))
                continue;
            seen.insert(name);
            names.append(name);
        }
        return names;
    }

    // True if topName is free under every storage root.
    bool nameFreeEverywhere(const PathList &storageRoots, const QString &topName
            , const std::function<bool (const Path &)> &isTaken)
    {
        for (const Path &root : storageRoots)
        {
            if (isTaken(absClaim(root, topName)))
                return false;
        }
        return true;
    }

    // True if any top name is taken under any root.
    bool anyNameTakenSomewhere(const PathList &storageRoots, const QStringList &topNames
            , const std::function<bool (const Path &)> &isTaken)
    {
        for (const QString &name : topNames)
        {
            if (!nameFreeEverywhere(storageRoots, name, isTaken))
                return true;
        }
        return false;
    }

    QString findFreeName(const PathList &storageRoots, const QString &baseName, const bool isFile
            , const std::function<bool (const Path &)> &isTaken)
    {
        if (nameFreeEverywhere(storageRoots, baseName, isTaken))
            return baseName;

        for (int n = 1; n <= MAX_SUFFIX; ++n)
        {
            const QString candidate = withNumericSuffix(baseName, isFile, n);
            if (nameFreeEverywhere(storageRoots, candidate, isTaken))
                return candidate;
        }
        return {};
    }

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
}

PathList BitTorrent::uniqueStorageRoots(const PathList &roots)
{
    PathList unique;
    for (const Path &root : roots)
    {
        if (root.isEmpty())
            continue;
        if (std::ranges::find(unique, root) != unique.cend())
            continue;
        unique.append(root);
    }
    return unique;
}

QSet<Path> BitTorrent::payloadClaimPaths(const Path &storageRoot, const PathList &filePaths)
{
    QSet<Path> claims;
    if (filePaths.isEmpty())
        return claims;

    const Path rootFolder = Path::findRootFolder(filePaths);
    if (!rootFolder.isEmpty())
    {
        claims.insert(absClaim(storageRoot, rootFolder.toString()));
        return claims;
    }

    if (filePaths.size() == 1)
    {
        claims.insert(absClaim(storageRoot, filePaths.at(0).toString()));
        return claims;
    }

    for (const QString &name : asConst(rootlessTopNames(filePaths)))
        claims.insert(absClaim(storageRoot, name));
    return claims;
}

QSet<Path> BitTorrent::payloadClaimPaths(const PathList &storageRoots, const PathList &filePaths)
{
    QSet<Path> claims;
    for (const Path &root : asConst(uniqueStorageRoots(storageRoots)))
        claims.unite(payloadClaimPaths(root, filePaths));
    return claims;
}

bool BitTorrent::payloadPathsConflict(const Path &left, const Path &right)
{
    if (left.isEmpty() || right.isEmpty())
        return false;
    return (left == right) || left.hasAncestor(right) || right.hasAncestor(left);
}

bool BitTorrent::isPayloadPathTaken(const Path &path, const QSet<Path> &occupied)
{
    for (const Path &other : occupied)
    {
        if (payloadPathsConflict(path, other))
            return true;
    }
    return false;
}

PathList BitTorrent::applyContentRename(const PathList &filePaths, const Path &oldPath, const Path &newPath)
{
    if (oldPath.isEmpty() || (oldPath == newPath))
        return filePaths;

    PathList result;
    result.reserve(filePaths.size());
    for (const Path &filePath : filePaths)
    {
        if (filePath == oldPath)
            result.append(newPath);
        else if (filePath.hasAncestor(oldPath))
            result.append(newPath / oldPath.relativePathOf(filePath));
        else
            result.append(filePath);
    }
    return result;
}

nonstd::expected<PathList, QString> BitTorrent::uniquifyPayloadPaths(
        const PathList &storageRootsIn, PathList filePaths, const bool allowRename
        , const std::function<bool (const Path &absolutePath)> &isTaken)
{
    Q_ASSERT(isTaken);

    if (filePaths.isEmpty())
        return filePaths;

    const PathList storageRoots = uniqueStorageRoots(storageRootsIn);
    // No usable roots → nothing to uniquify against; keep layout.
    if (storageRoots.isEmpty())
        return filePaths;

    const Path rootFolder = Path::findRootFolder(filePaths);

    // --- Shared root folder ---
    if (!rootFolder.isEmpty())
    {
        const QString rootName = rootFolder.toString();
        if (nameFreeEverywhere(storageRoots, rootName, isTaken))
            return filePaths;

        if (!allowRename)
        {
            return nonstd::make_unexpected(
                    QStringLiteral("Payload path is already used by another torrent: '%1'")
                            .arg(absClaim(storageRoots.constFirst(), rootName).toString()));
        }

        const QString freeName = findFreeName(storageRoots, rootName, false, isTaken);
        if (freeName.isEmpty())
        {
            return nonstd::make_unexpected(
                    QStringLiteral("Could not find a free payload folder name for: '%1'").arg(rootName));
        }
        return renameRootFolder(std::move(filePaths), rootName, freeName);
    }

    // --- Single file ---
    if (filePaths.size() == 1)
    {
        const QString fileName = filePaths.at(0).toString();
        if (nameFreeEverywhere(storageRoots, fileName, isTaken))
            return filePaths;

        if (!allowRename)
        {
            return nonstd::make_unexpected(
                    QStringLiteral("Payload path is already used by another torrent: '%1'")
                            .arg(absClaim(storageRoots.constFirst(), fileName).toString()));
        }

        const QString freeName = findFreeName(storageRoots, fileName, true, isTaken);
        if (freeName.isEmpty())
        {
            return nonstd::make_unexpected(
                    QStringLiteral("Could not find a free payload file name for: '%1'").arg(fileName));
        }
        return PathList {Path(freeName)};
    }

    // --- Rootless multi-file ---
    const QStringList tops = rootlessTopNames(filePaths);
    if (!anyNameTakenSomewhere(storageRoots, tops, isTaken))
        return filePaths;

    if (!allowRename)
    {
        return nonstd::make_unexpected(
                QStringLiteral("Payload path is already used by another torrent under one of the storage paths"));
    }

    QString folderBase = filePaths.at(0).removedExtension().toString();
    if (folderBase.isEmpty() || folderBase.contains(u'/'))
        folderBase = filePaths.at(0).rootItem().toString();
    if (folderBase.isEmpty())
        folderBase = u"Torrent"_s;

    const QString freeFolder = findFreeName(storageRoots, folderBase, false, isTaken);
    if (freeFolder.isEmpty())
    {
        return nonstd::make_unexpected(
                QStringLiteral("Could not find a free payload folder name for: '%1'").arg(folderBase));
    }

    Path::addRootFolder(filePaths, Path(freeFolder));
    return filePaths;
}

nonstd::expected<PathList, QString> BitTorrent::uniquifyPayloadPaths(
        const Path &storageRoot, PathList filePaths, const bool allowRename
        , const std::function<bool (const Path &absolutePath)> &isTaken)
{
    return uniquifyPayloadPaths(PathList {storageRoot}, std::move(filePaths), allowRename, isTaken);
}
