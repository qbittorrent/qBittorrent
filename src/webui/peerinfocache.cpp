/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent project
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
 * modified versions of it that use the same license as the "OpenSSL"
 * library), and distribute the linked executables. You must obey the GNU
 * General Public License in all respects for all of the code used other than
 * "OpenSSL".  If you modify file(s), you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version.
 */

#include "peerinfocache.h"

#include <algorithm>
#include <chrono>

#include <QCoreApplication>
#include <QFuture>
#include <QTimer>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"

using namespace std::chrono_literals;

namespace
{
    // stop refreshing (and free cached data) when nobody asked for peers for this long
    const std::chrono::milliseconds IDLE_TIMEOUT = 30s;
    const int MIN_REFRESH_INTERVAL = 500; // ms
}

PeerInfoCache *PeerInfoCache::instance()
{
    static PeerInfoCache *cache = new PeerInfoCache(QCoreApplication::instance());
    return cache;
}

PeerInfoCache::PeerInfoCache(QObject *parent)
    : QObject(parent)
    , m_timer {new QTimer(this)}
{
    m_timer->setTimerType(Qt::CoarseTimer);
    connect(m_timer, &QTimer::timeout, this, &PeerInfoCache::onTimeout);
}

QList<BitTorrent::PeerInfo> PeerInfoCache::peers(const BitTorrent::Torrent *torrent)
{
    touch();

    if (isFresh())
    {
        if (const auto it = m_peers.constFind(torrent->id()); it != m_peers.cend())
            return it.value();
    }

    // no usable cached data yet (e.g. first request or newly added torrent)
    return torrent->fetchPeerInfo().takeResult();
}

PeerInfoCache::PeersByTorrent PeerInfoCache::allPeers()
{
    touch();

    if (!isFresh())
    {
        m_peers = BitTorrent::Session::instance()->fetchPeerInfo({}).takeResult();
        m_lastUpdate.start();
    }

    return m_peers;
}

void PeerInfoCache::touch()
{
    m_lastAccess.start();
    if (!m_timer->isActive())
    {
        m_timer->start(refreshInterval());
        refresh();
    }
}

bool PeerInfoCache::isFresh() const
{
    if (!m_lastUpdate.isValid())
        return false;

    const qint64 maxAge = std::max<qint64>(3000, (2 * refreshInterval()));
    return !m_lastUpdate.hasExpired(maxAge);
}

int PeerInfoCache::refreshInterval() const
{
    return std::max(MIN_REFRESH_INTERVAL, BitTorrent::Session::instance()->refreshInterval());
}

void PeerInfoCache::onTimeout()
{
    if (m_lastAccess.hasExpired(IDLE_TIMEOUT.count()))
    {
        m_timer->stop();
        m_peers.clear();
        m_peers.squeeze();
        m_lastUpdate.invalidate();
        return;
    }

    m_timer->setInterval(refreshInterval());
    refresh();
}

void PeerInfoCache::refresh()
{
    if (m_isRefreshing)
        return;

    m_isRefreshing = true;
    BitTorrent::Session::instance()->fetchPeerInfo({}).then(this, [this](PeersByTorrent peers)
    {
        m_isRefreshing = false;
        if (!m_timer->isActive())
            return; // went idle meanwhile

        m_peers = std::move(peers);
        m_lastUpdate.start();
    });
}
