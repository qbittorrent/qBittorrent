/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <limits>

#include <QTest>

#include "base/streaming/helpers.h"

class TestStreamingHelpers final : public QObject
{
    Q_OBJECT

private slots:
    void parseRange_data();
    void parseRange();
    void rejectRange_data();
    void rejectRange();
    void mapPieces_data();
    void mapPieces();
    void rejectPieceMapping();
};

void TestStreamingHelpers::parseRange_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<qint64>("size");
    QTest::addColumn<qint64>("first");
    QTest::addColumn<qint64>("last");

    QTest::newRow("closed") << QByteArray("bytes=2-5") << 10LL << 2LL << 5LL;
    QTest::newRow("open-ended") << QByteArray("bytes=7-") << 10LL << 7LL << 9LL;
    QTest::newRow("suffix") << QByteArray("bytes=-4") << 10LL << 6LL << 9LL;
    QTest::newRow("large-suffix") << QByteArray("bytes=-20") << 10LL << 0LL << 9LL;
    QTest::newRow("clamped-end") << QByteArray("bytes=5-100") << 10LL << 5LL << 9LL;
    QTest::newRow("case-and-whitespace") << QByteArray(" Bytes = 0 - 0 ") << 10LL << 0LL << 0LL;
}

void TestStreamingHelpers::parseRange()
{
    QFETCH(QByteArray, header);
    QFETCH(qint64, size);
    QFETCH(qint64, first);
    QFETCH(qint64, last);

    const auto result = Streaming::parseHttpRange(header, size);
    QVERIFY(result.has_value());
    QCOMPARE(result->first, first);
    QCOMPARE(result->last, last);
    QCOMPARE(result->length(), (last - first + 1));
}

void TestStreamingHelpers::rejectRange_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<qint64>("size");
    QTest::addColumn<int>("error");

    using Streaming::RangeError;
    QTest::newRow("wrong-unit") << QByteArray("items=0-1") << 10LL << static_cast<int>(RangeError::Malformed);
    QTest::newRow("multiple") << QByteArray("bytes=0-1,4-5") << 10LL << static_cast<int>(RangeError::MultipleRanges);
    QTest::newRow("empty") << QByteArray("bytes=-") << 10LL << static_cast<int>(RangeError::Malformed);
    QTest::newRow("past-end") << QByteArray("bytes=10-") << 10LL << static_cast<int>(RangeError::Unsatisfiable);
    QTest::newRow("reversed") << QByteArray("bytes=6-5") << 10LL << static_cast<int>(RangeError::Unsatisfiable);
    QTest::newRow("zero-suffix") << QByteArray("bytes=-0") << 10LL << static_cast<int>(RangeError::Unsatisfiable);
    QTest::newRow("empty-resource") << QByteArray("bytes=0-0") << 0LL << static_cast<int>(RangeError::Unsatisfiable);
    QTest::newRow("not-a-number") << QByteArray("bytes=x-y") << 10LL << static_cast<int>(RangeError::Malformed);
    QTest::newRow("numeric-overflow") << QByteArray("bytes=999999999999999999999-") << 10LL << static_cast<int>(RangeError::Malformed);
    QTest::newRow("416-start-at-size") << QByteArray("bytes=10-20") << 10LL << static_cast<int>(RangeError::Unsatisfiable);
}

void TestStreamingHelpers::rejectRange()
{
    QFETCH(QByteArray, header);
    QFETCH(qint64, size);
    QFETCH(int, error);

    const auto result = Streaming::parseHttpRange(header, size);
    QVERIFY(!result.has_value());
    QCOMPARE(static_cast<int>(result.error()), error);
}

void TestStreamingHelpers::mapPieces_data()
{
    QTest::addColumn<qint64>("firstByte");
    QTest::addColumn<qint64>("lastByte");
    QTest::addColumn<qint64>("fileOffset");
    QTest::addColumn<qint64>("pieceLength");
    QTest::addColumn<int>("firstPiece");
    QTest::addColumn<int>("lastPiece");

    QTest::newRow("one-piece") << 0LL << 9LL << 0LL << 16LL << 0 << 0;
    QTest::newRow("file-offset-crossing") << 0LL << 2LL << 15LL << 16LL << 0 << 1;
    QTest::newRow("offset-file") << 10LL << 50LL << 100LL << 32LL << 3 << 4;
    QTest::newRow("inclusive-boundary") << 0LL << 16LL << 32LL << 16LL << 2 << 3;
}

void TestStreamingHelpers::mapPieces()
{
    QFETCH(qint64, firstByte);
    QFETCH(qint64, lastByte);
    QFETCH(qint64, fileOffset);
    QFETCH(qint64, pieceLength);
    QFETCH(int, firstPiece);
    QFETCH(int, lastPiece);

    const auto result = Streaming::mapByteRangeToPieces(firstByte, lastByte, fileOffset, pieceLength, 100);
    QVERIFY(result.has_value());
    QCOMPARE(result->first, firstPiece);
    QCOMPARE(result->last, lastPiece);
}

void TestStreamingHelpers::rejectPieceMapping()
{
    QVERIFY(!Streaming::mapByteRangeToPieces(-1, 2, 0, 16, 4));
    QVERIFY(!Streaming::mapByteRangeToPieces(2, 1, 0, 16, 4));
    QVERIFY(!Streaming::mapByteRangeToPieces(0, 1, -1, 16, 4));
    QVERIFY(!Streaming::mapByteRangeToPieces(0, 1, 0, 0, 4));
    QVERIFY(!Streaming::mapByteRangeToPieces(0, 64, 0, 16, 4));
    QVERIFY(!Streaming::mapByteRangeToPieces(0, std::numeric_limits<qint64>::max(), 1, 16, 4));
}

QTEST_APPLESS_MAIN(TestStreamingHelpers)
#include "teststreaminghelpers.moc"
