#include "streamingmanager.h"

#include "base/http/httperror.h"
#include "base/logger.h"
#include "streamfile.h"

namespace
{
    struct Range
    {
        qint64 firstBytePos, lastBytePos;
        static Range fromHttpRangeField(const QString &value, qint64 fileSize, bool &ok)
        {
            // range value field should be in form -> "<unit>=<range>"
            // where unit is "bytes"
            // range is "<firstBytePos>-<lastBytePos>"

            qDebug() << "Provided Range " << value;
            ok = false;
            const int unitRangeSeparatorPos = value.indexOf('=');
            if (unitRangeSeparatorPos == -1) {
                return {};
            }

            const QString unit = value.left(unitRangeSeparatorPos);
            const QString acceptedUnit = QLatin1String("bytes");
            if (unit != acceptedUnit) {
                return {};
            }

            const int rangeSeparatorPos = value.indexOf('-', (unitRangeSeparatorPos + 1));
            if (rangeSeparatorPos == -1) {
                return {};
            }

            bool isValidInt;
            const qint64 firstBytePos = value.midRef(unitRangeSeparatorPos + 1, (rangeSeparatorPos - unitRangeSeparatorPos - 1)).toULongLong(&isValidInt);
            if (!isValidInt) {
                return {};
            }

            qint64 lastBytePos = value.midRef(rangeSeparatorPos + 1).toULongLong(&isValidInt);
            if (!isValidInt)
                // lastBytePos is optional, if not provided we should assume lastBytePos is at the end of the file
                lastBytePos = fileSize - 1;

            if (firstBytePos > lastBytePos) {
                return {};
            }

            ok = true;
            return {firstBytePos, lastBytePos};
        }

        qint64 size() const
        {
            return lastBytePos - firstBytePos;
        }
    };
} // namespace

StreamingManager *StreamingManager::m_instance = nullptr;

void StreamingManager::initInstance()
{
    if (!m_instance)
        m_instance = new StreamingManager;
}

void StreamingManager::freeInstance()
{
    if (m_instance)
        delete m_instance;
}

StreamingManager *StreamingManager::instance()
{
    return m_instance;
}

StreamingManager::StreamingManager(QObject *parent)
    : StreamingServer(parent)
{
    start();
}

void StreamingManager::addFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle)
{
    Q_ASSERT(!isStreaming(fileIndex, torrentHandle));

    m_files.push_back(new StreamFile(fileIndex, torrentHandle));
}

QString StreamingManager::url(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const
{
    StreamFile *file = findFile(fileIndex, torrentHandle);
    if (!file)
        return {};
    return "http://localhost:" + QString::number(serverPort()) + '/' + file->name().toUtf8().toPercentEncoding();
}

bool StreamingManager::isStreaming(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const
{
    return findFile(fileIndex, torrentHandle) != nullptr;
}

void StreamingManager::doRequest(HttpSocket *socket)
{
    try {
        const Http::Request request = socket->request();
        if (request.method == Http::HEADER_REQUEST_METHOD_HEAD)
            doHead(socket);
        else if (request.method == Http::HEADER_REQUEST_METHOD_GET)
            doGet(socket);
        else
            throw MethodNotAllowedHTTPError();
    }
    catch (const HTTPError &error)
    {
        socket->sendStatus({static_cast<uint>(error.statusCode()), error.statusText()});
        if (!error.message().isEmpty()) {
            socket->sendHeaders({{Http::HEADER_CONTENT_TYPE, Http::CONTENT_TYPE_TXT},
                                    {Http::HEADER_CONTENT_LENGTH, QString::number(error.message().length())}});
            socket->send(error.message().toUtf8());
        }
        socket->close();
    }
}

void StreamingManager::doHead(HttpSocket *socket)
{
    const StreamFile *file = findFile(socket->request().path);
    if (!file)
        throw NotFoundHTTPError();

    socket->sendStatus({200, "OK"});
    socket->sendHeaders({{"accept-ranges", "bytes"},
                            {"connection", "close"},
                            {"content-length", QString::number(file->size())},
                            {Http::HEADER_CONTENT_TYPE, file->mimeType()}});

    socket->close();
}

void StreamingManager::doGet(HttpSocket *socket)
{
    const Http::Request request = socket->request();
    const QString rangeValue = request.headers.value("range");
    if (rangeValue.isEmpty())
        doHead(socket);

    StreamFile *file = findFile(socket->request().path);
    if (!file)
        throw NotFoundHTTPError();

    bool isValidRange = true;
    const Range range = Range::fromHttpRangeField(rangeValue, file->size(), isValidRange);
    if (!isValidRange)
        throw InvalidRangeHTTPError();

    socket->sendStatus({206, "Partial Content"});
    socket->sendHeaders({{"accept-ranges", "bytes"},
                            {"content-length", QString::number(range.size())},
                            {Http::HEADER_CONTENT_TYPE, file->mimeType()},
                            {"content-range", QString("bytes %1-%2/%3")
                             .arg(QString::number(range.firstBytePos), QString::number(range.lastBytePos), QString::number(file->size()))}});

    ReadRequest *readRequest {file->read(range.firstBytePos, range.size())};
    connect(readRequest, &ReadRequest::readyRead, socket
            , [socket] (const QByteArray &data, const bool isLastBlock)
    {
        socket->send(data);
        if (isLastBlock)
            qDebug("reached at end"), socket->close();
    });

    connect(readRequest, &ReadRequest::error, socket, [this, socket, range](const QString &message)
    {
        LogMsg(tr("Failed to serve request in range [%1,%2]. Reason: %3")
               .arg(QString::number(range.firstBytePos), QString::number(range.lastBytePos), message), Log::CRITICAL);

        socket->close();
    });

    connect(socket, &QObject::destroyed, readRequest, [readRequest] ()
    {
        qDebug("deleting request");
        readRequest->deleteLater();
    });
}

StreamFile *StreamingManager::findFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const
{
    const auto fileIter =
        std::find_if(m_files.begin(), m_files.end(), [fileIndex, torrentHandle](const StreamFile *file) {
        return file->isSameFile(fileIndex, torrentHandle);
    });

    if (fileIter == m_files.end())
        return nullptr;
    return *fileIter;
}

StreamFile *StreamingManager::findFile(const QString &path) const
{
    const QString pathWithoutSep = path.startsWith('/') ? path.mid(1) : path;
    const auto fileIter =
        std::find_if(m_files.begin(), m_files.end(), [pathWithoutSep](const StreamFile *file) {
        return file->name() == pathWithoutSep;
    });
    if (fileIter == m_files.end())
        return nullptr;
    return *fileIter;
}
