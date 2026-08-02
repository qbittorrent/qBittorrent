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

#include "base/global.h"
#include "base/utils/fs.h"

namespace
{
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
            // Windows forbids trailing dots/spaces; tag starts with a space so keep base clean.
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

        // Avoid doubling the tag if the name already ends with it.
        if (base.endsWith(tag))
            base.chop(tag.size());

        return base;
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

    const Path rootFolder = Path::findRootFolder(filePaths);
    if (!rootFolder.isEmpty())
    {
        if (rootFolder.toString() == folderName)
            return filePaths;
        return renameRootFolder(std::move(filePaths), rootFolder.toString(), folderName);
    }

    // Single file or rootless multi-file: wrap under the unique folder.
    Path::addRootFolder(filePaths, Path(folderName));
    return filePaths;
}
