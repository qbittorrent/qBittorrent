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

    bool exceedsFileNameLengthLimit(const QString &name)
    {
#ifdef Q_OS_WIN
        return (name.length() > 255);
#else
        return (name.toUtf8().length() > 255);
#endif
    }

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

        // Single-file: strip only when the folder base is the payload filename itself
        // (Subfolder parity, e.g. "movie.mkv" → "movie"). Keeps dotted folder identities
        // such as "ubuntu-24.04" / "Series.Name" intact, and stays idempotent after the
        // first wrap (path becomes "movie <hash>/movie.mkv" while the name is still the file).
        if (filePaths.size() == 1)
        {
            const QString fileName = filePaths.at(0).filename();
            if ((base == fileName) || (base == filePaths.at(0).toString()))
                base = Path(fileName).removedExtension().toString();
        }

        if (base.isEmpty())
            base = u"Torrent"_s;

        return base;
    }

    bool isUnderUniqueRoot(const Path &path, const Path &uniqueRoot)
    {
        return (path == uniqueRoot) || path.hasAncestor(uniqueRoot);
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
        return renameRootFolder(std::move(filePaths), rootFolder.toString(), folderName);
    }

    // Rootless: prepend unique folder to each full path (CD1/…, CD2/…).
    for (Path &path : filePaths)
    {
        if (!isUnderUniqueRoot(path, uniqueRoot))
            path = uniqueRoot / path;
    }
    return filePaths;
}

BitTorrent::UniqueSubfolderMigrationPlan BitTorrent::makeUniqueSubfolderMigrationPlan(
        const PathList &currentPaths, const TorrentID &id, const QString &torrentName
        , const TorrentContentLayout currentLayout)
{
    UniqueSubfolderMigrationPlan plan;
    if (currentPaths.isEmpty())
        return plan;

    const QString tag = uniqueSubfolderTag(id);
    plan.uniqueRoot = Path(uniqueSubfolderName(id, originalNameForUniqueDir(currentPaths, torrentName, tag)));
    if (plan.uniqueRoot.isEmpty())
    {
        plan.blocked = true;
        plan.blockReason = QCoreApplication::translate("BitTorrent", "Target unique subfolder is invalid.");
        return plan;
    }

    PathList remainingPaths;
    QList<int> remainingIndexes;
    remainingPaths.reserve(currentPaths.size());
    remainingIndexes.reserve(currentPaths.size());

    for (int i = 0; i < currentPaths.size(); ++i)
    {
        if (isUnderUniqueRoot(currentPaths.at(i), plan.uniqueRoot))
            continue;
        remainingPaths.append(currentPaths.at(i));
        remainingIndexes.append(i);
    }

    if (remainingIndexes.isEmpty())
    {
        if (currentLayout != TorrentContentLayout::UniqueSubfolder)
            plan.finalizeOnly = true;
        return plan;
    }

    if (currentLayout == TorrentContentLayout::NoSubfolder)
    {
        // Wrap every remaining full path under the unique folder (batch rename).
        QSet<QString> targets;
        for (int i = 0; i < remainingIndexes.size(); ++i)
        {
            const Path to = plan.uniqueRoot / remainingPaths.at(i);
            if (targets.contains(to.data()))
            {
                plan.blocked = true;
                plan.blockReason = QCoreApplication::translate("BitTorrent"
                        , "Migration cannot continue: two files map to the same destination: \"%1\".")
                        .arg(to.toString());
                plan.renames.clear();
                return plan;
            }
            targets.insert(to.data());
            plan.renames.append({.fileIndex = remainingIndexes.at(i), .to = to});
        }
        return plan;
    }

    // Original / Subfolder: require one shared top-level root among remaining nested paths.
    const Path sharedRoot = Path::findRootFolder(remainingPaths);
    if (!sharedRoot.isEmpty())
    {
        // Same operation as renameFolder(sharedRoot, uniqueRoot).
        plan.folderRenameOldRoot = sharedRoot;
        return plan;
    }

    // No shared root: allow only single-component paths (e.g. lone movie.mkv).
    for (const Path &path : remainingPaths)
    {
        if (path.data().contains(u'/'))
        {
            plan.blocked = true;
            plan.blockReason = QCoreApplication::translate("BitTorrent"
                    , "Migration cannot continue: torrent files do not share one top-level folder.");
            return plan;
        }
    }

    QSet<QString> targets;
    for (int i = 0; i < remainingIndexes.size(); ++i)
    {
        const Path to = plan.uniqueRoot / remainingPaths.at(i);
        if (targets.contains(to.data()))
        {
            plan.blocked = true;
            plan.blockReason = QCoreApplication::translate("BitTorrent"
                    , "Migration cannot continue: two files map to the same destination: \"%1\".")
                    .arg(to.toString());
            plan.renames.clear();
            return plan;
        }
        targets.insert(to.data());
        plan.renames.append({.fileIndex = remainingIndexes.at(i), .to = to});
    }
    return plan;
}
