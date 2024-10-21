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

#include <algorithm>
#include <chrono>
#include <memory>
#include <new>

#include <QtLogging>
#include <QNetworkProxy>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslKey>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>

#include "base/global.h"
#include "base/utils/net.h"
#include "base/utils/sslkey.h"
#include "connection.h"

using namespace std::chrono_literals;

namespace
{
    const int KEEP_ALIVE_DURATION = std::chrono::milliseconds(7s).count();
    const int CONNECTIONS_LIMIT = 500;
    const std::chrono::seconds CONNECTIONS_SCAN_INTERVAL {2};

    QList<QSslCipher> safeCipherList()
    {
        const QStringList badCiphers {u"idea"_s, u"rc4"_s};
        // Contains Ciphersuites that use RSA for the Key Exchange but they don't mention it in their name
        const QStringList badRSAShorthandSuites {
            u"AES256-GCM-SHA384"_s, u"AES128-GCM-SHA256"_s, u"AES256-SHA256"_s,
            u"AES128-SHA256"_s, u"AES256-SHA"_s, u"AES128-SHA"_s};
        // Contains Ciphersuites that use AES CBC mode but they don't mention it in their name
        const QStringList badAESShorthandSuites {
            u"ECDHE-ECDSA-AES256-SHA384"_s, u"ECDHE-RSA-AES256-SHA384"_s, u"DHE-RSA-AES256-SHA256"_s,
            u"ECDHE-ECDSA-AES128-SHA256"_s, u"ECDHE-RSA-AES128-SHA256"_s, u"DHE-RSA-AES128-SHA256"_s,
            u"ECDHE-ECDSA-AES256-SHA"_s, u"ECDHE-RSA-AES256-SHA"_s, u"DHE-RSA-AES256-SHA"_s,
            u"ECDHE-ECDSA-AES128-SHA"_s, u"ECDHE-RSA-AES128-SHA"_s, u"DHE-RSA-AES128-SHA"_s};
        const QList<QSslCipher> allCiphers {QSslConfiguration::supportedCiphers()};
        QList<QSslCipher> safeCiphers;
        std::copy_if(allCiphers.cbegin(), allCiphers.cend(), std::back_inserter(safeCiphers),
                     [&badCiphers, &badRSAShorthandSuites, &badAESShorthandSuites](const QSslCipher &cipher)
        {
            const QString name = cipher.name();
            if (name.contains(u"-cbc-"_s, Qt::CaseInsensitive) // AES CBC mode is considered vulnerable to BEAST attack
                || name.startsWith(u"adh-"_s, Qt::CaseInsensitive) // Key Exchange: Diffie-Hellman, doesn't support Perfect Forward Secrecy
                || name.startsWith(u"aecdh-"_s, Qt::CaseInsensitive) // Key Exchange: Elliptic Curve Diffie-Hellman, doesn't support Perfect Forward Secrecy
                || name.startsWith(u"psk-"_s, Qt::CaseInsensitive) // Key Exchange: Pre-Shared Key, doesn't support Perfect Forward Secrecy
                || name.startsWith(u"rsa-"_s, Qt::CaseInsensitive) // Key Exchange: Rivest Shamir Adleman (RSA), doesn't support Perfect Forward Secrecy
                || badRSAShorthandSuites.contains(name, Qt::CaseInsensitive)
                || badAESShorthandSuites.contains(name, Qt::CaseInsensitive))
            {
                return false;
            }

            return std::none_of(badCiphers.cbegin(), badCiphers.cend(), [&cipher](const QString &badCipher)
            {
                return cipher.name().contains(badCipher, Qt::CaseInsensitive);
            });
        });
        return safeCiphers;
    }
}

using namespace Http;

Server::Server(IRequestHandler *requestHandler, QObject *parent)
    : QTcpServer(parent)
    , m_requestHandler(requestHandler)
    , m_sslConfig {QSslConfiguration::defaultConfiguration()}
{
    setProxy(QNetworkProxy::NoProxy);

    m_sslConfig.setCiphers(safeCipherList());
    m_sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

    auto *dropConnectionTimer = new QTimer(this);
    connect(dropConnectionTimer, &QTimer::timeout, this, &Server::dropTimedOutConnection);
    dropConnectionTimer->start(CONNECTIONS_SCAN_INTERVAL);
}

void Server::incomingConnection(const qintptr socketDescriptor)
{
    std::unique_ptr<QTcpSocket> serverSocket = isHttps() ? std::make_unique<QSslSocket>(this) : std::make_unique<QTcpSocket>(this);
    if (!serverSocket->setSocketDescriptor(socketDescriptor))
        return;

    if (m_connections.size() >= CONNECTIONS_LIMIT)
    {
        qWarning("Too many connections. Exceeded CONNECTIONS_LIMIT (%d). Connection closed.", CONNECTIONS_LIMIT);
        return;
    }

    try
    {
        if (isHttps())
        {
            auto *sslSocket = static_cast<QSslSocket *>(serverSocket.get());
            sslSocket->setSslConfiguration(m_sslConfig);
            sslSocket->startServerEncryption();
        }

        auto *connection = new Connection(serverSocket.release(), m_requestHandler, this);
        m_connections.insert(connection);
        connect(connection, &Connection::closed, this, [this, connection] { removeConnection(connection); });
    }
    catch (const std::bad_alloc &exception)
    {
        // drop the connection instead of throwing exception and crash
        qWarning("Failed to allocate memory for HTTP connection. Connection closed.");
        return;
    }
}

void Server::removeConnection(Connection *connection)
{
    m_connections.remove(connection);
    connection->deleteLater();
}

void Server::dropTimedOutConnection()
{
    m_connections.removeIf([](Connection *connection)
    {
        if (!connection->hasExpired(KEEP_ALIVE_DURATION))
            return false;

        connection->deleteLater();
        return true;
    });
}

bool Server::setupHttps(const QByteArray &certificates, const QByteArray &privateKey)
{
    const QList<QSslCertificate> certs {Utils::Net::loadSSLCertificate(certificates)};
    const QSslKey key {Utils::SSLKey::load(privateKey)};

    if (certs.isEmpty() || key.isNull())
    {
        disableHttps();
        return false;
    }

    m_sslConfig.setLocalCertificateChain(certs);
    m_sslConfig.setPrivateKey(key);
    m_https = true;
    return true;
}

void Server::disableHttps()
{
    m_sslConfig.setLocalCertificateChain({});
    m_sslConfig.setPrivateKey({});
    m_https = false;
}

bool Server::isHttps() const
{
    return m_https;
}
