/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "qtlocalpeer.h"

#if defined(Q_OS_UNIX)
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

#include "base/utils/misc.h"

namespace QtLP_Private
{
#include "qtlockedfile.cpp"

#if defined(Q_OS_WIN)
#include "qtlockedfile_win.cpp"
#else
#include "qtlockedfile_unix.cpp"
#endif
}

const char* QtLocalPeer::ack = "ack";

QtLocalPeer::QtLocalPeer(QObject* parent, const QString &appId)
    : QObject(parent)
    , id(appId)
{
    QString prefix = id;
    if (id.isEmpty())
    {
        id = QCoreApplication::applicationFilePath();
#if defined(Q_OS_WIN)
        id = id.toLower();
#endif
        prefix = id.section(QLatin1Char('/'), -1);
    }
    prefix.remove(QRegExp("[^a-zA-Z]"));
    prefix.truncate(6);

    QByteArray idc = id.toUtf8();
    quint16 idNum = qChecksum(idc.constData(), idc.size());
    socketName = QLatin1String("qtsingleapp-") + prefix
                 + QLatin1Char('-') + QString::number(idNum, 16);

#if defined(Q_OS_WIN)
    DWORD sessionId = 0;
    ::ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
    socketName += (QLatin1Char('-') + QString::number(sessionId, 16));
#else
    socketName += (QLatin1Char('-') + QString::number(::getuid(), 16));
#endif

    server = new QLocalServer(this);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    QString lockName = QDir(QDir::tempPath()).absolutePath()
                       + QLatin1Char('/') + socketName
                       + QLatin1String("-lockfile");
    lockFile.setFileName(lockName);
    lockFile.open(QIODevice::ReadWrite);
}

bool QtLocalPeer::isClient()
{
    if (lockFile.isLocked())
        return false;

    if (!lockFile.lock(QtLP_Private::QtLockedFile::WriteLock, false))
        return true;

    bool res = server->listen(socketName);
#if defined(Q_OS_UNIX)
    // ### Workaround
    if (!res && server->serverError() == QAbstractSocket::AddressInUseError)
    {
        QFile::remove(QDir::cleanPath(QDir::tempPath())+QLatin1Char('/')+socketName);
        res = server->listen(socketName);
    }
#endif
    if (!res)
        qWarning("QtSingleCoreApplication: listen on local socket failed, %s", qPrintable(server->errorString()));
    connect(server, &QLocalServer::newConnection, this, &QtLocalPeer::receiveConnection);
    return false;
}

bool QtLocalPeer::sendMessage(const QString &message, const int timeout)
{
    if (!isClient())
        return false;

    QLocalSocket socket;
    bool connOk = false;
    for(int i = 0; i < 2; i++)
    {
        // Try twice, in case the other instance is just starting up
        socket.connectToServer(socketName);
        connOk = socket.waitForConnected(timeout/2);
        if (connOk || i)
            break;
        int ms = 250;
#if defined(Q_OS_WIN)
        Sleep(DWORD(ms));
#else
        struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
        nanosleep(&ts, NULL);
#endif
    }
    if (!connOk)
        return false;

    QByteArray uMsg(message.toUtf8());
    QDataStream ds(&socket);
    ds.writeBytes(uMsg.constData(), uMsg.size());
    bool res = socket.waitForBytesWritten(timeout);
    if (res)
    {
        res &= socket.waitForReadyRead(timeout);   // wait for ack
        if (res)
            res &= (socket.read(qstrlen(ack)) == ack);
    }
    return res;
}

QString QtLocalPeer::applicationId() const
{
    return id;
}

void QtLocalPeer::receiveConnection()
{
    QLocalSocket* socket = server->nextPendingConnection();
    if (!socket)
        return;

    while (true)
    {
        if (socket->state() == QLocalSocket::UnconnectedState)
        {
            qWarning("QtLocalPeer: Peer disconnected");
            delete socket;
            return;
        }
        if (socket->bytesAvailable() >= qint64(sizeof(quint32)))
            break;
        socket->waitForReadyRead();
    }

    QDataStream ds(socket);
    QByteArray uMsg;
    quint32 remaining;
    ds >> remaining;
    if (remaining > 65535)
    {
        // drop suspiciously large data
        delete socket;
        return;
    }

    uMsg.resize(remaining);
    int got = 0;
    char* uMsgBuf = uMsg.data();
    do
    {
        got = ds.readRawData(uMsgBuf, remaining);
        remaining -= got;
        uMsgBuf += got;
    } while (remaining && got >= 0 && socket->waitForReadyRead(2000));
    if (got < 0)
    {
        qWarning("QtLocalPeer: Message reception failed %s", socket->errorString().toLatin1().constData());
        delete socket;
        return;
    }
    QString message(QString::fromUtf8(uMsg));
    socket->write(ack, qstrlen(ack));
    socket->waitForBytesWritten(1000);
    socket->waitForDisconnected(1000); // make sure client reads ack
    delete socket;
    emit messageReceived(message); //### (might take a long time to return)
}
