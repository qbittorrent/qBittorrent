/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "helpers.h"

class QFile;
class QTcpSocket;

namespace BitTorrent
{
    class Torrent;
}

namespace Streaming
{
    class Server final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Server)

    public:
        static void initInstance();
        static void freeInstance();
        static Server *instance();

        bool isListening() const;
        QUrl streamUrl(const BitTorrent::TorrentID &torrentID, int fileIndex) const;

    private slots:
        void configure();
        void acceptConnections();
        void processRequestData();
        void pumpStream();

    private:
        Server();
        ~Server() override;

        struct Request
        {
            QByteArray method;
            QByteArray target;
            QHash<QByteArray, QByteArray> headers;
        };

        static Server *m_instance;

        void processRequest(QTcpSocket *socket, const QByteArray &data);
        void startResponse(QTcpSocket *socket, const Request &request);
        void sendError(QTcpSocket *socket, int status, const QByteArray &reason
                , const QList<QByteArray> &extraHeaders = {});
        void sendHeaders(QTcpSocket *socket, int status, const QByteArray &reason
                , const QList<QByteArray> &headers);
        void cancelActiveStream();
        void clearDeadlines();
        bool validateTorrentState() const;
        bool preparePieces(qint64 first, qint64 last);
        QString makeToken();

        QTcpServer m_listener;
        QHash<QTcpSocket *, QByteArray> m_requestBuffers;
        QPointer<QTcpSocket> m_activeSocket;
        QPointer<BitTorrent::Torrent> m_activeTorrent;
        std::unique_ptr<QFile> m_file;
        BitTorrent::TorrentID m_torrentID;
        int m_fileIndex = -1;
        qint64 m_fileOffset = 0;
        qint64 m_fileSize = 0;
        qint64 m_responsePosition = 0;
        qint64 m_responseLast = -1;
        qint64 m_pieceLength = 0;
        int m_piecesCount = 0;
        QHash<int, int> m_deadlinePieces;
        QElapsedTimer m_waitTimer;
        QTimer m_pollTimer;
    };
}
