
#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>

#include "../indexrange.h"
#include "../http/types.h"

class HttpSocket : public QObject
{
    Q_OBJECT

public:
    HttpSocket(QAbstractSocket *socket);

    void sendStatus(const Http::ResponseStatus &status);
    void sendHeaders(const Http::HeaderMap &headers);
    void send(const QByteArray &data);

    Http::Request request() const;
    qint64 inactivityTime() const;

    void close();

signals:
    void readyRequest();

private slots:
    void read();

private:
    QElapsedTimer m_idleTimer;
    QByteArray m_receivedData;
    Http::Request m_request;
    QAbstractSocket *m_socket;
    bool isServing;
};

class StreamingServer : public QTcpServer
{
public:
    using QTcpServer::QTcpServer;

    void start();
private:
    virtual void doRequest(HttpSocket *socket) = 0;

    void incomingConnection(qintptr socketDescriptor) override;
};
