/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QAbstractItemModel>
#include <QHash>

#include "base/pathfwd.h"

namespace BitTorrent
{
    class TorrentContentHandler;
}

class TorrentContentLayoutModel final : public QAbstractItemModel
{
     Q_OBJECT
     Q_DISABLE_COPY_MOVE(TorrentContentLayoutModel)

public:
    enum Column
    {
        COL_INDEX,
        COL_PATH,

        NB_COLUMNS
    };

    enum Role
    {
        PATH_CHANGED_ROLE = Qt::UserRole
    };

    explicit TorrentContentLayoutModel(BitTorrent::TorrentContentHandler *contentHandler, QObject *parent = nullptr);

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    bool haveChangedFilePaths() const;
    void applyChangedFilePaths();

signals:
    void renameFailed(const QString &firstErrorMessage, qsizetype errorsCount);

private:
    int getFilesCount() const;
    Path getFilePath(int index) const;

    void onFileRenamed(int fileIndex);
    void onFolderRenamed(const Path &newFolderPath, const Path &oldFolderPath, const QHash<int, Path> &renamedFiles);
    void onFolderRenamingFailed(const Path &newFolderPath, const Path &oldFolderPath, const QHash<int, Path> &renamedFiles);

    BitTorrent::TorrentContentHandler *m_contentHandler = nullptr;
    QHash<int, Path> m_changedFilePaths;
};
