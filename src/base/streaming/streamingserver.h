
#pragma once

#include <QTcpServer>

#include "../indexrange.h"

class StreamingServer : public QTcpServer
{
public:
    using QTcpServer::QTcpServer;

private:
    virtual void serveFile(const QString &path, IndexInterval<int64_t> range, QAbstractSocket *socket) = 0;
    void incomingConnection(qintptr socketDescriptor) override;
};