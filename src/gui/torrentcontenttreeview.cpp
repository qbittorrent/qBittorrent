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

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "autoexpandabledialog.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentmodelitem.h"

TorrentContentTreeView::TorrentContentTreeView(QWidget *parent)
    : QTreeView(parent)
{
    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(header());
    header()->setParent(this);
    header()->setStretchLastSection(false);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
}

void TorrentContentTreeView::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() != Qt::Key_Space) && (event->key() != Qt::Key_Select)) {
        QTreeView::keyPressEvent(event);
        return;
    }

    event->accept();

    QModelIndex current = currentNameCell();

    QVariant value = current.data(Qt::CheckStateRole);
    if (!value.isValid()) {
        Q_ASSERT(false);
        return;
    }

    Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked
                            ? Qt::Unchecked : Qt::Checked);

    const QModelIndexList selection = selectionModel()->selectedRows(TorrentContentModelItem::COL_NAME);

    for (const QModelIndex &index : selection) {
        Q_ASSERT(index.column() == TorrentContentModelItem::COL_NAME);
        model()->setData(index, state, Qt::CheckStateRole);
    }
}

void TorrentContentTreeView::renameSelectedFile(BitTorrent::TorrentHandle *torrent)
{
    if (!torrent) return;

    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QPersistentModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    const bool isFile = (model->itemType(modelIndex) == TorrentContentModelItem::FileType);

    // Ask for new name
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok || !modelIndex.isValid()) return;

    if (!Utils::Fs::isValidFileSystemName(newName)) {
        RaisedMessageBox::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (isFile) {
        const int fileIndex = model->getFileIndex(modelIndex);

        if (newName.endsWith(QB_EXT))
            newName.chop(QB_EXT.size());
        const QString oldFileName = torrent->fileName(fileIndex);
        const QString oldFilePath = torrent->filePath(fileIndex);

        const bool useFilenameExt = BitTorrent::Session::instance()->isAppendExtensionEnabled()
            && (torrent->filesProgress()[fileIndex] != 1);
        const QString newFileName = newName + (useFilenameExt ? QB_EXT : QString());
        const QString newFilePath = oldFilePath.leftRef(oldFilePath.size() - oldFileName.size()) + newFileName;

        if (oldFileName == newFileName) {
            qDebug("Name did not change: %s", qUtf8Printable(oldFileName));
            return;
        }

        // check if that name is already used
        for (int i = 0; i < torrent->filesCount(); ++i) {
            if (i == fileIndex) continue;
            if (Utils::Fs::sameFileNames(torrent->filePath(i), newFilePath)) {
                RaisedMessageBox::warning(this, tr("Rename error"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        qDebug("Renaming %s to %s", qUtf8Printable(oldFilePath), qUtf8Printable(newFilePath));
        torrent->renameFile(fileIndex, newFilePath);

        model->setData(modelIndex, newName);
    }
    else {
        // renaming a folder

        const QString oldName = modelIndex.data().toString();
        if (newName == oldName)
            return;  // Name did not change

        QString parentPath;
        for (QModelIndex idx = model->parent(modelIndex); idx.isValid(); idx = model->parent(idx))
            parentPath.prepend(idx.data().toString() + '/');

        const QString oldPath {parentPath + oldName + '/'};
        const QString newPath {parentPath + newName + '/'};

        // Check for overwriting
#if defined(Q_OS_WIN)
        const Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif

        for (int i = 0; i < torrent->filesCount(); ++i) {
            const QString currentPath = torrent->filePath(i);

            if (currentPath.startsWith(oldPath))
                continue;

            if (currentPath.startsWith(newPath, caseSensitivity)) {
                RaisedMessageBox::warning(this, tr("The folder could not be renamed"),
                                          tr("This name is already in use. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        // Replace path in all files
        bool needForceRecheck = false;

        for (int i = 0; i < torrent->filesCount(); ++i) {
            const QString currentPath = torrent->filePath(i);

            if (currentPath.startsWith(oldPath)) {
                const QString path {newPath + currentPath.mid(oldPath.length())};

                if (!needForceRecheck && QFile::exists(path))
                    needForceRecheck = true;

                torrent->renameFile(i, path);
            }
        }

        // Force recheck
        if (needForceRecheck)
            torrent->forceRecheck();

        model->setData(modelIndex, newName);
    }
}

void TorrentContentTreeView::renameSelectedFile(BitTorrent::TorrentInfo &torrent)
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QPersistentModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    const bool isFile = (model->itemType(modelIndex) == TorrentContentModelItem::FileType);

    // Ask for new name
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok || !modelIndex.isValid()) return;

    if (!Utils::Fs::isValidFileSystemName(newName)) {
        RaisedMessageBox::warning(this, tr("Rename error"),
                                  tr("The name is empty or contains forbidden characters, please choose a different one."),
                                  QMessageBox::Ok);
        return;
    }

    if (isFile) {
        const int fileIndex = model->getFileIndex(modelIndex);

        if (newName.endsWith(QB_EXT))
            newName.chop(QB_EXT.size());
        const QString oldFileName = torrent.fileName(fileIndex);
        const QString oldFilePath = torrent.filePath(fileIndex);
        const QString newFilePath = oldFilePath.leftRef(oldFilePath.size() - oldFileName.size()) + newName;

        if (oldFileName == newName) {
            qDebug("Name did not change: %s", qUtf8Printable(oldFileName));
            return;
        }

        // check if that name is already used
        for (int i = 0; i < torrent.filesCount(); ++i) {
            if (i == fileIndex) continue;
            if (Utils::Fs::sameFileNames(torrent.filePath(i), newFilePath)) {
                RaisedMessageBox::warning(this, tr("Rename error"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        qDebug("Renaming %s to %s", qUtf8Printable(oldFilePath), qUtf8Printable(newFilePath));
        torrent.renameFile(fileIndex, newFilePath);

        model->setData(modelIndex, newName);
    }
    else {
        // renaming a folder

        const QString oldName = modelIndex.data().toString();
        if (newName == oldName)
            return;  // Name did not change

        QString parentPath;
        for (QModelIndex idx = model->parent(modelIndex); idx.isValid(); idx = model->parent(idx))
            parentPath.prepend(idx.data().toString() + '/');

        const QString oldPath {parentPath + oldName + '/'};
        const QString newPath {parentPath + newName + '/'};

        // Check for overwriting
#if defined(Q_OS_WIN)
        const Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif

        for (int i = 0; i < torrent.filesCount(); ++i) {
            const QString currentPath = torrent.filePath(i);

            if (currentPath.startsWith(oldPath))
                continue;

            if (currentPath.startsWith(newPath, caseSensitivity)) {
                RaisedMessageBox::warning(this, tr("The folder could not be renamed"),
                                          tr("This name is already in use. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }

        // Replace path in all files
        for (int i = 0; i < torrent.filesCount(); ++i) {
            const QString currentPath = torrent.filePath(i);

            if (currentPath.startsWith(oldPath)) {
                const QString path {newPath + currentPath.mid(oldPath.length())};
                torrent.renameFile(i, path);
            }
        }

        model->setData(modelIndex, newName);
    }
}

QModelIndex TorrentContentTreeView::currentNameCell()
{
    QModelIndex current = currentIndex();
    if (!current.isValid()) {
        Q_ASSERT(false);
        return {};
    }

    return model()->index(current.row(), TorrentContentModelItem::COL_NAME, current.parent());
}
