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

#include <QDir>
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
        // CJK is multi-byte in UTF-8; must still keep full hash and fit platform limit.
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

    void testPlanDestAbsent() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QVERIFY(!plan.needsConfirmation());
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
    }

    void testPlanDestExistsNeedsConfirmation() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path dest = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(dest));

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(!plan.blocked);
        QVERIFY(plan.needsConfirmation());
        QCOMPARE(plan.existingUniqueFolder, dest);
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
    }

    void testPlanConflictingTorrentPathsStillRenamed() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path dest = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(dest));
        // Existing file that conflicts with a torrent path — plan still renames onto it (overwrite).
        QFile conflict {(dest / Path(u"ep.mkv"_s)).data()};
        QVERIFY(conflict.open(QIODevice::WriteOnly));
        conflict.write("old");
        conflict.close();

        const PathList current {Path(u"Show/ep.mkv"_s), Path(u"Show/extra.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(plan.needsConfirmation());
        QCOMPARE(plan.renames.size(), 2);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
        QCOMPARE(plan.renames.at(1).to, Path(u"Show a19f83c275d1/extra.mkv"_s));
    }

    void testPlanUnrelatedDestFilesNotInRenameList() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};
        const Path dest = storageRoot / Path(u"Show a19f83c275d1"_s);
        QVERIFY(Utils::Fs::mkpath(dest));
        QFile unrelated {(dest / Path(u"notes.txt"_s)).data()};
        QVERIFY(unrelated.open(QIODevice::WriteOnly));
        unrelated.write("keep me");
        unrelated.close();

        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(plan.needsConfirmation());
        // Only torrent file paths are planned — unrelated notes.txt is never a rename target.
        QCOMPARE(plan.renames.size(), 1);
        QCOMPARE(plan.renames.at(0).to, Path(u"Show a19f83c275d1/ep.mkv"_s));
        QVERIFY(QFile::exists((dest / Path(u"notes.txt"_s)).data()));
    }

    void testPlanEmptyWhenAlreadyUnique() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const Path storageRoot {tmp.path()};

        const PathList current {Path(u"Show a19f83c275d1/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, storageRoot);

        QVERIFY(plan.isEmpty());
        QVERIFY(!plan.needsConfirmation());
    }

    void testPlanBlockedWhenStorageUnknown() const
    {
        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, {});

        QVERIFY(plan.blocked);
        QVERIFY(!plan.blockReason.isEmpty());
        QVERIFY(plan.renames.isEmpty());
    }

    // Layout flag is only set by finishUniqueSubfolderMigration when all renames succeed
    // (TorrentImpl). Plan itself never changes content layout — it only lists renames.
    void testPlanDoesNotImplyLayoutChange() const
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const PathList current {Path(u"Show/ep.mkv"_s)};
        const UniqueSubfolderMigrationPlan plan = makeUniqueSubfolderMigrationPlan(
                current, sampleId, u"Show"_s, Path(tmp.path()));

        QVERIFY(!plan.renames.isEmpty());
        // No layout field on the plan — conversion is deferred until renames complete.
        QVERIFY(!plan.blocked);
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
