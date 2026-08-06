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

    void testPlanSubfolderIsFolderRootReplacement() const
    {
        const PathList current {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.isFolderRename());
        QCOMPARE(plan.folderRenameOldRoot, Path(u"Show"_s));
        QCOMPARE(plan.uniqueRoot, Path(u"Show a19f83c275d1"_s));
        QVERIFY(plan.renames.isEmpty());
    }

    void testPlanOriginalIsFolderRootReplacement() const
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

    void testPlanNoSubfolderPathWrapping() const
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

    void testPlanPartialMigrationRemainingRoot() const
    {
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
        const PathList current {Path(u"a.mkv"_s), Path(u"a.mkv"_s)};
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

    void testNoSubfolderPlanIsRootlessWrapNotFolderRename() const
    {
        // NoSubfolder conversion uses scheduleRenameJob with an empty old root.
        // That must not be treated as a real folder rename event (no folderRenamed("")).
        // Successful moves are reported as individual file renames; layout finalizes only after
        // the full job succeeds (finishUniqueSubfolderConversion), not in the plan itself.
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.folderRenameOldRoot.isEmpty());
        QVERIFY(!plan.isFolderRename());
        QCOMPARE(plan.renames.size(), 2);
        QVERIFY(!plan.finalizeOnly);

        // After applying renames, every path is under the unique folder (job success path).
        PathList after = current;
        for (const UniqueSubfolderRename &item : plan.renames)
            after[item.fileIndex] = item.to;
        for (const Path &path : after)
            QVERIFY(path.hasAncestor(plan.uniqueRoot));

        // Contrast: Subfolder has a real old root and uses folderRenamed(old, new).
        const UniqueSubfolderMigrationPlan folderPlan = makeUniqueSubfolderMigrationPlan(
                {Path(u"Show/ep.mkv"_s)}, sampleId, u"Show"_s, TorrentContentLayout::Subfolder);
        QVERIFY(folderPlan.isFolderRename());
        QVERIFY(!folderPlan.folderRenameOldRoot.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
