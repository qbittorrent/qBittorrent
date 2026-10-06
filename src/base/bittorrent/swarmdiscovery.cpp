#include "swarmdiscovery.h"

#include <QMutexLocker>

SwarmDiscoveryManager& SwarmDiscoveryManager::instance()
{
    static SwarmDiscoveryManager inst;
    return inst;
}

SwarmDiscoveryManager::SwarmDiscoveryManager(QObject *parent)
    : QObject(parent)
{
}

SwarmDiscoveryManager::~SwarmDiscoveryManager() = default;

void SwarmDiscoveryManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void SwarmDiscoveryManager::registerTorrent(const QString &torrentId, const QSet<QString> &pieceHashes)
{
    if (!m_enabled || torrentId.isEmpty())
        return;

    m_torrentToPieces[torrentId] = pieceHashes;
    for (const auto &hash : pieceHashes) {
        m_pieceToTorrents[hash].insert(torrentId);
    }
}

void SwarmDiscoveryManager::unregisterTorrent(const QString &torrentId)
{
    if (!m_enabled)
        return;

    const auto pieces = m_torrentToPieces.take(torrentId);
    for (const auto &hash : pieces) {
        auto &set = m_pieceToTorrents[hash];
        set.remove(torrentId);
        if (set.isEmpty())
            m_pieceToTorrents.remove(hash);
    }
}

QSet<QString> SwarmDiscoveryManager::findSharedTorrents(const QString &torrentId) const
{
    QSet<QString> result;
    if (!m_enabled)
        return result;

    const auto pieces = m_torrentToPieces.value(torrentId);
    for (const auto &hash : pieces) {
        const auto &owners = m_pieceToTorrents.value(hash);
        for (const auto &owner : owners) {
            if (owner != torrentId)
                result.insert(owner);
        }
    }
    return result;
}

QSet<QString> SwarmDiscoveryManager::findSharedPieces(const QString &torrentId) const
{
    QSet<QString> result;
    if (!m_enabled)
        return result;

    const auto pieces = m_torrentToPieces.value(torrentId);
    for (const auto &hash : pieces) {
        const auto &owners = m_pieceToTorrents.value(hash);
        if (owners.size() > 1)
            result.insert(hash);
    }
    return result;
}
