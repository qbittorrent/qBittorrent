/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Prince Gupta <guptaprince8832@gmail.com>
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez <chris@qbittorrent.org>
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

#include <QElapsedTimer>
#include <QObject>

#include "types.h"

class QTcpSocket;

namespace Http
{
    class Connection : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Connection)

    public:
        // Connection takes ownership of socket
        explicit Connection(QTcpSocket *socket, QObject *parent = nullptr);
        ~Connection();

        void sendStatus(const ResponseStatus &status);
        void sendHeaders(const HeaderMap &headers);
        void sendContent(const QByteArray &data);
        void close();

        Environment enviroment() const;
        qint64 inactivityTime() const;
        qint64 bytesToWrite() const;

    signals:
        void bytesWritten(qint64 bytes);
        void disconnected();
        void requestReady(Request request);

    private slots:
        void read();

    private:
        QTcpSocket *m_socket;
        QElapsedTimer m_idleTimer;
        QByteArray m_receivedData;
        QByteArray m_statusHeaderBuffer;
        Request m_request;
        qint64 m_pendingContentSize = 0;
        bool m_persistentConnection = true;
    };
}
