/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "transfercontroller.h"

#include <QJsonObject>
#include <QVector>

#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/global.h"
#include "apierror.h"

const char KEY_TRANSFER_DLSPEED[] = "dl_info_speed";
const char KEY_TRANSFER_DLDATA[] = "dl_info_data";
const char KEY_TRANSFER_DLRATELIMIT[] = "dl_rate_limit";
const char KEY_TRANSFER_UPSPEED[] = "up_info_speed";
const char KEY_TRANSFER_UPDATA[] = "up_info_data";
const char KEY_TRANSFER_UPRATELIMIT[] = "up_rate_limit";
const char KEY_TRANSFER_DHT_NODES[] = "dht_nodes";
const char KEY_TRANSFER_CONNECTION_STATUS[] = "connection_status";

// Returns the global transfer information in JSON format.
// The return value is a JSON-formatted dictionary.
// The dictionary keys are:
//   - "dl_info_speed": Global download rate
//   - "dl_info_data": Data downloaded this session
//   - "up_info_speed": Global upload rate
//   - "up_info_data": Data uploaded this session
//   - "dl_rate_limit": Download rate limit
//   - "up_rate_limit": Upload rate limit
//   - "dht_nodes": DHT nodes connected to
//   - "connection_status": Connection status
void TransferController::infoAction()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    QJsonObject dict;

    dict[KEY_TRANSFER_DLSPEED] = static_cast<qint64>(sessionStatus.payloadDownloadRate);
    dict[KEY_TRANSFER_DLDATA] = static_cast<qint64>(sessionStatus.totalPayloadDownload);
    dict[KEY_TRANSFER_UPSPEED] = static_cast<qint64>(sessionStatus.payloadUploadRate);
    dict[KEY_TRANSFER_UPDATA] = static_cast<qint64>(sessionStatus.totalPayloadUpload);
    dict[KEY_TRANSFER_DLRATELIMIT] = BitTorrent::Session::instance()->downloadSpeedLimit();
    dict[KEY_TRANSFER_UPRATELIMIT] = BitTorrent::Session::instance()->uploadSpeedLimit();
    dict[KEY_TRANSFER_DHT_NODES] = static_cast<qint64>(sessionStatus.dhtNodes);
    if (!BitTorrent::Session::instance()->isListening())
        dict[KEY_TRANSFER_CONNECTION_STATUS] = QLatin1String("disconnected");
    else
        dict[KEY_TRANSFER_CONNECTION_STATUS] = QLatin1String(sessionStatus.hasIncomingConnections ? "connected" : "firewalled");

    setResult(dict);
}

void TransferController::uploadLimitAction()
{
    setResult(QString::number(BitTorrent::Session::instance()->uploadSpeedLimit()));
}

void TransferController::downloadLimitAction()
{
    setResult(QString::number(BitTorrent::Session::instance()->downloadSpeedLimit()));
}

void TransferController::setUploadLimitAction()
{
    requireParams({"limit"});
    qlonglong limit = params()["limit"].toLongLong();
    if (limit == 0) limit = -1;

    BitTorrent::Session::instance()->setUploadSpeedLimit(limit);
}

void TransferController::setDownloadLimitAction()
{
    requireParams({"limit"});
    qlonglong limit = params()["limit"].toLongLong();
    if (limit == 0) limit = -1;

    BitTorrent::Session::instance()->setDownloadSpeedLimit(limit);
}

void TransferController::toggleSpeedLimitsModeAction()
{
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setAltGlobalSpeedLimitEnabled(!session->isAltGlobalSpeedLimitEnabled());
}

void TransferController::speedLimitsModeAction()
{
    setResult(QString::number(BitTorrent::Session::instance()->isAltGlobalSpeedLimitEnabled()));
}

void TransferController::banPeersAction()
{
    requireParams({"peers"});

    const QStringList peers = params()["peers"].split('|');
    for (const QString &peer : peers)
    {
        const BitTorrent::PeerAddress addr = BitTorrent::PeerAddress::parse(peer.trimmed());
        if (!addr.ip.isNull())
            BitTorrent::Session::instance()->banIP(addr.ip.toString());
    }
}
