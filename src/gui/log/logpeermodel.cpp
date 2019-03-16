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
    , m_items(bulkPeerMessages())
{
    connect(Logger::instance(), &Logger::newLogPeer, this, &LogPeerModel::appendLine);
}

int LogPeerModel::rowCount(const QModelIndex &) const
{
    return m_items.size();
}

int LogPeerModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant LogPeerModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) return {};

    if (role == Qt::DisplayRole)
        return m_items.at(m_items.size() - index.row() - 1);

    return {};
}

void LogPeerModel::appendLine(const Log::Peer &peer)
{
    const QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);
    QString text = QString("%1 - ").arg(time.toString(Qt::SystemLocaleShortDate));

    if (peer.blocked)
        text.append(tr("%1 was blocked %2", "x.y.z.w was blocked").arg(peer.ip, peer.reason));
    else
        text.append(tr("%1 was banned", "x.y.z.w was banned").arg(peer.ip));


    beginInsertRows(QModelIndex(), 0, 0);
    m_items.append(text);
    endInsertRows();

    const int count = rowCount();
    if (count > MAX_LOG_MESSAGES) {
        beginRemoveRows(QModelIndex(), count - 1, count - 1);
        m_items.removeFirst();
        endRemoveRows();
    }
}

void LogPeerModel::reset()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

QStringList LogPeerModel::bulkPeerMessages()
{
    const Logger *const logger = Logger::instance();
    const QVector<Log::Peer> peers = logger->getPeers();
    QStringList list;
    list.reserve(peers.size());
    for (const Log::Peer &peer : asConst(logger->getPeers())) {
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);
        QString text = QString("%1 - ").arg(time.toString(Qt::SystemLocaleShortDate));

        if (peer.blocked)
            text.append(tr("%1 was blocked %2", "x.y.z.w was blocked").arg(peer.ip, peer.reason));
        else
            text.append(tr("%1 was banned", "x.y.z.w was banned").arg(peer.ip));

        list.append(text);
    }

    return list;
}
