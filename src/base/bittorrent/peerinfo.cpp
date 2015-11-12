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

#include "base/net/geoipmanager.h"
#include "base/utils/string.h"
#include "base/unicodestrings.h"
#include "base/bittorrent/torrenthandle.h"
#include "peerinfo.h"

namespace libt = libtorrent;
using namespace BitTorrent;

// PeerAddress

PeerAddress::PeerAddress()
    : port(0)
{
}

PeerAddress::PeerAddress(QHostAddress ip, ushort port)
    : ip(ip)
    , port(port)
{
}

// PeerInfo

PeerInfo::PeerInfo(const TorrentHandle *torrent, const libt::peer_info &nativeInfo)
    : m_nativeInfo(nativeInfo)
{
    calcRelevance(torrent);
    determineFlags();
}

bool PeerInfo::fromDHT() const
{
    return (m_nativeInfo.source & libt::peer_info::dht);
}

bool PeerInfo::fromPeX() const
{
    return (m_nativeInfo.source & libt::peer_info::pex);
}

bool PeerInfo::fromLSD() const
{
    return (m_nativeInfo.source & libt::peer_info::lsd);
}

#ifndef DISABLE_COUNTRIES_RESOLUTION
QString PeerInfo::country() const
{
    return Net::GeoIPManager::instance()->lookup(address().ip);
}
#endif

bool PeerInfo::isInteresting() const
{
    return (m_nativeInfo.flags & libt::peer_info::interesting);
}

bool PeerInfo::isChocked() const
{
    return (m_nativeInfo.flags & libt::peer_info::choked);
}

bool PeerInfo::isRemoteInterested() const
{
    return (m_nativeInfo.flags & libt::peer_info::remote_interested);
}

bool PeerInfo::isRemoteChocked() const
{
    return (m_nativeInfo.flags & libt::peer_info::remote_choked);
}


bool PeerInfo::isSupportsExtensions() const
{
    return (m_nativeInfo.flags & libt::peer_info::supports_extensions);
}

bool PeerInfo::isLocalConnection() const
{
    return (m_nativeInfo.flags & libt::peer_info::local_connection);
}

bool PeerInfo::isHandshake() const
{
    return (m_nativeInfo.flags & libt::peer_info::handshake);
}


bool PeerInfo::isConnecting() const
{
    return (m_nativeInfo.flags & libt::peer_info::connecting);
}

bool PeerInfo::isQueued() const
{
    return (m_nativeInfo.flags & libt::peer_info::queued);
}


bool PeerInfo::isOnParole() const
{
    return (m_nativeInfo.flags & libt::peer_info::on_parole);
}

bool PeerInfo::isSeed() const
{
    return (m_nativeInfo.flags & libt::peer_info::seed);
}

bool PeerInfo::optimisticUnchoke() const
{
    return (m_nativeInfo.flags & libt::peer_info::optimistic_unchoke);
}


bool PeerInfo::isSnubbed() const
{
    return (m_nativeInfo.flags & libt::peer_info::snubbed);
}

bool PeerInfo::isUploadOnly() const
{
    return (m_nativeInfo.flags & libt::peer_info::upload_only);
}

bool PeerInfo::isEndgameMode() const
{
    return (m_nativeInfo.flags & libt::peer_info::endgame_mode);
}


bool PeerInfo::isHolepunched() const
{
    return (m_nativeInfo.flags & libt::peer_info::holepunched);
}

bool PeerInfo::useI2PSocket() const
{
    return (m_nativeInfo.flags & libt::peer_info::i2p_socket);
}

bool PeerInfo::useUTPSocket() const
{
    return (m_nativeInfo.flags & libt::peer_info::utp_socket);
}

bool PeerInfo::useSSLSocket() const
{
    return (m_nativeInfo.flags & libt::peer_info::ssl_socket);
}

bool PeerInfo::isRC4Encrypted() const
{
    return (m_nativeInfo.flags & libt::peer_info::rc4_encrypted);
}

bool PeerInfo::isPlaintextEncrypted() const
{
    return (m_nativeInfo.flags & libt::peer_info::plaintext_encrypted);
}


PeerAddress PeerInfo::address() const
{
    return PeerAddress(QHostAddress(QString::fromStdString(m_nativeInfo.ip.address().to_string())),
                       m_nativeInfo.ip.port());
}

QString PeerInfo::client() const
{
    return Utils::String::fromStdString(m_nativeInfo.client);
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

    for (int i = 0; i < m_nativeInfo.pieces.size(); ++i)
        result.setBit(i, m_nativeInfo.pieces.get_bit(i));

    return result;
}

QString PeerInfo::connectionType() const
{
    if (m_nativeInfo.flags & libt::peer_info::utp_socket)
        return QString::fromUtf8(C_UTP);

    QString connection;
    switch(m_nativeInfo.connection_type) {
    case libt::peer_info::http_seed:
    case libt::peer_info::web_seed:
        connection = "Web";
        break;
    default:
        connection = "BT";
    }

    return connection;
}

void PeerInfo::calcRelevance(const TorrentHandle *torrent)
{
    const QBitArray &allPieces = torrent->pieces();
    const QBitArray &peerPieces = pieces();

    int localMissing = 0;
    int remoteHaves = 0;

    for (int i = 0; i < allPieces.size(); ++i) {
        if (!allPieces[i]) {
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
    if (isInteresting()) {
        //d = Your client wants to download, but peer doesn't want to send (interested and choked)
        if (isRemoteChocked()) {
            m_flags += "d ";
            m_flagsDescription += tr("interested(local) and choked(peer)");
            m_flagsDescription += ", ";
        }
        else {
            //D = Currently downloading (interested and not choked)
            m_flags += "D ";
            m_flagsDescription += tr("interested(local) and unchoked(peer)");
            m_flagsDescription += ", ";
        }
    }

    if (isRemoteInterested()) {
        //u = Peer wants your client to upload, but your client doesn't want to (interested and choked)
        if (isChocked()) {
            m_flags += "u ";
            m_flagsDescription += tr("interested(peer) and choked(local)");
            m_flagsDescription += ", ";
        }
        else {
            //U = Currently uploading (interested and not choked)
            m_flags += "U ";
            m_flagsDescription += tr("interested(peer) and unchoked(local)");
            m_flagsDescription += ", ";
        }
    }

    //O = Optimistic unchoke
    if (optimisticUnchoke()) {
        m_flags += "O ";
        m_flagsDescription += tr("optimistic unchoke");
        m_flagsDescription += ", ";
    }

    //S = Peer is snubbed
    if (isSnubbed()) {
        m_flags += "S ";
        m_flagsDescription += tr("peer snubbed");
        m_flagsDescription += ", ";
    }

    //I = Peer is an incoming connection
    if (!isLocalConnection()) {
        m_flags += "I ";
        m_flagsDescription += tr("incoming connection");
        m_flagsDescription += ", ";
    }

    //K = Peer is unchoking your client, but your client is not interested
    if (!isRemoteChocked() && !isInteresting()) {
        m_flags += "K ";
        m_flagsDescription += tr("not interested(local) and unchoked(peer)");
        m_flagsDescription += ", ";
    }

    //? = Your client unchoked the peer but the peer is not interested
    if (!isChocked() && !isRemoteInterested()) {
        m_flags += "? ";
        m_flagsDescription += tr("not interested(peer) and unchoked(local)");
        m_flagsDescription += ", ";
    }

    //X = Peer was included in peerlists obtained through Peer Exchange (PEX)
    if (fromPeX()) {
        m_flags += "X ";
        m_flagsDescription += tr("peer from PEX");
        m_flagsDescription += ", ";
    }

    //H = Peer was obtained through DHT
    if (fromDHT()) {
        m_flags += "H ";
        m_flagsDescription += tr("peer from DHT");
        m_flagsDescription += ", ";
    }

    //E = Peer is using Protocol Encryption (all traffic)
    if (isRC4Encrypted()) {
        m_flags += "E ";
        m_flagsDescription += tr("encrypted traffic");
        m_flagsDescription += ", ";
    }

    //e = Peer is using Protocol Encryption (handshake)
    if (isPlaintextEncrypted()) {
        m_flags += "e ";
        m_flagsDescription += tr("encrypted handshake");
        m_flagsDescription += ", ";
    }

    //P = Peer is using uTorrent uTP

    if (useUTPSocket()) {
        m_flags += "P ";
        m_flagsDescription += QString::fromUtf8(C_UTP);
        m_flagsDescription += ", ";
    }

    //L = Peer is local
    if (fromLSD()) {
        m_flags += "L";
        m_flagsDescription += tr("peer from LSD");
    }

    m_flags = m_flags.trimmed();
    m_flagsDescription = m_flagsDescription.trimmed();
    if (m_flagsDescription.endsWith(',', Qt::CaseInsensitive))
        m_flagsDescription.chop(1);
}

QString PeerInfo::flags() const
{
    return m_flags;
}

QString PeerInfo::flagsDescription() const
{
    return m_flagsDescription;
}
