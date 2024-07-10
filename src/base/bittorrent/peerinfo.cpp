/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "base/bittorrent/ltqbitarray.h"
#include "base/net/geoipmanager.h"
#include "base/unicodestrings.h"
#include "base/utils/bytearray.h"
#include "peeraddress.h"

using namespace BitTorrent;

PeerInfo::PeerInfo(const lt::peer_info &nativeInfo, const QBitArray &allPieces)
    : m_nativeInfo(nativeInfo)
    , m_relevance(calcRelevance(allPieces))
{
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
    if (useI2PSocket())
        return {};

    // fast path for platforms which boost.asio internal struct maps to `sockaddr`
    return {QHostAddress(m_nativeInfo.ip.data()), m_nativeInfo.ip.port()};
    // slow path for the others
    //return {QHostAddress(QString::fromStdString(m_nativeInfo.ip.address().to_string()))
    //    , m_nativeInfo.ip.port()};
}

QString PeerInfo::I2PAddress() const
{
    if (!useI2PSocket())
        return {};

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    if (m_I2PAddress.isEmpty())
    {
        const lt::sha256_hash destHash = m_nativeInfo.i2p_destination();
        const QByteArray base32Dest = Utils::ByteArray::toBase32({destHash.data(), destHash.size()}).replace('=', "").toLower();
        m_I2PAddress = QString::fromLatin1(base32Dest) + u".b32.i2p";
    }
#endif

    return m_I2PAddress;
}

QString PeerInfo::client() const
{
    auto client = QString::fromStdString(m_nativeInfo.client).simplified();

    // remove non-printable characters
    erase_if(client, [](const QChar &c) { return !c.isPrint(); });

    return client;
}

QString PeerInfo::peerIdClient() const
{
    // when peer ID is not known yet it contains only zero bytes,
    // do not create string in such case, return empty string instead
    if (m_nativeInfo.pid.is_all_zeros())
        return {};

    QString result;

    // interesting part of a typical peer ID is first 8 chars
    for (int i = 0; i < 8; ++i)
    {
        const std::uint8_t c = m_nativeInfo.pid[i];

        // ensure that the peer ID slice consists only of printable ASCII characters,
        // this should filter out most of the improper IDs
        if ((c < 32) || (c > 126))
            return tr("Unknown");

        result += QChar::fromLatin1(c);
    }

    return result;
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
    return LT::toQBitArray(m_nativeInfo.pieces);
}

QString PeerInfo::connectionType() const
{
    if (m_nativeInfo.flags & lt::peer_info::utp_socket)
        return C_UTP;

    return (m_nativeInfo.connection_type == lt::peer_info::standard_bittorrent)
        ? u"BT"_s
        : u"Web"_s;
}

qreal PeerInfo::calcRelevance(const QBitArray &allPieces) const
{
    const int localMissing = allPieces.count(false);
    if (localMissing <= 0)
        return 0;

    const QBitArray peerPieces = pieces();
    const int remoteHaves = (peerPieces & (~allPieces)).count(true);
    return static_cast<qreal>(remoteHaves) / localMissing;
}

qreal PeerInfo::relevance() const
{
    return m_relevance;
}

void PeerInfo::determineFlags()
{
    const auto updateFlags = [this](const QChar specifier, const QString &explanation)
    {
        m_flags += (specifier + u' ');
        m_flagsDescription += u"%1 = %2\n"_s.arg(specifier, explanation);
    };

    if (isInteresting())
    {
        if (isRemoteChocked())
        {
            // d = Your client wants to download, but peer doesn't want to send (interested and choked)
            updateFlags(u'd', tr("Interested (local) and choked (peer)"));
        }
        else
        {
            // D = Currently downloading (interested and not choked)
            updateFlags(u'D', tr("Interested (local) and unchoked (peer)"));
        }
    }

    if (isRemoteInterested())
    {
        if (isChocked())
        {
            // u = Peer wants your client to upload, but your client doesn't want to (interested and choked)
            updateFlags(u'u', tr("Interested (peer) and choked (local)"));
        }
        else
        {
            // U = Currently uploading (interested and not choked)
            updateFlags(u'U', tr("Interested (peer) and unchoked (local)"));
        }
    }

    // K = Peer is unchoking your client, but your client is not interested
    if (!isRemoteChocked() && !isInteresting())
        updateFlags(u'K', tr("Not interested (local) and unchoked (peer)"));

    // ? = Your client unchoked the peer but the peer is not interested
    if (!isChocked() && !isRemoteInterested())
        updateFlags(u'?', tr("Not interested (peer) and unchoked (local)"));

    // O = Optimistic unchoke
    if (optimisticUnchoke())
        updateFlags(u'O', tr("Optimistic unchoke"));

    // S = Peer is snubbed
    if (isSnubbed())
        updateFlags(u'S', tr("Peer snubbed"));

    // I = Peer is an incoming connection
    if (!isLocalConnection())
        updateFlags(u'I', tr("Incoming connection"));

    // H = Peer was obtained through DHT
    if (fromDHT())
        updateFlags(u'H', tr("Peer from DHT"));

    // X = Peer was included in peerlists obtained through Peer Exchange (PEX)
    if (fromPeX())
        updateFlags(u'X', tr("Peer from PEX"));

    // L = Peer is local
    if (fromLSD())
        updateFlags(u'L', tr("Peer from LSD"));

    // E = Peer is using Protocol Encryption (all traffic)
    if (isRC4Encrypted())
        updateFlags(u'E', tr("Encrypted traffic"));

    // e = Peer is using Protocol Encryption (handshake)
    if (isPlaintextEncrypted())
        updateFlags(u'e', tr("Encrypted handshake"));

    // P = Peer is using uTorrent uTP
    if (useUTPSocket())
        updateFlags(u'P', C_UTP);

    // h = Peer is using NAT hole punching
    if (isHolepunched())
        updateFlags(u'h', tr("Peer is using NAT hole punching"));

    m_flags.chop(1);
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

int PeerInfo::downloadingPieceIndex() const
{
    return static_cast<int>(m_nativeInfo.downloading_piece_index);
}
