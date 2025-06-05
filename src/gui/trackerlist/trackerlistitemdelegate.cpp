/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "trackerlistitemdelegate.h"

#include <QModelIndex>
#include <QPainter>

#include "trackerlistmodel.h"
#include "trackerlistwidget.h"

TrackerListItemDelegate::TrackerListItemDelegate(TrackerListWidget *view)
    : QStyledItemDelegate(view)
    , m_view {view}
{
    Q_ASSERT(m_view);
}

void TrackerListItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    if (index.parent().isValid() || !m_view->isExpanded(index.siblingAtColumn(0)))
        return;

    switch (index.column())
    {
    case TrackerListModel::COL_PEERS:
    case TrackerListModel::COL_SEEDS:
    case TrackerListModel::COL_LEECHES:
    case TrackerListModel::COL_TIMES_DOWNLOADED:
    case TrackerListModel::COL_MSG:
    case TrackerListModel::COL_NEXT_ANNOUNCE:
    case TrackerListModel::COL_MIN_ANNOUNCE:
        option->text.clear();
        break;
    default:
        break;
    }
}
