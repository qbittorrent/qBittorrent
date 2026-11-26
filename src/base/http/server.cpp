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
#include <QSocketNotifier>
#include <QSslCertificate>
#include <QSslCipher>
#include <QSslKey>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>
#include <QTcpSocket>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

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
        std::ranges::copy_if(allCiphers, std::back_inserter(safeCiphers),
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

            return std::ranges::none_of(badCiphers, [&cipher](const QString &badCipher)
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

Server::~Server()
{
    closeUnixSocket();
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

#if defined(Q_OS_UNIX)

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0
#endif

namespace
{
    int portableAccept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    {
#if defined(__linux__)
        return ::accept4(sockfd, addr, addrlen, SOCK_CLOEXEC | SOCK_NONBLOCK);
#else
        const int fd = ::accept(sockfd, addr, addrlen);
        if (fd < 0)
            return fd;

        if (::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0)
            qWarning("WebUI: Failed to set FD_CLOEXEC on accepted Unix socket: %s", ::strerror(errno));
        if (::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK) < 0)
            qWarning("WebUI: Failed to set O_NONBLOCK on accepted Unix socket: %s", ::strerror(errno));
        return fd;
#endif
    }

    bool setSocketFlags(const int fd)
    {
#if !defined(SOCK_CLOEXEC) || (SOCK_CLOEXEC == 0)
        if (::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0)
            return false;
#endif
#if !defined(SOCK_NONBLOCK) || (SOCK_NONBLOCK == 0)
        if (::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK) < 0)
            return false;
#endif
        return true;
    }
}

bool Server::listenUnixSocket(const QString &path, const int permissions)
{
    Q_ASSERT(!path.isEmpty());

    // Close previous Unix socket if any
    closeUnixSocket();

    const QByteArray pathBytes = path.toUtf8();
    const bool isAbstract = path.startsWith(u'@');

    m_unixListenFd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (m_unixListenFd < 0)
    {
        m_unixErrorString = QString::fromUtf8(::strerror(errno));
        qWarning() << "WebUI: Failed to create Unix socket:" << m_unixErrorString;
        return false;
    }

    if (!setSocketFlags(m_unixListenFd))
    {
        m_unixErrorString = QString::fromUtf8(::strerror(errno));
        qWarning() << "WebUI: Failed to set socket flags:" << m_unixErrorString;
        ::close(m_unixListenFd);
        m_unixListenFd = -1;
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    int nameLen = 0;

    if (isAbstract)
    {
        // Abstract socket: first byte of sun_path is null
        const QByteArray name = pathBytes.mid(1);  // skip '@'
        const auto maxLen = static_cast<qsizetype>(sizeof(addr.sun_path)) - 1;
        nameLen = static_cast<int>(std::min(name.size(), maxLen));
        if (nameLen > 0)
            std::memcpy(addr.sun_path + 1, name.constData(), nameLen);
    }
    else
    {
        // Filesystem socket
        const auto maxLen = static_cast<qsizetype>(sizeof(addr.sun_path)) - 1;
        nameLen = static_cast<int>(std::min(pathBytes.size(), maxLen));
        if (nameLen > 0)
            std::memcpy(addr.sun_path, pathBytes.constData(), nameLen);

        // Remove existing socket file
        ::unlink(pathBytes.constData());
    }

    const int addrLen = (isAbstract
        ? offsetof(struct sockaddr_un, sun_path) + 1 + nameLen
        : sizeof(addr));

    if (::bind(m_unixListenFd, reinterpret_cast<struct sockaddr *>(&addr), addrLen) < 0)
    {
        m_unixErrorString = QString::fromUtf8(::strerror(errno));
        qWarning() << "WebUI: Failed to bind Unix socket:" << m_unixErrorString;
        ::close(m_unixListenFd);
        m_unixListenFd = -1;
        return false;
    }

    // Set permissions for non-abstract sockets
    if (!isAbstract)
    {
        if (::chmod(pathBytes.constData(), permissions) < 0)
        {
            qWarning() << "WebUI: Failed to set Unix socket permissions:" << ::strerror(errno);
        }
    }

    if (::listen(m_unixListenFd, SOMAXCONN) < 0)
    {
        m_unixErrorString = QString::fromUtf8(::strerror(errno));
        qWarning() << "WebUI: Failed to listen on Unix socket:" << m_unixErrorString;
        ::close(m_unixListenFd);
        m_unixListenFd = -1;
        if (!isAbstract)
            ::unlink(pathBytes.constData());
        return false;
    }

    m_unixSocketNotifier = new QSocketNotifier(m_unixListenFd, QSocketNotifier::Read, this);
    connect(m_unixSocketNotifier, &QSocketNotifier::activated, this, &Server::acceptUnixConnection);

    m_unixErrorString.clear();
    m_isUnixSocket = true;
    m_unixSocketPath = path;
    m_unixSocketPermissions = permissions;

    return true;
}

void Server::closeUnixSocket()
{
    if (m_unixListenFd < 0)
        return;

    delete m_unixSocketNotifier;
    m_unixSocketNotifier = nullptr;

    ::close(m_unixListenFd);
    m_unixListenFd = -1;

    // Remove socket file for non-abstract sockets
    if (!m_unixSocketPath.startsWith(u'@'))
        ::unlink(m_unixSocketPath.toUtf8().constData());

    m_isUnixSocket = false;
    m_unixSocketPath.clear();
}

void Server::acceptUnixConnection()
{
    if (m_unixListenFd < 0)
        return;

    struct sockaddr_un peerAddr;
    socklen_t peerAddrLen = sizeof(peerAddr);

    const int clientFd = portableAccept4(m_unixListenFd, reinterpret_cast<struct sockaddr *>(&peerAddr)
                                         , &peerAddrLen);
    if (clientFd < 0)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            qWarning() << "WebUI: Failed to accept Unix socket connection:" << ::strerror(errno);
        return;
    }

    if (m_connections.size() >= CONNECTIONS_LIMIT)
    {
        qWarning("Too many connections on Unix socket. Exceeded CONNECTIONS_LIMIT (%d). Connection closed.", CONNECTIONS_LIMIT);
        ::close(clientFd);
        return;
    }

    // Wrap the Unix domain socket fd in a QTcpSocket to reuse the existing
    // HTTP connection pipeline (Connection, ResponseWriterImpl) which operates
    // on QAbstractSocket. QAbstractSocket methods that query TCP-specific info
    // (peerAddress, localPort, etc.) will return invalid data for Unix sockets,
    // but those are only used in log messages where empty/zero is acceptable.
    std::unique_ptr<QTcpSocket> socket = std::make_unique<QTcpSocket>(this);
    if (!socket->setSocketDescriptor(clientFd))
    {
        qWarning() << "WebUI: Failed to set socket descriptor for Unix connection";
        ::close(clientFd);
        return;
    }

    try
    {
        auto *connection = new Connection(socket.release(), m_requestHandler, this);
        m_connections.insert(connection);
        connect(connection, &Connection::closed, this, [this, connection] { removeConnection(connection); });
    }
    catch (const std::bad_alloc &)
    {
        qWarning("Failed to allocate memory for HTTP connection on Unix socket. Connection closed.");
    }
}
#else
bool Server::listenUnixSocket(const QString &path, int permissions)
{
    Q_UNUSED(path)
    Q_UNUSED(permissions)
    return false;
}

void Server::closeUnixSocket()
{
}

void Server::acceptUnixConnection()
{
}
#endif
