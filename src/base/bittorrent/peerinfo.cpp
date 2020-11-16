/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "peerinfo.h"

#include <QBitArray>

#include "base/bittorrent/torrenthandle.h"
#include "base/net/geoipmanager.h"
#include "base/unicodestrings.h"
#include "peeraddress.h"

using namespace BitTorrent;

PeerInfo::PeerInfo(const TorrentHandle *torrent, const lt::peer_info &nativeInfo)
    : m_nativeInfo(nativeInfo)
{
    calcRelevance(torrent);
    determineFlags();
}

bool PeerInfo::fromDHT() const
{
    return static_cast<bool>(m_nativeInfo.source & lt::peer_info::dht);
}

bool PeerInfo::fromPeX() const
{
    return static_cast<bool>(m_nativeInfo.source & lt::peer_info::pex);
}

bool PeerInfo::fromLSD() const
{
    return static_cast<bool>(m_nativeInfo.source & lt::peer_info::lsd);
}

QString PeerInfo::country() const
{
    if (m_country.isEmpty())
        m_country = Net::GeoIPManager::instance()->lookup(address().ip);
    return m_country;
}

bool PeerInfo::isInteresting() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::interesting);
}

bool PeerInfo::isChocked() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::choked);
}

bool PeerInfo::isRemoteInterested() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::remote_interested);
}

bool PeerInfo::isRemoteChocked() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::remote_choked);
}

bool PeerInfo::isSupportsExtensions() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::supports_extensions);
}

bool PeerInfo::isLocalConnection() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::local_connection);
}

bool PeerInfo::isHandshake() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::handshake);
}

bool PeerInfo::isConnecting() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::connecting);
}

bool PeerInfo::isOnParole() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::on_parole);
}

bool PeerInfo::isSeed() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::seed);
}

bool PeerInfo::optimisticUnchoke() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::optimistic_unchoke);
}

bool PeerInfo::isSnubbed() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::snubbed);
}

bool PeerInfo::isUploadOnly() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::upload_only);
}

bool PeerInfo::isEndgameMode() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::endgame_mode);
}

bool PeerInfo::isHolepunched() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::holepunched);
}

bool PeerInfo::useI2PSocket() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::i2p_socket);
}

bool PeerInfo::useUTPSocket() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::utp_socket);
}

bool PeerInfo::useSSLSocket() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::ssl_socket);
}

bool PeerInfo::isRC4Encrypted() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::rc4_encrypted);
}

bool PeerInfo::isPlaintextEncrypted() const
{
    return static_cast<bool>(m_nativeInfo.flags & lt::peer_info::plaintext_encrypted);
}

PeerAddress PeerInfo::address() const
{
    // fast path for platforms which boost.asio internal struct maps to `sockaddr`
    return {QHostAddress(m_nativeInfo.ip.data()), m_nativeInfo.ip.port()};
    // slow path for the others
    //return {QHostAddress(QString::fromStdString(m_nativeInfo.ip.address().to_string()))
    //    , m_nativeInfo.ip.port()};
}

QString PeerInfo::client() const
{
    return QString::fromStdString(m_nativeInfo.client);
}

qreal PeerInfo::progress() const
{
    return m_nativeInfo.progress;
}

int PeerInfo::payloadUpSpeed() const
{
    return m_nativeInfo.payload_up_speed;
}

int PeerInfo::payloadDownSpeed() const
{
    return m_nativeInfo.payload_down_speed;
}

qlonglong PeerInfo::totalUpload() const
{
    return m_nativeInfo.total_upload;
}

qlonglong PeerInfo::totalDownload() const
{
    return m_nativeInfo.total_download;
}

QBitArray PeerInfo::pieces() const
{
    QBitArray result(m_nativeInfo.pieces.size());
    for (int i = 0; i < result.size(); ++i)
    {
        if (m_nativeInfo.pieces[lt::piece_index_t {i}])
            result.setBit(i, true);
    }
    return result;
}

QString PeerInfo::connectionType() const
{
    if (m_nativeInfo.flags & lt::peer_info::utp_socket)
        return QString::fromUtf8(C_UTP);

    return (m_nativeInfo.connection_type == lt::peer_info::standard_bittorrent)
        ? QLatin1String {"BT"}
        : QLatin1String {"Web"};
}

void PeerInfo::calcRelevance(const TorrentHandle *torrent)
{
    const QBitArray allPieces = torrent->pieces();
    const QBitArray peerPieces = pieces();

    int localMissing = 0;
    int remoteHaves = 0;

    for (int i = 0; i < allPieces.size(); ++i)
    {
        if (!allPieces[i])
        {
            ++localMissing;
            if (peerPieces[i])
                ++remoteHaves;
        }
    }

    if (localMissing == 0)
        m_relevance = 0.0;
    else
        m_relevance = static_cast<qreal>(remoteHaves) / localMissing;
}

qreal PeerInfo::relevance() const
{
    return m_relevance;
}

void PeerInfo::determineFlags()
{
    if (isInteresting())
    {
        // d = Your client wants to download, but peer doesn't want to send (interested and choked)
        if (isRemoteChocked())
        {
            m_flags += "d ";
            m_flagsDescription += ("d = "
                + tr("Interested(local) and Choked(peer)") + '\n');
        }
        else
        {
            // D = Currently downloading (interested and not choked)
            m_flags += "D ";
            m_flagsDescription += ("D = "
                + tr("interested(local) and unchoked(peer)") + '\n');
        }
    }

    if (isRemoteInterested())
    {
        // u = Peer wants your client to upload, but your client doesn't want to (interested and choked)
        if (isChocked())
        {
            m_flags += "u ";
            m_flagsDescription += ("u = "
                + tr("interested(peer) and choked(local)") + '\n');
        }
        else
        {
            // U = Currently uploading (interested and not choked)
            m_flags += "U ";
            m_flagsDescription += ("U = "
                + tr("interested(peer) and unchoked(local)") + '\n');
        }
    }

    // O = Optimistic unchoke
    if (optimisticUnchoke())
    {
        m_flags += "O ";
        m_flagsDescription += ("O = " + tr("optimistic unchoke") + '\n');
    }

    // S = Peer is snubbed
    if (isSnubbed())
    {
        m_flags += "S ";
        m_flagsDescription += ("S = " + tr("peer snubbed") + '\n');
    }

    // I = Peer is an incoming connection
    if (!isLocalConnection())
    {
        m_flags += "I ";
        m_flagsDescription += ("I = " + tr("incoming connection") + '\n');
    }

    // K = Peer is unchoking your client, but your client is not interested
    if (!isRemoteChocked() && !isInteresting())
    {
        m_flags += "K ";
        m_flagsDescription += ("K = "
            + tr("not interested(local) and unchoked(peer)") + '\n');
    }

    // ? = Your client unchoked the peer but the peer is not interested
    if (!isChocked() && !isRemoteInterested())
    {
        m_flags += "? ";
        m_flagsDescription += ("? = "
            + tr("not interested(peer) and unchoked(local)") + '\n');
    }

    // X = Peer was included in peerlists obtained through Peer Exchange (PEX)
    if (fromPeX())
    {
        m_flags += "X ";
        m_flagsDescription += ("X = " + tr("peer from PEX") + '\n');
    }

    // H = Peer was obtained through DHT
    if (fromDHT())
    {
        m_flags += "H ";
        m_flagsDescription += ("H = " + tr("peer from DHT") + '\n');
    }

    // E = Peer is using Protocol Encryption (all traffic)
    if (isRC4Encrypted())
    {
        m_flags += "E ";
        m_flagsDescription += ("E = " + tr("encrypted traffic")  + '\n');
    }

    // e = Peer is using Protocol Encryption (handshake)
    if (isPlaintextEncrypted())
    {
        m_flags += "e ";
        m_flagsDescription += ("e = " + tr("encrypted handshake") + '\n');
    }

    // P = Peer is using uTorrent uTP
    if (useUTPSocket())
    {
        m_flags += "P ";
        m_flagsDescription += ("P = " + QString::fromUtf8(C_UTP) + '\n');
    }

    // L = Peer is local
    if (fromLSD())
    {
        m_flags += "L ";
        m_flagsDescription += ("L = " + tr("peer from LSD") + '\n');
    }

    m_flags = m_flags.trimmed();
    m_flagsDescription = m_flagsDescription.trimmed();
}

QString PeerInfo::flags() const
{
    return m_flags;
}

QString PeerInfo::flagsDescription() const
{
    return m_flagsDescription;
}

int PeerInfo::downloadingPieceIndex() const
{
    return static_cast<int>(m_nativeInfo.downloading_piece_index);
}
