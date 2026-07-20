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

#pragma once

#include <functional>

#include <QSet>
#include <QString>

#include "base/3rdparty/expected.hpp"
#include "base/path.h"

namespace BitTorrent
{
    // Absolute payload path(s) a layout claims under one storage root.
    QSet<Path> payloadClaimPaths(const Path &storageRoot, const PathList &filePaths);

    // Claims under every storage root (final save, incomplete, current, …).
    QSet<Path> payloadClaimPaths(const PathList &storageRoots, const PathList &filePaths);

    // Deduplicate roots (skips empties).
    PathList uniqueStorageRoots(const PathList &roots);

    bool payloadPathsConflict(const Path &left, const Path &right);
    bool isPayloadPathTaken(const Path &path, const QSet<Path> &occupied);

    PathList applyContentRename(const PathList &filePaths, const Path &oldPath, const Path &newPath);

    // Uniquify relative paths so the chosen payload name is free under *every* storage root.
    // Example: incomplete=/tmp/qbit final=/disk2/anime → both /tmp/qbit/Show and /disk2/anime/Show
    // must be free for "Show" to be accepted; otherwise try "Show (1)", etc.
    //
    // 1) Shared root folder → rename folder
    // 2) Single file → rename file (suffix before extension)
    // 3) Rootless multi → on any collision, wrap in a unique folder
    //
    // allowRename=false → error on collision. Session-known torrents only (no disk scan).
    nonstd::expected<PathList, QString> uniquifyPayloadPaths(
            const PathList &storageRoots, PathList filePaths, bool allowRename
            , const std::function<bool (const Path &absolutePath)> &isTaken);

    // Convenience: single storage root.
    nonstd::expected<PathList, QString> uniquifyPayloadPaths(
            const Path &storageRoot, PathList filePaths, bool allowRename
            , const std::function<bool (const Path &absolutePath)> &isTaken);
}
