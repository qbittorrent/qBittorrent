/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "server.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QIODevice>
#include <QLocale>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStringList>
#include <QTcpSocket>
#include <QUrlQuery>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/password.h"

namespace
{
    using namespace Qt::Literals::StringLiterals;

    constexpr qint64 BLOCK_SIZE = 1024 * 1024;
    constexpr qint64 MAX_REQUEST_SIZE = 32 * 1024;
    constexpr qint64 WRITE_BUFFER_LIMIT = 4 * BLOCK_SIZE;

    QByteArray httpDate()
    {
        return QLocale::c().toString(QDateTime::currentDateTimeUtc()
                , u"ddd, dd MMM yyyy HH:mm:ss 'GMT'"_s).toLatin1();
    }

    bool tokenMatches(const QString &provided, const QString &expected)
    {
        const QByteArray providedBytes = provided.toUtf8();
        const QByteArray expectedBytes = expected.toUtf8();
        return !expected.isEmpty() && Utils::Password::slowEquals(providedBytes, expectedBytes);
    }

    QByteArray dispositionHeader(const QString &fileName)
    {
        QString fallback = fileName;
        fallback.replace(QRegularExpression(uR"([\x00-\x1f\x7f"\\/])"_s), u"_"_s);
        const QByteArray encoded = QUrl::toPercentEncoding(fileName);
        return "Content-Disposition: inline; filename=\"" + fallback.toLatin1()
                + "\"; filename*=UTF-8''" + encoded;
    }
}

namespace Streaming
{
    Server *Server::m_instance = nullptr;

    void Server::initInstance()
    {
        if (!m_instance)
            m_instance = new Server;
    }

    void Server::freeInstance()
    {
        delete m_instance;
        m_instance = nullptr;
    }

    Server *Server::instance()
    {
        return m_instance;
    }

    Server::Server()
    {
        m_pollTimer.setInterval(100);
        m_pollTimer.setSingleShot(true);

        Preferences *const preferences = Preferences::instance();
        if (preferences->streamingToken().isEmpty())
            preferences->setStreamingToken(makeToken());

        connect(preferences, &Preferences::changed, this, &Server::configure);
        connect(&m_listener, &QTcpServer::newConnection, this, &Server::acceptConnections);
        connect(&m_pollTimer, &QTimer::timeout, this, &Server::pumpStream);

        BitTorrent::Session *const session = BitTorrent::Session::instance();
        connect(session, &BitTorrent::Session::torrentAboutToBeRemoved, this, [this](BitTorrent::Torrent *torrent)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });
        connect(session, &BitTorrent::Session::torrentSavePathChanged, this, [this](BitTorrent::Torrent *torrent)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });
        connect(session, &BitTorrent::Session::torrentSavingModeChanged, this, [this](BitTorrent::Torrent *torrent)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });
        connect(session, &BitTorrent::Session::torrentContentFileRenamed, this
                , [this](BitTorrent::Torrent *torrent, int, const Path &)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });
        connect(session, &BitTorrent::Session::torrentContentFolderRenamed, this
                , [this](BitTorrent::Torrent *torrent, const Path &, const Path &, const QHash<int, Path> &)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });
        connect(session, &BitTorrent::Session::torrentIOError, this, [this](BitTorrent::Torrent *torrent)
        {
            if (torrent == m_activeTorrent)
                cancelActiveStream();
        });

        configure();
    }

    Server::~Server()
    {
        m_listener.close();
        cancelActiveStream();
        const QList<QTcpSocket *> sockets = m_requestBuffers.keys();
        m_requestBuffers.clear();
        for (QTcpSocket *socket : sockets)
            socket->abort();
    }

    bool Server::isListening() const
    {
        return m_listener.isListening();
    }

    QUrl Server::streamUrl(const BitTorrent::TorrentID &torrentID, const int fileIndex) const
    {
        if (!isListening() || (fileIndex < 0) || (Preferences::instance()->streamingToken().size() < 32))
            return {};

        QHostAddress address = m_listener.serverAddress();
        if (address.isEqual(QHostAddress::Any, QHostAddress::ConvertUnspecifiedAddress))
            return {};

        QUrl url;
        url.setScheme(u"http"_s);
        url.setHost(address.toString());
        url.setPort(m_listener.serverPort());
        url.setPath(u"/stream/%1/%2"_s.arg(torrentID.toString(), QString::number(fileIndex)));
        QUrlQuery query;
        query.addQueryItem(u"token"_s, Preferences::instance()->streamingToken());
        url.setQuery(query);
        return url;
    }

    void Server::configure()
    {
        m_listener.close();
        cancelActiveStream();
        const QList<QTcpSocket *> sockets = m_requestBuffers.keys();
        m_requestBuffers.clear();
        for (QTcpSocket *socket : sockets)
            socket->abort();

        const Preferences *const preferences = Preferences::instance();
        if (!preferences->isStreamingEnabled())
            return;

        if (preferences->streamingToken().size() < 32)
        {
            LogMsg(tr("Streaming server cannot listen without a valid access token"), Log::WARNING);
            return;
        }

        const QString configuredAddress = preferences->streamingAddress();
        if (configuredAddress.isEmpty())
        {
            LogMsg(tr("Streaming server cannot listen: the address is empty"), Log::WARNING);
            return;
        }

        QHostAddress address;
        if (!address.setAddress(configuredAddress))
        {
            LogMsg(tr("Streaming server cannot listen: invalid address %1").arg(configuredAddress), Log::WARNING);
            return;
        }

        if (address.isEqual(QHostAddress::Any, QHostAddress::ConvertUnspecifiedAddress))
        {
            LogMsg(tr("Streaming server cannot use a wildcard address; configure an explicit address"), Log::WARNING);
            return;
        }

        if (!address.isLoopback() && !preferences->isStreamingLANAllowed())
        {
            LogMsg(tr("Streaming server refused a non-loopback address because LAN access is disabled"), Log::WARNING);
            return;
        }

        const quint16 port = preferences->streamingPort();
        if (port == 0)
        {
            LogMsg(tr("Streaming server cannot listen on port 0"), Log::WARNING);
            return;
        }

        if (!m_listener.listen(address, port))
        {
            LogMsg(tr("Streaming server failed to listen on %1:%2. Reason: %3")
                    .arg(address.toString(), QString::number(port), m_listener.errorString()), Log::WARNING);
        }
    }

    void Server::acceptConnections()
    {
        while (QTcpSocket *socket = m_listener.nextPendingConnection())
        {
            m_requestBuffers.insert(socket, {});
            connect(socket, &QTcpSocket::readyRead, this, &Server::processRequestData);
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]
            {
                m_requestBuffers.remove(socket);
                if (socket == m_activeSocket)
                    cancelActiveStream();
                socket->deleteLater();
            });
        }
    }

    void Server::processRequestData()
    {
        auto *const socket = qobject_cast<QTcpSocket *>(sender());
        if (!socket || !m_requestBuffers.contains(socket))
            return;

        QByteArray &buffer = m_requestBuffers[socket];
        buffer += socket->readAll();
        if (buffer.size() > MAX_REQUEST_SIZE)
        {
            m_requestBuffers.remove(socket);
            sendError(socket, 431, "Request Header Fields Too Large");
            return;
        }

        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QByteArray requestData = buffer.left(headerEnd + 4);
        m_requestBuffers.remove(socket);
        processRequest(socket, requestData);
    }

    void Server::processRequest(QTcpSocket *socket, const QByteArray &data)
    {
        QList<QByteArray> lines = data.split('\n');
        if (lines.isEmpty())
        {
            sendError(socket, 400, "Bad Request");
            return;
        }

        const QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
        if ((requestLine.size() != 3) || !requestLine[2].startsWith("HTTP/1."))
        {
            sendError(socket, 400, "Bad Request");
            return;
        }

        Request request {.method = requestLine[0], .target = requestLine[1], .headers = {}};
        for (const QByteArray &rawLine : lines)
        {
            const QByteArray line = rawLine.trimmed();
            if (line.isEmpty())
                continue;

            const qsizetype colonPos = line.indexOf(':');
            if (colonPos <= 0)
            {
                sendError(socket, 400, "Bad Request");
                return;
            }

            const QByteArray name = line.left(colonPos).trimmed().toLower();
            if (request.headers.contains(name))
            {
                sendError(socket, 400, "Bad Request");
                return;
            }
            request.headers.insert(name, line.mid(colonPos + 1).trimmed());
        }

        startResponse(socket, request);
    }

    void Server::startResponse(QTcpSocket *socket, const Request &request)
    {
        if (!isListening() || !Preferences::instance()->isStreamingEnabled())
        {
            sendError(socket, 503, "Service Unavailable");
            return;
        }

        if ((request.method != "GET") && (request.method != "HEAD"))
        {
            sendError(socket, 405, "Method Not Allowed", {"Allow: GET, HEAD"});
            return;
        }

        const QUrl url = QUrl::fromEncoded(request.target, QUrl::StrictMode);
        const QStringList segments = url.path().split(u'/', Qt::SkipEmptyParts);
        if (!url.isValid() || (segments.size() != 3) || (segments[0] != u"stream"))
        {
            sendError(socket, 404, "Not Found");
            return;
        }

        const QString token = QUrlQuery(url).queryItemValue(u"token"_s, QUrl::FullyDecoded);
        if (!tokenMatches(token, Preferences::instance()->streamingToken()))
        {
            sendError(socket, 403, "Forbidden");
            return;
        }

        bool fileIndexOK = false;
        const int fileIndex = segments[2].toInt(&fileIndexOK);
        const BitTorrent::TorrentID torrentID = BitTorrent::TorrentID::fromString(segments[1]);
        BitTorrent::Torrent *const torrent = BitTorrent::Session::instance()->getTorrent(torrentID);
        if (!fileIndexOK || (fileIndex < 0) || !torrent)
        {
            sendError(socket, 404, "Not Found");
            return;
        }

        if (!torrent->hasMetadata() || (fileIndex >= torrent->filesCount()))
        {
            sendError(socket, 409, "Conflict");
            return;
        }

        const QList<BitTorrent::DownloadPriority> priorities = torrent->filePriorities();
        if ((fileIndex >= priorities.size()) || (priorities[fileIndex] == BitTorrent::DownloadPriority::Ignored)
                || torrent->isChecking() || torrent->isMoving()
                || torrent->isErrored() || torrent->hasMissingFiles())
        {
            sendError(socket, 409, "Conflict");
            return;
        }

        const qint64 fileSize = torrent->fileSize(fileIndex);
        // actualFilePath() is qBittorrent's resolved libtorrent path. It includes
        // the .!qB suffix when append-extension mode is active for this file.
        const Path filePath = torrent->actualStorageLocation() / torrent->actualFilePath(fileIndex);
        const QFileInfo fileInfo(filePath.data());
        if ((fileSize < 0) || !fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable())
        {
            sendError(socket, 404, "Not Found");
            return;
        }

        ByteRange range {.first = 0, .last = fileSize - 1};
        const bool isPartial = request.headers.contains("range");
        if (isPartial)
        {
            const auto parsedRange = parseHttpRange(request.headers.value("range"), fileSize);
            if (!parsedRange)
            {
                sendError(socket, 416, "Range Not Satisfiable"
                        , {"Accept-Ranges: bytes", "Content-Range: bytes */" + QByteArray::number(fileSize)});
                return;
            }
            range = parsedRange.value();
        }

        const QString displayFileName = torrent->filePath(fileIndex).filename();
        const QByteArray mimeType = QMimeDatabase().mimeTypeForFile(displayFileName
                , QMimeDatabase::MatchExtension).name().toLatin1();
        QList<QByteArray> headers {
            "Accept-Ranges: bytes",
            "Content-Length: " + QByteArray::number((fileSize == 0) ? 0 : range.length()),
            "Content-Type: " + (mimeType.isEmpty() ? QByteArray("application/octet-stream") : mimeType),
            dispositionHeader(displayFileName)
        };
        if (isPartial)
        {
            headers.append("Content-Range: bytes " + QByteArray::number(range.first) + '-'
                    + QByteArray::number(range.last) + '/' + QByteArray::number(fileSize));
        }

        auto streamFile = std::make_unique<QFile>(filePath.data());
        if (!streamFile->open(QIODevice::ReadOnly))
        {
            sendError(socket, 404, "Not Found");
            return;
        }

        if ((request.method == "HEAD") || (fileSize == 0))
        {
            sendHeaders(socket, isPartial ? 206 : 200, isPartial ? "Partial Content" : "OK", headers);
            socket->disconnectFromHost();
            return;
        }

        cancelActiveStream();
        m_activeSocket = socket;
        m_activeTorrent = torrent;
        m_file = std::move(streamFile);
        m_torrentID = torrentID;
        m_fileIndex = fileIndex;
        m_fileOffset = torrent->info().fileOffset(fileIndex);
        m_fileSize = fileSize;
        m_responsePosition = range.first;
        m_responseLast = range.last;
        m_pieceLength = torrent->pieceLength();
        m_piecesCount = torrent->piecesCount();

        connect(socket, &QTcpSocket::bytesWritten, this, &Server::pumpStream);
        sendHeaders(socket, isPartial ? 206 : 200, isPartial ? "Partial Content" : "OK", headers);
        m_waitTimer.start();
        pumpStream();
    }

    void Server::pumpStream()
    {
        if (!m_activeSocket || !m_file || !validateTorrentState())
        {
            cancelActiveStream();
            return;
        }

        if (m_responsePosition > m_responseLast)
        {
            QTcpSocket *const socket = m_activeSocket;
            clearDeadlines();
            m_pollTimer.stop();
            disconnect(socket, &QTcpSocket::bytesWritten, this, &Server::pumpStream);
            m_activeSocket = nullptr;
            m_activeTorrent = nullptr;
            m_file.reset();
            m_torrentID = {};
            m_fileIndex = -1;
            m_fileOffset = 0;
            m_fileSize = 0;
            m_responsePosition = 0;
            m_responseLast = -1;
            m_pieceLength = 0;
            m_piecesCount = 0;
            socket->disconnectFromHost();
            return;
        }

        if (m_activeSocket->bytesToWrite() >= WRITE_BUFFER_LIMIT)
            return;

        const qint64 blockLength = std::min(BLOCK_SIZE, (m_responseLast - m_responsePosition + 1));
        const qint64 blockLast = m_responsePosition + blockLength - 1;
        if (!preparePieces(m_responsePosition, blockLast))
        {
            const qint64 timeout = static_cast<qint64>(Preferences::instance()->streamingWaitTimeout()) * 1000;
            if (m_waitTimer.elapsed() >= timeout)
                cancelActiveStream();
            else
                m_pollTimer.start();
            return;
        }

        if (!m_file->seek(m_responsePosition))
        {
            cancelActiveStream();
            return;
        }

        const QByteArray block = m_file->read(blockLast - m_responsePosition + 1);
        if (block.size() != (blockLast - m_responsePosition + 1))
        {
            cancelActiveStream();
            return;
        }

        if (m_activeSocket->write(block) != block.size())
        {
            cancelActiveStream();
            return;
        }

        m_responsePosition = blockLast + 1;
        m_waitTimer.restart();
        QMetaObject::invokeMethod(this, &Server::pumpStream, Qt::QueuedConnection);
    }

    void Server::sendError(QTcpSocket *socket, const int status, const QByteArray &reason
            , const QList<QByteArray> &extraHeaders)
    {
        QList<QByteArray> headers = extraHeaders;
        headers.append("Content-Length: 0");
        sendHeaders(socket, status, reason, headers);
        socket->disconnectFromHost();
    }

    void Server::sendHeaders(QTcpSocket *socket, const int status, const QByteArray &reason
            , const QList<QByteArray> &headers)
    {
        QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n"
                + "Date: " + httpDate() + "\r\n"
                + "Connection: close\r\n"
                + "Server: qBittorrent streaming\r\n";
        for (const QByteArray &header : headers)
            response += header + "\r\n";
        response += "\r\n";
        socket->write(response);
    }

    void Server::cancelActiveStream()
    {
        m_pollTimer.stop();
        clearDeadlines();

        if (m_activeSocket)
        {
            QTcpSocket *const socket = m_activeSocket;
            disconnect(socket, &QTcpSocket::bytesWritten, this, &Server::pumpStream);
            m_activeSocket = nullptr;
            socket->abort();
        }

        m_activeTorrent = nullptr;
        m_file.reset();
        m_torrentID = {};
        m_fileIndex = -1;
        m_fileOffset = 0;
        m_fileSize = 0;
        m_responsePosition = 0;
        m_responseLast = -1;
        m_pieceLength = 0;
        m_piecesCount = 0;
    }

    void Server::clearDeadlines()
    {
        if (m_activeTorrent)
        {
            for (const int piece : m_deadlinePieces.keys())
                m_activeTorrent->resetStreamingPieceDeadline(piece);
        }
        m_deadlinePieces.clear();
    }

    bool Server::validateTorrentState() const
    {
        if (!m_activeTorrent || (BitTorrent::Session::instance()->getTorrent(m_torrentID) != m_activeTorrent))
            return false;

        if (!m_activeTorrent->hasMetadata() || (m_fileIndex < 0) || (m_fileIndex >= m_activeTorrent->filesCount())
                || m_activeTorrent->isChecking() || m_activeTorrent->isMoving()
                || m_activeTorrent->isErrored() || m_activeTorrent->hasMissingFiles())
            return false;

        const QList<BitTorrent::DownloadPriority> priorities = m_activeTorrent->filePriorities();
        return (m_fileIndex < priorities.size()) && (priorities[m_fileIndex] != BitTorrent::DownloadPriority::Ignored);
    }

    bool Server::preparePieces(const qint64 first, const qint64 last)
    {
        const auto currentPieces = mapByteRangeToPieces(first, last, m_fileOffset, m_pieceLength, m_piecesCount);
        if (!currentPieces)
            return false;

        const qint64 readAheadBytes = static_cast<qint64>(Preferences::instance()->streamingReadAheadMiB()) * 1024 * 1024;
        const qint64 fileLast = m_fileSize - 1;
        const qint64 bytesRemaining = fileLast - last;
        const qint64 readAheadLast = (readAheadBytes >= bytesRemaining) ? fileLast : (last + readAheadBytes);
        const auto requestedPieces = mapByteRangeToPieces(first, readAheadLast, m_fileOffset, m_pieceLength, m_piecesCount);
        if (!requestedPieces)
            return false;

        QHash<int, int> nextDeadlines;
        int order = 0;
        for (int piece = requestedPieces->first; piece <= requestedPieces->last; ++piece)
        {
            if (!m_activeTorrent->havePiece(piece))
            {
                const int deadline = std::min(order, (std::numeric_limits<int>::max() / 1000)) * 1000;
                nextDeadlines.insert(piece, deadline);
                if (!m_deadlinePieces.contains(piece) || (m_deadlinePieces.value(piece) != deadline))
                    m_activeTorrent->setStreamingPieceDeadline(piece, deadline);
                ++order;
            }
        }

        for (const int piece : m_deadlinePieces.keys())
        {
            if (!nextDeadlines.contains(piece))
                m_activeTorrent->resetStreamingPieceDeadline(piece);
        }
        m_deadlinePieces = std::move(nextDeadlines);

        for (int piece = currentPieces->first; piece <= currentPieces->last; ++piece)
        {
            if (!m_activeTorrent->havePiece(piece))
                return false;
        }
        return true;
    }

    QString Server::makeToken()
    {
        return Utils::Password::generate(48);
    }
}
