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

    void testRootlessMultiFilePreservesBranches() const
    {
        // CD1/movie.mkv + CD2/movie.mkv must not collapse both to Unique/movie.mkv.
        const PathList out = applyUniqueSubfolderLayout(
                {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)}, sampleId, u"Show"_s);
        QCOMPARE(out.at(0), Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(out.at(1), Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
    }

    void testIdempotentMultiFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"Show/ep.mkv"_s)}, sampleId, u"Show"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"Show"_s);
        QCOMPARE(twice, once);
    }

    void testIdempotentSingleFile() const
    {
        const PathList once = applyUniqueSubfolderLayout({Path(u"movie.mkv"_s)}, sampleId, u"movie"_s);
        const PathList twice = applyUniqueSubfolderLayout(once, sampleId, u"movie"_s);
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

    void testPlanNoSubfolderCD1CD2() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const PathList current {Path(u"CD1/movie.mkv"_s), Path(u"CD2/movie.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/CD1/movie.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/CD2/movie.mkv"_s));
    }

    void testPlanSubfolderReplacesTopLevel() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const PathList current {Path(u"Show/ep.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
    }

    void testPlanDestDirectoryExistsEmptyOk() const
    {
        // Unique root directory may exist; only exact target paths block.
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

    void testPlanUnrelatedFileInDestOk() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path destDir = storageRoot / uniqueShow;
        QVERIFY(Utils::Fs::mkpath(destDir));
        QFile unrelated {(destDir / Path(u"notes.txt"_s)).data()};
        QVERIFY(unrelated.open(QIODevice::WriteOnly));
        unrelated.write("keep");
        unrelated.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QVERIFY(QFile::exists((destDir / Path(u"notes.txt"_s)).data()));
    }

    void testPlanTargetPathExistingFileBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path destDir = storageRoot / uniqueShow;
        QVERIFY(Utils::Fs::mkpath(destDir));
        QFile conflict {(destDir / Path(u"ep.mkv"_s)).data()};
        QVERIFY(conflict.open(QIODevice::WriteOnly));
        conflict.write("old");
        conflict.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(plan.blocked);
        QVERIFY(plan.renames.isEmpty());
    }

    void testPlanTargetPathExistingDirectoryBlocks() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        // Exact target path exists as a directory.
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
        // Parent component "Show a19f83c275d1" is a regular file, not a directory.
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

    void testPlanPartialMigrationRetry() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const PathList partial {
            Path(u"Show a19f83c275d1/ep1.mkv"_s),
            Path(u"Show/ep2.mkv"_s)
        };

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                partial, sampleId, u"Show"_s, storageRoot, TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).fileIndex, 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep2.mkv"_s));
    }

    void testPlanFinalizeOnlyWhenAlreadyUnderUnique() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::Subfolder);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.renames.isEmpty());
        QVERIFY(plan.finalizeOnly);
        QVERIFY(!plan.isEmpty());
    }

    void testPlanEmptyWhenAlreadyUniqueLayout() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()), TorrentContentLayout::UniqueSubfolder);

        QVERIFY(plan.isEmpty());
        QVERIFY(!plan.finalizeOnly);
    }

    void testPlanSingleFileMovie() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"movie.mkv"_s)};

        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"movie"_s, Path(tmp.path()), TorrentContentLayout::NoSubfolder);

        QVERIFY(!plan.blocked);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"movie a19f83c275d1/movie.mkv"_s));
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
