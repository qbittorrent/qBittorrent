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

#include "mcpserver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "base/global.h"
#include "base/version.h"
#include "webui/api/apierror.h"
#include "mcperror.h"
#include "mcpprotocol.h"
#include "mcptoolregistry.h"

namespace
{
    QByteArray dumpJson(const QJsonObject &o)
    {
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }

    bool isNotification(const QJsonObject &req)
    {
        return !req.contains(u"id"_s);
    }

    QString negotiateVersion(const QString &requested)
    {
        if (MCP::Protocol::SUPPORTED_VERSIONS.contains(requested))
            return requested;
        return MCP::Protocol::SUPPORTED_VERSIONS.first();
    }
}

MCP::Server::Server(IApplication *app)
    : m_app(app)
{
}

MCP::ServerResponse MCP::Server::handle(const QByteArray &method,
                                         const QHostAddress &remote,
                                         const QString &sessionHeader,
                                         const QString &versionHeader,
                                         const QByteArray &body)
{
    if (method == "GET")
    {
        ServerResponse r;
        r.httpStatus = 405;
        r.headers.insert("Allow", "POST, DELETE");
        return r;
    }
    if (method == "DELETE")
        return handleDelete(sessionHeader);
    if (method == "POST")
        return handlePost(remote, sessionHeader, versionHeader, body);

    ServerResponse r;
    r.httpStatus = 405;
    r.headers.insert("Allow", "POST, DELETE");
    return r;
}

MCP::ServerResponse MCP::Server::handleDelete(const QString &sessionHeader)
{
    if (!sessionHeader.isEmpty())
        m_sessions.remove(sessionHeader);
    ServerResponse r;
    r.httpStatus = 200;
    return r;
}

MCP::ServerResponse MCP::Server::handlePost(const QHostAddress &remote,
                                             const QString &sessionHeader,
                                             const QString &versionHeader,
                                             const QByteArray &body)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError)
    {
        ServerResponse r;
        r.headers.insert("Content-Type", "application/json");
        r.body = dumpJson(makeJsonRpcError(QJsonValue::Null, Protocol::ERR_PARSE, parseErr.errorString()));
        return r;
    }
    if (doc.isArray())
    {
        ServerResponse r;
        r.httpStatus = 400;
        r.body = "Batch requests are not supported";
        return r;
    }
    if (!doc.isObject())
    {
        ServerResponse r;
        r.headers.insert("Content-Type", "application/json");
        r.body = dumpJson(makeJsonRpcError(QJsonValue::Null, Protocol::ERR_INVALID_REQUEST,
                                           u"Expected JSON-RPC object"_s));
        return r;
    }

    const QJsonObject req = doc.object();
    const QString method = req.value(u"method"_s).toString();

    // Notifications: 202 with no body
    if (isNotification(req))
    {
        ServerResponse r;
        r.httpStatus = 202;
        return r;
    }

    // Session & version validation for non-initialize methods
    if (method != u"initialize"_s)
    {
        if (sessionHeader.isEmpty() || !m_sessions.find(sessionHeader).has_value())
        {
            ServerResponse r;
            r.httpStatus = 400;
            r.body = "Missing or invalid Mcp-Session-Id";
            return r;
        }
        const QString effectiveVersion = versionHeader.isEmpty()
            ? Protocol::DEFAULT_BACKWARD_COMPAT_VERSION : versionHeader;
        if (!Protocol::SUPPORTED_VERSIONS.contains(effectiveVersion)
            && effectiveVersion != Protocol::DEFAULT_BACKWARD_COMPAT_VERSION)
        {
            ServerResponse r;
            r.httpStatus = 400;
            r.body = "Unsupported MCP-Protocol-Version";
            return r;
        }
        m_sessions.touch(sessionHeader);
    }

    // Dispatch JSON-RPC
    QString newSessionId;
    const QJsonObject respEnv = dispatchJsonRpc(req, remote, &newSessionId);

    ServerResponse r;
    r.headers.insert("Content-Type", "application/json");
    if (!newSessionId.isEmpty())
        r.headers.insert(Protocol::HEADER_SESSION_ID, newSessionId.toUtf8());
    r.body = dumpJson(respEnv);
    return r;
}

QJsonObject MCP::Server::dispatchJsonRpc(const QJsonObject &req,
                                          const QHostAddress &remote,
                                          QString *newSessionId)
{
    const QJsonValue id = req.value(u"id"_s);
    const QString method = req.value(u"method"_s).toString();
    const QJsonObject params = req.value(u"params"_s).toObject();

    if (method == u"initialize"_s)
    {
        const QString negotiated = negotiateVersion(params.value(u"protocolVersion"_s).toString());
        const QString sid = m_sessions.create(remote, negotiated);
        if (sid.isEmpty())
            return makeJsonRpcError(id, Protocol::ERR_INTERNAL, u"Session cap exceeded"_s);
        *newSessionId = sid;
        return makeJsonRpcResult(id, QJsonObject{
            {u"protocolVersion"_s, negotiated},
            {u"capabilities"_s, QJsonObject{{u"tools"_s, QJsonObject{{u"listChanged"_s, false}}}}},
            {u"serverInfo"_s, QJsonObject{
                {u"name"_s, u"qBittorrent"_s},
                {u"version"_s, QString::fromLatin1(QBT_VERSION)},
                {u"title"_s, u"qBittorrent MCP Server"_s}
            }}
        });
    }
    if (method == u"ping"_s)
        return makeJsonRpcResult(id, QJsonObject{});
    if (method == u"tools/list"_s)
    {
        return makeJsonRpcResult(id, QJsonObject{
            {u"tools"_s, ToolRegistry::instance().asCatalog()}
        });
    }
    if (method == u"tools/call"_s)
    {
        const QString toolName = params.value(u"name"_s).toString();
        const QJsonObject arguments = params.value(u"arguments"_s).toObject();
        const ToolDescriptor *tool = ToolRegistry::instance().find(toolName);
        if (!tool)
            return makeJsonRpcError(id, Protocol::ERR_INVALID_PARAMS,
                u"Unknown tool: %1"_s.arg(toolName));

        return makeJsonRpcResult(id, dispatchToolCall(*tool, arguments));
    }

    return makeJsonRpcError(id, Protocol::ERR_METHOD_NOT_FOUND,
        u"Method not found: %1"_s.arg(method));
}

QJsonObject MCP::Server::dispatchToolCall(const ToolDescriptor &tool, const QJsonObject &arguments)
{
    try
    {
        const APIResult r = tool.handler(arguments);

        // Serialize result.
        // MCP 2025-06-18 requires `structuredContent` to be a JSON object (a
        // record-like map from string keys to values). JSON arrays are NOT
        // valid at that position. Wrap array-shaped results under the `items`
        // key so clients can still access them structurally while staying
        // spec-compliant.
        QJsonValue structured;
        QString textRepr;
        if (r.data.canConvert<QJsonDocument>())
        {
            const QJsonDocument doc = r.data.toJsonDocument();
            if (doc.isObject())
                structured = doc.object();
            else if (doc.isArray())
                structured = QJsonObject{{u"items"_s, doc.array()}};
            textRepr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        }
        else if (r.data.metaType() == QMetaType::fromType<QByteArray>())
        {
            const QByteArray raw = r.data.toByteArray();
            return {
                {u"isError"_s, false},
                {u"content"_s, QJsonArray{QJsonObject{
                    {u"type"_s, u"resource"_s},
                    {u"resource"_s, QJsonObject{
                        {u"mimeType"_s, r.mimeType.isEmpty() ? u"application/octet-stream"_s : r.mimeType},
                        {u"blob"_s, QString::fromUtf8(raw.toBase64())}
                    }}
                }}}
            };
        }
        else
        {
            textRepr = r.data.toString();
        }

        QJsonObject out{
            {u"isError"_s, false},
            {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, textRepr}}}}
        };
        if (!structured.isNull() && !structured.isUndefined())
            out.insert(u"structuredContent"_s, structured);
        return out;
    }
    catch (const APIError &e)
    {
        return {
            {u"isError"_s, true},
            {u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, e.message()}}}}
        };
    }
}
