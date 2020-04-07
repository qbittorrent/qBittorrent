/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
 * Copyright (C) 2019  sledgehammer999 <hammered999@gmail.com>
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

#include "loglistview.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>

#include "logmodel.h"
#include "uithememanager.h"

namespace
{
    static const QString SEPERATOR = QStringLiteral(" - ");

    class LogItemDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            const QPen originalPen = painter->pen();
            painter->save();

            QStyledItemDelegate::paint(painter, option, index); // paints background, focus rect and selection rect

            const int textHMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin, &option);
            const int textVMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameVMargin, &option);
            const QRect textRect = QApplication::style()->subElementRect(QStyle::SE_ItemViewItemText, &option)
                                   .adjusted(textHMargin, textVMargin, 0, 0);

            QPen coloredPen = originalPen;
            coloredPen.setColor(Qt::gray);
            painter->setPen(coloredPen);
            const QString time = index.data(BaseLogModel::TimeRole).toString();
            painter->drawText(textRect, time);

            painter->setPen(originalPen);
            const QFontMetrics fontMetrics = painter->fontMetrics();
            const int timeWidth = fontMetrics.width(time);
            painter->drawText(textRect.adjusted(timeWidth, 0, 0, 0), SEPERATOR);

            const QVariant foreground = index.data(Qt::ForegroundRole);
            if (foreground.canConvert<QColor>()) {             // peermodel may give invalid foreground role
                coloredPen.setColor(foreground.value<QColor>());
                painter->setPen(coloredPen);
            }

            painter->drawText(textRect.adjusted((timeWidth + fontMetrics.width(SEPERATOR)), 0, 0, 0)
                              , index.data(BaseLogModel::MessageRole).toString());
            painter->restore();
        }

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            return QSize(option.fontMetrics.width(QString::fromLatin1("%1%2%3")
                                                  .arg(index.data(BaseLogModel::TimeRole).toString(), SEPERATOR
                                                       , index.data(BaseLogModel::MessageRole).toString()))
                         , QStyledItemDelegate::sizeHint(option, index).height());
        }
    };
}

LogListView::LogListView(QWidget *parent)
    : QListView(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemDelegate(new LogItemDelegate(this));

#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
}

void LogListView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy))
        copySelection();
    else
        QListView::keyPressEvent(event);
}

void LogListView::copySelection() const
{
    QStringList list;
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    for (const QModelIndex &index : selectedIndexes) {
        list.append(QString::fromLatin1("%1%2%3").arg(index.data(BaseLogModel::TimeRole).toString(), SEPERATOR
                                                      , index.data(BaseLogModel::MessageRole).toString()));
    }
    QApplication::clipboard()->setText(list.join('\n'));
}
