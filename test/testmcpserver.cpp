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

#include <QObject>
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/global.h"
#include "webui/mcp/mcpprotocol.h"
#include "webui/mcp/mcpserver.h"

class TestMCPServer final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestMCPServer)

public:
    TestMCPServer() = default;

private:
    static QByteArray toJson(const QJsonObject &o)
    {
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }

    static QJsonObject parse(const QByteArray &b)
    {
        return QJsonDocument::fromJson(b).object();
    }

    static QJsonObject initRequest(int id = 1)
    {
        return {
            {u"jsonrpc"_s, u"2.0"_s},
            {u"id"_s, id},
            {u"method"_s, u"initialize"_s},
            {u"params"_s, QJsonObject{
                {u"protocolVersion"_s, u"2025-06-18"_s},
                {u"capabilities"_s, QJsonObject{}},
                {u"clientInfo"_s, QJsonObject{{u"name"_s, u"test"_s}, {u"version"_s, u"0"_s}}}
            }}
        };
    }

private slots:
    void getReturns405() const
    {
        MCP::Server srv;
        const auto resp = srv.handle("GET", QHostAddress::LocalHost, {}, {}, {});
        QCOMPARE(resp.httpStatus, 405);
        QCOMPARE(resp.headers.value("Allow"), QByteArray("POST, DELETE"));
    }

    void malformedJsonReturnsParseError() const
    {
        MCP::Server srv;
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, "{not json");
        QCOMPARE(resp.httpStatus, 200);
        const QJsonObject env = parse(resp.body);
        QCOMPARE(env.value(u"error"_s).toObject().value(u"code"_s).toInt(),
                 MCP::Protocol::ERR_PARSE);
    }

    void batchBodyReturns400() const
    {
        MCP::Server srv;
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, "[]");
        QCOMPARE(resp.httpStatus, 400);
    }

    void initializeIssuesSessionId() const
    {
        MCP::Server srv;
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        QCOMPARE(resp.httpStatus, 200);
        QVERIFY(resp.headers.contains(MCP::Protocol::HEADER_SESSION_ID));
        QVERIFY(!resp.headers.value(MCP::Protocol::HEADER_SESSION_ID).isEmpty());

        const QJsonObject env = parse(resp.body);
        const QJsonObject result = env.value(u"result"_s).toObject();
        QCOMPARE(result.value(u"protocolVersion"_s).toString(), u"2025-06-18"_s);
        QVERIFY(result.contains(u"capabilities"_s));
        QVERIFY(result.contains(u"serverInfo"_s));
    }

    void initialize_negotiatesUnsupportedVersionToLatest() const
    {
        MCP::Server srv;
        QJsonObject req = initRequest();
        QJsonObject params = req.value(u"params"_s).toObject();
        params.insert(u"protocolVersion"_s, u"1999-01-01"_s);
        req.insert(u"params"_s, params);

        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(req));
        const QJsonObject env = parse(resp.body);
        QCOMPARE(env.value(u"result"_s).toObject().value(u"protocolVersion"_s).toString(),
                 u"2025-06-18"_s);
    }

    void notification_returns202NoBody() const
    {
        MCP::Server srv;
        const QJsonObject req {
            {u"jsonrpc"_s, u"2.0"_s},
            {u"method"_s, u"notifications/initialized"_s}
        };
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, u"2025-06-18"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 202);
        QVERIFY(resp.body.isEmpty());
    }

    void unknownMethodReturnsNotFound() const
    {
        MCP::Server srv;
        const auto initResp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        const QString sid = QString::fromUtf8(initResp.headers.value(MCP::Protocol::HEADER_SESSION_ID));

        const QJsonObject req {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 2}, {u"method"_s, u"doesNotExist"_s}};
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, sid, u"2025-06-18"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 200);
        QCOMPARE(parse(resp.body).value(u"error"_s).toObject().value(u"code"_s).toInt(),
                 MCP::Protocol::ERR_METHOD_NOT_FOUND);
    }

    void missingSessionAfterInitializeReturns400() const
    {
        MCP::Server srv;
        const QJsonObject req {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 1}, {u"method"_s, u"ping"_s}};
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, {}, u"2025-06-18"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 400);
    }

    void pingReturnsEmptySuccess() const
    {
        MCP::Server srv;
        const auto initResp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        const QString sid = QString::fromUtf8(initResp.headers.value(MCP::Protocol::HEADER_SESSION_ID));

        const QJsonObject req {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 2}, {u"method"_s, u"ping"_s}};
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, sid, u"2025-06-18"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 200);
        QVERIFY(parse(resp.body).contains(u"result"_s));
    }

    void deleteWithSessionReturns200AndRemovesSession() const
    {
        MCP::Server srv;
        const auto initResp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        const QString sid = QString::fromUtf8(initResp.headers.value(MCP::Protocol::HEADER_SESSION_ID));

        const auto delResp = srv.handle("DELETE", QHostAddress::LocalHost, sid, {}, {});
        QCOMPARE(delResp.httpStatus, 200);

        // Session should be gone — subsequent ping returns 400 Missing/invalid session.
        const QJsonObject ping {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 99}, {u"method"_s, u"ping"_s}};
        const auto pingResp = srv.handle("POST", QHostAddress::LocalHost, sid, u"2025-06-18"_s, toJson(ping));
        QCOMPARE(pingResp.httpStatus, 400);
    }

    void unsupportedProtocolVersionHeaderReturns400() const
    {
        MCP::Server srv;
        const auto initResp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        const QString sid = QString::fromUtf8(initResp.headers.value(MCP::Protocol::HEADER_SESSION_ID));

        const QJsonObject req {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 2}, {u"method"_s, u"ping"_s}};
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, sid, u"1999-01-01"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 400);
    }

    void toolsCallWithoutRegistryReturnsInvalidParams() const
    {
        MCP::Server srv;
        const auto initResp = srv.handle("POST", QHostAddress::LocalHost, {}, {}, toJson(initRequest()));
        const QString sid = QString::fromUtf8(initResp.headers.value(MCP::Protocol::HEADER_SESSION_ID));

        const QJsonObject req {
            {u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, 2}, {u"method"_s, u"tools/call"_s},
            {u"params"_s, QJsonObject{{u"name"_s, u"nothing"_s}, {u"arguments"_s, QJsonObject{}}}}
        };
        const auto resp = srv.handle("POST", QHostAddress::LocalHost, sid, u"2025-06-18"_s, toJson(req));
        QCOMPARE(resp.httpStatus, 200);
        QCOMPARE(parse(resp.body).value(u"error"_s).toObject().value(u"code"_s).toInt(),
                 MCP::Protocol::ERR_INVALID_PARAMS);
    }
};

QTEST_APPLESS_MAIN(TestMCPServer)
#include "testmcpserver.moc"
