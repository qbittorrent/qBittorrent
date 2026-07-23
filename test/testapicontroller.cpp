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
#include <QChar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QTest>

#include "base/global.h"
#include "webui/api/apicontroller.h"
#include "webui/api/apierror.h"

// The parse helpers are protected and only valid during run(), so we drive them through Q_INVOKABLE
// actions; app() is never touched, so a null IApplication is fine.
class ParseListHarness final : public APIController
{
    Q_OBJECT

public:
    using APIController::APIController;

    QStringList listResult;

    Q_INVOKABLE void splitAction()
    {
        const QString sepStr = params().value(u"sep"_s);
        const QChar separator = sepStr.isEmpty() ? u'|' : sepStr.front();
        const Qt::SplitBehavior behavior = (params().value(u"skip"_s) == u"1"_s)
            ? Qt::SkipEmptyParts : Qt::KeepEmptyParts;
        listResult = parseList(u"value"_s, separator, behavior);
    }

    Q_INVOKABLE void urlListAction()
    {
        const QString sepStr = params().value(u"sep"_s);
        const QChar separator = sepStr.isEmpty() ? u'|' : sepStr.front();
        const Qt::SplitBehavior behavior = (params().value(u"keep"_s) == u"1"_s)
            ? Qt::KeepEmptyParts : Qt::SkipEmptyParts;
        listResult = parseUrlList(u"value"_s, separator, behavior);
    }

    Q_INVOKABLE void rejectArrayAction()
    {
        rejectArrayParam(u"value"_s);
    }
};

namespace
{
    QString skipFlag(const Qt::SplitBehavior behavior)
    {
        return (behavior == Qt::SkipEmptyParts) ? u"1"_s : u"0"_s;
    }

    QString keepFlag(const Qt::SplitBehavior behavior)
    {
        return (behavior == Qt::KeepEmptyParts) ? u"1"_s : u"0"_s;
    }

    // parses `json` into a QJsonValue with its real type
    QJsonValue toJsonValue(const QByteArray &json)
    {
        return QJsonDocument::fromJson("{\"v\":" + json + '}').object().value(u"v"_s);
    }

    QStringList parseListForm(const QString &value, const QChar separator, const Qt::SplitBehavior behavior)
    {
        ParseListHarness harness {nullptr};
        harness.run(u"split"_s, {.params = {{u"value"_s, value}, {u"sep"_s, QString(separator)}
            , {u"skip"_s, skipFlag(behavior)}}, .isJson = false});
        return harness.listResult;
    }

    QStringList parseListJson(const QJsonValue &value, const QChar separator, const Qt::SplitBehavior behavior)
    {
        ParseListHarness harness {nullptr};
        harness.run(u"split"_s, {.params = {{u"sep"_s, QString(separator)}, {u"skip"_s, skipFlag(behavior)}}
            , .isJson = true, .jsonBody = QJsonObject {{u"value"_s, value}}});
        return harness.listResult;
    }

    QStringList parseUrlListForm(const QString &value, const QChar separator
            , const Qt::SplitBehavior behavior = Qt::SkipEmptyParts)
    {
        ParseListHarness harness {nullptr};
        harness.run(u"urlList"_s, {.params = {{u"value"_s, value}, {u"sep"_s, QString(separator)}
            , {u"keep"_s, keepFlag(behavior)}}, .isJson = false});
        return harness.listResult;
    }

    QStringList parseUrlListJson(const QJsonValue &value, const QChar separator
            , const Qt::SplitBehavior behavior = Qt::SkipEmptyParts)
    {
        ParseListHarness harness {nullptr};
        harness.run(u"urlList"_s, {.params = {{u"sep"_s, QString(separator)}, {u"keep"_s, keepFlag(behavior)}}
            , .isJson = true, .jsonBody = QJsonObject {{u"value"_s, value}}});
        return harness.listResult;
    }
}

class TestApiController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestApiController)

public:
    TestApiController() = default;

private slots:
    // Core promise: a form delimited string and a JSON array produce the identical QStringList.
    void testListEquivalence_data() const
    {
        QTest::addColumn<QString>("formValue");
        QTest::addColumn<QByteArray>("jsonArray");
        QTest::addColumn<QChar>("separator");
        QTest::addColumn<bool>("skipEmpty");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("pipe")
            << u"a|b|c"_s << R"(["a","b","c"])"_ba << QChar(u'|') << false << QStringList {u"a"_s, u"b"_s, u"c"_s};
        // filePriorities: comma-delimited string vs JSON array of numbers
        QTest::newRow("comma numbers")
            << u"0,1,7"_s << "[0,1,7]"_ba << QChar(u',') << true << QStringList {u"0"_s, u"1"_s, u"7"_s};
        QTest::newRow("skip empty parts")
            << u"a||b"_s << R"(["a","","b"])"_ba << QChar(u'|') << true << QStringList {u"a"_s, u"b"_s};
        QTest::newRow("keep empty parts")
            << u"a||b"_s << R"(["a","","b"])"_ba << QChar(u'|') << false << QStringList {u"a"_s, u""_s, u"b"_s};
        // torrents/add `urls` and torrents/removeCategories `categories` split on '\n'
        QTest::newRow("newline")
            << u"a\nb\nc"_s << R"(["a","b","c"])"_ba << QChar(u'\n') << true << QStringList {u"a"_s, u"b"_s, u"c"_s};
    }

    void testListEquivalence() const
    {
        QFETCH(QString, formValue);
        QFETCH(QByteArray, jsonArray);
        QFETCH(QChar, separator);
        QFETCH(bool, skipEmpty);
        QFETCH(QStringList, expected);

        const Qt::SplitBehavior behavior = skipEmpty ? Qt::SkipEmptyParts : Qt::KeepEmptyParts;
        QCOMPARE(parseListForm(formValue, separator, behavior), expected);
        QCOMPARE(parseListJson(toJsonValue(jsonArray), separator, behavior), expected);
    }

    void testJsonArrayNumberAndBoolElements() const
    {
        QCOMPARE(parseListJson(toJsonValue("[1,true,false]"_ba), u'|', Qt::KeepEmptyParts)
            , (QStringList {u"1"_s, u"true"_s, u"false"_s}));
    }

    void testJsonArrayElementMayContainSeparator() const
    {
        // unlike a delimited string, an array element keeps a literal separator character
        QCOMPARE(parseListJson(toJsonValue(R"(["a|b","c"])"_ba), u'|', Qt::KeepEmptyParts)
            , (QStringList {u"a|b"_s, u"c"_s}));
    }

    void testJsonSingleElementArray() const
    {
        QCOMPARE(parseListJson(toJsonValue(R"(["solo"])"_ba), u'|', Qt::KeepEmptyParts), (QStringList {u"solo"_s}));
    }

    // strict: a JSON list parameter must be an array; any scalar or object is rejected
    void testJsonNonArrayRejected() const
    {
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(QJsonValue(u"abc"_s), u'|', Qt::KeepEmptyParts));
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(QJsonValue(5), u'|', Qt::KeepEmptyParts));
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(QJsonValue(true), u'|', Qt::KeepEmptyParts));
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(toJsonValue("{}"_ba), u'|', Qt::KeepEmptyParts));
    }

    void testJsonArrayLikeStringNotReinterpreted() const
    {
        // a JSON string that looks like array syntax is still a scalar (detection is by type, not text)
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(QJsonValue(uR"(["a","b"])"_s), u'|', Qt::KeepEmptyParts));
    }

    void testJsonNonScalarElementRejected() const
    {
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(toJsonValue(R"(["a",{}])"_ba), u'|', Qt::KeepEmptyParts));
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(toJsonValue(R"(["a",[1]])"_ba), u'|', Qt::KeepEmptyParts));
        QVERIFY_THROWS_EXCEPTION(APIError, parseListJson(toJsonValue(R"(["a",null])"_ba), u'|', Qt::KeepEmptyParts));
    }

    void testJsonAbsentOrNullIsEmpty() const
    {
        // a null or omitted value is "not provided" -> empty list (requireParams enforces required-ness)
        QCOMPARE(parseListJson(QJsonValue(QJsonValue::Null), u'|', Qt::KeepEmptyParts), QStringList {});

        ParseListHarness harness {nullptr};  // omitted key
        harness.run(u"split"_s, {.params = {{u"sep"_s, u"|"_s}}, .isJson = true, .jsonBody = QJsonObject {}});
        QCOMPARE(harness.listResult, QStringList {});
    }

    void testFormSplitsLiterally() const
    {
        QCOMPARE(parseListForm(u"a|b"_s, u'|', Qt::KeepEmptyParts), (QStringList {u"a"_s, u"b"_s}));
        // a form value that looks like JSON array text is split literally, never parsed as an array
        QCOMPARE(parseListForm(uR"(["a","b"])"_s, u'|', Qt::KeepEmptyParts), (QStringList {uR"(["a","b"])"_s}));
    }

    // parseUrlList: form requests decode each URL; JSON arrays carry raw URLs verbatim.
    void testUrlListFormIsPercentDecoded() const
    {
        QCOMPARE(parseUrlListForm(u"a%20b|c"_s, u'|'), (QStringList {u"a b"_s, u"c"_s}));
    }

    void testUrlListJsonArrayIsVerbatim() const
    {
        QCOMPARE(parseUrlListJson(toJsonValue(R"(["a%20b","c"])"_ba), u'|'), (QStringList {u"a%20b"_s, u"c"_s}));
    }

    void testUrlListJsonNonArrayRejected() const
    {
        // strict: a JSON URL list must be an array, not a delimited string
        QVERIFY_THROWS_EXCEPTION(APIError, parseUrlListJson(QJsonValue(u"a|b"_s), u'|'));
    }

    void testUrlListSkipsEmpty() const
    {
        QCOMPARE(parseUrlListForm(u"a||c"_s, u'|'), (QStringList {u"a"_s, u"c"_s}));
        QCOMPARE(parseUrlListJson(toJsonValue(R"(["a","","c"])"_ba), u'|'), (QStringList {u"a"_s, u"c"_s}));
    }

    void testRejectArrayParam() const
    {
        // a param an endpoint reads as a scalar/string (e.g. addTrackers `urls`) rejects a JSON array
        ParseListHarness arrayHarness {nullptr};
        QVERIFY_THROWS_EXCEPTION(APIError, arrayHarness.run(u"rejectArray"_s
            , {.isJson = true, .jsonBody = QJsonObject {{u"value"_s, QJsonArray {u"a"_s}}}}));

        // no-op for a JSON scalar, and for a form request (jsonBody empty)
        ParseListHarness scalarHarness {nullptr};
        QVERIFY_THROWS_NO_EXCEPTION(scalarHarness.run(u"rejectArray"_s
            , {.isJson = true, .jsonBody = QJsonObject {{u"value"_s, u"a"_s}}}));
        ParseListHarness formHarness {nullptr};
        QVERIFY_THROWS_NO_EXCEPTION(formHarness.run(u"rejectArray"_s, {.params = {{u"value"_s, u"a"_s}}}));
    }

    void testUrlListKeepEmptyParts() const
    {
        // empty array elements mark tier boundaries (torrent creator); KeepEmptyParts preserves them
        QCOMPARE(parseUrlListForm(u"a||c"_s, u'|', Qt::KeepEmptyParts), (QStringList {u"a"_s, u""_s, u"c"_s}));
        QCOMPARE(parseUrlListJson(toJsonValue(R"(["a","","c"])"_ba), u'|', Qt::KeepEmptyParts)
            , (QStringList {u"a"_s, u""_s, u"c"_s}));
    }
};

QTEST_APPLESS_MAIN(TestApiController)
#include "testapicontroller.moc"
