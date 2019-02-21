/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  sledgehammer999 <hammered999@gmail.com>
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

#include "logger.h"

#include <QDateTime>
#include "base/utils/string.h"

Logger *Logger::m_instance = nullptr;

Logger::Logger()
    : m_lock(QReadWriteLock::Recursive)
    , m_msgCounter(0)
    , m_peerCounter(0)
{
}

Logger::~Logger() {}

Logger *Logger::instance()
{
    return m_instance;
}

void Logger::initInstance()
{
    if (!m_instance)
        m_instance = new Logger;
}

void Logger::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void Logger::addMessage(const QString &message, const Log::MsgType &type)
{
    QWriteLocker locker(&m_lock);

    const Log::Msg temp = {m_msgCounter++, QDateTime::currentMSecsSinceEpoch(), type, message.toHtmlEscaped()};
    m_messages.push_back(temp);

    if (m_messages.size() >= MAX_LOG_MESSAGES)
        m_messages.pop_front();

    emit newLogMessage(temp);
}

void Logger::addPeer(const QString &ip, const bool blocked, const QString &reason)
{
    QWriteLocker locker(&m_lock);

    const Log::Peer temp = {m_peerCounter++, QDateTime::currentMSecsSinceEpoch(), ip.toHtmlEscaped(), blocked, reason.toHtmlEscaped()};
    m_peers.push_back(temp);

    if (m_peers.size() >= MAX_LOG_MESSAGES)
        m_peers.pop_front();

    emit newLogPeer(temp);
}

QVector<Log::Msg> Logger::getMessages(const int lastKnownId) const
{
    QReadLocker locker(&m_lock);

    const int diff = m_msgCounter - lastKnownId - 1;
    const int size = m_messages.size();

    if ((lastKnownId == -1) || (diff >= size))
        return m_messages;

    if (diff <= 0)
        return {};

    return m_messages.mid(size - diff);
}

QVector<Log::Peer> Logger::getPeers(const int lastKnownId) const
{
    QReadLocker locker(&m_lock);

    const int diff = m_peerCounter - lastKnownId - 1;
    const int size = m_peers.size();

    if ((lastKnownId == -1) || (diff >= size))
        return m_peers;

    if (diff <= 0)
        return {};

    return m_peers.mid(size - diff);
}

void LogMsg(const QString &message, const Log::MsgType &type)
{
    Logger::instance()->addMessage(message, type);
}
