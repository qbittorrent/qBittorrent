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

#include "logmodel.h"

#include <QApplication>
#include <QDateTime>
#include <QColor>
#include <QPalette>

#include "base/global.h"
#include "base/logger.h"

int BaseLogModel::rowCount(const QModelIndex &) const
{
    return m_messages.size();
}

int BaseLogModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant BaseLogModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    const size_t msgIndex = m_messages.size() - index.row() - 1;
    if ((msgIndex < 0) || (msgIndex >= m_messages.size()))
        return {};

    const Message &message = m_messages[msgIndex];
    switch (role) {
    case Qt::ForegroundRole:
        return message.foregroundRole;
    case Qt::UserRole:
        return message.userRole;
    case TimeRole:
        return message.time;
    case MessageRole:
        return message.message;
    default:
        return {};
    }
}

void BaseLogModel::addNewMessage(BaseLogModel::Message message)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_messages.emplace_back(std::move(message));
    endInsertRows();

    const int count = rowCount();
    if (count > MAX_LOG_MESSAGES) {
        const int lastMessage = count - 1;
        beginRemoveRows(QModelIndex(), lastMessage, lastMessage);
        m_messages.pop_front();
        endRemoveRows();
    }
}

void BaseLogModel::reset()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

LogMessageModel::LogMessageModel(QObject *parent)
    : BaseLogModel(parent)
{
    for (const Log::Msg &msg : asConst(Logger::instance()->getMessages()))
        handleNewMessage(msg);
    connect(Logger::instance(), &Logger::newLogMessage, this, &LogMessageModel::handleNewMessage);
}

void LogMessageModel::handleNewMessage(const Log::Msg &log)
{
    Message message;

    message.time = QDateTime::fromMSecsSinceEpoch(log.timestamp).toString(Qt::SystemLocaleShortDate);
    message.message = log.message;

    switch (log.type) {
    // The RGB QColor constructor is used for performance
    case Log::NORMAL:
        message.foregroundRole = QApplication::palette().color(QPalette::WindowText);
        break;
    case Log::INFO:
        message.foregroundRole = QColor(0, 0, 255); // blue
        break;
    case Log::WARNING:
        message.foregroundRole = QColor(255, 165, 0);  // orange
        break;
    case Log::CRITICAL:
        message.foregroundRole = QColor(255, 0, 0);  // red
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    message.userRole = log.type;
    addNewMessage(std::move(message));
}

LogPeerModel::LogPeerModel(QObject *parent)
    : BaseLogModel(parent)
{
    for (const Log::Peer &peer : asConst(Logger::instance()->getPeers()))
        handleNewMessage(peer);
    connect(Logger::instance(), &Logger::newLogPeer, this, &LogPeerModel::handleNewMessage);
}

void LogPeerModel::handleNewMessage(const Log::Peer &peer)
{
    const QString time = QDateTime::fromMSecsSinceEpoch(peer.timestamp).toString(Qt::SystemLocaleShortDate);
    const QString message = (peer.blocked
                             ? tr("%1 was blocked %2", "x.y.z.w was blocked").arg(peer.ip, peer.reason)
                             : tr("%1 was banned", "x.y.z.w was banned").arg(peer.ip));

    addNewMessage({time, message, {}, {}});
}
