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

#pragma once

#include <boost/circular_buffer.hpp>

#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QtContainerFwd>

inline const int MAX_LOG_MESSAGES = 20000;

namespace Log
{
    enum MsgType
    {
        ALL = -1,
        NORMAL = 0x1,
        INFO = 0x2,
        WARNING = 0x4,
        CRITICAL = 0x8 // ERROR is defined by libtorrent and results in compiler error
    };
    Q_DECLARE_FLAGS(MsgTypes, MsgType)

    struct Msg
    {
        int id = -1;
        MsgType type = ALL;
        qint64 timestamp = -1;
        QString message;
    };

    struct Peer
    {
        int id = -1;
        bool blocked = false;
        qint64 timestamp = -1;
        QString ip;
        QString reason;
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Log::MsgTypes)

class Logger final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Logger)

public:
    static void initInstance();
    static void freeInstance();
    static Logger *instance();

    void addMessage(const QString &message, const Log::MsgType &type = Log::NORMAL);
    void addPeer(const QString &ip, bool blocked, const QString &reason = {});
    QList<Log::Msg> getMessages(int lastKnownId = -1) const;
    QList<Log::Peer> getPeers(int lastKnownId = -1) const;

signals:
    void newLogMessage(const Log::Msg &message);
    void newLogPeer(const Log::Peer &peer);

private:
    Logger();
    ~Logger() = default;

    static Logger *m_instance;
    boost::circular_buffer_space_optimized<Log::Msg> m_messages;
    boost::circular_buffer_space_optimized<Log::Peer> m_peers;
    mutable QReadWriteLock m_lock;
    int m_msgCounter = 0;
    int m_peerCounter = 0;
};

// Helper function
void LogMsg(const QString &message, const Log::MsgType &type = Log::NORMAL);
