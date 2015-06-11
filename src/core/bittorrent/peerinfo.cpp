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

#include <libtorrent/version.hpp>

#include "core/utils/string.h"
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

PeerInfo::PeerInfo(const libt::peer_info &nativeInfo)
    : m_nativeInfo(nativeInfo)
{
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

QString PeerInfo::country() const
{
    return QString(QByteArray(m_nativeInfo.country, 2));
}


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
#if LIBTORRENT_VERSION_NUM < 10000
    return (m_nativeInfo.connection_type & libt::peer_info::bittorrent_utp);
#else
    return (m_nativeInfo.flags & libt::peer_info::utp_socket);
#endif
}

bool PeerInfo::useSSLSocket() const
{
#if LIBTORRENT_VERSION_NUM < 10000
    return false;
#else
    return (m_nativeInfo.flags & libt::peer_info::ssl_socket);
#endif
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

#if LIBTORRENT_VERSION_NUM < 10000
    typedef size_t pieces_size_t;
#else
    typedef int pieces_size_t;
#endif
    for (pieces_size_t i = 0; i < m_nativeInfo.pieces.size(); ++i)
        result.setBit(i, m_nativeInfo.pieces.get_bit(i));

    return result;
}

QString PeerInfo::connectionType() const
{
#if LIBTORRENT_VERSION_NUM < 10000
    if (m_nativeInfo.connection_type & libt::peer_info::bittorrent_utp)
#else
    if (m_nativeInfo.flags & libt::peer_info::utp_socket)
#endif
        return QString::fromUtf8("μTP");

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
