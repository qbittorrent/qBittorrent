
#pragma once

#include <QObject>
#include <QHash>

#include "streamingserver.h"

namespace BitTorrent
{
    class TorrentHandle;
}

class StreamFile;

class StreamingManager : public StreamingServer
{
public:
    static void initInstance();
    static void freeInstance();
    static StreamingManager *instance();

    void addFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle);
    QString url(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const;
    bool isStreaming(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const;

private:
    StreamingManager(QObject *parent = nullptr);

    void doRequest(HttpSocket *socket) override;
    void doHead(HttpSocket *socket);
    void doGet(HttpSocket *socket);

    StreamFile *findFile(int fileIndex, BitTorrent::TorrentHandle *torrentHandle) const;
    StreamFile *findFile(const QString &path) const;

    static StreamingManager *m_instance;
    QVector<StreamFile *> m_files;
};
