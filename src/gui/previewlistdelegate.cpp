/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "previewlistdelegate.h"

#include <QModelIndex>
#include <QPainter>

#include "base/utils/string.h"
#include "previewselectdialog.h"

PreviewListDelegate::PreviewListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void PreviewListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    switch (index.column())
    {
    case PreviewSelectDialog::PROGRESS:
        {
            const qreal progress = (index.data().toReal() * 100);
            const QString text = (progress >= 100)
                ? u"100%"_s
                : (Utils::String::fromDouble(progress, 1) + u'%');

            m_progressBarPainter.paint(painter, option, text, static_cast<int>(progress));
        }
        break;

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }

    painter->restore();
}

QWidget *PreviewListDelegate::createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const
{
    // No editor here
    return nullptr;
}
