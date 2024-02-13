#pragma once

#include "apicontroller.h"

namespace BitTorrent
{
class Torrent;
}

class WebSession;

class TransmissionRPCController : public APIController
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransmissionRPCController)

public:
    explicit TransmissionRPCController(IApplication *app, WebSession *parent = nullptr);

private slots:
    void rpcAction();

private:
    void saveMapping(BitTorrent::Torrent*);
    void removeMapping(BitTorrent::Torrent*);
    QJsonObject torrentGet(const QJsonObject &) const;
    QVector<std::pair<int, BitTorrent::Torrent*>> collectTorrentsForRequest(const QJsonValue&) const;

    QHash<int, BitTorrent::Torrent*> m_idToTorrent;
    QHash<BitTorrent::Torrent*, int> m_torrentToId;
    int m_lastId = 1;
    QString m_sid;
};