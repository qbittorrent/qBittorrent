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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/utils/json.h"

namespace
{
    // Parses `{"x": <json>}` and returns x with the real type the parser sees (notably ints as Double).
    QJsonValue jsonValue(const QByteArray &json)
    {
        return QJsonDocument::fromJson("{\"x\":" + json + "}").object().value(u"x"_s);
    }
}

class TestUtilsJson final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsJson)

public:
    TestUtilsJson() = default;

private slots:
    void testFlattenString() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("\"hello\"")), u"hello"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("\"\"")), u""_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("\"caf\xC3\xA9\"")), QString::fromUtf8("caf\xC3\xA9"));
    }

    void testFlattenBool() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("true")), u"true"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("false")), u"false"_s);
    }

    void testFlattenNumber() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("100")), u"100"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("0")), u"0"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("-1")), u"-1"_s);
        // large int64 value stays a plain decimal (no scientific notation)
        QCOMPARE(Utils::Json::flattenValue(jsonValue("10000000000")), u"10000000000"_s);
        // integral doubles drop the fraction
        QCOMPARE(Utils::Json::flattenValue(jsonValue("2.0")), u"2"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("1.5")), u"1.5"_s);
    }

    void testFlattenNullAndUndefined() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("null")), u""_s);
        QCOMPARE(Utils::Json::flattenValue(QJsonValue {QJsonValue::Undefined}), u""_s);
    }

    void testFlattenArray() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("[\"a\",\"b\"]")), u"[\"a\",\"b\"]"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("[1,2]")), u"[1,2]"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("[]")), u"[]"_s);
    }

    void testFlattenObject() const
    {
        QCOMPARE(Utils::Json::flattenValue(jsonValue("{\"a\":1}")), u"{\"a\":1}"_s);
        QCOMPARE(Utils::Json::flattenValue(jsonValue("{}")), u"{}"_s);
    }
};

QTEST_APPLESS_MAIN(TestUtilsJson)
#include "testutilsjson.moc"
