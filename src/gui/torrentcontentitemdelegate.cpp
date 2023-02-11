/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcontentitemdelegate.h"

#include <QComboBox>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrent.h"
#include "gui/torrentcontentmodel.h"

TorrentContentItemDelegate::TorrentContentItemDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{
}

void TorrentContentItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *combobox = static_cast<QComboBox *>(editor);
    // Set combobox index
    const int priority = index.data(TorrentContentModel::UnderlyingDataRole).toInt();
    switch (static_cast<BitTorrent::DownloadPriority>(priority))
    {
    case BitTorrent::DownloadPriority::Ignored:
        combobox->setCurrentIndex(0);
        break;
    case BitTorrent::DownloadPriority::High:
        combobox->setCurrentIndex(2);
        break;
    case BitTorrent::DownloadPriority::Maximum:
        combobox->setCurrentIndex(3);
        break;
    case BitTorrent::DownloadPriority::Mixed:
        combobox->setCurrentIndex(4);
        break;
    default:
        combobox->setCurrentIndex(1);
        break;
    }
}

QWidget *TorrentContentItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() != TorrentContentModelItem::COL_PRIO)
        return nullptr;

    auto *editor = new QComboBox(parent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(tr("Do not download", "Do not download (priority)"));
    editor->addItem(tr("Normal", "Normal (priority)"));
    editor->addItem(tr("High", "High (priority)"));
    editor->addItem(tr("Maximum", "Maximum (priority)"));

    // add Mixed priority item to the new combobox only for those items with Mixed priority
    const auto priority = static_cast<BitTorrent::DownloadPriority>(index.data(TorrentContentModel::UnderlyingDataRole).toInt());
    if (priority == BitTorrent::DownloadPriority::Mixed)
    {
        editor->addItem(tr("Mixed", "Mixed (priorities)"));
    }

    connect(editor, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, editor]()
    {
        emit const_cast<TorrentContentItemDelegate *>(this)->commitData(editor);
    });

    return editor;
}

void TorrentContentItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    const auto *combobox = static_cast<QComboBox *>(editor);

    BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal; // NORMAL
    switch (combobox->currentIndex())
    {
    case 0:
        prio = BitTorrent::DownloadPriority::Ignored; // IGNORED
        break;
    case 2:
        prio = BitTorrent::DownloadPriority::High; // HIGH
        break;
    case 3:
        prio = BitTorrent::DownloadPriority::Maximum; // MAX
        break;
    case 4:
        prio = BitTorrent::DownloadPriority::Mixed; // MIXED
        break;
    }

    const int newPriority = static_cast<int>(prio);
    const int previousPriority = index.data(TorrentContentModel::UnderlyingDataRole).toInt();

    if (newPriority != previousPriority)
    {
        model->setData(index, newPriority);
    }
}

void TorrentContentItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void TorrentContentItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    switch (index.column())
    {
    case TorrentContentModelItem::COL_PROGRESS:
        {
            const int progress = static_cast<int>(index.data(TorrentContentModel::UnderlyingDataRole).toReal());
            const int priority = index.sibling(index.row(), TorrentContentModelItem::COL_PRIO).data(TorrentContentModel::UnderlyingDataRole).toInt();
            const bool isEnabled = static_cast<BitTorrent::DownloadPriority>(priority) != BitTorrent::DownloadPriority::Ignored;

            QStyleOptionViewItem customOption {option};
            customOption.state.setFlag(QStyle::State_Enabled, isEnabled);

            m_progressBarPainter.paint(painter, customOption, index.data().toString(), progress);
        }
        break;
    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
