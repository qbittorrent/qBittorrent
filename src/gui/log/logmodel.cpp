/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "logmodel.h"

#include <QApplication>
#include <QDateTime>
#include <QColor>
#include <QPalette>

#include <base/logger.h>

LogModel::LogModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_items(Logger::instance()->getMessagesRange())
{
    connect(Logger::instance(), &Logger::newLogMessage, this, &LogModel::appendLine);
}

int LogModel::rowCount(const QModelIndex &) const
{
    if ((m_items.first < 0) || (m_items.second < 0) || (m_items.second < m_items.first))
        return 0;
    else
        return m_items.second - m_items.first + 1;
}

int LogModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant LogModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) return {};

    const int id = m_items.second - index.row();
    const Log::Msg &msg = Logger::instance()->getMessage(id);

    if (msg.id != id)
        return {};

    switch (role) {
    case Qt::DisplayRole: {
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
        return QString("%1 - %2").arg(time.toString(Qt::SystemLocaleShortDate), msg.message);
    }
    case Qt::ForegroundRole:
        switch (msg.type) {
        // The RGB QColor constructor is used for performance
        case Log::INFO:
            return QColor(0, 0, 255); // blue
        case Log::WARNING:
            return QColor(255, 165, 0); // orange
        case Log::CRITICAL:
            return QColor(255, 0, 0); // red
        default:
            return QApplication::palette().color(QPalette::WindowText);
        }
    case Qt::UserRole:
        return msg.type;
    }

    return {};
}

void LogModel::appendLine(const Log::Msg &msg)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.second = msg.id;
    if (m_items.first < 0)
        m_items.first = msg.id;
    endInsertRows();

    const int count = rowCount();
    if (count > MAX_LOG_MESSAGES) {
        beginRemoveRows(QModelIndex(), count - 1, count - 1);
        m_items.first++;
        endRemoveRows();
    }
}

void LogModel::reset()
{
    beginResetModel();
    m_items.first = -1;
    m_items.second = -1;
    endResetModel();
}
