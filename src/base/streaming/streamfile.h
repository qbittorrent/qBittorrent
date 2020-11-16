#pragma once

#include <QObject>
#include <QQueue>

namespace BitTorrent
{
    class TorrentHandle;
}

class ReadRequest : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ReadRequest)
    friend class StreamFile;

public:
    ReadRequest(quint64 initialPosition, quint64 maxSize, QObject *parent = nullptr);

    quint64 currentPosition() const;
    quint64 leftSize() const;

    void notifyLastBlockReceived();

public slots:
    void notifyError(const QString &message);

signals:
    void readyRead(const QByteArray &data, bool isLastBlock);
    void error(const QString &message);
    void servedLastBlock();

private:
    void feed(const QByteArray &data);

    quint64 m_currentPosition {};
    quint64 m_leftSize {};
};

class StreamFile : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(StreamFile)

public:
    StreamFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle, QObject *parent = nullptr);

    qint64 size() const;
    QString name() const;
    QString mimeType() const;
    ReadRequest *read(quint64 position, quint64 size);

    bool isSameFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const;

private:
    void doRead(ReadRequest *request);

    BitTorrent::TorrentHandle *m_torrentHandle;
    int m_index;
    QString m_name;
    QString m_mimeType;
    qlonglong m_size;
    int m_lastPiece;
    int m_pieceLength;
};
