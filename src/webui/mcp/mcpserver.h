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
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QString>

#include "mcpsessionmanager.h"

class IApplication;

namespace MCP
{
    struct ServerResponse
    {
        int httpStatus = 200;
        QHash<QByteArray, QByteArray> headers;  // additional headers beyond Content-Type
        QByteArray body;
    };

    class Server
    {
    public:
        Q_DISABLE_COPY_MOVE(Server)

        explicit Server(IApplication *app = nullptr);

        void setApplication(IApplication *app) { m_app = app; }

        /**
         * Handle a single MCP HTTP request.
         * @param method         HTTP method ("POST", "DELETE", "GET", ...).
         * @param remote         client IP.
         * @param sessionHeader  value of Mcp-Session-Id header (may be empty).
         * @param versionHeader  value of MCP-Protocol-Version header (may be empty).
         * @param body           raw request body (for POST).
         */
        ServerResponse handle(const QByteArray &method,
                              const QHostAddress &remote,
                              const QString &sessionHeader,
                              const QString &versionHeader,
                              const QByteArray &body);

    private:
        IApplication *m_app = nullptr;
        SessionManager m_sessions;

        ServerResponse handlePost(const QHostAddress &remote,
                                  const QString &sessionHeader,
                                  const QString &versionHeader,
                                  const QByteArray &body);
        ServerResponse handleDelete(const QString &sessionHeader);
        QJsonObject dispatchJsonRpc(const QJsonObject &req,
                                    const QHostAddress &remote,
                                    QString *newSessionId);
    };
}
