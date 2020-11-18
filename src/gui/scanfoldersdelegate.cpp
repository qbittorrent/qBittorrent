/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  sledgehammer999 <hammered999@gmail.com>
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

#include "scanfoldersdelegate.h"

#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QTreeView>

#include "base/bittorrent/session.h"
#include "base/scanfoldersmodel.h"

ScanFoldersDelegate::ScanFoldersDelegate(QObject *parent, QTreeView *foldersView)
    : QStyledItemDelegate(parent)
    , m_folderView(foldersView)
{
}

void ScanFoldersDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *combobox = static_cast<QComboBox*>(editor);
    // Set combobox index
    if (index.data(Qt::UserRole).toInt() == ScanFoldersModel::CUSTOM_LOCATION)
        combobox->setCurrentIndex(4); // '4' is the index of the item after the separator in the QComboBox menu
    else
        combobox->setCurrentIndex(index.data(Qt::UserRole).toInt());
}

QWidget *ScanFoldersDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() != ScanFoldersModel::DOWNLOAD) return nullptr;

    auto *editor = new QComboBox(parent);

    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(ScanFoldersModel::pathTypeDisplayName(ScanFoldersModel::DOWNLOAD_IN_WATCH_FOLDER));
    editor->addItem(ScanFoldersModel::pathTypeDisplayName(ScanFoldersModel::DEFAULT_LOCATION));
    editor->addItem(ScanFoldersModel::pathTypeDisplayName(ScanFoldersModel::CUSTOM_LOCATION));
    if (index.data(Qt::UserRole).toInt() == ScanFoldersModel::CUSTOM_LOCATION)
    {
        editor->insertSeparator(3);
        editor->addItem(index.data().toString());
    }

    connect(editor, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &ScanFoldersDelegate::comboboxIndexChanged);
    return editor;
}

void ScanFoldersDelegate::comboboxIndexChanged(int index)
{
    if (index == ScanFoldersModel::CUSTOM_LOCATION)
    {
        auto *w = static_cast<QWidget *>(sender());
        if (w && w->parentWidget())
            w->parentWidget()->setFocus();
    }
}

void ScanFoldersDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    auto *combobox = static_cast<QComboBox*>(editor);
    int value = combobox->currentIndex();

    switch (value)
    {
    case ScanFoldersModel::DOWNLOAD_IN_WATCH_FOLDER:
    case ScanFoldersModel::DEFAULT_LOCATION:
        model->setData(index, value, Qt::UserRole);
        break;

    case ScanFoldersModel::CUSTOM_LOCATION:
        model->setData(
                    index,
                    QFileDialog::getExistingDirectory(
                        nullptr, tr("Select save location"),
                        index.data(Qt::UserRole).toInt() == ScanFoldersModel::CUSTOM_LOCATION ?
                            index.data().toString() :
                            BitTorrent::Session::instance()->defaultSavePath()),
                    Qt::DisplayRole);
        break;

    default:
        break;
    }
}

void ScanFoldersDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    qDebug("UpdateEditor Geometry called");
    editor->setGeometry(option.rect);
}
