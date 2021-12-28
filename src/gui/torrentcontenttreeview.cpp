/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "torrentcontenttreeview.h"

#include <QDir>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndexList>
#include <QTableView>
#include <QThread>
#include <QVector>

#include "base/bittorrent/abstractfilestorage.h"
#include "base/bittorrent/common.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "autoexpandabledialog.h"
#include "gui/torrentcontentmodel.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodelitem.h"
#include "regexreplacementdialog.h"

TorrentContentTreeView::TorrentContentTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setExpandsOnDoubleClick(false);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    header()->setStretchLastSection(false);
    header()->setTextElideMode(Qt::ElideRight);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

void TorrentContentTreeView::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() != Qt::Key_Space) && (event->key() != Qt::Key_Select))
    {
        QTreeView::keyPressEvent(event);
        return;
    }

    event->accept();

    QModelIndex current = currentNameCell();

    QVariant value = current.data(Qt::CheckStateRole);
    if (!value.isValid())
    {
        Q_ASSERT(false);
        return;
    }

    Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked
                            ? Qt::Unchecked : Qt::Checked);

    const QModelIndexList selection = selectionModel()->selectedRows(TorrentContentModelItem::COL_NAME);

    for (const QModelIndex &index : selection)
    {
        Q_ASSERT(index.column() == TorrentContentModelItem::COL_NAME);
        model()->setData(index, state, Qt::CheckStateRole);
    }
}

void TorrentContentTreeView::renameSelectedFiles(BitTorrent::AbstractFileStorage &fileStorage)
{
    QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    // if nothing is selected, then add all rows
    if (selectedIndexes.size() == 0)
    {
        for (int i = 0; i < model()->rowCount(); i++)
        {
            selectedIndexes.push_back(model()->index(i, 0));
        }
    }

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    if (selectedIndexes.size() == 1)
    {
        auto *item = model->item(selectedIndexes[0]);
        bool isFile = item->itemType() == TorrentContentModelItem::FileType;
        QString oldFileName = selectedIndexes[0].data().toString();
        QString parentPath = selectedIndexes[0].parent().isValid() ? model->item(selectedIndexes[0].parent())->path() : "";
        bool ok;
        QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
                                                        , oldFileName, &ok, isFile).trimmed();
        QString newPath = Utils::Fs::combinePaths(parentPath, newName);
        
        if (!ok) return;
        try
        {
            if (isFile)
                fileStorage.renameFileChecked(model->getFileIndex(selectedIndexes[0]), newPath);
            else
                fileStorage.renameFolder(model->item(selectedIndexes[0])->path(), newPath);

            model->model()->relayout();
        }
        catch (const RuntimeError &error)
        {
            RaisedMessageBox::warning(this, tr("Rename error"), error.message(), QMessageBox::Ok);
        }
    }
    else // multiple files selected
    {
        RegexReplacementDialog regexDialog(this, tr("Batch Renaming Files"));
        bool ok = regexDialog.prompt();
        if (!ok) return;
        std::function<QString (const QString &)> nameTransformer;
        nameTransformer = [&regexDialog](const QString &oldPath) -> QString
            {
                return regexDialog.replace(Utils::Fs::fileName(oldPath));
            };

        // torrent indexes, not Qt model indexes
        QVector<int> torrentIndexes;
        QVector<QString> oldPaths;
        for (auto it = selectedIndexes.begin(); it != selectedIndexes.end(); it++)
        {
            auto item = model->item(*it);
            if (it->isValid() && item->itemType() == TorrentContentModelItem::FileType)
            {
                torrentIndexes.push_back(model->getFileIndex(*it));
                oldPaths.push_back(item->path());
            }
            else
            {
                selectedIndexes.erase(it);
            }
        }
        try
        {
            QVector<QString> newPaths = Utils::Fs::renamePaths(oldPaths, nameTransformer, false);
            fileStorage.renameFiles(torrentIndexes, newPaths);
            model->model()->relayout();
        }
        catch (const RuntimeError &error)
        {
            RaisedMessageBox::warning(this, tr("Rename error"), error.message(), QMessageBox::Ok);
        }
    }
}

QModelIndex TorrentContentTreeView::currentNameCell()
{
    QModelIndex current = currentIndex();
    if (!current.isValid())
    {
        Q_ASSERT(false);
        return {};
    }

    return model()->index(current.row(), TorrentContentModelItem::COL_NAME, current.parent());
}
