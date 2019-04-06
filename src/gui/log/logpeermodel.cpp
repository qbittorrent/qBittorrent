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

#include "logpeermodel.h"

#include <QDateTime>

#include <base/global.h>
#include <base/logger.h>

LogPeerModel::LogPeerModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_items(Logger::instance()->getPeersRange())
{
    connect(Logger::instance(), &Logger::newLogPeer, this, &LogPeerModel::appendLine);
}

int LogPeerModel::rowCount(const QModelIndex &) const
{
    if ((m_items.first < 0) || (m_items.second < 0) || (m_items.second < m_items.first))
        return 0;
    else
        return m_items.second - m_items.first + 1;
}

int LogPeerModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant LogPeerModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) return {};

    const int id = m_items.second - index.row();
    const Log::Peer& peer = Logger::instance()->getPeer(id);

    if (peer.id != id)
        return {};

    if (role == Qt::DisplayRole) {
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);
        QString text = QString("%1 - ").arg(time.toString(Qt::SystemLocaleShortDate));

        if (peer.blocked)
            text.append(tr("%1 was blocked %2", "x.y.z.w was blocked").arg(peer.ip, peer.reason));
        else
            text.append(tr("%1 was banned", "x.y.z.w was banned").arg(peer.ip));

        return text;
    }

    return {};
}

void LogPeerModel::appendLine(const Log::Peer &peer)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.second = peer.id;
    if (m_items.first < 0)
        m_items.first = peer.id;
    endInsertRows();

    const int count = rowCount();
    if (count > MAX_LOG_MESSAGES) {
        beginRemoveRows(QModelIndex(), count - 1, count - 1);
        m_items.first++;
        endRemoveRows();
    }
}

void LogPeerModel::reset()
{
    beginResetModel();
    m_items.first = -1;
    m_items.second = -1;
    endResetModel();
}
