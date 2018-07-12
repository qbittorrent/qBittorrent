/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Mike Tzou (Chocobo1)
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

#include "connection.h"

#include <QTcpSocket>

#include "base/logger.h"
#include "base/preferences.h"
#include "irequesthandler.h"
#include "requestparser.h"
#include "responsegenerator.h"

using namespace Http;

Connection::Connection(QTcpSocket *socket, IRequestHandler *requestHandler, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_requestHandler(requestHandler)
{
    m_socket->setParent(this);
    m_idleTimer.start();
    connect(m_socket, &QTcpSocket::readyRead, this, &Connection::read);
}

Connection::~Connection()
{
    m_socket->close();
}

void Connection::read()
{
    m_idleTimer.restart();
    m_receivedData.append(m_socket->readAll());

    while (!m_receivedData.isEmpty()) {
        const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);

        switch (result.status) {
        case RequestParser::ParseStatus::Incomplete: {
                const long bufferLimit = RequestParser::MAX_CONTENT_SIZE * 1.1;  // some margin for headers
                if (m_receivedData.size() > bufferLimit) {
                    Logger::instance()->addMessage(tr("Http request size exceeds limiation, closing socket. Limit: %ld, IP: %s")
                        .arg(bufferLimit).arg(m_socket->peerAddress().toString()), Log::WARNING);

                    Response resp(413, "Payload Too Large");
                    resp.headers[HEADER_CONNECTION] = "close";

                    sendResponse(resp);
                    m_socket->close();
                }
            }
            return;

        case RequestParser::ParseStatus::BadRequest: {
                Logger::instance()->addMessage(tr("Bad Http request, closing socket. IP: %s")
                    .arg(m_socket->peerAddress().toString()), Log::WARNING);

                Response resp(400, "Bad Request");
                resp.headers[HEADER_CONNECTION] = "close";

                sendResponse(resp);
                m_socket->close();
            }
            return;

        case RequestParser::ParseStatus::OK: {
                const Environment env {m_socket->localAddress(), m_socket->localPort(), resolvePeerAddress(result.request), m_socket->peerPort()};

                Response resp = m_requestHandler->processRequest(result.request, env);

                if (acceptsGzipEncoding(result.request.headers["accept-encoding"]))
                    resp.headers[HEADER_CONTENT_ENCODING] = "gzip";

                resp.headers[HEADER_CONNECTION] = "keep-alive";

                sendResponse(resp);
                m_receivedData = m_receivedData.mid(result.frameSize);
            }
            break;

        default:
            Q_ASSERT(false);
            return;
        }
    }
}

void Connection::sendResponse(const Response &response) const
{
    m_socket->write(toByteArray(response));
}

bool Connection::hasExpired(const qint64 timeout) const
{
    return m_idleTimer.hasExpired(timeout);
}

bool Connection::isClosed() const
{
    return (m_socket->state() == QAbstractSocket::UnconnectedState);
}

bool Connection::acceptsGzipEncoding(QString codings)
{
    // [rfc7231] 5.3.4. Accept-Encoding

    const auto isCodingAvailable = [](const QStringList &list, const QString &encoding) -> bool
    {
        foreach (const QString &str, list) {
            if (!str.startsWith(encoding))
                continue;

            // without quality values
            if (str == encoding)
                return true;

            // [rfc7231] 5.3.1. Quality Values
            const QStringRef substr = str.midRef(encoding.size() + 3);  // ex. skip over "gzip;q="

            bool ok = false;
            const double qvalue = substr.toDouble(&ok);
            if (!ok || (qvalue <= 0.0))
                return false;

            return true;
        }
        return false;
    };

    const QStringList list = codings.remove(' ').remove('\t').split(',', QString::SkipEmptyParts);
    if (list.isEmpty())
        return false;

    const bool canGzip = isCodingAvailable(list, QLatin1String("gzip"));
    if (canGzip)
        return true;

    const bool canAny = isCodingAvailable(list, QLatin1String("*"));
    if (canAny)
        return true;

    return false;
}

QHostAddress Connection::resolvePeerAddress(const Http::Request &request)
{
    QString reverseProxyAddressString = Preferences::instance()->getReverseProxyAddress();
    QHostAddress peerAddress = m_socket->peerAddress();
    QHostAddress reverseProxyAddress;

    // Only reverse proxy can overwrite peer address
    if (reverseProxyAddress.setAddress(reverseProxyAddressString) && peerAddress.isEqual(reverseProxyAddress)) {
        QString forwardedFor = request.headers.value(Http::HEADER_X_FORWARDED_FOR);

        if (!forwardedFor.isEmpty()) {
            // peer address is the 1st global IP in X-Forwarded-For or, if none available, the 1st IP in the list
            QStringList remoteIpList= forwardedFor.split(",");
            bool hasGlobalIp = false;

            foreach (const QString &remoteIp, remoteIpList) {
               if (peerAddress.setAddress(remoteIp) && isGlobal(peerAddress)) {
                   hasGlobalIp = true;
                   break;
               }
            }

            if (!hasGlobalIp)
                peerAddress.setAddress(remoteIpList.at(0));
        }
    }

    return peerAddress;
}

bool Connection::isGlobal(const QHostAddress ip)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    // check IP against IPv4 special addresses
    if (ip.isInSubnet(QHostAddress("10.0.0.0"), 8))
        return false;

    if (ip.isInSubnet(QHostAddress("100.64.0.0"), 10))
        return false;

    if (ip.isInSubnet(QHostAddress("127.0.0.0"), 8))
        return false;

    if (ip.isInSubnet(QHostAddress("169.254.0.0"), 16))
        return false;

    if (ip.isInSubnet(QHostAddress("172.16.0.0"), 12))
        return false;

    if (ip.isInSubnet(QHostAddress("192.0.0.0"), 24))
        return false;

    if (ip.isInSubnet(QHostAddress("192.0.2.0"), 24))
        return false;

    if (ip.isInSubnet(QHostAddress("192.88.99.0"), 24))
        return false;

    if (ip.isInSubnet(QHostAddress("192.168.0.0"), 16))
        return false;

    if (ip.isInSubnet(QHostAddress("198.18.0.0"), 15))
        return false;

    if (ip.isInSubnet(QHostAddress("198.51.100.0"), 24))
        return false;

    if (ip.isInSubnet(QHostAddress("203.0.113.0"), 24))
        return false;

    if (ip.isInSubnet(QHostAddress("224.0.0.0"), 4))
        return false;

    if (ip.isInSubnet(QHostAddress("240.0.0.0"), 4))
        return false;

    if (ip.isInSubnet(QHostAddress("255.255.255.255"), 32))
        return false;

    // check IP against IPv6 special addresses
    if (ip.isInSubnet(QHostAddress("::1"), 128))
        return false;

    if (ip.isInSubnet(QHostAddress("100::"), 64))
        return false;

    if (ip.isInSubnet(QHostAddress("2001:db8::"), 32))
        return false;

    if (ip.isInSubnet(QHostAddress("fc00::"), 7))
        return false;

    if (ip.isInSubnet(QHostAddress("fe80::"), 10))
        return false;

    if (ip.isInSubnet(QHostAddress("ff00::"), 8))
        return false;

    return true;
#else
    return ip.isGlobal();
#endif
}
