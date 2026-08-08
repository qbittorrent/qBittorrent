/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/http/request.h"
#include "base/http/requestparser.h"

using namespace Http;

namespace
{
    QByteArray makeRequest(const QByteArray &method, const QByteArray &target
            , const QByteArray &contentType, const QByteArray &body)
    {
        QByteArray request = method + ' ' + target + " HTTP/1.1\r\n";
        request += "Host: localhost\r\n";
        if (!contentType.isEmpty())
            request += "Content-Type: " + contentType + "\r\n";
        request += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        request += "\r\n";
        request += body;
        return request;
    }

    RequestParser::ParseResult parsePost(const QByteArray &contentType, const QByteArray &body)
    {
        return RequestParser::parse(makeRequest("POST", "/api/v2/test", contentType, body));
    }
}

class TestHttpRequestParser final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestHttpRequestParser)

public:
    TestHttpRequestParser() = default;

private slots:
    void testJsonScalars() const
    {
        const auto result = parsePost("application/json", R"({"a":"x","b":5,"c":true,"d":false})");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(result.request.isJson);
        QCOMPARE(result.request.posts.value(u"a"_s), u"x"_s);
        QCOMPARE(result.request.posts.value(u"b"_s), u"5"_s);
        QCOMPARE(result.request.posts.value(u"c"_s), u"true"_s);
        QCOMPARE(result.request.posts.value(u"d"_s), u"false"_s);
    }

    void testJsonBodyPreservesType() const
    {
        // the typed body is kept (so list params read as arrays); posts holds the flattened scalar forms
        const auto result = parsePost("application/json", R"({"hashes":["a","b"],"name":"x"})");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(result.request.jsonBody.value(u"hashes"_s).isArray());
        QCOMPARE(result.request.jsonBody.value(u"hashes"_s).toArray().size(), 2);
        QVERIFY(result.request.jsonBody.value(u"name"_s).isString());
        QCOMPARE(result.request.posts.value(u"name"_s), u"x"_s);
    }

    void testJsonNested() const
    {
        // nested arrays/objects become compact JSON
        const auto result = parsePost("application/json", R"({"obj":{"k":1},"arr":[1,2]})");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.posts.value(u"obj"_s), u"{\"k\":1}"_s);
        QCOMPARE(result.request.posts.value(u"arr"_s), u"[1,2]"_s);
    }

    void testJsonNullSkipped() const
    {
        // a JSON null is treated as an omitted field
        const auto result = parsePost("application/json", R"({"a":"x","b":null})");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(result.request.posts.contains(u"a"_s));
        QVERIFY(!result.request.posts.contains(u"b"_s));
    }

    void testJsonEmptyObject() const
    {
        const auto result = parsePost("application/json", "{}");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(result.request.isJson);
        QVERIFY(result.request.posts.isEmpty());
    }

    void testJsonEmptyBody() const
    {
        // an empty application/json body is treated as an empty object
        const auto result = parsePost("application/json", "");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(result.request.isJson);
        QVERIFY(result.request.posts.isEmpty());
    }

    void testInvalidJson() const
    {
        const auto result = parsePost("application/json", "{bad");
        QCOMPARE(result.status, RequestParser::ParseStatus::BadRequest);
    }

    void testNonObjectJson() const
    {
        // a non-object top-level value is rejected
        QCOMPARE(parsePost("application/json", "[1,2]").status, RequestParser::ParseStatus::BadRequest);
        QCOMPARE(parsePost("application/json", "\"x\"").status, RequestParser::ParseStatus::BadRequest);
        QCOMPARE(parsePost("application/json", "5").status, RequestParser::ParseStatus::BadRequest);
    }

    void testJsonContentTypeWithCharset() const
    {
        const auto result = parsePost("application/json; charset=utf-8", R"({"a":"x"})");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.posts.value(u"a"_s), u"x"_s);
    }

    void testJsonUtf8Value() const
    {
        const auto result = parsePost("application/json", "{\"name\":\"caf\xC3\xA9\"}");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.posts.value(u"name"_s), QString::fromUtf8("caf\xC3\xA9"));
    }

    void testFormRegression() const
    {
        const auto result = parsePost("application/x-www-form-urlencoded", "a=x&b=5&c=true");
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QVERIFY(!result.request.isJson);
        QCOMPARE(result.request.posts.value(u"a"_s), u"x"_s);
        QCOMPARE(result.request.posts.value(u"b"_s), u"5"_s);
        QCOMPARE(result.request.posts.value(u"c"_s), u"true"_s);
    }

    void testFormAndJsonScalarsAreEquivalent() const
    {
        // form and equivalent JSON requests produce identical params
        const auto formResult = parsePost("application/x-www-form-urlencoded", "a=x&b=5&c=true&d=false");
        const auto jsonResult = parsePost("application/json", R"({"a":"x","b":5,"c":true,"d":false})");
        QCOMPARE(formResult.status, RequestParser::ParseStatus::OK);
        QCOMPARE(jsonResult.status, RequestParser::ParseStatus::OK);
        QVERIFY(formResult.request.posts == jsonResult.request.posts);
    }

    void testGetQueryUnaffected() const
    {
        const auto result = RequestParser::parse(makeRequest("GET", "/api/v2/test?a=b&c=d", "", ""));
        QCOMPARE(result.status, RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.query.value(u"a"_s), QByteArray("b"));
        QCOMPARE(result.request.query.value(u"c"_s), QByteArray("d"));
        QVERIFY(result.request.posts.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestHttpRequestParser)
#include "testhttprequestparser.moc"
