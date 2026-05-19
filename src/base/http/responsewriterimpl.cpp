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

#include "responsewriterimpl.h"

#include <algorithm>
#include <optional>
#include <variant>

#include <QAbstractSocket>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QMimeDatabase>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QSemaphore>
#include <QStringTokenizer>
#include <QStringView>
#include <QThread>

#include "base/path.h"
#include "base/utils/gzip.h"
#include "constants.h"

const qint64 CHUNK_SIZE = 256 * 1024;
const qint64 MAX_BUFFER_SIZE = 1024 * 1024;

namespace
{
    struct Range
    {
        qint64 start = 0;
        qint64 end = -1;
    };

    // RangeRequest is represented by one of the following:
    // - Range: range [start, end (optional)]
    // - qint64: suffix length
    using RangeRequest = std::variant<Range, qint64>;

    QString httpDate()
    {
        // [RFC 7231] 7.1.1.1. Date/Time Formats
        // example: "Sun, 06 Nov 1994 08:49:37 GMT"

        return QLocale::c().toString(QDateTime::currentDateTimeUtc(), u"ddd, dd MMM yyyy HH:mm:ss"_s)
            .append(u" GMT");
    }

    bool acceptsGzipEncoding(QStringView encodings)
    {
        // [rfc7231] 5.3.4. Accept-Encoding

        const auto matchEncoding = [](const QStringView encodingEntry, const QStringView encoding) -> bool
        {
            if (!encodingEntry.startsWith(encoding))
                return false;

            // without quality values
            if (encodingEntry == encoding)
                return true;

            // [rfc7231] 5.3.1. Quality Values
            const QStringView qualityStr = encodingEntry.sliced(encoding.size() + 1).trimmed();  // ex. skip over "gzip;"
            if (!qualityStr.startsWith(u"q="))
                return false;

            bool ok = false;
            const double qvalue = qualityStr.sliced(2).toDouble(&ok);
            if (ok && (qvalue > 0))
                return true;

            return false;
        };

        return std::ranges::any_of(qTokenize(encodings, u',', Qt::SkipEmptyParts)
                , [&matchEncoding](QStringView encodingEntry)
        {
            encodingEntry = encodingEntry.trimmed();
            return matchEncoding(encodingEntry, u"gzip") || matchEncoding(encodingEntry, u"*");
        });
    }

    bool needCompressContent(const Http::Response &response, const Http::Request &request)
    {
        if (!acceptsGzipEncoding(request.headers.value(u"accept-encoding"_s)))
            return false;

        // for very small files, compressing them only wastes cpu cycles
        if (response.content.size() <= 1024)  // 1 kb
            return false;

        // filter out known hard-to-compress types
        const QString contentType = response.headers[Http::HEADER_CONTENT_TYPE];
        if ((contentType == Http::CONTENT_TYPE_GIF) || (contentType == Http::CONTENT_TYPE_JPEG)
                || (contentType == Http::CONTENT_TYPE_PNG) || (contentType == Http::CONTENT_TYPE_WEBP))
        {
            return false;
        }

        return true;
    }

    std::optional<RangeRequest> parseRangeHeader(const QStringView rangeHeader)
    {
        Q_ASSERT(!rangeHeader.isEmpty());
        if (rangeHeader.isEmpty()) [[unlikely]]
            return std::nullopt;

        const QRegularExpression rangeHeaderPattern {u"^bytes=(((?<rangestart>\\d+)-(?<rangeend>\\d*))|(-(?<suffixlength>\\d+)))$"_s};
        const QRegularExpressionMatch match = rangeHeaderPattern.matchView(rangeHeader);
        if (!match.hasMatch())
            return std::nullopt;

        if (match.hasCaptured(u"rangestart"))
        {
            const QStringView startStr = match.capturedView(u"rangestart");
            const QStringView endStr = match.capturedView(u"rangeend");

            const qint64 start = startStr.toLongLong();
            const qint64 end = endStr.isEmpty() ? -1 : endStr.toLongLong();
            if ((end != -1) && (end < start))
                return std::nullopt;

            return Range {.start = start, .end = end};
        }

        // suffix length mode
        const QStringView suffixlengthStr = match.capturedView(u"suffixlength");
        const qint64 suffixlength = suffixlengthStr.toLongLong();
        if (suffixlength <= 0)
            return std::nullopt;

        return suffixlength;
    }

    QByteArray serializeResponseHead(const Http::ResponseStatus &responseStatus, const Http::HeaderMap &responseHeaders)
    {
        QByteArray buf;
        buf.reserve(1024);

        const auto serializeHeader = [&buf](const QString &name, const QString &value)
        {
            buf.append(name.toLatin1())
                .append(": ")
                .append(value.toLatin1())
                .append(Http::CRLF);
        };

        // Status Line
        buf.append("HTTP/1.1 ")  // TODO: depends on request
            .append(QByteArray::number(responseStatus.code))
            .append(' ')
            .append(responseStatus.text.toLatin1())
            .append(Http::CRLF);

        // Header Fields
        serializeHeader(Http::HEADER_DATE, httpDate());
        if (!responseHeaders.contains(Http::HEADER_CONNECTION))
            serializeHeader(Http::HEADER_CONNECTION, u"keep-alive"_s);
        for (const auto &[headerName, headerValue] : responseHeaders.asKeyValueRange())
            serializeHeader(headerName, headerValue);

        // the first empty line
        buf.append(Http::CRLF);

        return buf;
    }

    QByteArray serializeResponse(const Http::Response &response, const Http::Request &request)
    {
        const qsizetype responseContentSize = response.content.size();

        Q_ASSERT(responseContentSize >= 0);
        Q_ASSERT(!response.headers.contains(Http::HEADER_CONTENT_LENGTH)); // shouldn't be explicitly set

        Http::HeaderMap responseHeaders = response.headers;
        QByteArray responseContent = response.content;

        if ((responseContentSize > 0) && needCompressContent(response, request))
        {
            // try compressing
            bool ok = false;
            const QByteArray compressedData = Utils::Gzip::compress(responseContent, 6, &ok);
            if (ok)
            {
                // "Content-Encoding: gzip\r\n" is 24 bytes long
                if ((compressedData.size() + 24) < responseContent.size())
                {
                    responseContent = compressedData;
                    responseHeaders.insert(Http::HEADER_CONTENT_ENCODING, u"gzip"_s);
                }
            }
        }

        responseHeaders.insert(Http::HEADER_CONTENT_LENGTH, QString::number(responseContent.size()));

        QByteArray data = serializeResponseHead(response.status, responseHeaders);
        if (request.method != Http::HEADER_REQUEST_METHOD_HEAD)
            data.append(responseContent);

        return data;
    }

    qint64 adjustDataSize(const qint64 dataSize)
    {
        return CHUNK_SIZE * (dataSize / CHUNK_SIZE);
    }
}

class Http::ResponseWriterImpl::Worker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Worker)

public:
    Worker(const Path &filePath, const Http::Request &request, const HeaderMap &headers);

public:
    void run();
    QByteArray fetchData(qint64 maxSize);
    void abort();

signals:
    void dataReady();
    void finished();
    void failed();

private:
    bool isAborted();

    Path m_filePath;
    Http::Request m_request;
    Http::HeaderMap m_headers;

    QFile *m_file = nullptr;
    qint64 m_remainingSize = -1;

    bool m_isAborted = false;
    QReadWriteLock m_abortedStateLock;

    QByteArray m_buffer;
    QReadWriteLock m_bufferLock;
    QSemaphore m_bufferSemaphore;
    bool m_isBufferRead = false;
};

Http::ResponseWriterImpl::ResponseWriterImpl(QAbstractSocket *socket, QObject *parent)
    : ResponseWriter(parent)
    , m_socket {socket}
{
}

Http::ResponseWriterImpl::~ResponseWriterImpl()
{
    if (m_isWritingContent)
        m_asyncWorker->abort();
}

void Http::ResponseWriterImpl::prepare(const Request &request)
{
    Q_ASSERT(!m_isWritingContent);

    m_isAsyncWorkerFinished = false;
    m_isFinished = false;
    m_request = request;
}

void Http::ResponseWriterImpl::setResponse(const Response &response)
{
    Q_ASSERT(!m_isFinished && !m_isWritingContent);
    if (m_isFinished || m_isWritingContent) [[unlikely]]
        return;

    const QByteArray data = serializeResponse(response, m_request);
    m_socket->write(data);

    finish();
}

void Http::ResponseWriterImpl::streamFile(const Path &filePath, const HeaderMap &headers)
{
    Q_ASSERT(!m_isFinished && !m_isWritingContent);
    if (m_isFinished || m_isWritingContent) [[unlikely]]
        return;

    m_asyncWorker = new Worker(filePath, m_request, headers);
    m_workerThread = new QThread;
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    m_asyncWorker->moveToThread(m_workerThread);

    connect(m_socket, &QAbstractSocket::bytesWritten, this, [this]
    {
        if (m_isFinished)
            return;

        const qint64 bufferedDataSize = m_socket->bytesToWrite();
        const qint64 allowedDataSizeToWrite = adjustDataSize(MAX_BUFFER_SIZE - bufferedDataSize);
        if (allowedDataSizeToWrite <= 0)
            return;

        const QByteArray dataToWrite = m_asyncWorker->fetchData(allowedDataSizeToWrite);
        if (!dataToWrite.isEmpty())
        {
            writeData(dataToWrite);
        }
        else if (m_isAsyncWorkerFinished)
        {
            m_asyncWorker->deleteLater();
            m_workerThread->quit();
            finish();
        }
    });

    connect(m_asyncWorker, &Worker::dataReady, this, [this]
    {
        // If a queued connection is disconnected, already scheduled events
        // may still be delivered, causing the receiver to be called after
        // the connection is disconnected.
        if (m_isAsyncWorkerFinished)
            return;

        const qint64 bufferedDataSize = m_socket->bytesToWrite();
        const qint64 allowedDataSizeToWrite = adjustDataSize(MAX_BUFFER_SIZE - bufferedDataSize);
        if (allowedDataSizeToWrite <= 0)
            return;

        const QByteArray dataToWrite = m_asyncWorker->fetchData(allowedDataSizeToWrite);
        if (!dataToWrite.isEmpty())
            writeData(dataToWrite);
    });

    connect(m_asyncWorker, &Worker::finished, this, [this]
    {
        m_asyncWorker->disconnect(this);
        m_isAsyncWorkerFinished = true;
    });

    connect(m_asyncWorker, &Worker::failed, this, [this]
    {
        m_asyncWorker->disconnect(this);
        m_asyncWorker->deleteLater();
        m_workerThread->quit();

        if (m_socket && m_socket->isOpen())
            m_socket->close();
    });

    m_workerThread->start();
    QMetaObject::invokeMethod(m_asyncWorker, &Worker::run);
    m_isWritingContent = true;
}

void Http::ResponseWriterImpl::finish()
{
    if (m_isFinished)
        return;

    m_isWritingContent = false;
    m_isFinished = true;
    emit finished();
}

bool Http::ResponseWriterImpl::isFinished() const
{
    return m_isFinished;
}

void Http::ResponseWriterImpl::writeData(const QByteArray &data)
{
    Q_ASSERT(!data.isEmpty());
    if (data.isEmpty()) [[unlikely]]
        return;

    if (!m_socket)
        return;

    const qint64 bytesWritten = m_socket->write(data);
    if (bytesWritten < 0)
        m_asyncWorker->abort();

    const qint64 bytesToWrite = m_socket->bytesToWrite();
    if (bytesToWrite >= MAX_BUFFER_SIZE)
        m_socket->flush();
}

Http::ResponseWriterImpl::Worker::Worker(const Path &filePath, const Request &request, const HeaderMap &headers)
    : m_filePath {filePath}
    , m_request {request}
    , m_headers {headers}
    , m_bufferSemaphore {MAX_BUFFER_SIZE}
{
    Q_ASSERT(filePath.isValid());

    m_buffer.reserve(MAX_BUFFER_SIZE);
}

void Http::ResponseWriterImpl::Worker::run()
{
    std::optional<RangeRequest> rangeRequest;
    if (const auto it = m_request.headers.constFind(Http::HEADER_RANGE); it != m_request.headers.constEnd())
    {
        const std::optional<RangeRequest> parseRangeResult = parseRangeHeader(*it);
        if (!parseRangeResult)
        {
            const QWriteLocker locker {&m_bufferLock};
            m_buffer = serializeResponse({.status = {.code = 400, .text = u"Bad Request"_s}}, {});
            emit dataReady();
            emit finished();
            return;
        }

        rangeRequest = parseRangeResult;
    }

    m_file = new QFile(m_filePath.data(), this);

    if (!m_file->open(QIODevice::ReadOnly))
    {
        const QWriteLocker locker {&m_bufferLock};
        m_buffer = serializeResponse({.status = {.code = 500, .text = u"Internal Server Error"_s}, .content = m_file->errorString().toUtf8()}, {});
        emit dataReady();
        emit finished();
        return;
    }

    const qint64 fileSize = m_file->size();
    m_remainingSize = fileSize;
    qint64 offset = 0;
    if (rangeRequest)
    {
        qint64 rangeEnd = -1;
        if (std::holds_alternative<qint64>(*rangeRequest)) // suffix length mode
        {
            const auto suffixLength = std::get<qint64>(*rangeRequest);
            Q_ASSERT(suffixLength >= 0); // `suffixLength` is actually unsigned

            m_remainingSize -= suffixLength;
            offset = fileSize - m_remainingSize;
            rangeEnd = fileSize - 1;
        }
        else
        {
            const auto range = std::get<Range>(*rangeRequest);
            offset = range.start;
            if (range.end >= 0)
            {
                rangeEnd = range.end;
                m_remainingSize = (rangeEnd + 1) - offset;
            }
            else
            {
                rangeEnd = fileSize - 1;
                m_remainingSize = fileSize - offset;
            }
        }

        if ((offset < 0) || (m_remainingSize < 0) || ((offset + m_remainingSize) > fileSize))
        {
            const QWriteLocker locker {&m_bufferLock};
            m_buffer = serializeResponse({
                    .status = {.code = 416, .text = u"Range Not Satisfiable"_s},
                    .headers = {{HEADER_CONTENT_RANGE, u"bytes */%1"_s.arg(QString::number(fileSize))}}}
                    , {});
            emit dataReady();
            emit finished();
            return;
        }

        m_headers.insert(HEADER_CONTENT_RANGE, u"bytes %1-%2/%3"_s
                .arg(QString::number(offset), QString::number(rangeEnd), QString::number(fileSize)));

        if ((offset > 0) && !m_file->seek(offset))
        {
            const QWriteLocker locker {&m_bufferLock};
            m_buffer = serializeResponse({.status = {.code = 500, .text = u"Internal Server Error"_s}, .content = m_file->errorString().toUtf8()}, {});
            emit dataReady();
            emit finished();
            return;
        }
    }

    const Http::ResponseStatus responseStatus = (fileSize > m_remainingSize)
            ? Http::ResponseStatus {.code = 206, .text = u"Partial Content"_s}
            : Http::ResponseStatus {.code = 200, .text = u"OK"_s};

    m_headers.insert(HEADER_CONTENT_LENGTH, QString::number(m_remainingSize));
    m_headers.insert(HEADER_ACCEPT_RANGES, u"bytes"_s);
    m_headers.insert(HEADER_CONTENT_TYPE, QMimeDatabase().mimeTypeForFile(m_filePath.data()).name());
    m_headers.insert(HEADER_CONTENT_DISPOSITION, u"attachment; filename=\"%1\""_s.arg(m_filePath.filename()));

    {
        const QWriteLocker locker {&m_bufferLock};
        m_buffer = serializeResponseHead(responseStatus, m_headers);
        m_bufferSemaphore.acquire(m_buffer.size());
        emit dataReady();
    }

    if (m_request.method == HEADER_REQUEST_METHOD_HEAD)
    {
        emit finished();
        return;
    }

    while (!isAborted() && (m_remainingSize > 0))
    {
        const qint64 sizeToRead = std::min(CHUNK_SIZE, m_remainingSize);

        m_bufferSemaphore.acquire(CHUNK_SIZE);
        const QByteArray chunk = m_file->read(sizeToRead);
        if (chunk.isEmpty())
        {
            m_bufferSemaphore.release(CHUNK_SIZE);
            abort();
            return;
        }

        const QWriteLocker locker {&m_bufferLock};

        const qint64 chunkSize = chunk.size();
        m_bufferSemaphore.release(CHUNK_SIZE - chunkSize);
        m_buffer.append(chunk);
        if (m_isBufferRead)
        {
            m_isBufferRead = false;
            emit dataReady();
        }

        m_remainingSize -= chunkSize;
    }

    if (m_remainingSize == 0)
        emit finished();
}

QByteArray Http::ResponseWriterImpl::Worker::fetchData(const qint64 maxSize)
{
    const QReadLocker locker {&m_bufferLock};
    const qint64 sizeToFetch = std::min(maxSize, m_buffer.size());
    const QByteArray data = m_buffer.first(sizeToFetch);
    m_buffer.remove(0, sizeToFetch);
    m_bufferSemaphore.release(sizeToFetch);
    m_isBufferRead = true;
    return data;
}

void Http::ResponseWriterImpl::Worker::abort()
{
    if (isAborted())
        return;

    const QWriteLocker locker {&m_abortedStateLock};
    m_isAborted = true;

    emit failed();
}

bool Http::ResponseWriterImpl::Worker::isAborted()
{
    const QReadLocker locker {&m_abortedStateLock};
    return m_isAborted;
}

#include "responsewriterimpl.moc"
