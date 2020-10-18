/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "progressbardelegate.h"

#include <QApplication>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

ProgressBarDelegate::ProgressBarDelegate(const int progressColumn, const int dataRole, QObject *parent)
    : QStyledItemDelegate {parent}
    , m_progressColumn {progressColumn}
    , m_dataRole {dataRole}
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    m_dummyProgressBar.setStyle(new QProxyStyle {"fusion"});
#endif
}

void ProgressBarDelegate::initProgressStyleOption(QStyleOptionProgressBar &option, const QModelIndex &index) const
{
    option.text = index.data().toString();
    option.progress = static_cast<int>(index.data(m_dataRole).toReal());
    option.maximum = 100;
    option.minimum = 0;
    option.state |= (QStyle::State_Enabled | QStyle::State_Horizontal);
    option.textVisible = true;
}

void ProgressBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() != m_progressColumn)
        return QStyledItemDelegate::paint(painter, option, index);

    QStyleOptionProgressBar newopt;
    newopt.initFrom(&m_dummyProgressBar);
    newopt.rect = option.rect;
    initProgressStyleOption(newopt, index);

    painter->save();
    m_dummyProgressBar.style()->drawControl(QStyle::CE_ProgressBar, &newopt, painter, &m_dummyProgressBar);
    painter->restore();
}
