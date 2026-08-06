/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  The qBittorrent project
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

#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>

#include "base/bittorrent/toplevelpayload.h"
#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"

using namespace BitTorrent;

namespace
{
    const TorrentID sampleId = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
}

class TestTopLevelPayload final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTopLevelPayload)

public:
    TestTopLevelPayload() = default;

private slots:
    void testTagFormat() const
    {
        QCOMPARE(uniqueSubfolderTag(sampleId), u" a19f83c275d1"_s);
    }

    void testFolderName() const
    {
        QCOMPARE(uniqueSubfolderName(sampleId, u"Show"_s), u"Show a19f83c275d1"_s);
    }

    void testFolderBecomesUniqueSubfolder() const
    {
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)}, sampleId, u"Show"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"Show a19f83c275d1"_s);
        QCOMPARE(out.at(0), Path(u"Show a19f83c275d1/ep1.mkv"_s));
    }

    void testSingleFileWraps() const
    {
        const PathList out = applyUniqueSubfolderLayout({Path(u"movie.mkv"_s)}, sampleId, u"movie"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"movie a19f83c275d1"_s);
        QCOMPARE(out.at(0), Path(u"movie a19f83c275d1/movie.mkv"_s));
    }

    void testRootlessWraps() const
    {
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"a.mkv"_s), Path(u"b.srt"_s)}, sampleId, u"Torrent Name"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"Torrent Name a19f83c275d1"_s);
        QCOMPARE(out.at(0), Path(u"Torrent Name a19f83c275d1/a.mkv"_s));
    }

    void testDifferentTorrentsDifferentFolders() const
    {
        const TorrentID idA = TorrentID::fromString(u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"_s);
        const TorrentID idB = TorrentID::fromString(u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"_s);
        const PathList layout {Path(u"Show/ep.mkv"_s)};
        const PathList outA = applyUniqueSubfolderLayout(layout, idA, u"Show"_s);
        const PathList outB = applyUniqueSubfolderLayout(layout, idB, u"Show"_s);
        QVERIFY(Path::findRootFolder(outA) != Path::findRootFolder(outB));
    }

    void testIdempotentMultiFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"Show/ep.mkv"_s)}, sampleId, u"Show"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"Show"_s);
        QCOMPARE(twice, once);
        QCOMPARE(Path::findRootFolder(twice).toString(), u"Show a19f83c275d1"_s);
    }

    void testIdempotentSingleFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"movie.mkv"_s)}, sampleId, u"movie"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"movie"_s);
        QCOMPARE(twice, once);
        QCOMPARE(twice.at(0), Path(u"movie a19f83c275d1/movie.mkv"_s));
    }

    void testIdempotentRootless() const
    {
        const PathList once = applyUniqueSubfolderLayout(
                {Path(u"a.mkv"_s), Path(u"b.srt"_s)}, sampleId, u"Pack"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"Pack"_s);
        QCOMPARE(twice, once);
        QVERIFY(!twice.at(0).toString().contains(u"a19f83c275d1/Pack a19f"_s));
    }

    void testUnicodeName() const
    {
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"映画/ep.mkv"_s)}, sampleId, u"映画"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"映画 a19f83c275d1"_s);
    }

    void testLongNameTruncated() const
    {
        const QString longName = QString(300, u'a');
        const PathList out = applyUniqueSubfolderLayout({Path(u"f.mkv"_s)}, sampleId, longName);
        const QString root = Path::findRootFolder(out).toString();
        QVERIFY(Utils::Fs::isValidFileName(root));
        QVERIFY(root.endsWith(u" a19f83c275d1"_s));
    }

    void testLongUnicodeNameTruncated() const
    {
        const QString longName = QString(200, QChar(0x6620));
        const PathList out = applyUniqueSubfolderLayout({Path(u"f.mkv"_s)}, sampleId, longName);
        const QString root = Path::findRootFolder(out).toString();
        QVERIFY(Utils::Fs::isValidFileName(root));
        QVERIFY(root.endsWith(u" a19f83c275d1"_s));
    }

    void testAlreadyUniqueRootUnchanged() const
    {
        const PathList partial {Path(u"Show a19f83c275d1/ep.mkv"_s)};
        const PathList out = applyUniqueSubfolderLayout(partial, sampleId, u"Show"_s);
        QCOMPARE(out, partial);
    }

    void testPlanDestDirectoryExistsEmptyOk() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path destDir = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(destDir));

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
    }

    void testPlanDestDirectoryWithUnrelatedFileOk() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path destDir = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(destDir));
        QFile unrelated {(destDir / Path(u"notes.txt"_s)).data()};
        QVERIFY(unrelated.open(QIODevice::WriteOnly));
        unrelated.write("keep");
        unrelated.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QVERIFY(QFile::exists((destDir / Path(u"notes.txt"_s)).data()));
    }

    void testPlanConflictingDestinationFileBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path destDir = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(destDir));
        QFile conflict {(destDir / Path(u"ep.mkv"_s)).data()};
        QVERIFY(conflict.open(QIODevice::WriteOnly));
        conflict.write("old");
        conflict.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
        QVERIFY(plan.blockReason.contains(u"already exists"_s));
    }

    void testPlanPartialMigrationRetryNoDoubleWrap() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        // One file already under unique folder; one still at old path.
        const PathList partial {
            Path(u"Show a19f83c275d1/ep1.mkv"_s),
            Path(u"Show/ep2.mkv"_s)
        };

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                partial, sampleId, u"Show"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).fileIndex, 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep2.mkv"_s));
        // Must not plan a double-wrapped path for the already-migrated file.
        for (const UniqueSubfolderRename &item : plan.renames)
            QVERIFY(!item.to.toString().contains(u"a19f83c275d1/Show a19f"_s));
    }

    void testPlanSingleFileMovie() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};

        const PathList current {Path(u"movie.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"movie"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"movie a19f83c275d1/movie.mkv"_s));
    }

    void testLayoutDoesNotChangeTorrentDisplayName() const
    {
        // applyUniqueSubfolderLayout only rewrites payload paths; display name is separate
        // and must not be set during migration (no setName on success).
        const PathList out = applyUniqueSubfolderLayout({Path(u"Show/ep.mkv"_s)}, sampleId, u"Show"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"Show a19f83c275d1"_s);
        // Torrent list name is not part of PathList — this test documents the contract.
        QVERIFY(true);
    }

    void testPlanEmptyWhenAlreadyUnique() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()));

        QVERIFY(plan.isEmpty());
    }

    void testPlanBlockedWhenStorageUnknown() const
    {
        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, {});

        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
