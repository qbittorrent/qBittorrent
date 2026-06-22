/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019-2026  Mike Tzou (Chocobo1)
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
****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************
*/

#include "localpeer.h"

#include <QtSystemDetection>

#include <QByteArray>
#include <QByteArrayView>
#include <QDataStream>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSysInfo>
#include <QThread>

#include "base/3rdparty/expected.hpp"
#include "base/path.h"
#include "base/utils/bytearray.h"
#include "base/utils/io.h"

using namespace std::chrono_literals;

const QByteArray ACK = QByteArrayLiteral("ack");
const int MAX_MESSAGE_SIZE = 65535;

namespace
{
    enum class ParseMessageErrorType
    {
        DataIncomplete,
        DataInvalid
    };

    nonstd::expected<QString, ParseMessageErrorType> parseMessage(const QByteArray &data)
    {
        // Deserialize data from `QDataStream::writeBytes()`

        const int lengthOffset = sizeof(quint32);
        if (data.size() < lengthOffset)
            return nonstd::make_unexpected(ParseMessageErrorType::DataIncomplete);

        QDataStream ds {data};
        quint32 remaining = 0;
        ds >> remaining;

        if (remaining == 0)
            return QString();

        if (remaining > MAX_MESSAGE_SIZE)  // drop suspiciously large data
            return nonstd::make_unexpected(ParseMessageErrorType::DataInvalid);

        if ((data.size() - lengthOffset) < remaining)
            return nonstd::make_unexpected(ParseMessageErrorType::DataIncomplete);

        return QString::fromUtf8((data.constData() + lengthOffset), remaining);
    }
}

LocalPeer::LocalPeer(const QString &path, QObject *parent)
    : QObject(parent)
    , m_socketName {path + u"/ipc-socket"}
    , m_server {new QLocalServer(this)}
    , m_lockFile {path + u"/lockfile"}
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    m_lockFile.setStaleLockTime(0);
}

bool LocalPeer::isClient()
{
    if (m_lockFile.isLocked())
        return false;

    if (!m_lockFile.tryLock())
    {
        // Since v5.2 `QLockFile` is adopted as part of "single instance" feature implementation.
        // It may have conflict with old locking mechanism in case there is stale lockfile
        // created by previous qBittorrent version exist when new one is starting.
        // The following code is intended to prevent this issue by removing stale lockfile.
        if (m_lockFile.error() != QLockFile::LockFailedError)
            return true;

        if (const std::optional<LockInfo> lockInfo = getLockInfo())
        {
            if ((lockInfo->hostid == QSysInfo::machineUniqueId())
                    && (lockInfo->hostname == QSysInfo::machineHostName()))
            {
                if (hasActiveServer(100ms))
                    return true;
            }

            if (!m_lockFile.removeStaleLockFile() || !m_lockFile.tryLock())
                return true;
        }
        else
        {
            if (!m_lockFile.removeStaleLockFile() || !m_lockFile.tryLock())
                return true;
        }
    }

    bool res = m_server->listen(m_socketName);
#if defined(Q_OS_UNIX)
    // ### Workaround
    if (!res && m_server->serverError() == QAbstractSocket::AddressInUseError)
    {
        QFile::remove(m_socketName);
        res = m_server->listen(m_socketName);
    }
#endif
    if (!res)
        qWarning("LocalPeer: failed to listen on local socket. Error: %s", qUtf8Printable(m_server->errorString()));

    connect(m_server, &QLocalServer::newConnection, this, &LocalPeer::receivedConnection);
    return false;
}

bool LocalPeer::sendMessage(const QString &message, const std::chrono::milliseconds timeout)
{
    if (!isClient())
        return false;

    const QByteArray buffer = message.toUtf8();
    if (buffer.size() > MAX_MESSAGE_SIZE)  // drop suspiciously large data
        return false;

    QLocalSocket socket;
    bool isConnected = false;

    // Try a few times, in case the other instance is just starting up
    const int retries = 2;
    for (int i = 0; i < retries; ++i)
    {
        socket.connectToServer(m_socketName);
        isConnected = socket.waitForConnected(timeout.count() / retries);
        if (isConnected)
            break;

        QThread::sleep(250ms);
    }

    if (!isConnected)
    {
        qWarning("LocalPeer: failed to connect to server.");
        return false;
    }

    QDataStream ds {&socket};
    ds.writeBytes(buffer.constData(), buffer.size());
    if (!socket.waitForBytesWritten(timeout.count()))
    {
        qWarning("LocalPeer: failed to write data to server.");
        return false;
    }
    if (!socket.waitForReadyRead(timeout.count()))
    {
        qWarning("LocalPeer: failed to read data from server.");
        return false;
    }
    if (socket.read(ACK.size()) != ACK)
    {
        qWarning("LocalPeer: failed to receive ACK from server.");
        return false;
    }

    return true;
}

bool LocalPeer::hasActiveServer(const std::chrono::milliseconds timeout) const
{
    QLocalSocket socket;
    socket.connectToServer(m_socketName);
    return socket.waitForConnected(timeout.count());
}

void LocalPeer::receivedConnection()
{
    QLocalSocket *socket = m_server->nextPendingConnection();
    if (!socket) [[unlikely]]
        return;

    auto *buffer = new QByteArray;

    connect(socket, &QLocalSocket::disconnected, this, [buffer, socket]
    {
        socket->deleteLater();
        delete buffer;
    });
    connect(socket, &QIODevice::readyRead, this, [this, buffer, socket]
    {
        const qint64 bytes = socket->bytesAvailable();
        if ((buffer->size() + bytes) > MAX_MESSAGE_SIZE)
        {
            socket->disconnectFromServer();
            qWarning("LocalPeer: received data exceeding max limit.");
            return;
        }

        buffer->append(socket->read(bytes));

        const auto result = parseMessage(*buffer);
        if (!result)
        {
            switch (result.error())
            {
            case ParseMessageErrorType::DataIncomplete:
                return;
            case ParseMessageErrorType::DataInvalid:
                socket->disconnectFromServer();
                qWarning("LocalPeer: received invalid data.");
                return;
            }
        }

        socket->write(ACK);
        socket->disconnectFromServer();

        emit messageReceived(result.value()); // might take a long time to return
    });
}

std::optional<LocalPeer::LockInfo> LocalPeer::getLockInfo() const
{
    const auto readResult = Utils::IO::readFile(Path(m_lockFile.fileName()), 1024);
    if (!readResult)
        return std::nullopt;

    const QList<QByteArrayView> lines = Utils::ByteArray::splitToViews(readResult.value(), "\n", Qt::KeepEmptyParts);
    if (lines.size() < 5)
        return std::nullopt;

    const QByteArrayView pidLine = lines.at(0);
    if (pidLine.isEmpty())
        return std::nullopt;

    bool ok = false;
    const qint64 pid = pidLine.toLongLong(&ok);
    if (!ok || (pid <= 0))
        return std::nullopt;

    return LockInfo {
        .pid = pid,
        .appname = QString::fromUtf8(lines.at(1)),
        .hostname = QString::fromUtf8(lines.at(2)),
        .hostid = lines.at(3).toByteArray(),
        .bootid = lines.at(4).toByteArray()
    };
}
