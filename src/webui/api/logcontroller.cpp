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

#include "logcontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/string.h"

const char KEY_LOG_ID[] = "id";
const char KEY_LOG_TIMESTAMP[] = "timestamp";
const char KEY_LOG_MSG_TYPE[] = "type";
const char KEY_LOG_MSG_MESSAGE[] = "message";
const char KEY_LOG_PEER_IP[] = "ip";
const char KEY_LOG_PEER_BLOCKED[] = "blocked";
const char KEY_LOG_PEER_REASON[] = "reason";

// Returns the log in JSON format.
// The return value is an array of dictionaries.
// The dictionary keys are:
//   - "id": id of the message
//   - "timestamp": milliseconds since epoch
//   - "type": type of the message (int, see MsgType)
//   - "message": text of the message
// GET params:
//   - normal (bool): include normal messages (default true)
//   - info (bool): include info messages (default true)
//   - warning (bool): include warning messages (default true)
//   - critical (bool): include critical messages (default true)
//   - last_known_id (int): exclude messages with id <= 'last_known_id' (default -1)
void LogController::mainAction()
{
    using Utils::String::parseBool;

    const bool isNormal = parseBool(params()["normal"]).value_or(true);
    const bool isInfo = parseBool(params()["info"]).value_or(true);
    const bool isWarning = parseBool(params()["warning"]).value_or(true);
    const bool isCritical = parseBool(params()["critical"]).value_or(true);

    bool ok = false;
    int lastKnownId = params()["last_known_id"].toInt(&ok);
    if (!ok)
        lastKnownId = -1;

    Logger *const logger = Logger::instance();
    QJsonArray msgList;

    for (const Log::Msg &msg : asConst(logger->getMessages(lastKnownId)))
    {
        if (!(((msg.type == Log::NORMAL) && isNormal)
              || ((msg.type == Log::INFO) && isInfo)
              || ((msg.type == Log::WARNING) && isWarning)
              || ((msg.type == Log::CRITICAL) && isCritical)))
            continue;

        msgList.append(QJsonObject
        {
            {QLatin1String(KEY_LOG_ID), msg.id},
            {QLatin1String(KEY_LOG_TIMESTAMP), msg.timestamp},
            {QLatin1String(KEY_LOG_MSG_TYPE), msg.type},
            {QLatin1String(KEY_LOG_MSG_MESSAGE), msg.message}
        });
    }

    setResult(msgList);
}

// Returns the peer log in JSON format.
// The return value is an array of dictionaries.
// The dictionary keys are:
//   - "id": id of the message
//   - "timestamp": milliseconds since epoch
//   - "ip": IP of the peer
//   - "blocked": whether or not the peer was blocked
//   - "reason": reason of the block
// GET params:
//   - last_known_id (int): exclude messages with id <= 'last_known_id' (default -1)
void LogController::peersAction()
{
    bool ok = false;
    int lastKnownId = params()["last_known_id"].toInt(&ok);
    if (!ok)
        lastKnownId = -1;

    Logger *const logger = Logger::instance();
    QJsonArray peerList;

    for (const Log::Peer &peer : asConst(logger->getPeers(lastKnownId)))
    {
        peerList.append(QJsonObject
        {
            {QLatin1String(KEY_LOG_ID), peer.id},
            {QLatin1String(KEY_LOG_TIMESTAMP), peer.timestamp},
            {QLatin1String(KEY_LOG_PEER_IP), peer.ip},
            {QLatin1String(KEY_LOG_PEER_BLOCKED), peer.blocked},
            {QLatin1String(KEY_LOG_PEER_REASON), peer.reason}
        });
    }

    setResult(peerList);
}
