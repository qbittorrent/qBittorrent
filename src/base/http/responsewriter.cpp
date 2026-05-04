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

#include "responsewriter.h"

#include <algorithm>
#include <optional>
#include <variant>

#include <QAbstractSocket>
#include <QDateTime>
#include <QFile>
#include <QLocale>
#include <QMimeDatabase>
#include <QMutex>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QThread>
#include <QWaitCondition>

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

    using RangeRequest = std::variant<Range, qint64>;

    QString httpDate()
    {
        // [RFC 7231] 7.1.1.1. Date/Time Formats
        // example: "Sun, 06 Nov 1994 08:49:37 GMT"

        return QLocale::c().toString(QDateTime::currentDateTimeUtc(), u"ddd, dd MMM yyyy HH:mm:ss"_s)
            .append(u" GMT");
    }

    bool acceptsGzipEncoding(QString codings)
    {
        // [rfc7231] 5.3.4. Accept-Encoding

        const auto isCodingAvailable = [](const QList<QStringView> &list, const QStringView encoding) -> bool
        {
            for (const QStringView str : list)
            {
                if (!str.startsWith(encoding))
                    continue;

                // without quality values
                if (str == encoding)
                    return true;

                // [rfc7231] 5.3.1. Quality Values
                const QStringView substr = str.mid(encoding.size() + 3);  // ex. skip over "gzip;q="

                bool ok = false;
                const double qvalue = substr.toDouble(&ok);
                if (!ok || (qvalue <= 0))
                    return false;

                return true;
            }

            return false;
        };

        const QList<QStringView> list = QStringView(codings.remove(u' ').remove(u'\t')).split(u',', Qt::SkipEmptyParts);
        if (list.isEmpty())
            return false;

        const bool canGzip = isCodingAvailable(list, u"gzip"_s);
        if (canGzip)
            return true;

        const bool canAny = isCodingAvailable(list, u"*"_s);
        if (canAny)
            return true;

        return false;
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
}

class Http::ResponseWriter::Worker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Worker)

public:
    Worker(const Path &filePath, const Http::Request &request, const HeaderMap &headers);

public:
    void run();
    void read();
    QByteArray fetchData();
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
};

Http::ResponseWriter::ResponseWriter(QAbstractSocket *socket, Request request, QObject *parent)
    : QObject(parent)
    , m_socket {socket}
    , m_request {std::move(request)}
{
}

Http::ResponseWriter::~ResponseWriter()
{
    if (m_isWritingContent)
        m_asyncWorker->abort();
}

void Http::ResponseWriter::setResponse(const Response &response)
{
    Q_ASSERT(!m_isFinished && !m_isWritingContent);
    if (m_isFinished || m_isWritingContent) [[unlikely]]
        return;

    const QByteArray data = serializeResponse(response, m_request);
    m_socket->write(data);

    finish();
}

void Http::ResponseWriter::streamFile(const Path &filePath, const HeaderMap &headers)
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

        if (m_dataToWrite.isEmpty())
            m_dataToWrite = m_asyncWorker->fetchData();

        if (!m_dataToWrite.isEmpty())
            writeCurrentData();

        if (m_isAsyncWorkerFinished && m_dataToWrite.isEmpty())
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

        if (!m_dataToWrite.isEmpty())
            return;

        m_dataToWrite = m_asyncWorker->fetchData();
        if (!m_dataToWrite.isEmpty())
            writeCurrentData();
    });

    connect(m_asyncWorker, &Worker::finished, this, [this]
    {
        m_asyncWorker->disconnect(this);
        m_isAsyncWorkerFinished = true;
        if (m_dataToWrite.isEmpty())
        {
            m_asyncWorker->deleteLater();
            m_workerThread->quit();
            finish();
        }
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

void Http::ResponseWriter::finish()
{
    if (m_isFinished)
        return;

    m_isWritingContent = false;
    m_isFinished = true;
    emit finished(QPrivateSignal());
}

bool Http::ResponseWriter::isFinished() const
{
    return m_isFinished;
}

void Http::ResponseWriter::writeCurrentData()
{
    Q_ASSERT(!m_dataToWrite.isEmpty());
    if (m_dataToWrite.isEmpty()) [[unlikely]]
        return;

    if (!m_socket)
        return;

    const qint64 bytesToWrite = m_socket->bytesToWrite();
    const qint64 allowedBytesToWrite = MAX_BUFFER_SIZE - bytesToWrite;
    if (allowedBytesToWrite > 0)
    {
        const qint64 bytesWritten = m_socket->write(m_dataToWrite.data(), std::min(m_dataToWrite.size(), allowedBytesToWrite));
        if (bytesWritten >= 0)
            m_dataToWrite.remove(0, bytesWritten);
        else
            m_asyncWorker->abort();
    }
    else
    {
        m_socket->flush();
    }
}

Http::ResponseWriter::Worker::Worker(const Path &filePath, const Request &request, const HeaderMap &headers)
    : m_filePath {filePath}
    , m_request {request}
    , m_headers {headers}
{
    Q_ASSERT(filePath.isValid());

    m_buffer.reserve(MAX_BUFFER_SIZE);
}

void Http::ResponseWriter::Worker::run()
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
        emit dataReady();
    }

    if (m_request.method == HEADER_REQUEST_METHOD_HEAD)
    {
        emit finished();
        return;
    }

    read();
}

void Http::ResponseWriter::Worker::read()
{
    while (!isAborted() && (m_remainingSize > 0)
           && (m_buffer.size() <= (MAX_BUFFER_SIZE - CHUNK_SIZE)))
    {
        const qint64 sizeToRead = std::min(CHUNK_SIZE, m_remainingSize);
        const QByteArray chunk = m_file->read(sizeToRead);
        if (chunk.isEmpty())
        {
            abort();
            return;
        }

        const QWriteLocker locker {&m_bufferLock};
        m_buffer.append(chunk);
        if (m_buffer.size() == chunk.size())
            emit dataReady();

        m_remainingSize -= chunk.size();
    }

    if (m_remainingSize == 0)
        emit finished();
}

QByteArray Http::ResponseWriter::Worker::fetchData()
{
    const QReadLocker locker {&m_bufferLock};
    QByteArray newBuffer;
    newBuffer.reserve(MAX_BUFFER_SIZE);
    QByteArray data = std::exchange(m_buffer, newBuffer);
    if (data.size() == MAX_BUFFER_SIZE)
        QMetaObject::invokeMethod(this, &Worker::read, Qt::QueuedConnection);
    return data;
}

void Http::ResponseWriter::Worker::abort()
{
    if (isAborted())
        return;

    const QWriteLocker locker {&m_abortedStateLock};
    m_isAborted = true;

    emit failed();
}

bool Http::ResponseWriter::Worker::isAborted()
{
    const QReadLocker locker {&m_abortedStateLock};
    return m_isAborted;
}

#include "responsewriter.moc"
