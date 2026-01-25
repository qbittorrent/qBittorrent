/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Thomas Piccirello <thomas@piccirello.com>
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

#include "base/global.h"
#include "base/http/types.h"

class TestHttpRangeParser final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestHttpRangeParser)

public:
    TestHttpRangeParser() = default;

private slots:
    // Valid range formats
    void testSimpleRange() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-499"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 499);
        QCOMPARE(result.length, 500);
    }

    void testMiddleRange() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=500-999"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 500);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 500);
    }

    void testOpenEndedRange() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=500-"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 500);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 500);
    }

    void testSuffixRange() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-500"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 500);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 500);
    }

    void testSingleByte() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-0"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 0);
        QCOMPARE(result.length, 1);
    }

    void testLastByte() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-1"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 999);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 1);
    }

    void testEntireFileViaRange() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-999"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 1000);
    }

    // Boundary conditions - clamping
    void testRangeExceedsFileSize() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-2000"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 999);  // Clamped to file size
        QCOMPARE(result.length, 1000);
    }

    void testStartAtLastByte() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=999-"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 999);
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 1);
    }

    void testSuffixLargerThanFile() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-2000"_s, 1000);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);  // Clamps to start of file
        QCOMPARE(result.end, 999);
        QCOMPARE(result.length, 1000);
    }

    // Invalid/malformed ranges - should return isValid=false
    void testNoRangeHeader() const
    {
        const auto result = Http::parseRangeHeader(QString(), 1000);
        QVERIFY(!result.isValid);
    }

    void testEmptyRangeHeader() const
    {
        const auto result = Http::parseRangeHeader(u""_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testMissingEquals() const
    {
        const auto result = Http::parseRangeHeader(u"bytes 0-499"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testWrongUnit() const
    {
        const auto result = Http::parseRangeHeader(u"items=0-499"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testNoDash() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=500"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testStartGreaterThanEnd() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=500-100"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testStartAtFileSize() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=1000-"_s, 1000);
        QVERIFY(!result.isValid);  // Start at 1000 is out of bounds for size 1000
    }

    void testStartBeyondFileSize() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=1500-"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testNonNumericStart() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=abc-500"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testNonNumericEnd() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-def"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testEmptyDash() const
    {
        // "bytes=-" with nothing after is invalid
        const auto result = Http::parseRangeHeader(u"bytes=-"_s, 1000);
        QVERIFY(!result.isValid);
    }

    void testNegativeStart() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-10-500"_s, 1000);
        QVERIFY(!result.isValid);  // This is actually parsed as suffix range "-10-500" which is invalid
    }

    void testZeroSuffix() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-0"_s, 1000);
        QVERIFY(!result.isValid);  // Zero-length suffix is invalid
    }

    void testNegativeSuffix() const
    {
        // Note: "-" followed by negative number like "bytes=--5"
        // The parser sees startStr="" and endStr="-5" which is invalid as a suffix length
        const auto result = Http::parseRangeHeader(u"bytes=--5"_s, 1000);
        QVERIFY(!result.isValid);
    }

    // Large file tests (multi-GB)
    void testLargeFileRange() const
    {
        const qint64 fileSize = Q_INT64_C(10737418240);  // 10 GB
        const auto result = Http::parseRangeHeader(u"bytes=5368709120-"_s, fileSize);  // Start at 5GB
        QVERIFY(result.isValid);
        QCOMPARE(result.start, Q_INT64_C(5368709120));
        QCOMPARE(result.end, Q_INT64_C(10737418239));
        QCOMPARE(result.length, Q_INT64_C(5368709120));
    }

    void testLargeFileSuffix() const
    {
        const qint64 fileSize = Q_INT64_C(10737418240);  // 10 GB
        const auto result = Http::parseRangeHeader(u"bytes=-1073741824"_s, fileSize);  // Last 1GB
        QVERIFY(result.isValid);
        QCOMPARE(result.start, Q_INT64_C(9663676416));  // 10GB - 1GB
        QCOMPARE(result.end, Q_INT64_C(10737418239));
        QCOMPARE(result.length, Q_INT64_C(1073741824));
    }

    // Edge case: file size of 1
    void testSingleByteFile() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-0"_s, 1);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 0);
        QCOMPARE(result.length, 1);
    }

    void testSingleByteFileSuffix() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-1"_s, 1);
        QVERIFY(result.isValid);
        QCOMPARE(result.start, 0);
        QCOMPARE(result.end, 0);
        QCOMPARE(result.length, 1);
    }

    // Edge case: file size of 0 (should all be invalid)
    void testEmptyFile() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=0-0"_s, 0);
        QVERIFY(!result.isValid);
    }

    void testEmptyFileSuffix() const
    {
        const auto result = Http::parseRangeHeader(u"bytes=-1"_s, 0);
        QVERIFY(!result.isValid);
    }
};

QTEST_APPLESS_MAIN(TestHttpRangeParser)
#include "testhttprangeparser.moc"
