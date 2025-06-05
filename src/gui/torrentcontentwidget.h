/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2014  Ivan Sorokin <vanyacpp@gmail.com>
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

#include <QTreeView>

#include "base/bittorrent/downloadpriority.h"
#include "base/pathfwd.h"
#include "filterpatternformat.h"

class QShortcut;

namespace BitTorrent
{
    class Torrent;
    class TorrentContentHandler;
    class TorrentInfo;
}

class TorrentContentFilterModel;
class TorrentContentModel;

class TorrentContentWidget final : public QTreeView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentContentWidget)

public:
    enum Column
    {
        Name,
        Size,
        Progress,
        Priority,
        Remaining,
        Availability
    };

    enum class DoubleClickAction
    {
        Open,
        Rename
    };

    enum class ColumnsVisibilityMode
    {
        Editable,
        Locked
    };

    explicit TorrentContentWidget(QWidget *parent = nullptr);

    void setContentHandler(BitTorrent::TorrentContentHandler *contentHandler);
    BitTorrent::TorrentContentHandler *contentHandler() const;
    void refresh();

    bool openByEnterKey() const;
    void setOpenByEnterKey(bool value);

    DoubleClickAction doubleClickAction() const;
    void setDoubleClickAction(DoubleClickAction action);

    ColumnsVisibilityMode columnsVisibilityMode() const;
    void setColumnsVisibilityMode(ColumnsVisibilityMode mode);

    int getFileIndex(const QModelIndex &index) const;
    Path getItemPath(const QModelIndex &index) const;

    void setFilterPattern(const QString &patternText, FilterPatternFormat format = FilterPatternFormat::Wildcards);

    void checkAll();
    void checkNone();

signals:
    void stateChanged();

private:
    void setModel(QAbstractItemModel *model) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    QModelIndex currentNameCell() const;
    void displayColumnHeaderMenu();
    void displayContextMenu();
    void openItem(const QModelIndex &index) const;
    void openParentFolder(const QModelIndex &index) const;
    void openSelectedFile();
    void renameSelectedFile();
    void applyPriorities(BitTorrent::DownloadPriority priority);
    void applyPrioritiesByOrder();
    Path getFullPath(const QModelIndex &index) const;
    void onItemDoubleClicked(const QModelIndex &index);
    // Expand single-item folders recursively.
    // This will trigger sorting and filtering so do it after all relevant data is loaded.
    void expandRecursively();

    TorrentContentModel *m_model;
    TorrentContentFilterModel *m_filterModel;
    DoubleClickAction m_doubleClickAction = DoubleClickAction::Rename;
    ColumnsVisibilityMode m_columnsVisibilityMode = ColumnsVisibilityMode::Editable;
    QShortcut *m_openFileHotkeyEnter = nullptr;
    QShortcut *m_openFileHotkeyReturn = nullptr;
};
