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

#include "torrentcontentlayoutmodel.h"

#include "base/bittorrent/torrentcontenthandler.h"

#include "base/global.h"
#include "base/path.h"

TorrentContentLayoutModel::TorrentContentLayoutModel(BitTorrent::TorrentContentHandler *contentHandler, QObject *parent)
    : QAbstractItemModel(parent)
    , m_contentHandler {contentHandler}
{
    Q_ASSERT(m_contentHandler);

    connect(m_contentHandler, &BitTorrent::TorrentContentHandler::fileRenamed
            , this, &TorrentContentLayoutModel::onFileRenamed);
    connect(m_contentHandler, &BitTorrent::TorrentContentHandler::folderRenamed
            , this, &TorrentContentLayoutModel::onFolderRenamed);
    connect(m_contentHandler, &BitTorrent::TorrentContentHandler::folderRenamingFailed
            , this, &TorrentContentLayoutModel::onFolderRenamingFailed);
}

QVariant TorrentContentLayoutModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if ((role != Qt::DisplayRole) || (orientation != Qt::Horizontal))
        return {};

    switch (section)
    {
    case COL_INDEX:
        return tr("Index");

    case COL_PATH:
        return tr("Path");
    }

    return {};
}

QModelIndex TorrentContentLayoutModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() // no items with valid parent
        || (row < 0) || (row >= getFilesCount())
        || (column < 0) || (column >= NB_COLUMNS))
    {
        return {};
    }

    return createIndex(row, column);
}

QModelIndex TorrentContentLayoutModel::parent([[maybe_unused]] const QModelIndex &index) const
{
    return {};
}

int TorrentContentLayoutModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return getFilesCount();
}

int TorrentContentLayoutModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return NB_COLUMNS;
}

QVariant TorrentContentLayoutModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() >= getFilesCount()))
        return {};

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (index.column())
        {
        case COL_INDEX:
            return index.row();

        case COL_PATH:
            return getFilePath(index.row()).toString();
        }

        break;

    case Qt::TextAlignmentRole:
        if (index.column() == COL_INDEX)
            return Qt::AlignRight;

        break;

    case PATH_CHANGED_ROLE:
        if (index.column() == COL_PATH)
            return m_changedFilePaths.contains(index.row());

        break;
    }

    return {};
}

bool TorrentContentLayoutModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    if (index.column() == COL_PATH)
    {
        const int fileIndex = index.row();
        Path filePath {value.toString()};
        if (getFilePath(fileIndex) != filePath)
        {
            if (filePath == m_contentHandler->filePath(fileIndex))
                m_changedFilePaths.remove(fileIndex);
            else
                m_changedFilePaths.emplace(fileIndex, std::move(filePath));

            emit dataChanged(index, index);
        }

        return true;
    }

    return false;
}

bool TorrentContentLayoutModel::haveChangedFilePaths() const
{
    return !m_changedFilePaths.isEmpty();
}

void TorrentContentLayoutModel::applyChangedFilePaths()
{
    for (const auto &[index, changedFilePath] : asConst(m_changedFilePaths).asKeyValueRange())
    {
        const Path currentFilePath = m_contentHandler->filePath(index);
        m_contentHandler->renameFile(currentFilePath, changedFilePath);
    }
}

int TorrentContentLayoutModel::getFilesCount() const
{
    return m_contentHandler->filesCount();
}

Path TorrentContentLayoutModel::getFilePath(const int index) const
{
    return m_changedFilePaths.value(index, m_contentHandler->filePath(index));
}

void TorrentContentLayoutModel::onFileRenamed(const int fileIndex)
{
    const Path newFilePath = m_contentHandler->filePath(fileIndex);
    const QModelIndex modelIndex = index(fileIndex, COL_PATH);

    if (newFilePath == m_contentHandler->filePath(fileIndex))
    {
        if (m_changedFilePaths.remove(fileIndex))
            emit dataChanged(modelIndex, modelIndex);
    }
    else
    {
        m_changedFilePaths.insert(fileIndex, newFilePath);
        emit dataChanged(modelIndex, modelIndex);
    }
}

void TorrentContentLayoutModel::onFolderRenamed([[maybe_unused]] const Path &newFolderPath
        , [[maybe_unused]] const Path &oldFolderPath, const QHash<int, Path> &renamedFiles)
{
    for (auto iter = renamedFiles.keyBegin(); iter != renamedFiles.keyEnd(); ++iter)
        onFileRenamed(*iter);
}

void TorrentContentLayoutModel::onFolderRenamingFailed([[maybe_unused]] const Path &newFolderPath
        , [[maybe_unused]] const Path &oldFolderPath, const QHash<int, Path> &renamedFiles)
{
    for (auto iter = renamedFiles.keyBegin(); iter != renamedFiles.keyEnd(); ++iter)
        onFileRenamed(*iter);
}

Qt::ItemFlags TorrentContentLayoutModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags customFlags = Qt::ItemNeverHasChildren;

    if (index.column() == COL_PATH)
        customFlags |= Qt::ItemIsEditable;

    return QAbstractItemModel::flags(index) | customFlags;
}
