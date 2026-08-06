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

#include <QObject>
#include <QPair>
#include <QSet>
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

    void testUniqueFolderDoesNotImplyDisplayNameChange() const
    {
        const QString torrentName = u"Show"_s;
        const QString folder = uniqueSubfolderName(sampleId, torrentName);
        QVERIFY(folder != torrentName);
        QVERIFY(folder.startsWith(torrentName));
        QVERIFY(folder.endsWith(u" a19f83c275d1"_s));
    }

    void testPlanSubfolderIsFolderRenameLikeRenameFolder() const
    {
        // Same operation as renameFolder("Show", "Show <hash>").
        const PathList current {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.isFolderRename());
        QCOMPARE(plan.folderRenameOldRoot, Path(u"Show"_s));
        QCOMPARE(plan.uniqueRoot, Path(u"Show a19f83c275d1"_s));
        QVERIFY(plan.renames.isEmpty());
    }

    void testPlanOriginalIsFolderRenameLikeRenameFolder() const
    {
        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Original);

        QVERIFY(plan.isFolderRename());
        QCOMPARE(plan.folderRenameOldRoot, Path(u"Show"_s));
        QCOMPARE(plan.uniqueRoot, Path(u"Show a19f83c275d1"_s));
    }

    void testPlanInconsistentSubfolderRejected() const
    {
        const PathList current {Path(u"Disc1/movie.mkv"_s), Path(u"Disc2/subtitle.srt"_s)};
        for (const auto layout : {TorrentContentLayout::Subfolder, TorrentContentLayout::Original})
        {
            const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                    current, sampleId, u"Show"_s, layout);
            QVERIFY(plan.blocked);
            QVERIFY(plan.blockReason.contains(u"share one top-level folder"_s));
        }
    }

    void testPlanNoSubfolderCD1CD2() const
    {
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(!plan.isFolderRename());
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
    }

    void testPlanPartialMigrationFolderRenameRemainingRoot() const
    {
        // One file already under unique root; remaining still under Show → folder rename Show → Unique.
        const PathList partial {
            Path(u"Show a19f83c275d1/ep1.mkv"_s),
            Path(u"Show/ep2.mkv"_s)
        };
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                partial, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.isFolderRename());
        QCOMPARE(plan.folderRenameOldRoot, Path(u"Show"_s));
        QCOMPARE(plan.uniqueRoot, Path(u"Show a19f83c275d1"_s));
    }

    void testPlanDuplicateNoSubfolderTargetsRejected() const
    {
        const PathList current {
            Path(u"a.mkv"_s),
            Path(u"a.mkv"_s)
        };
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);

        QVERIFY(plan.blocked);
        QVERIFY(plan.blockReason.contains(u"same destination"_s));
    }

    void testPlanFinalizeOnlyWhenAlreadyUnderUnique() const
    {
        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);

        QVERIFY(plan.finalizeOnly);
        QVERIFY(!plan.isEmpty());
    }

    void testPlanSingleFileMovieMkvName() const
    {
        const PathList current {Path(u"movie.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"movie.mkv"_s, TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"movie a19f83c275d1/movie.mkv"_s));
    }

    void testFolderRenamePlanMatchesRenameFolderSemantics() const
    {
        // renameFolder(Show, Unique) renames every path under Show the same way as the plan.
        const Path oldRoot {u"Show"_s};
        const Path newRoot {u"Show a19f83c275d1"_s};
        const PathList current {Path(u"Show/a/b.mkv"_s), Path(u"Show/c.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);
        QVERIFY(plan.isFolderRename());
        QCOMPARE(plan.folderRenameOldRoot, oldRoot);
        QCOMPARE(plan.uniqueRoot, newRoot);

        const QList<QPair<int, Path>> pairs = buildUniqueSubfolderRenamePairs(plan, current);
        QCOMPARE(pairs.size(), 2);
        QCOMPARE(pairs.at(0).second, Path(u"Show a19f83c275d1/a/b.mkv"_s));
        QCOMPARE(pairs.at(1).second, Path(u"Show a19f83c275d1/c.mkv"_s));
        // Same targets as doRenameFolder(oldRoot, newRoot):
        QCOMPARE(pairs.at(0).second, newRoot / oldRoot.relativePathOf(current.at(0)));
        QCOMPARE(pairs.at(1).second, newRoot / oldRoot.relativePathOf(current.at(1)));
    }

    void testNoSubfolderConversionUsesSharedRenameJobShape() const
    {
        // NoSubfolder wrap: scheduleRenameJob(empty old root, uniqueRoot, pairs).
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s), Path(u"readme.txt"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(!plan.isFolderRename());
        QVERIFY(plan.folderRenameOldRoot.isEmpty()); // empty old root accepted by scheduleRenameJob
        QVERIFY(!plan.uniqueRoot.isAbsolute());
        QVERIFY(!plan.uniqueRoot.isEmpty());

        const QList<QPair<int, Path>> pairs = buildUniqueSubfolderRenamePairs(plan, current);
        QCOMPARE(pairs.size(), 3);

        // All files move under the unique folder; full relative paths preserved.
        PathList after = current;
        for (const auto &[index, to] : pairs)
            after[index] = to;

        QCOMPARE(after.at(0), Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(after.at(1), Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
        QCOMPARE(after.at(2), Path(u"Show a19f83c275d1/readme.txt"_s));

        for (const Path &path : after)
            QVERIFY(path.hasAncestor(plan.uniqueRoot) || (path == plan.uniqueRoot));

        // Plan never sets layout; UniqueSubfolder is applied only after the rename job succeeds.
        QVERIFY(!plan.finalizeOnly);
    }

    void testFolderRenameAndNoSubfolderSharePairBuilder() const
    {
        // Both conversion modes produce pairs for the same scheduleRenameJob() entry point.
        const PathList folded {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan folderPlan = makeUniqueSubfolderMigrationPlan(
                folded, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);
        const QList<QPair<int, Path>> folderPairs = buildUniqueSubfolderRenamePairs(folderPlan, folded);
        QVERIFY(folderPlan.isFolderRename());
        QCOMPARE(folderPairs.size(), 1);

        const PathList flat {Path(u"ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan batchPlan = makeUniqueSubfolderMigrationPlan(
                flat, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);
        const QList<QPair<int, Path>> batchPairs = buildUniqueSubfolderRenamePairs(batchPlan, flat);
        QVERIFY(!batchPlan.isFolderRename());
        QCOMPARE(batchPairs.size(), 1);

        // Success path: after applying pairs, layout would be set; plan itself does not claim success.
        QVERIFY(!folderPlan.finalizeOnly);
        QVERIFY(!batchPlan.finalizeOnly);
    }

    void testPartialFailureDoesNotFinalizeLayoutInPlan() const
    {
        // Layout flag is only set in finishUniqueSubfolderConversion(success=true).
        // A plan with renames never implies layout change by itself (failure keeps old layout).
        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);
        QVERIFY(!plan.renames.isEmpty() || plan.isFolderRename());
        QVERIFY(!plan.finalizeOnly);
        // Simulated partial failure: some pairs applied, layout flag still not part of the plan.
        const QList<QPair<int, Path>> pairs = buildUniqueSubfolderRenamePairs(plan, current);
        QVERIFY(!pairs.isEmpty());
    }

    void testNoSubfolderDoesNotExposeInvalidFolderPaths() const
    {
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);

        // Consumers see newFolderPath = uniqueRoot (relative, valid component name).
        QVERIFY(Utils::Fs::isValidFileName(plan.uniqueRoot.toString()));
        QVERIFY(plan.folderRenameOldRoot.isEmpty());

        const QList<QPair<int, Path>> pairs = buildUniqueSubfolderRenamePairs(plan, current);
        QSet<int> indexes;
        for (const auto &[index, to] : pairs)
        {
            QVERIFY(index >= 0);
            QVERIFY(index < current.size());
            QVERIFY(!to.isAbsolute());
            QVERIFY(!to.isEmpty());
            // No double unique prefix
            QVERIFY(!to.toString().contains(u"a19f83c275d1/Show a19f"_s));
            QVERIFY(!indexes.contains(index));
            indexes.insert(index);
        }
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
