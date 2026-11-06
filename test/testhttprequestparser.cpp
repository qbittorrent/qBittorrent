/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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
#include <QObject>
#include <QString>
#include <QTest>

#include "base/http/requestparser.h"

using namespace Qt::Literals::StringLiterals;

class TestHttpRequestParser final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestHttpRequestParser)

public:
    TestHttpRequestParser() = default;

private slots:
    void testQueryParameters_data() const
    {
        QTest::addColumn<QByteArray>("encodedName");
        QTest::addColumn<QByteArray>("encodedValue");
        QTest::addColumn<QString>("name");
        QTest::addColumn<QByteArray>("value");

        const auto addRow = [](const char *label, const QByteArray &encodedName,
                               const QByteArray &encodedValue, const QString &name,
                               const QByteArray &value) -> void
        {
            QTest::newRow(label) << encodedName << encodedValue << name << value;
        };
        addRow("plain", "name", "value", u"name"_s, "value");
        addRow("form-plus", "display+name", "one+two", u"display name"_s, "one two");
        addRow("escaped-plus-name", "display%2Bname", "value", u"display+name"_s, "value");
        addRow("escaped-plus-value", "name", "one%2Btwo", u"name"_s, "one+two");
        addRow("mixed-plus", "ke%2By+name", "one%2Btwo+three", u"ke+y name"_s, "one+two three");
        addRow("double-encoded-plus", "name", "one%252Btwo", u"name"_s, "one%2Btwo");
        addRow("encoded-space", "name", "one%20two", u"name"_s, "one two");
        addRow("encoded-separators", "name", "a%26b%3Dc%3Fd%23e", u"name"_s, "a&b=c?d#e");
        addRow("encoded-percent", "name", "%25", u"name"_s, "%");
        addRow("utf8-plus", "%E5%90%8D", "%E5%80%BC%2B", u"名"_s, QByteArray::fromHex("e580bc2b"));
    }

    void testQueryParameters() const
    {
        QFETCH(const QByteArray, encodedName);
        QFETCH(const QByteArray, encodedValue);
        QFETCH(const QString, name);
        QFETCH(const QByteArray, value);

        const QByteArray data = "GET /?" + encodedName + '=' + encodedValue
            + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
        const Http::RequestParser::ParseResult result = Http::RequestParser::parse(data);
        QCOMPARE(result.status, Http::RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.query.size(), 1);
        QVERIFY(result.request.query.contains(name));
        QCOMPARE(result.request.query.value(name), value);
    }

    void testPostParameters_data() const
    {
        testQueryParameters_data();
    }

    void testPostParameters() const
    {
        QFETCH(const QByteArray, encodedName);
        QFETCH(const QByteArray, encodedValue);
        QFETCH(const QString, name);
        QFETCH(const QByteArray, value);

        const QByteArray body = encodedName + '=' + encodedValue;
        const QByteArray data = "POST / HTTP/1.1\r\nHost: localhost\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: "
            + QByteArray::number(body.size()) + "\r\n\r\n" + body;
        const Http::RequestParser::ParseResult result = Http::RequestParser::parse(data);
        QCOMPARE(result.status, Http::RequestParser::ParseStatus::OK);
        QCOMPARE(result.request.posts.size(), 1);
        QVERIFY(result.request.posts.contains(name));
        QCOMPARE(result.request.posts.value(name), QString::fromUtf8(value));
    }
};

QTEST_APPLESS_MAIN(TestHttpRequestParser)
#include "testhttprequestparser.moc"
