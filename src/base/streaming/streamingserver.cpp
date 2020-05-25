#include "streamingserver.h"
#include "../http/requestparser.h"
#include "../http/responsegenerator.h"
#include "../logger.h"

HttpSocket::HttpSocket(QObject *parent)
    : QTcpSocket(parent), isServing(false)
{
    connect(this, &HttpSocket::readyRead, this, &HttpSocket::read);
}

void HttpSocket::read()
{
    using namespace Http;

    m_idleTimer.restart();
    m_receivedData.append(readAll());
    Q_ASSERT(!isServing);

    while (!m_receivedData.isEmpty()) {
        const RequestParser::ParseResult result = RequestParser::parse(m_receivedData);

        switch (result.status) {
        case RequestParser::ParseStatus::Incomplete: {
                const long bufferLimit = RequestParser::MAX_CONTENT_SIZE * 1.1; // some margin for headers
                if (m_receivedData.size() > bufferLimit) {
                    Logger::instance()->addMessage(tr("Http request size exceeds limiation, closing socket. Limit: %1, IP: %2")
                                                   .arg(bufferLimit)
                                                   .arg(peerAddress().toString()),
                                                   Log::WARNING);

                    sendStatus({413, "Payload Too Large"});
                    sendHeaders({{HEADER_CONNECTION, "close"}});

                    close();
                }
            }
            return;

        case RequestParser::ParseStatus::BadRequest: {
                Logger::instance()->addMessage(tr("Bad Http request, closing socket. IP: %1")
                                               .arg(peerAddress().toString()),
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
    m_idleTimer.restart();
    write(QString("HTTP/%1 %2 %3")
          .arg("1.1",     // TODO: depends on request
               QString::number(status.code),
               status.text)
          .toLatin1()
          .append(Http::CRLF));
}

void HttpSocket::sendHeaders(const Http::HeaderMap &headers)
{
    m_idleTimer.restart();

    const auto writeHeader = [this](const QString &key, const QString &value) {
                                 write(QString::fromLatin1("%1: %2").arg(key, value).toLatin1().append(Http::CRLF));
                             };

    for (auto i = headers.constBegin(); i != headers.constEnd(); ++i)
        writeHeader(i.key(), i.value());

    if (!headers.contains(Http::HEADER_DATE))
        writeHeader(Http::HEADER_DATE, Http::httpDate());
}

void HttpSocket::send(const QByteArray &data)
{
    m_idleTimer.restart();

    write(data);
}

Http::Request HttpSocket::request() const
{
    return m_request;
}

qint64 HttpSocket::inactivityTime() const
{
    return m_idleTimer.elapsed();
}

void StreamingServer::incomingConnection(qintptr socketDescriptor)
{
    HttpSocket *socket = new HttpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }

    connect(socket, &HttpSocket::readyRequest, this, [this, socket]() {
        doRequest(socket);
    });
}
