/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

/**
 * Utility functions related to file system.
 */

#include <QString>

#include "base/3rdparty/expected.hpp"
#include "base/global.h"
#include "base/pathfwd.h"

class QDateTime;

namespace Utils::Fs
{
    qint64 computePathSize(const Path &path);
    qint64 freeDiskSpaceOnPath(const Path &path);

    bool isRegularFile(const Path &path);
    bool isDir(const Path &path);
    bool isReadable(const Path &path);
    bool isWritable(const Path &path);
    bool isNetworkFileSystem(const Path &path);
    QDateTime lastModified(const Path &path);
    bool sameFiles(const Path &path1, const Path &path2);

    QString toValidFileName(const QString &name, const QString &pad = u" "_s);
    Path toValidPath(const QString &name, const QString &pad = u" "_s);
    Path toAbsolutePath(const Path &path);
    Path toCanonicalPath(const Path &path);

    bool copyFile(const Path &from, const Path &to);
    bool renameFile(const Path &from, const Path &to);
    nonstd::expected<void, QString> removeFile(const Path &path);
    nonstd::expected<void, QString> moveFileToTrash(const Path &path);
    bool mkdir(const Path &dirPath);
    bool mkpath(const Path &dirPath);
    bool rmdir(const Path &dirPath);
    void removeDirRecursively(const Path &path);
    bool smartRemoveEmptyFolderTree(const Path &path);

    Path homePath();
    Path tempPath();
}
