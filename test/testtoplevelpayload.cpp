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
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"

using namespace BitTorrent;

namespace
{
    const TorrentID sampleId = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
    const Path uniqueShow {u"Show a19f83c275d1"_s};
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

    void testSharedRootReplacementOnAdd() const
    {
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)}, sampleId, u"Show"_s);
        QCOMPARE(out.at(0), Path(u"Show a19f83c275d1/ep1.mkv"_s));
        QCOMPARE(out.at(1), Path(u"Show a19f83c275d1/ep2.mkv"_s));
    }

    void testRootlessMultiFilePreservesBranchesOnAdd() const
    {
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)}, sampleId, u"Show"_s);
        QCOMPARE(out.at(0), Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(out.at(1), Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
    }

    void testSingleFileUsesNameWithoutExtension() const
    {
        // Match Subfolder: movie.mkv → folder "movie <hash>", file "movie.mkv" inside.
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"movie.mkv"_s)}, sampleId, u"movie.mkv"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"movie a19f83c275d1"_s);
        QCOMPARE(out.at(0), Path(u"movie a19f83c275d1/movie.mkv"_s));
    }

    void testIdempotentMultiFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"Show/ep.mkv"_s)}, sampleId, u"Show"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"Show"_s);
        QCOMPARE(twice, once);
    }

    void testIdempotentSingleFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"movie.mkv"_s)}, sampleId, u"movie.mkv"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"movie.mkv"_s);
        QCOMPARE(twice, once);
    }

    void testLongNameTruncated() const
    {
        const QString longName = QString(300, u'a');
        const PathList out = applyUniqueSubfolderLayout({Path(u"f.mkv"_s)}, sampleId, longName);
        const QString root = Path::findRootFolder(out).toString();
        QVERIFY(Utils::Fs::isValidFileName(root));
        QVERIFY(root.endsWith(u" a19f83c275d1"_s));
    }

    void testPlanNoSubfolderPrependsFullPath() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
    }

    void testPlanSharedRootReplacement() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep1.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/ep2.mkv"_s));
    }

    void testPlanInconsistentSubfolderDifferentFirstFolders() const
    {
        // Stored as Subfolder but paths have no shared root (Disc1/Disc2).
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Disc1/movie.mkv"_s), Path(u"Disc2/subtitle.srt"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/Disc1/movie.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/Disc2/subtitle.srt"_s));
    }

    void testPlanPartialMigrationKeepsUniquePaths() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList partial {
            Path(u"Show a19f83c275d1/ep1.mkv"_s),
            Path(u"Show/ep2.mkv"_s)
        };

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                partial, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).fileIndex, 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep2.mkv"_s));
        QVERIFY(!plan.renames.at(0).to.toString().contains(u"a19f83c275d1/Show a19f"_s));
    }

    void testPlanDuplicateTargetPathsRejected() const
    {
        // Two different top-level folders mapping to the same relative file under Unique
        // would collide if first components were stripped blindly; with no shared root
        // both keep their prefixes. Force a duplicate by using NoSubfolder with same full path
        // is impossible — use Subfolder-style same relative after strip with shared root no:
        // Construct: NoSubfolder paths that already equal after prepend... actually
        // Disc1/a.mkv and Disc1/a.mkv can't both exist as two indexes with same path.
        // Instead: two files under different folders that share name when stripped wrongly —
        // with fixed mapping Disc1/a + Disc2/a stay unique.
        // Duplicate only if shared root replace of Show/a + Show/a — invalid list.
        // Use Original layout with paths outside unique that map identically:
        // "A/x.mkv" and "B/x.mkv" with shared root empty → Unique/A/x and Unique/B/x — no duplicate.
        // Shared root "Pack" with Pack/x and Pack/x — can't have two same paths.
        // Simulate via two paths that strip to same: only if we had shared root X and files X/a, X/a.
        // Real duplicate case: NoSubfolder "a.mkv" and "a.mkv" — impossible in PathList as distinct?
        // We'll use Subfolder with inconsistent mapping intentional:
        // remaining after unique skip none: paths "Show/ep.mkv" and "Other/ep.mkv" → prepend full → no dup.
        // Force collision: layout Subfolder, only one remaining shared root path isn't enough.
        // When shared root is Show, Show/ep.mkv and Show/ep.mkv can't be two entries.
        // Alternative: block when target list has duplicates from any source —
        // craft with NoSubfolder: "ep.mkv" and "./ep.mkv" normalize differently...
        // Simplest realistic collision under OLD bug: Subfolder Disc1/movie + Disc2/movie both → Unique/movie.
        // With NEW code that's Unique/Disc1/movie + Unique/Disc2/movie.
        // For duplicate detection unit test, call map by using shared root on:
        // "Root/a/f.mkv" and "Root/a/f.mkv" — PathList can have identical paths at two indexes.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {
            Path(u"Root/a/f.mkv"_s),
            Path(u"Root/a/f.mkv"_s)
        };

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        // Both map to the same unique target; migration must refuse.
        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
        QVERIFY(plan.blockReason.contains(u"same destination"_s));
    }

    void testPlanTargetPathExistingDirectoryBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        QVERIFY(Utils::Fs::mkpath(storageRoot / Path(u"Show a19f83c275d1/ep.mkv"_s)));

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
    }

    void testPlanTargetParentIsFileBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        QFile parentFile {(storageRoot / uniqueShow).data()};
        QVERIFY(parentFile.open(QIODevice::WriteOnly));
        parentFile.write("not a dir");
        parentFile.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
    }

    void testPlanTargetExistingFileBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        QVERIFY(Utils::Fs::mkpath(storageRoot / uniqueShow));
        QFile conflict {(storageRoot / Path(u"Show a19f83c275d1/ep.mkv"_s)).data()};
        QVERIFY(conflict.open(QIODevice::WriteOnly));
        conflict.write("old");
        conflict.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(plan.blocked);
    }

    void testPlanUniqueRootDirMayExist() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        QVERIFY(Utils::Fs::mkpath(storageRoot / uniqueShow));

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
    }

    void testPlanFinalizeOnlyWhenAlreadyUnderUnique() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        QVERIFY(plan.finalizeOnly);
        QVERIFY(plan.renames.isEmpty());
        QVERIFY(!plan.isEmpty());
    }

    void testPlanSingleFileMovieMkvName() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"movie.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"movie.mkv"_s, Path(tmp.path()), TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"movie a19f83c275d1/movie.mkv"_s));
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
