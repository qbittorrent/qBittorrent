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
#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "propertieswidget.h"
#include "torrentcontentmodelitem.h"

namespace
{

    QPalette progressBarDisabledPalette()
    {
        auto getPalette = []() {
                              QProgressBar bar;
                              bar.setEnabled(false);
                              QStyleOptionProgressBar opt;
                              opt.initFrom(&bar);
                              return opt.palette;
                          };
        static QPalette palette = getPalette();
        return palette;
    }
}

PropListDelegate::PropListDelegate(PropertiesWidget *properties)
    : QItemDelegate(properties)
    , m_properties(properties)
{
}

void PropListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    QStyleOptionViewItem opt = QItemDelegate::setOptions(index, option);
    QItemDelegate::drawBackground(painter, opt, index);

    switch (index.column()) {
    case PCSIZE:
    case REMAINING:
        QItemDelegate::drawDisplay(painter, opt, option.rect, Utils::Misc::friendlyUnit(index.data().toLongLong()));
        break;
    case PROGRESS: {
            if (index.data().toDouble() < 0)
                break;

            QStyleOptionProgressBar newopt;
            qreal progress = index.data().toDouble() * 100.;
            newopt.rect = opt.rect;
            newopt.text = ((progress == 100.0) ? QString("100%") : Utils::String::fromDouble(progress, 1) + '%');
            newopt.progress = int(progress);
            newopt.maximum = 100;
            newopt.minimum = 0;
            newopt.textVisible = true;
            if (index.sibling(index.row(), PRIORITY).data().toInt() == static_cast<int>(BitTorrent::DownloadPriority::Ignored)) {
                newopt.state &= ~QStyle::State_Enabled;
                newopt.palette = progressBarDisabledPalette();
            }
            else {
                newopt.state |= QStyle::State_Enabled;
            }

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter);
#else
            // XXX: To avoid having the progress text on the right of the bar
            QProxyStyle("fusion").drawControl(QStyle::CE_ProgressBar, &newopt, painter, 0);
#endif
        }
        break;
    case PRIORITY: {
            QString text = "";
            switch (static_cast<BitTorrent::DownloadPriority>(index.data().toInt())) {
            case BitTorrent::DownloadPriority::Mixed:
                text = tr("Mixed", "Mixed (priorities");
                break;
            case BitTorrent::DownloadPriority::Ignored:
                text = tr("Not downloaded");
                break;
            case BitTorrent::DownloadPriority::High:
                text = tr("High", "High (priority)");
                break;
            case BitTorrent::DownloadPriority::Maximum:
                text = tr("Maximum", "Maximum (priority)");
                break;
            default:
                text = tr("Normal", "Normal (priority)");
                break;
            }
            QItemDelegate::drawDisplay(painter, opt, option.rect, text);
        }
        break;
    case AVAILABILITY: {
            const qreal availability = index.data().toDouble();
            if (availability < 0) {
                QItemDelegate::drawDisplay(painter, opt, option.rect, tr("N/A"));
            }
            else {
                const QString value = (availability >= 1.0)
                                        ? QLatin1String("100")
                                        : Utils::String::fromDouble(availability * 100., 1);
                QItemDelegate::drawDisplay(painter, opt, option.rect, value + C_THIN_SPACE + QLatin1Char('%'));
            }
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
    auto *combobox = static_cast<QComboBox *>(editor);
    // Set combobox index
    switch (static_cast<BitTorrent::DownloadPriority>(index.data().toInt())) {
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

    if (m_properties) {
        BitTorrent::TorrentHandle *const torrent = m_properties->getCurrentTorrent();
        if (!torrent || !torrent->hasMetadata() || torrent->isSeed())
            return nullptr;
    }

    if (index.data().toInt() == static_cast<int>(BitTorrent::DownloadPriority::Mixed))
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
    auto *combobox = static_cast<QComboBox *>(editor);
    int value = combobox->currentIndex();
    qDebug("PropListDelegate: setModelData(%d)", value);

    BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal; // NORMAL
    switch (value) {
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
    qDebug("UpdateEditor Geometry called");
    editor->setGeometry(option.rect);
}
