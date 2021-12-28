/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtGlobal>
#include <QCoreApplication>

class QString;

namespace BitTorrent
{
    class AbstractFileStorage
    {
        Q_DECLARE_TR_FUNCTIONS(AbstractFileStorage)

    public:
        virtual ~AbstractFileStorage() = default;

        virtual int filesCount() const = 0;
        virtual QString filePath(int index) const = 0;
        virtual qlonglong fileSize(int index) const = 0;

        // rename the file without checking name's validity
        virtual void renameFile(int index, const QString &name) = 0;

        // return indexes of all files in the given folder.
        QVector<int> folderIndexes(const QString &folder) const;

        // rename multiple files by applying the nameTransformer to each of their names. Checks
        // validity of new file names. If paths=true, then the transformer is run on the whole path,
        // not just file name. The QB_EXT, if present, will be invisible to the transformer.
        void renameFiles(const QVector<int> &indexes, const QVector<QString> &names);
        // rename file, checking that new path is valid
        void renameFileChecked(int index, const QString &newPath);
        // rename file, checking that old and new paths are valid
        void renameFile(const QString &oldPath, const QString &newPath);
        // rename all files in a folder, checking that old and new paths are valid
        void renameFolder(const QString &oldPath, const QString &newPath);
        // rename all files in a folder, checking new path is valid
        void renameFolder(int index, const QString &newPath);
    };
}
