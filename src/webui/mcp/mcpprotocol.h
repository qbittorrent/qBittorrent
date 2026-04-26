/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Ortes <malo.allee@gmail.com>
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

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "base/global.h"

namespace MCP::Protocol
{
    // URL path
    inline const QString PATH = u"/mcp/"_s;

    // Supported spec versions, newest first
    inline const QStringList SUPPORTED_VERSIONS = {u"2025-06-18"_s};
    inline const QString DEFAULT_BACKWARD_COMPAT_VERSION = u"2025-03-26"_s;

    // HTTP header names (canonical casing per spec)
    inline const QByteArray HEADER_SESSION_ID = "Mcp-Session-Id";
    inline const QByteArray HEADER_PROTOCOL_VERSION = "MCP-Protocol-Version";

    // Session caps
    inline constexpr int SESSION_IDLE_TIMEOUT_SECONDS = 30 * 60;
    inline constexpr int SESSION_CAP_PER_IP = 10;
    inline constexpr int SESSION_CAP_GLOBAL = 100;

    // JSON-RPC error codes
    inline constexpr int ERR_PARSE = -32700;
    inline constexpr int ERR_INVALID_REQUEST = -32600;
    inline constexpr int ERR_METHOD_NOT_FOUND = -32601;
    inline constexpr int ERR_INVALID_PARAMS = -32602;
    inline constexpr int ERR_INTERNAL = -32603;
}
