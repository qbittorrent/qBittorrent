#include "streamfile.h"

#include <QDebug>
#include <QTimer>
#include <QMimeType>
#include <QMimeDatabase>

#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/torrentinfo.h"

namespace
{
    const int DEADLINE_TIME = 100;
    const int ADVANCE_PIECES = 4;
}

ReadRequest::ReadRequest(quint64 initialPosition, quint64 maxSize, QObject *parent)
    : QObject(parent)
    , m_currentPosition(initialPosition)
    , m_leftSize(maxSize)
{
}

quint64 ReadRequest::leftSize() const
{
    return m_leftSize;
}

quint64 ReadRequest::currentPosition() const
{
    return m_currentPosition;
}

void ReadRequest::feed(const QByteArray &data)
{
    Q_ASSERT(data.size() <= m_leftSize);

    const bool isLastBlock = (data.size() - m_leftSize) == 0;
    emit readyRead(data, isLastBlock);
    m_currentPosition += data.size();
    m_leftSize -= data.size();
}

void ReadRequest::notifyErrorAndDie(const QString &message)
{
    emit error(message);
    deleteLater();
}

StreamFile::StreamFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle, QObject *parent)
    : QObject(parent)
    , m_index(fileIndex)
    , m_torrentHandle(torrentHandle)
{
    const BitTorrent::TorrentInfo info = m_torrentHandle->info();
    m_name = info.name() + '/' + info.fileName(fileIndex);
    m_mimeType = QMimeDatabase().mimeTypeForFile(m_name).name();
    m_size = info.fileSize(fileIndex);
    m_lastPiece = info.filePieces(fileIndex).last();
    m_pieceLength = info.pieceLength();
}

qint64 StreamFile::size() const
{
    return m_size;
}

QString StreamFile::name() const
{
    return m_name;
}

QString StreamFile::mimeType() const
{
    return m_mimeType;
}

ReadRequest *StreamFile::read(const quint64 position, const quint64 size)
{
    ReadRequest *request = new ReadRequest(position, size, this);
    doRead(request);
    return request;
}

void StreamFile::doRead(ReadRequest *request)
{
    if (request->leftSize() == 0) {
        request->deleteLater();
        qDebug("Completed Request");
        return;
    }

    const BitTorrent::PieceFileInfo pieceInfo
        = m_torrentHandle->info().mapFile(m_index, request->currentPosition(), std::min<quint64>(request->leftSize(), m_pieceLength));
    qDebug() << "mappedto " << pieceInfo.index << request->currentPosition() << request->leftSize();
    const BitTorrent::PieceRequest *pieceRequest = m_torrentHandle->havePiece(pieceInfo.index)
                                                   ? m_torrentHandle->readPiece(pieceInfo.index)
                                                   : m_torrentHandle->setPieceDeadline(pieceInfo.index, DEADLINE_TIME, true);

    for (int i = 1; i <= ADVANCE_PIECES && pieceInfo.index + i < m_lastPiece; i++)
        m_torrentHandle->setPieceDeadline(pieceInfo.index + i, DEADLINE_TIME * 2, false); // prepare for next read

    auto cnt = connect(pieceRequest, &BitTorrent::PieceRequest::complete, request
            , [this, request, pieceInfo] (const QByteArray &data)
    {
        qDebug() << "done request" << pieceInfo.index << pieceInfo.start << data.size() << pieceInfo.length;
        request->feed(data.mid(pieceInfo.start, pieceInfo.length));
        QMetaObject::invokeMethod(this, [this, request] () { doRead(request); });
    });
    Q_ASSERT(cnt);

    connect(pieceRequest, &BitTorrent::PieceRequest::error, request, &ReadRequest::notifyErrorAndDie);
}

bool StreamFile::isSameFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const
{
    return m_index == fileIndex && m_torrentHandle == torrentHandle;
}
