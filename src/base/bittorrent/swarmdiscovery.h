#pragma once

#include <QObject>
#include <QSet>
#include <QHash>
#include <QSharedPointer>

class TorrentHandle;

class SwarmDiscoveryManager final : public QObject
{
    Q_OBJECT
public:
    static SwarmDiscoveryManager& instance();

    explicit SwarmDiscoveryManager(QObject *parent = nullptr);
    ~SwarmDiscoveryManager() override;

    // Registro de torrents para descoberta de peças compartilhadas
    void registerTorrent(const QString &torrentId, const QSet<QString> &pieceHashes);
    void unregisterTorrent(const QString &torrentId);

    // Consulta de torrents que compartilham peças com o torrent informado
    QSet<QString> findSharedTorrents(const QString &torrentId) const;
    QSet<QString> findSharedPieces(const QString &torrentId) const;

    // Habilita/desabilita a descoberta de swarm
    void setEnabled(bool enabled);
    bool isEnabled() const noexcept { return m_enabled; }

signals:
    void sharedPiecesFound(const QString &torrentId, const QSet<QString> &sharedTorrents);

private:
    bool m_enabled = false;
    // Mapa de hash de peça -> conjunto de torrentIds que a possuem
    QHash<QString, QSet<QString>> m_pieceToTorrents;
    // Mapa de torrentId -> conjunto de hashes de peças
    QHash<QString, QSet<QString>> m_torrentToPieces;
};
