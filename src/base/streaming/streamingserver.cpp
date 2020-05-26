#include "streamingserver.h"
#include "../http/requestparser.h"
#include "../http/responsegenerator.h"
#include "../logger.h"

HttpSocket::HttpSocket(QAbstractSocket *socket)
    : QObject(socket)
    , isServing(false)
    , m_socket(socket)
{
    connect(m_socket, &QAbstractSocket::readyRead, this, &HttpSocket::read);
}

void HttpSocket::read()
{
    using namespace Http;

    m_idleTimer.restart();
    m_receivedData.append(m_socket->readAll());
    Q_ASSERT(!isServing);

    while (!m_receivedData.isEmpty()) {
        const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);

        switch (result.status) {
        case RequestParser::ParseStatus::Incomplete: {
                const long bufferLimit = RequestParser::MAX_CONTENT_SIZE * 1.1; // some margin for headers
                if (m_receivedData.size() > bufferLimit) {
                    Logger::instance()->addMessage(tr("Http request size exceeds limiation, closing socket. Limit: %1, IP: %2")
                                                   .arg(bufferLimit)
                                                   .arg(m_socket->peerAddress().toString()),
                                                   Log::WARNING);

                    sendStatus({413, "Payload Too Large"});
                    sendHeaders({{HEADER_CONNECTION, "close"}});

                    close();
                }
            }
            return;

        case RequestParser::ParseStatus::BadRequest: {
                Logger::instance()->addMessage(tr("Bad Http request, closing socket. IP: %1")
                                               .arg(m_socket->peerAddress().toString()),
                                               Log::WARNING);

                sendStatus({400, "Bad Request"});
                sendHeaders({{HEADER_CONNECTION, "close"}});

                close();
            }
            return;

        case RequestParser::ParseStatus::OK: {
                isServing = true;
                m_request = result.request;
                m_receivedData = m_receivedData.mid(result.frameSize);

                emit readyRequest();
                break;
            }

        default:
            Q_ASSERT(false);
            return;
        }
    }
}

void HttpSocket::sendStatus(const Http::ResponseStatus &status)
{
    send(QString("HTTP/%1 %2 %3")
         .arg("1.1",            // TODO: depends on request
              QString::number(status.code),
              status.text)
         .toLatin1()
         .append(Http::CRLF));
}

void HttpSocket::sendHeaders(const Http::HeaderMap &headers)
{
    const auto writeHeader =
        [this](const QString &key, const QString &value)
        {
            m_socket->write(QString::fromLatin1("%1: %2").arg(key, value).toLatin1().append(Http::CRLF));
        };

    for (auto i = headers.constBegin(); i != headers.constEnd(); ++i)
        writeHeader(i.key(), i.value());

    if (!headers.contains(Http::HEADER_DATE))
        writeHeader(Http::HEADER_DATE, Http::httpDate());

    send(Http::CRLF);
}

void HttpSocket::send(const QByteArray &data)
{
    m_idleTimer.restart();

    m_socket->write(data);
}

Http::Request HttpSocket::request() const
{
    return m_request;
}

qint64 HttpSocket::inactivityTime() const
{
    return m_idleTimer.elapsed();
}

void HttpSocket::close()
{
    m_socket->close();
}

void StreamingServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }

    HttpSocket *httpSocket = new HttpSocket(socket);
    connect(httpSocket, &HttpSocket::readyRequest, this, [this, httpSocket]() {
        doRequest(httpSocket);
    });
    
    connect(socket, &QAbstractSocket::disconnected, this, [httpSocket]() {
        httpSocket->deleteLater();
    });
}

void StreamingServer::start()
{
    const QHostAddress ip = QHostAddress::Any;
    const int port = 9696;

    if (isListening()) {
        if (serverPort() == port)
            // Already listening on the right port, just return
            return;

        // Wrong port, closing the server
        close();
    }

    // Listen on the predefined port
    const bool listenSuccess = listen(ip, port);

    if (listenSuccess) {
        LogMsg(tr("Torrent streaming server: Now listening on IP: %1, port: %2")
               .arg(ip.toString(), QString::number(port)), Log::INFO);
    }
    else {
        LogMsg(tr("Torrent streaming server: Unable to bind to IP: %1, port: %2. Reason: %3")
               .arg(ip.toString(), QString::number(port), errorString())
               , Log::WARNING);
    }
}
