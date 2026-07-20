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
#include <QSet>
#include <QTest>

#include "base/bittorrent/toplevelpayload.h"
#include "base/global.h"
#include "base/path.h"

using namespace BitTorrent;

namespace
{
    class Occupied final
    {
    public:
        void add(const PathList &roots, const PathList &files)
        {
            m_paths.unite(payloadClaimPaths(roots, files));
        }

        void add(const Path &root, const PathList &files)
        {
            add(PathList {root}, files);
        }

        bool taken(const Path &path) const
        {
            return isPayloadPathTaken(path, m_paths);
        }

        nonstd::expected<PathList, QString> uniquify(const PathList &roots, PathList files, const bool allowRename) const
        {
            return uniquifyPayloadPaths(roots, std::move(files), allowRename
                    , [this](const Path &p) { return taken(p); });
        }

    private:
        QSet<Path> m_paths;
    };
}

class TestTopLevelPayload final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTopLevelPayload)

public:
    TestTopLevelPayload() = default;

private slots:
    void testIncompleteAndFinalBothReserved() const
    {
        // Torrent A downloading in /tmp/qbit with final /disk2/anime
        Occupied occ;
        const PathList aRoots {Path(u"/tmp/qbit"_s), Path(u"/disk2/anime"_s)};
        const PathList layout {Path(u"Show/ep.mkv"_s)};
        occ.add(aRoots, layout);

        // Second torrent targets final path /disk2/anime/Show → must rename
        const auto r = occ.uniquify({Path(u"/disk2/anime"_s)}, layout, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show (1)"_s);
    }

    void testDifferentFinalAndIncompleteNoRename() const
    {
        Occupied occ;
        occ.add({Path(u"/tmp/a"_s), Path(u"/disk/a"_s)}, {Path(u"Show/x.mkv"_s)});

        // Different incomplete + different final → no collision
        const auto r = occ.uniquify({Path(u"/tmp/b"_s), Path(u"/disk/b"_s)}, {Path(u"Show/x.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show"_s);
    }

    void testSharedIncompleteForcesRename() const
    {
        // Same incomplete dir, different finals — incomplete still collides
        Occupied occ;
        occ.add({Path(u"/tmp/qbit"_s), Path(u"/disk/a"_s)}, {Path(u"Show/x.mkv"_s)});

        const auto r = occ.uniquify({Path(u"/tmp/qbit"_s), Path(u"/disk/b"_s)}, {Path(u"Show/y.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show (1)"_s);
    }

    void testNameMustBeFreeUnderAllRoots() const
    {
        Occupied occ;
        // Only final path has Show (1); incomplete is free for Show (1)
        occ.add(Path(u"/disk2/anime"_s), {Path(u"Show (1)/a.mkv"_s)});

        // First torrent wants free "Show" under both roots
        const PathList roots {Path(u"/tmp/qbit"_s), Path(u"/disk2/anime"_s)};
        // Occupied has Show free at both? Show free. Good.
        // Candidate Show (1) taken on final only → findFreeName for rename from Show when Show taken...

        occ.add(Path(u"/tmp/qbit"_s), {Path(u"Show/a.mkv"_s)});
        // Now Show taken on incomplete; final Show free. uniquify must rename.
        // Show (1) taken on final → need Show (2)
        const auto r = occ.uniquify(roots, {Path(u"Show/b.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show (2)"_s);
    }

    void testRootFolderRename() const
    {
        Occupied occ;
        occ.add(Path(u"/d"_s), {Path(u"Show/a.mkv"_s)});
        const auto r = occ.uniquify({Path(u"/d"_s)}, {Path(u"Show/b.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show (1)"_s);
    }

    void testSingleFile() const
    {
        Occupied occ;
        occ.add(Path(u"/d"_s), {Path(u"movie.mkv"_s)});
        const auto r = occ.uniquify({Path(u"/d"_s)}, {Path(u"movie.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(r.value().at(0), Path(u"movie (1).mkv"_s));
    }

    void testRootlessWrap() const
    {
        Occupied occ;
        occ.add(Path(u"/d"_s), {Path(u"episode.mkv"_s)});
        const auto r = occ.uniquify({Path(u"/d"_s)}, {
            Path(u"episode.mkv"_s), Path(u"sub.srt"_s)
        }, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"episode"_s);
    }

    void testSkipCheckRejects() const
    {
        Occupied occ;
        occ.add(Path(u"/d"_s), {Path(u"Show/a.mkv"_s)});
        QVERIFY(!occ.uniquify({Path(u"/d"_s)}, {Path(u"Show/b.mkv"_s)}, false).has_value());
    }

    void testMagnetSkipCheckingMetadataRejectsOccupiedPath() const
    {
        // Magnet added with skip-checking/seed-mode: allowRename=false when metadata arrives.
        Occupied occ;
        occ.add(Path(u"/d"_s), {Path(u"Show/existing.mkv"_s)});

        const auto renamed = occ.uniquify({Path(u"/d"_s)}, {Path(u"Show/from-magnet.mkv"_s)}, true);
        QVERIFY(renamed.has_value());
        QCOMPARE(Path::findRootFolder(renamed.value()).toString(), u"Show (1)"_s);

        const auto seedMode = occ.uniquify({Path(u"/d"_s)}, {Path(u"Show/from-magnet.mkv"_s)}, false);
        QVERIFY(!seedMode.has_value());
    }

    void testSameTorrentPendingRenamesCannotShareDestination() const
    {
        // Simulates isPhysicalPayloadPathTaken without skipping same-torrent pending renames.
        Occupied occ;
        const Path root {u"/d"_s};
        // First rename reserved the proposed layout claiming result.mkv
        occ.add(root, {Path(u"result.mkv"_s), Path(u"b.mkv"_s)});

        // Second rename from the same torrent also wants result.mkv as a new claim.
        // Newly introduced path must still see the pending reservation as occupied.
        QVERIFY(occ.taken(Path(u"/d/result.mkv"_s)));

        const PathList current {Path(u"a.mkv"_s), Path(u"b.mkv"_s)};
        const PathList proposed = applyContentRename(current, Path(u"b.mkv"_s), Path(u"result.mkv"_s));
        const QSet<Path> currentClaims = payloadClaimPaths(root, current);
        const QSet<Path> proposedClaims = payloadClaimPaths(root, proposed);

        bool canReserve = true;
        for (const Path &path : asConst(proposedClaims))
        {
            if (isPayloadPathTaken(path, currentClaims))
                continue;
            if (occ.taken(path))
            {
                canReserve = false;
                break;
            }
        }
        QVERIFY(!canReserve);
    }

    void testNestedLayoutCollision() const
    {
        Occupied occ;
        occ.add(Path(u"/d/Show"_s), {Path(u"episode.mkv"_s)});
        const auto r = occ.uniquify({Path(u"/d"_s)}, {Path(u"Show/episode.mkv"_s)}, true);
        QVERIFY(r.has_value());
        QCOMPARE(Path::findRootFolder(r.value()).toString(), u"Show (1)"_s);
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
