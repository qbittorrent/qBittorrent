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

    const QModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    // Ask for new name
    bool ok = false;
    const bool isFile = (model->itemType(modelIndex) == TorrentContentModelItem::FileType);
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
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
        QStringList pathItems;
        pathItems << modelIndex.data().toString();
        QModelIndex parent = model->parent(modelIndex);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = model->parent(parent);
        }
        const QString oldPath = pathItems.join('/');
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join('/');
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith('/')) newPath += '/';
        // Check for overwriting
        for (int i = 0; i < torrent->filesCount(); ++i) {
            const QString currentName = torrent->filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
            if (currentName.startsWith(newPath, Qt::CaseSensitive)) {
#else
            if (currentName.startsWith(newPath, Qt::CaseInsensitive)) {
#endif
                QMessageBox::warning(this, tr("The folder could not be renamed"),
                                     tr("This name is already in use in this folder. Please use a different name."),
                                     QMessageBox::Ok);
                return;
            }
        }
        bool forceRecheck = false;
        // Replace path in all files
        for (int i = 0; i < torrent->filesCount(); ++i) {
            const QString currentName = torrent->filePath(i);
            if (currentName.startsWith(oldPath)) {
                QString newName = currentName;
                newName.replace(0, oldPath.length(), newPath);
                if (!forceRecheck && QDir(torrent->savePath(true)).exists(newName))
                    forceRecheck = true;
                newName = Utils::Fs::expandPath(newName);
                qDebug("Rename %s to %s", qUtf8Printable(currentName), qUtf8Printable(newName));
                torrent->renameFile(i, newName);
            }
        }
        // Force recheck
        if (forceRecheck) torrent->forceRecheck();
        // Rename folder in torrent files model too
        model->setData(modelIndex, newName);
        // Remove old folder
        const QDir oldFolder(torrent->savePath(true) + '/' + oldPath);
        int timeout = 10;
        while (!QDir().rmpath(oldFolder.absolutePath()) && (timeout > 0)) {
            // FIXME: We should not sleep here (freezes the UI for 1 second)
            QThread::msleep(100);
            --timeout;
        }
    }
}

void TorrentContentTreeView::renameSelectedFile(BitTorrent::TorrentInfo &torrent)
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1) return;

    const QModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid()) return;

    auto model = dynamic_cast<TorrentContentFilterModel *>(TorrentContentTreeView::model());
    if (!model) return;

    // Ask for new name
    bool ok = false;
    const bool isFile = (model->itemType(modelIndex) == TorrentContentModelItem::FileType);
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok) return;

    if (newName.isEmpty() || !Utils::Fs::isValidFileSystemName(newName)) {
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
        QStringList pathItems;
        pathItems << modelIndex.data().toString();
        QModelIndex parent = model->parent(modelIndex);
        while (parent.isValid()) {
            pathItems.prepend(parent.data().toString());
            parent = model->parent(parent);
        }
        const QString oldPath = pathItems.join('/');
        pathItems.removeLast();
        pathItems << newName;
        QString newPath = pathItems.join('/');
        if (Utils::Fs::sameFileNames(oldPath, newPath)) {
            qDebug("Name did not change");
            return;
        }
        if (!newPath.endsWith('/')) newPath += '/';
        // Check for overwriting
        for (int i = 0; i < torrent.filesCount(); ++i) {
            const QString currentName = torrent.filePath(i);
#if defined(Q_OS_UNIX) || defined(Q_WS_QWS)
            if (currentName.startsWith(newPath, Qt::CaseSensitive)) {
#else
            if (currentName.startsWith(newPath, Qt::CaseInsensitive)) {
#endif
                RaisedMessageBox::warning(this, tr("The folder could not be renamed"),
                                          tr("This name is already in use in this folder. Please use a different name."),
                                          QMessageBox::Ok);
                return;
            }
        }
        // Replace path in all files
        for (int i = 0; i < torrent.filesCount(); ++i) {
            const QString currentName = torrent.filePath(i);
            if (currentName.startsWith(oldPath)) {
                QString newName = currentName;
                newName.replace(0, oldPath.length(), newPath);
                newName = Utils::Fs::expandPath(newName);
                qDebug("Rename %s to %s", qUtf8Printable(currentName), qUtf8Printable(newName));
                torrent.renameFile(i, newName);
            }
        }

        // Rename folder in torrent files model too
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
