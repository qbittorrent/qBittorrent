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

#include "base/bittorrent/abstractfilestorage.h"
#include "base/bittorrent/common.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "autoexpandabledialog.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodelitem.h"
#include "regexreplacementdialog.h"

namespace
{
    QString getFullPath(const QModelIndex &idx)
    {
        QStringList paths;
        for (QModelIndex i = idx; i.isValid(); i = i.parent())
            paths.prepend(i.data().toString());
        return paths.join(QLatin1Char {'/'});
    }
}

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
    QModelIndexList transientSelectedIndexes = selectionModel()->selectedRows(0);
    // if nothing is selected, then add all rows
    if (transientSelectedIndexes.size() == 0)
    {
        for (int i = 0; i < model()->rowCount(); i++)
        {
            transientSelectedIndexes.push_back(model()->index(i, 0));
        }
    }
    // make persistent copies of all the indexes
    QList<QPersistentModelIndex> selectedIndexes;
    for (const QModelIndex &modelIndex : transientSelectedIndexes)
    {
        selectedIndexes.push_back(modelIndex);
    }
    bool singleIndex = selectedIndexes.size() == 1;

    std::function<QString (const QString &, bool isFile)> nameTransformer;
    std::unique_ptr<RegexReplacementDialog> regexDialog; // TODO: avoid unique ptr, just return the lambda from rrd?
    if (singleIndex)
    {
        nameTransformer = [this](const QString &arg, bool isFile) -> QString
            {
                bool ok;
                QString result = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
                                                               , arg, &ok, isFile).trimmed();
                return ok ? result : QString("");
            };
    }
    else
    {
        regexDialog = std::unique_ptr<RegexReplacementDialog>(new RegexReplacementDialog(this, tr("Batch Renaming Files")));
        bool ok = regexDialog->prompt();
        if (!ok) return;
        nameTransformer = [&regexDialog](const QString &arg, bool) -> QString
            {
                return regexDialog->replace(arg);
            };
    }

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    for (const QPersistentModelIndex modelIndex : selectedIndexes)
    { // TODO: is it right to make a copy of the modelIndex?
        if (!modelIndex.isValid()) continue;
        const bool isFile = (model->itemType(modelIndex) == TorrentContentModelItem::FileType);
        const QString oldName = modelIndex.data().toString();
        const QString newName = nameTransformer(oldName, isFile);
        if (newName == oldName) continue;
        if (!singleIndex && !isFile) continue; // TODO: could we rename folders in a batch without breaking things horribly?
        const QString parentPath = getFullPath(modelIndex.parent());

        const QString oldPath {parentPath.isEmpty() ? oldName : parentPath + QLatin1Char {'/'} + oldName};
        const QString newPath {parentPath.isEmpty() ? newName : parentPath + QLatin1Char {'/'} + newName};
        try
        {
            if (isFile)
                fileStorage.renameFile(oldPath, newPath);
            else
                fileStorage.renameFolder(oldPath, newPath);

            model->setData(modelIndex, newName);
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
