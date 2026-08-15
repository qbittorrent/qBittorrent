/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "asyncfilesender.h"

#include <algorithm>
#include <optional>
#include <variant>

#include <QAbstractSocket>
#include <QFile>
#include <QMimeDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QString>
#include <QThread>

#include "constants.h"
#include "rangerequest.h"
#include "response.h"
#include "responseserialization.h"

using namespace Qt::StringLiterals;

const qint64 CHUNK_SIZE = 256 * 1024;
const qint64 MAX_BUFFER_SIZE = 1024 * 1024;

namespace
{
    qint64 adjustDataSize(const qint64 dataSize)
    {
        return CHUNK_SIZE * (dataSize / CHUNK_SIZE);
    }
}

class Http::AsyncFileSender::DataPipe final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DataPipe)

public:
    explicit DataPipe(QObject *parent = nullptr)
        : QObject(parent)
        , m_bufferSemaphore {MAX_BUFFER_SIZE}
    {
        m_buffer.reserve(MAX_BUFFER_SIZE);
    }

    struct ReadResult
    {
        QByteArray data;
        bool atEnd = false;
    };

    void write(const QByteArray &data, const bool close = true)
    {
        const qsizetype dataSize = data.size();
        m_bufferSemaphore.acquire(dataSize);

        const QMutexLocker locker {&m_bufferMutex};

        // it can be closed while waiting for free buffer space
        if (m_isClosed)
        {
            m_bufferSemaphore.release(dataSize);
            return;
        }

        m_buffer.append(data);
        m_isClosed = close;

        if (!m_hasNewData)
        {
            m_hasNewData = true;
            emit readyRead();
        }
    }

    ReadResult read(const qint64 maxSize)
    {
        const QMutexLocker locker {&m_bufferMutex};
        const auto sizeToFetch = std::min<qint64>(maxSize, m_buffer.size());
        const QByteArray data = m_buffer.first(sizeToFetch);
        m_buffer.remove(0, sizeToFetch);
        m_bufferSemaphore.release(sizeToFetch);
        m_hasNewData = false;
        return {.data = data, .atEnd = (m_isClosed && m_buffer.isEmpty())};
    }

signals:
    void readyRead();

private:
    QByteArray m_buffer;
    QMutex m_bufferMutex;
    QSemaphore m_bufferSemaphore;
    bool m_hasNewData = false;
    bool m_isClosed = false;
};

class Http::AsyncFileSender::Worker final : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Worker)

public:
    Worker(const Path &filePath, const Request &request, const HeaderMap &responseHeaders
           , std::shared_ptr<DataPipe> dataPipe)
        : m_filePath {filePath}
        , m_request {request}
        , m_responseHeaders {responseHeaders}
        , m_dataPipe {std::move(dataPipe)}
    {
        Q_ASSERT(filePath.isValid());
    }

signals:
    void failed();

private:
    bool initializeRangeRequest(const RangeRequest &rangeRequest, QFile &file, qint64 &size, qint64 &offset)
    {
        const qint64 fileSize = file.size();

        qint64 rangeEnd = -1;
        if (std::holds_alternative<qint64>(rangeRequest)) // suffix length mode
        {
            const auto suffixLength = std::get<qint64>(rangeRequest);
            Q_ASSERT(suffixLength >= 0); // `suffixLength` is actually unsigned

            size = suffixLength;
            offset = fileSize - size;
            rangeEnd = fileSize - 1;
        }
        else
        {
            const auto range = std::get<Range>(rangeRequest);
            offset = range.start;
            if (range.end >= 0)
            {
                rangeEnd = range.end;
                size = (rangeEnd + 1) - offset;
            }
            else
            {
                rangeEnd = fileSize - 1;
                size = fileSize - offset;
            }
        }

        if ((offset < 0) || (size <= 0) || ((offset + size) > fileSize))
        {
            const QByteArray data = serializeResponse(
                    {.status = {.code = 416, .text = u"Range Not Satisfiable"_s},
                            .headers = {{HEADER_CONTENT_RANGE, u"bytes */%1"_s.arg(QString::number(fileSize))}}}
                    , m_request);
            m_dataPipe->write(data);
            return false;
        }

        m_responseHeaders.insert(HEADER_CONTENT_RANGE, u"bytes %1-%2/%3"_s
                .arg(QString::number(offset), QString::number(rangeEnd), QString::number(fileSize)));

        if ((offset > 0) && !file.seek(offset))
        {
            const QByteArray data = serializeResponse(
                    {.status = {.code = 500, .text = u"Internal Server Error"_s},
                            .content = file.errorString().toUtf8()}
                    , m_request);
            m_dataPipe->write(data);
            return false;
        }

        return true;
    }

    void run() override
    {
        std::optional<RangeRequest> rangeRequest;
        if (const QString rangeHeader = m_request.headers.value(HEADER_RANGE); !rangeHeader.isEmpty())
        {
            const std::optional<RangeRequest> parseRangeResult = parseRangeHeader(rangeHeader);
            if (!parseRangeResult)
            {
                const QByteArray data = serializeResponse({.status = {.code = 400, .text = u"Bad Request"_s}}, m_request);
                m_dataPipe->write(data);
                return;
            }

            rangeRequest = parseRangeResult;
        }

        QFile file {m_filePath.data()};

        if (!file.open(QIODevice::ReadOnly))
        {
            const QByteArray data = serializeResponse(
                    {.status = {.code = 500, .text = u"Internal Server Error"_s}, .content = file.errorString().toUtf8()}
                    , m_request);
            m_dataPipe->write(data);
            return;
        }

        const qint64 fileSize = file.size();
        qint64 remainingSize = fileSize;
        qint64 offset = 0;
        if (rangeRequest)
        {
            if (!initializeRangeRequest(*rangeRequest, file, remainingSize, offset))
                return;
        }

        const Http::ResponseStatus responseStatus = rangeRequest
                ? Http::ResponseStatus {.code = 206, .text = u"Partial Content"_s}
                : Http::ResponseStatus {.code = 200, .text = u"OK"_s};

        m_responseHeaders.insert(HEADER_CONTENT_LENGTH, QString::number(remainingSize));
        m_responseHeaders.insert(HEADER_ACCEPT_RANGES, u"bytes"_s);
        m_responseHeaders.insert(HEADER_CONTENT_TYPE, QMimeDatabase().mimeTypeForFile(m_filePath.data()).name());
        m_responseHeaders.insert(HEADER_CONTENT_DISPOSITION, u"attachment; filename=\"%1\""_s.arg(m_filePath.filename()));

        const QByteArray headData = serializeResponseHead(responseStatus, m_responseHeaders);
        if (m_request.method == HEADER_REQUEST_METHOD_HEAD)
            remainingSize = 0;
        m_dataPipe->write(headData, (remainingSize == 0));

        while ((remainingSize > 0) && !isInterruptionRequested())
        {
            const qint64 sizeToRead = std::min(CHUNK_SIZE, remainingSize);
            const QByteArray chunk = file.read(sizeToRead);
            if (chunk.isEmpty())
            {
                emit failed();
                return;
            }

            const qint64 chunkSize = chunk.size();
            remainingSize -= chunkSize;
            m_dataPipe->write(chunk, (remainingSize == 0));
        }
    }

    Path m_filePath;
    Request m_request;
    HeaderMap m_responseHeaders;
    std::shared_ptr<DataPipe> m_dataPipe;
};

Http::AsyncFileSender::AsyncFileSender(const Request &request, const Path &filePath
        , const HeaderMap &headers, QAbstractSocket *socket, QObject *parent)
    : QObject(parent)
    , m_request {request}
    , m_filePath {filePath}
    , m_headers {headers}
    , m_socket {socket}
{
    Q_ASSERT(socket);
}

Http::AsyncFileSender::~AsyncFileSender()
{
    if (m_worker)
    {
        m_worker->requestInterruption();
        if (m_dataPipe)
        {
            // Need to release the data pipe in case the working thread is waiting for write.
            m_dataPipe->read(MAX_BUFFER_SIZE);
        }
    }
}

Http::AsyncFileSender::State Http::AsyncFileSender::state() const
{
    return m_state;
}

void Http::AsyncFileSender::run()
{
    Q_ASSERT(m_state == State::Ready);
    if (m_state != State::Ready) [[unlikely]]
        return;

    m_dataPipe = std::make_shared<DataPipe>();
    m_worker = new Worker(m_filePath, m_request, m_headers, m_dataPipe);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    // `Worker` (aka `QThread`) instance lives in the main thread that instantiated it,
    // not in the new thread that calls `run()`. So there is no problem having `std::shared_ptr<DataPipe>`
    // as member of `Worker`. Even if `AsyncFileSender` is destroyed while the `Worker` is still running,
    // the last `shared_ptr` reference will be released from the main thread since `QObject::deleteLater`
    // will be invoked in the thread the `Worker` lives in.

    connect(m_socket, &QAbstractSocket::bytesWritten, this, &AsyncFileSender::processNextData);
    connect(m_dataPipe.get(), &DataPipe::readyRead, this, &AsyncFileSender::processNextData);
    connect(m_worker, &Worker::failed, this, &AsyncFileSender::fail);

    m_state = State::Running;
    m_worker->start();
}

void Http::AsyncFileSender::processNextData()
{
    if (m_state != State::Running)
        return;

    const qint64 bufferedDataSize = m_socket->bytesToWrite();
    const qint64 allowedDataSizeToWrite = adjustDataSize(MAX_BUFFER_SIZE - bufferedDataSize);
    if (allowedDataSizeToWrite <= 0)
        return;

    const auto [dataToWrite, atEnd] = m_dataPipe->read(allowedDataSizeToWrite);
    if (!dataToWrite.isEmpty())
    {
        const qint64 bytesWritten = m_socket->write(dataToWrite);
        if (bytesWritten < 0)
        {
            m_worker->requestInterruption();
            // Need to release the data pipe in case the working thread is waiting for write.
            m_dataPipe->read(MAX_BUFFER_SIZE);
            fail();
            return;
        }

        const qint64 bytesToWrite = m_socket->bytesToWrite();
        if (bytesToWrite >= MAX_BUFFER_SIZE)
            m_socket->flush();
    }

    if (atEnd)
        finish();
}

void Http::AsyncFileSender::fail()
{
    Q_ASSERT(m_state == State::Running);
    if (m_state != State::Running) [[unlikely]]
        return;

    m_state = State::Failed;
    emit failed();
}

void Http::AsyncFileSender::finish()
{
    Q_ASSERT(m_state == State::Running);
    if (m_state != State::Running) [[unlikely]]
        return;

    m_state = State::Finished;
    emit finished();
}

#include "asyncfilesender.moc"
