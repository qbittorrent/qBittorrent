/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QObject>

#include "base/pathfwd.h"
#include "downloadpriority.h"

template <typename T> class QFuture;

namespace BitTorrent
{
    class TorrentContentHandler : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentContentHandler)

    public:
        using QObject::QObject;

        virtual bool hasMetadata() const = 0;
        virtual int filesCount() const = 0;
        virtual Path filePath(int index) const = 0;
        virtual qlonglong fileSize(int index) const = 0;
        virtual Path actualStorageLocation() const = 0;
        virtual Path actualFilePath(int fileIndex) const = 0;
        virtual QList<DownloadPriority> filePriorities() const = 0;
        virtual QList<qreal> filesProgress() const = 0;
        /**
         * @brief fraction of file pieces that are available at least from one peer
         *
         * This is not the same as torrrent availability, it is just a fraction of pieces
         * that can be downloaded right now. It varies between 0 to 1.
         */
        virtual QFuture<QList<qreal>> fetchAvailableFileFractions() const = 0;

        virtual void renameFile(int index, const Path &newPath) = 0;
        virtual void prioritizeFiles(const QList<DownloadPriority> &priorities) = 0;
        virtual void flushCache() const = 0;

        void renameFile(const Path &oldPath, const Path &newPath);
        void renameFolder(const Path &oldFolderPath, const Path &newFolderPath);

    signals:
        void metadataReceived();
        void fileRenamed(int index, const Path &oldFilePath);
        void folderRenamed(const Path &newFolderPath, const Path &oldFolderPath, const QHash<int, Path> &renamedFiles);
        void folderRenamingFailed(const Path &newFolderPath, const Path &oldFolderPath
                , const QHash<int, Path> &renamedFiles, const QList<int> &failedFileIndexes);

    protected:
        virtual void doRenameFolder(const Path &oldFolderPath, const Path &newFolderPath) = 0;
    };
}
