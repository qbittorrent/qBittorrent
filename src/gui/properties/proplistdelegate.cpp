/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QStyleOptionProgressBarV2>
#include <QStyleOptionViewItemV2>
#include <QStyleOptionComboBox>
#include <QComboBox>
#include <QModelIndex>
#include <QPainter>
#include <QProgressBar>
#include <QApplication>

#ifdef Q_OS_WIN
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <QPlastiqueStyle>
#else
#include <QProxyStyle>
#endif
#endif

#include "core/utils/misc.h"
#include "core/utils/string.h"
#include "propertieswidget.h"
#include "proplistdelegate.h"
#include "torrentcontentmodelitem.h"

PropListDelegate::PropListDelegate(PropertiesWidget *properties, QObject *parent)
    : QItemDelegate(parent)
    , m_properties(properties)
{
}

void PropListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    QStyleOptionViewItemV2 opt = QItemDelegate::setOptions(index, option);

    switch(index.column()) {
    case PCSIZE:
        QItemDelegate::drawBackground(painter, opt, index);
        QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(index.data().toLongLong()));
        break;
    case PROGRESS:
        if (index.data().toDouble() >= 0) {
            QStyleOptionProgressBarV2 newopt;
            qreal progress = index.data().toDouble() * 100.;
            newopt.rect = opt.rect;
            newopt.text = ((progress == 100.0) ? QString("100%") : Utils::String::fromDouble(progress, 1) + "%");
            newopt.progress = (int)progress;
            newopt.maximum = 100;
            newopt.minimum = 0;
            newopt.state |= QStyle::State_Enabled;
            newopt.textVisible = true;
#ifndef Q_OS_WIN
            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
#else
            // XXX: To avoid having the progress text on the right of the bar
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
            QPlastiqueStyle st;
#else
            QProxyStyle st("fusion");
#endif
            st.drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
#endif
        }
        else {
            // Do not display anything if the file is disabled (progress  == 0)
            QItemDelegate::drawBackground(painter, opt, index);
        }
        break;
    case PRIORITY: {
            QItemDelegate::drawBackground(painter, opt, index);
            QString text = "";
            switch (index.data().toInt()) {
            case prio::MIXED:
                text = tr("Mixed", "Mixed (priorities");
                break;
            case prio::IGNORED:
                text = tr("Not downloaded");
                break;
            case prio::HIGH:
                text = tr("High", "High (priority)");
                break;
            case prio::MAXIMUM:
                text = tr("Maximum", "Maximum (priority)");
                break;
            default:
                text = tr("Normal", "Normal (priority)");
                break;
            }
            QItemDelegate::drawDisplay(painter, opt, option.rect, text);
        }
        break;
    default:
        QItemDelegate::paint(painter, option, index);
        break;
    }
    painter->restore();
}

void PropListDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QComboBox *combobox = static_cast<QComboBox*>(editor);
    // Set combobox index
    switch(index.data().toInt()) {
    case prio::HIGH:
        combobox->setCurrentIndex(1);
        break;
    case prio::MAXIMUM:
        combobox->setCurrentIndex(2);
        break;
    default:
        combobox->setCurrentIndex(0);
        break;
    }
}

QWidget *PropListDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() != PRIORITY) return 0;

    if (m_properties) {
        BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
        if (!torrent || !torrent->hasMetadata() || torrent->isSeed())
            return 0;
    }

    if (index.data().toInt() <= 0) {
        // IGNORED or MIXED
        return 0;
    }

    QComboBox* editor = new QComboBox(parent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(tr("Normal", "Normal (priority)"));
    editor->addItem(tr("High", "High (priority)"));
    editor->addItem(tr("Maximum", "Maximum (priority)"));
    return editor;
}

void PropListDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *combobox = static_cast<QComboBox*>(editor);
    int value = combobox->currentIndex();
    qDebug("PropListDelegate: setModelData(%d)", value);

    switch(value)  {
    case 1:
        model->setData(index, prio::HIGH); // HIGH
        break;
    case 2:
        model->setData(index, prio::MAXIMUM); // MAX
        break;
    default:
        model->setData(index, prio::NORMAL); // NORMAL
    }

    emit filteredFilesChanged();
}

void PropListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    qDebug("UpdateEditor Geometry called");
    editor->setGeometry(option.rect);
}
