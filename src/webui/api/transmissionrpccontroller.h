#pragma once

#include "apicontroller.h"

namespace BitTorrent
{
class Torrent;
}

class TransmissionRPCController : public APIController
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransmissionRPCController)

public:
    explicit TransmissionRPCController(IApplication *app, QObject *parent = nullptr);

private slots:
    void rpcAction();

private:
    void saveMapping(BitTorrent::Torrent*);
    void removeMapping(BitTorrent::Torrent*);
    QJsonObject torrentGet(const QJsonObject &) const;
    QVector<BitTorrent::Torrent*> collectTorrentsForRequest(const QJsonValue&) const;

    QHash<int, BitTorrent::Torrent*> m_idToTorrent;
    QHash<BitTorrent::Torrent*, int> m_torrentToId;
    int m_lastId = 1;
};
