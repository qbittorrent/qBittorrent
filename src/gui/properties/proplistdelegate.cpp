/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "proplistdelegate.h"

#include <QApplication>
#include <QComboBox>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QProgressBar>
#include <QStyleOptionProgressBar>

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include <QProxyStyle>
#endif

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrenthandle.h"
#include "gui/torrentcontentmodel.h"
#include "propertieswidget.h"

namespace
{
    QPalette progressBarDisabledPalette()
    {
        static const QPalette palette = []()
        {
            QProgressBar bar;
            bar.setEnabled(false);
            QStyleOptionProgressBar opt;
            opt.initFrom(&bar);
            return opt.palette;
        }();
        return palette;
    }
}

PropListDelegate::PropListDelegate(PropertiesWidget *properties)
    : ProgressBarDelegate {PROGRESS, TorrentContentModel::UnderlyingDataRole, properties}
    , m_properties(properties)
{
}

void PropListDelegate::initProgressStyleOption(QStyleOptionProgressBar &option, const QModelIndex &index) const
{
    ProgressBarDelegate::initProgressStyleOption(option, index);
    const int priority
        = index.sibling(index.row(), PRIORITY).data(TorrentContentModel::UnderlyingDataRole).toInt();
    if (static_cast<BitTorrent::DownloadPriority>(priority) == BitTorrent::DownloadPriority::Ignored)
    {
        option.state &= ~QStyle::State_Enabled;
        option.palette = progressBarDisabledPalette();
    }
    else
    {
        option.state |= QStyle::State_Enabled;
    }
}

void PropListDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
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
    default:
        combobox->setCurrentIndex(1);
        break;
    }
}

QWidget *PropListDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() != PRIORITY) return nullptr;

    if (m_properties)
    {
        const BitTorrent::TorrentHandle *torrent = m_properties->getCurrentTorrent();
        if (!torrent || !torrent->hasMetadata() || torrent->isSeed())
            return nullptr;
    }

    const int priority = index.data(TorrentContentModel::UnderlyingDataRole).toInt();
    if (static_cast<BitTorrent::DownloadPriority>(priority) == BitTorrent::DownloadPriority::Mixed)
        return nullptr;

    auto *editor = new QComboBox(parent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(tr("Do not download", "Do not download (priority)"));
    editor->addItem(tr("Normal", "Normal (priority)"));
    editor->addItem(tr("High", "High (priority)"));
    editor->addItem(tr("Maximum", "Maximum (priority)"));
    return editor;
}

void PropListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    const auto *combobox = static_cast<QComboBox *>(editor);
    const int value = combobox->currentIndex();

    BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal; // NORMAL
    switch (value)
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
    }

    model->setData(index, static_cast<int>(prio));
    emit filteredFilesChanged();
}

void PropListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
