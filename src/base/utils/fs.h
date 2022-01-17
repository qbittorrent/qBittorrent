/*
 * Bittorrent Client using Qt and libtorrent.
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

namespace Utils::Fs
{
    /**
     * Converts a path to a string suitable for display.
     * This function makes sure the directory separator used is consistent
     * with the OS being run.
     */
    QString toNativePath(const QString &path);

    /**
     * Converts a path to a string suitable for processing.
     * This function makes sure the directory separator used is independent
     * from the OS being run so it is the same on all supported platforms.
     * Slash ('/') is used as "uniform" directory separator.
     */
    QString toUniformPath(const QString &path);

    /**
     * If `path is relative then resolves it against `basePath`, otherwise returns the `path` itself
     */
    QString resolvePath(const QString &relativePath, const QString &basePath);

    /**
     * Returns the file extension part of a file name.
     */
    QString fileExtension(const QString &filename);

    QString fileName(const QString &filePath);
    QString folderName(const QString &filePath);
    qint64 computePathSize(const QString &path);
    bool sameFiles(const QString &path1, const QString &path2);
    QString toValidFileSystemName(const QString &name, bool allowSeparators = false
            , const QString &pad = QLatin1String(" "));
    bool isValidFileSystemName(const QString &name, bool allowSeparators = false);
    qint64 freeDiskSpaceOnPath(const QString &path);
    QString branchPath(const QString &filePath, QString *removed = nullptr);
    bool sameFileNames(const QString &first, const QString &second);
    QString expandPath(const QString &path);
    QString expandPathAbs(const QString &path);
    bool isRegularFile(const QString &path);

    bool smartRemoveEmptyFolderTree(const QString &path);
    bool forceRemove(const QString &filePath);
    void removeDirRecursive(const QString &path);

    QString tempPath();

    QString findRootFolder(const QStringList &filePaths);
    void stripRootFolder(QStringList &filePaths);
    void addRootFolder(QStringList &filePaths, const QString &name);

#if !defined Q_OS_HAIKU
    bool isNetworkFileSystem(const QString &path);
#endif
}
