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

#include <algorithm>

#include <QDateTime>
#include <QList>

namespace
{
    template <typename T>
    QList<T> loadFromBuffer(const boost::circular_buffer_space_optimized<T> &src, const int offset = 0)
    {
        QList<T> ret;
        ret.reserve(static_cast<typename decltype(ret)::size_type>(src.size()) - offset);
        std::copy((src.begin() + offset), src.end(), std::back_inserter(ret));
        return ret;
    }
}

Logger *Logger::m_instance = nullptr;

Logger::Logger()
    : m_messages(MAX_LOG_MESSAGES)
    , m_peers(MAX_LOG_MESSAGES)
{
}

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
    delete m_instance;
    m_instance = nullptr;
}

void Logger::addMessage(const QString &message, const Log::MsgType &type)
{
    QWriteLocker locker(&m_lock);
    const Log::Msg msg = {m_msgCounter++, type, QDateTime::currentSecsSinceEpoch(), message};
    m_messages.push_back(msg);
    locker.unlock();

    emit newLogMessage(msg);
}

void Logger::addPeer(const QString &ip, const bool blocked, const QString &reason)
{
    QWriteLocker locker(&m_lock);
    const Log::Peer msg = {m_peerCounter++, blocked, QDateTime::currentSecsSinceEpoch(), ip, reason};
    m_peers.push_back(msg);
    locker.unlock();

    emit newLogPeer(msg);
}

QList<Log::Msg> Logger::getMessages(const int lastKnownId) const
{
    const QReadLocker locker(&m_lock);

    const int diff = m_msgCounter - lastKnownId - 1;
    const int size = static_cast<int>(m_messages.size());

    if ((lastKnownId == -1) || (diff >= size))
        return loadFromBuffer(m_messages);

    if (diff <= 0)
        return {};

    return loadFromBuffer(m_messages, (size - diff));
}

QList<Log::Peer> Logger::getPeers(const int lastKnownId) const
{
    const QReadLocker locker(&m_lock);

    const int diff = m_peerCounter - lastKnownId - 1;
    const int size = static_cast<int>(m_peers.size());

    if ((lastKnownId == -1) || (diff >= size))
        return loadFromBuffer(m_peers);

    if (diff <= 0)
        return {};

    return loadFromBuffer(m_peers, (size - diff));
}

void LogMsg(const QString &message, const Log::MsgType &type)
{
    Logger::instance()->addMessage(message, type);
}
