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

#include "server.h"

#include <QMutableListIterator>
#include <QNetworkProxy>
#include <QStringList>
#include <QTimer>
#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#else
#include <QTcpSocket>
#endif

#include "connection.h"

static const int KEEP_ALIVE_DURATION = 7 * 1000;  // milliseconds
static const int CONNECTIONS_LIMIT = 500;
static const int CONNECTIONS_SCAN_INTERVAL = 2;  // seconds

using namespace Http;

Server::Server(IRequestHandler *requestHandler, QObject *parent)
    : QTcpServer(parent)
    , m_requestHandler(requestHandler)
#ifndef QT_NO_OPENSSL
    , m_https(false)
#endif
{
    setProxy(QNetworkProxy::NoProxy);
#ifndef QT_NO_OPENSSL
    QSslSocket::setDefaultCiphers(safeCipherList());
#endif

    QTimer *dropConnectionTimer = new QTimer(this);
    connect(dropConnectionTimer, &QTimer::timeout, this, &Server::dropTimedOutConnection);
    dropConnectionTimer->start(CONNECTIONS_SCAN_INTERVAL * 1000);
}

Server::~Server()
{
}

void Server::incomingConnection(qintptr socketDescriptor)
{
    if (m_connections.size() >= CONNECTIONS_LIMIT) return;

    QTcpSocket *serverSocket;
#ifndef QT_NO_OPENSSL
    if (m_https)
        serverSocket = new QSslSocket(this);
    else
#endif
        serverSocket = new QTcpSocket(this);

    if (!serverSocket->setSocketDescriptor(socketDescriptor)) {
        delete serverSocket;
        return;
    }

#ifndef QT_NO_OPENSSL
    if (m_https) {
        static_cast<QSslSocket *>(serverSocket)->setProtocol(QSsl::SecureProtocols);
        static_cast<QSslSocket *>(serverSocket)->setPrivateKey(m_key);
        static_cast<QSslSocket *>(serverSocket)->setLocalCertificateChain(m_certificates);
        static_cast<QSslSocket *>(serverSocket)->setPeerVerifyMode(QSslSocket::VerifyNone);
        static_cast<QSslSocket *>(serverSocket)->startServerEncryption();
    }
#endif

    Connection *c = new Connection(serverSocket, m_requestHandler, this);
    m_connections.insert(c);
    connect(serverSocket, &QAbstractSocket::disconnected, this, [c, this]() { removeConnection(c); });
}

void Server::removeConnection(Connection *connection)
{
    m_connections.remove(connection);
    connection->deleteLater();
}

void Server::dropTimedOutConnection()
{
    QMutableSetIterator<Connection *> i(m_connections);
    while (i.hasNext()) {
        Connection *connection = i.next();
        if (connection->hasExpired(KEEP_ALIVE_DURATION)) {
            connection->deleteLater();
            i.remove();
        }
    }
}

#ifndef QT_NO_OPENSSL
bool Server::setupHttps(const QByteArray &certificates, const QByteArray &key)
{
    QSslKey sslKey(key, QSsl::Rsa);
    if (sslKey.isNull())
        sslKey = QSslKey(key, QSsl::Ec);

    const QList<QSslCertificate> certs = QSslCertificate::fromData(certificates);
    const bool areCertsValid = !certs.empty() && std::all_of(certs.begin(), certs.end(), [](const QSslCertificate &c) { return !c.isNull(); });

    if (!sslKey.isNull() && areCertsValid) {
        m_key = sslKey;
        m_certificates = certs;
        m_https = true;
        return true;
    }
    else {
        disableHttps();
        return false;
    }
}

void Server::disableHttps()
{
    m_https = false;
    m_certificates.clear();
    m_key.clear();
}

QList<QSslCipher> Server::safeCipherList() const
{
    const QStringList badCiphers = {"idea", "rc4"};
    const QList<QSslCipher> allCiphers = QSslSocket::supportedCiphers();
    QList<QSslCipher> safeCiphers;
    for (const QSslCipher &cipher : allCiphers) {
        bool isSafe = true;
        for (const QString &badCipher : badCiphers) {
            if (cipher.name().contains(badCipher, Qt::CaseInsensitive)) {
                isSafe = false;
                break;
            }
        }

        if (isSafe)
            safeCiphers += cipher;
    }

    return safeCiphers;
}
#endif // QT_NO_OPENSSL
