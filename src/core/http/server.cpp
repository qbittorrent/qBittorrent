/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006  Ishan Arora <ishan@qbittorrent.org>
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

#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#else
#include <QTcpSocket>
#endif
#include "connection.h"
#include "server.h"

using namespace Http;

Server::Server(IRequestHandler *requestHandler, QObject *parent)
    : QTcpServer(parent)
    , m_requestHandler(requestHandler)
#ifndef QT_NO_OPENSSL
    , m_https(false)
#endif
{
}

Server::~Server()
{
}

#ifndef QT_NO_OPENSSL
void Server::enableHttps(const QSslCertificate &certificate, const QSslKey &key)
{
    m_certificate = certificate;
    m_key = key;
    m_https = true;
}

void Server::disableHttps()
{
    m_https = false;
    m_certificate.clear();
    m_key.clear();
}
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
void Server::incomingConnection(qintptr socketDescriptor)
#else
void Server::incomingConnection(int socketDescriptor)
#endif
{
    QTcpSocket *serverSocket;
#ifndef QT_NO_OPENSSL
    if (m_https)
        serverSocket = new QSslSocket(this);
    else
#endif
        serverSocket = new QTcpSocket(this);

    if (serverSocket->setSocketDescriptor(socketDescriptor)) {
#ifndef QT_NO_OPENSSL
        if (m_https) {
            static_cast<QSslSocket*>(serverSocket)->setProtocol(QSsl::AnyProtocol);
            static_cast<QSslSocket*>(serverSocket)->setPrivateKey(m_key);
            static_cast<QSslSocket*>(serverSocket)->setLocalCertificate(m_certificate);
            static_cast<QSslSocket*>(serverSocket)->startServerEncryption();
        }
#endif
        new Connection(serverSocket, m_requestHandler, this);
    }
    else {
        serverSocket->deleteLater();
    }
}
