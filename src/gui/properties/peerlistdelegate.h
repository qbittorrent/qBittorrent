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

#ifndef PEERLISTDELEGATE_H
#define PEERLISTDELEGATE_H

#include <QItemDelegate>
#include <QPainter>

#include "base/preferences.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"

class PeerListDelegate : public QItemDelegate
{
    Q_OBJECT

public:
    enum PeerListColumns
    {
        COUNTRY,
        IP,
        PORT,
        CONNECTION,
        FLAGS,
        CLIENT,
        PROGRESS,
        DOWN_SPEED,
        UP_SPEED,
        TOT_DOWN,
        TOT_UP,
        RELEVANCE,
        DOWNLOADING_PIECE,
        IP_HIDDEN,

        COL_COUNT
    };

    PeerListDelegate(QObject *parent) : QItemDelegate(parent) {}

    ~PeerListDelegate() override {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();

        const bool hideValues = Preferences::instance()->getHideZeroValues();
        QStyleOptionViewItem opt = QItemDelegate::setOptions(index, option);
        QItemDelegate::drawBackground(painter, opt, index);

        switch (index.column()) {
        case PORT:
            opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
            QItemDelegate::drawDisplay(painter, opt, option.rect, index.data().toString());
            break;
        case TOT_DOWN:
        case TOT_UP: {
                qlonglong size = index.data().toLongLong();
                if (hideValues && (size <= 0))
                    break;
                opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
                QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(size));
            }
            break;
        case DOWN_SPEED:
        case UP_SPEED: {
                qreal speed = index.data().toDouble();
                if (speed <= 0.0)
                    break;
                opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
                QItemDelegate::drawDisplay(painter, opt, opt.rect, Utils::Misc::friendlyUnit(speed, true));
            }
            break;
        case PROGRESS:
        case RELEVANCE: {
                qreal progress = index.data().toDouble();
                opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
                QItemDelegate::drawDisplay(painter, opt, opt.rect, Utils::String::fromDouble(progress * 100.0, 1) + '%');
            }
            break;
        default:
            QItemDelegate::paint(painter, option, index);
        }

        painter->restore();
    }

    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        // No editor here
        return nullptr;
    }
};

#endif // PEERLISTDELEGATE_H
