/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Kieran Devlin
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

#include <limits>
#include <vector>

#include <QObject>
#include <QTest>

#include "base/bittorrent/filescheduler.h"

using namespace BitTorrent;
using namespace BitTorrent::FileScheduler;

namespace
{
    FileEntry makeFile(const int firstPiece, const int lastPiece, const int rank
            , const bool sequential = false, const int priority = 1)
    {
        return {.firstPiece = firstPiece, .lastPiece = lastPiece, .priority = priority
                , .rank = rank, .sequential = sequential, .edgePieces = 0};
    }

    int priorityAt(const Plan &plan, const int piece)
    {
        return plan.piecePriorities.at(piece);
    }

    void markHave(Input &input, const int first, const int last)
    {
        input.havePieces.resize(input.pieceCount, false);
        for (int piece = first; piece <= last; ++piece)
            input.havePieces[piece] = true;
    }
}

class TestBittorrentFileScheduler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentFileScheduler)

public:
    TestBittorrentFileScheduler() = default;

private slots:
    void testIsActive() const
    {
        QVERIFY(!isActive({}));
        QVERIFY(!isActive({makeFile(0, 1, UNRANKED)}));
        QVERIFY(!isActive({makeFile(0, 1, -5)})); // negative ranks are treated as unranked
        QVERIFY(isActive({makeFile(0, 1, 3)}));
        QVERIFY(isActive({makeFile(0, 1, UNRANKED, true)}));
    }

    // The example from https://github.com/qbittorrent/qBittorrent/issues/21430:
    // files A, B, C ranked 3, 1, 2; A and C are sequential.
    void testIssueExample() const
    {
        Input input;
        input.pieceCount = 30;
        input.windowSize = 4;
        input.files = {makeFile(0, 9, 3, true), makeFile(10, 19, 1), makeFile(20, 29, 2, true)};

        Plan plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({1}));
        for (int piece = 10; piece <= 19; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7); // B is downloaded first
        // C is next: only its sequential window may use spare bandwidth
        for (int piece = 20; piece <= 23; ++piece)
            QCOMPARE(priorityAt(plan, piece), 6);
        for (int piece = 24; piece <= 29; ++piece)
            QCOMPARE(priorityAt(plan, piece), 0);
        // A is last
        for (int piece = 0; piece <= 3; ++piece)
            QCOMPARE(priorityAt(plan, piece), 5);
        for (int piece = 4; piece <= 9; ++piece)
            QCOMPARE(priorityAt(plan, piece), 0);
        QVERIFY(plan.holdsPieces);

        // B is complete: C is at the head, its window is downloaded first
        markHave(input, 10, 19);
        plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({2}));
        for (int piece = 20; piece <= 23; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7);
        for (int piece = 24; piece <= 29; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_SEQUENTIAL_REST});
        for (int piece = 0; piece <= 3; ++piece)
            QCOMPARE(priorityAt(plan, piece), 6);

        // The window moves forward as pieces complete
        markHave(input, 20, 21);
        plan = makePlan(input);
        for (int piece = 22; piece <= 25; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7);
        for (int piece = 26; piece <= 29; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_SEQUENTIAL_REST});
    }

    void testSoftModeBands() const
    {
        // 100 files, 10 of them ranked (file 9 has rank 1, file 0 has rank 10)
        Input input;
        input.pieceCount = 100;
        for (int i = 0; i < 100; ++i)
            input.files.push_back(makeFile(i, i, ((i < 10) ? (10 - i) : UNRANKED)));

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 9), 7);
        QCOMPARE(priorityAt(plan, 8), 6);
        QCOMPARE(priorityAt(plan, 7), 5);
        QCOMPARE(priorityAt(plan, 6), 4);
        QCOMPARE(priorityAt(plan, 0), 4);
        // Unranked files are below every ranked file
        for (int piece = 10; piece < 100; ++piece)
            QCOMPARE(priorityAt(plan, piece), 1);
        QVERIFY(!plan.holdsPieces);
    }

    void testUnrankedKeepRelativePriority() const
    {
        Input input;
        input.pieceCount = 4;
        input.files = {makeFile(0, 0, 1), makeFile(1, 1, UNRANKED, false, 1)
                , makeFile(2, 2, UNRANKED, false, 6), makeFile(3, 3, UNRANKED, false, 7)};

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 0), 7);
        QCOMPARE(priorityAt(plan, 1), 1);
        QCOMPARE(priorityAt(plan, 2), 2);
        QCOMPARE(priorityAt(plan, 3), 3);
    }

    void testRankedFirstMode() const
    {
        Input input;
        input.pieceCount = 100;
        input.mode = FileSchedulingMode::RankedFirst;
        for (int i = 0; i < 100; ++i)
            input.files.push_back(makeFile(i, i, ((i < 50) ? (i + 1) : UNRANKED)));

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 0), 7);
        for (int piece = 50; piece < 100; ++piece)
            QCOMPARE(priorityAt(plan, piece), 0);
        QVERIFY(plan.holdsPieces);
    }

    void testStrictMode() const
    {
        // File 0 (rank 1) has 100 pieces, so no lookahead piece is released.
        // Files 1..9 are ranked 2..10, files 10..99 are unranked.
        Input input;
        input.pieceCount = 199;
        input.mode = FileSchedulingMode::Strict;
        input.files.push_back(makeFile(0, 99, 1));
        for (int i = 1; i < 100; ++i)
            input.files.push_back(makeFile((99 + i), (99 + i), ((i < 10) ? (i + 1) : UNRANKED)));

        const Plan plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({0}));
        int nonZero = 0;
        for (const std::uint8_t priority : plan.piecePriorities)
        {
            if (priority > 0)
                ++nonZero;
        }
        QCOMPARE(nonZero, 100);
        QVERIFY(plan.holdsPieces);
    }

    void testLookahead() const
    {
        // Only one piece of the head file is missing, so held pieces of the next files are
        // released (up to LOOKAHEAD_THRESHOLD wanted pieces) to avoid a false "torrent finished" event.
        Input input;
        input.pieceCount = 1 + LOOKAHEAD_THRESHOLD + 10;
        input.mode = FileSchedulingMode::Strict;
        input.files = {makeFile(0, 0, 1), makeFile(1, (input.pieceCount - 1), 2)};

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 0), 7);
        for (int piece = 1; piece < LOOKAHEAD_THRESHOLD; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_LOOKAHEAD});
        for (int piece = LOOKAHEAD_THRESHOLD; piece < input.pieceCount; ++piece)
            QCOMPARE(priorityAt(plan, piece), 0);
        QVERIFY(plan.holdsPieces);
    }

    void testEqualRanksShareHead() const
    {
        Input input;
        input.pieceCount = 4;
        input.files = {makeFile(0, 1, 5), makeFile(2, 3, 5)};

        const Plan plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({0, 1}));
        for (int piece = 0; piece < 4; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7);
    }

    void testLargeRanks() const
    {
        Input input;
        input.pieceCount = 2;
        input.files = {makeFile(0, 0, std::numeric_limits<int>::max()), makeFile(1, 1, (std::numeric_limits<int>::max() - 1))};

        const Plan plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({1}));
        QCOMPARE(priorityAt(plan, 1), 7);
        QCOMPARE(priorityAt(plan, 0), 6);
    }

    void testIgnoredFilesAndSharedPieces() const
    {
        Input input;
        input.pieceCount = 5;
        // piece 2 is shared by both files, the second file is not wanted
        input.files = {makeFile(0, 2, 1), makeFile(2, 4, UNRANKED, false, 0)};

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 2), 7);
        QCOMPARE(priorityAt(plan, 3), 0);
        QCOMPARE(priorityAt(plan, 4), 0);
        QVERIFY(!plan.holdsPieces);
    }

    void testEdgePieces() const
    {
        // Only the edge pieces of the files at the head are raised
        Input input;
        input.pieceCount = 20;
        input.windowSize = 2;
        input.files = {makeFile(0, 9, 1, true), makeFile(10, 19, 2)};
        input.files[0].edgePieces = 1;
        input.files[1].edgePieces = 1;

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 0), 7);
        QCOMPARE(priorityAt(plan, 1), 7);
        for (int piece = 2; piece <= 8; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_SEQUENTIAL_REST});
        QCOMPARE(priorityAt(plan, 9), 7);
        for (int piece = 10; piece <= 19; ++piece)
            QCOMPARE(priorityAt(plan, piece), 6);
    }

    void testCompleted() const
    {
        Input input;
        input.pieceCount = 2;
        input.files = {makeFile(0, 1, 1)};
        markHave(input, 0, 1);

        const Plan plan = makePlan(input);
        QVERIFY(plan.headFiles.empty());
        QVERIFY(!plan.holdsPieces);
        QCOMPARE(priorityAt(plan, 0), 1); // downloaded pieces stay "wanted"
    }

    // A sequential file at the head of the queue is downloaded through its window in every mode,
    // the rest of the file stays wanted, and the files the mode holds back stay held.
    void testSequentialHeadInEveryMode_data() const
    {
        QTest::addColumn<FileSchedulingMode>("mode");
        QTest::addColumn<int>("nextFilePriority");
        QTest::addColumn<int>("unrankedPriority");

        QTest::newRow("Soft") << FileSchedulingMode::Soft << 6 << 1;
        QTest::newRow("RankedFirst") << FileSchedulingMode::RankedFirst << 6 << 0;
        QTest::newRow("Strict") << FileSchedulingMode::Strict << 0 << 0;
    }

    void testSequentialHeadInEveryMode() const
    {
        QFETCH(const FileSchedulingMode, mode);
        QFETCH(const int, nextFilePriority);
        QFETCH(const int, unrankedPriority);

        // Head: sequential file with 100 pieces, then a ranked file and an unranked file
        Input input;
        input.pieceCount = 300;
        input.mode = mode;
        input.windowSize = 8;
        input.files = {makeFile(0, 99, 1, true), makeFile(100, 199, 2), makeFile(200, 299, UNRANKED)};
        markHave(input, 0, 9);

        const Plan plan = makePlan(input);
        QCOMPARE(plan.headFiles, std::vector<int>({0}));
        for (int piece = 10; piece <= 17; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7);
        for (int piece = 18; piece <= 99; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_SEQUENTIAL_REST});
        for (int piece = 100; piece <= 199; ++piece)
            QCOMPARE(priorityAt(plan, piece), nextFilePriority);
        for (int piece = 200; piece <= 299; ++piece)
            QCOMPARE(priorityAt(plan, piece), unrankedPriority);
        QCOMPARE(plan.holdsPieces, (mode != FileSchedulingMode::Soft));
    }

    // The window of a sequential head file is the most preferred part of the torrent,
    // even when the head itself is in a low band (no ranked files in Soft mode).
    void testSequentialHeadWindowIsTopPriority() const
    {
        Input input;
        input.pieceCount = 20;
        input.windowSize = 4;
        input.files = {makeFile(0, 9, UNRANKED, true), makeFile(10, 19, UNRANKED)};

        const Plan plan = makePlan(input);
        for (int piece = 0; piece <= 3; ++piece)
            QCOMPARE(priorityAt(plan, piece), 7);
        for (int piece = 4; piece <= 19; ++piece)
            QCOMPARE(priorityAt(plan, piece), 1);
    }

    // The window always contains the next missing pieces, also when pieces after it are already downloaded.
    void testSequentialWindowSkipsDownloadedPieces() const
    {
        Input input;
        input.pieceCount = 10;
        input.windowSize = 3;
        input.files = {makeFile(0, 9, 1, true)};
        markHave(input, 0, 1);
        markHave(input, 3, 4);

        const Plan plan = makePlan(input);
        QCOMPARE(priorityAt(plan, 2), 7);
        QCOMPARE(priorityAt(plan, 5), 7);
        QCOMPARE(priorityAt(plan, 6), 7);
        for (int piece = 7; piece <= 9; ++piece)
            QCOMPARE(priorityAt(plan, piece), int {PRIORITY_SEQUENTIAL_REST});
    }
};

QTEST_APPLESS_MAIN(TestBittorrentFileScheduler)
#include "testbittorrentfilescheduler.moc"
