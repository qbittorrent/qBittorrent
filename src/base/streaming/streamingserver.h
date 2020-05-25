
#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>

#include "../indexrange.h"
#include "../http/types.h"

class HttpSocket : public QTcpSocket
{
    Q_OBJECT

public:
    HttpSocket(QObject *parent = nullptr);

    void sendStatus(const Http::ResponseStatus &status);
    void sendHeaders(const Http::HeaderMap &headers);
    void send(const QByteArray &data);

    Http::Request request() const;
    qint64 inactivityTime() const;

signals:
    void readyRequest();

private slots:
    void read();

private:
    QElapsedTimer m_idleTimer;
    QByteArray m_receivedData;
    Http::Request m_request;
    bool isServing;
};

class StreamingServer : public QTcpServer
{
public:
    using QTcpServer::QTcpServer;

private:
    virtual void doRequest(HttpSocket *socket) = 0;

    void incomingConnection(qintptr socketDescriptor) override;
};