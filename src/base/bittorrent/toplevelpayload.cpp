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
    constexpr int MAX_COMPONENT_LEN = 255;

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

    // Sanitize and append tag, leaving room for the tag within MAX_COMPONENT_LEN.
    QString hashedComponent(const QString &name, const QString &tag)
    {
        QString base = Utils::Fs::toValidFileName(name.trimmed());
        if (base.isEmpty())
            base = u"Torrent"_s;

        if (base.endsWith(tag))
            return base;

        const int maxBaseLen = std::max(1, MAX_COMPONENT_LEN - static_cast<int>(tag.size()));
        if (base.size() > maxBaseLen)
            base = base.left(maxBaseLen);

        return base + tag;
    }

    QString originalNameForHashDir(const PathList &filePaths, const QString &torrentName, const QString &tag)
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

        // Avoid "Name [qb-… ] [qb-…]" if the display/root name already carries this tag.
        if (base.endsWith(tag))
            base.chop(tag.size());

        return base;
    }
}

QString BitTorrent::payloadHashTag(const TorrentID &id)
{
    return u" [qb-"_s + id.toString().left(HASH_TAG_HEX_LEN) + u']';
}

QString BitTorrent::payloadHashDirectoryName(const TorrentID &id, const QString &originalName)
{
    return hashedComponent(originalName, payloadHashTag(id));
}

PathList BitTorrent::applyPayloadHashNaming(PathList filePaths, const TorrentID &id, const QString &torrentName)
{
    if (filePaths.isEmpty())
        return filePaths;

    const QString tag = payloadHashTag(id);
    const QString hashDir = payloadHashDirectoryName(id, originalNameForHashDir(filePaths, torrentName, tag));

    const Path rootFolder = Path::findRootFolder(filePaths);
    if (!rootFolder.isEmpty())
    {
        // Already under the correct hash directory.
        if (rootFolder.toString() == hashDir)
            return filePaths;
        // Rename existing top-level folder (e.g. Show/ → Show [qb-HASH]/).
        return renameRootFolder(std::move(filePaths), rootFolder.toString(), hashDir);
    }

    // Single file or rootless multi-file: wrap under the hash directory.
    Path::addRootFolder(filePaths, Path(hashDir));
    return filePaths;
}
