/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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

#include "base/bittorrent/filesearcher.h"
#include "base/global.h"
#include "base/path.h"

class TestBittorrentCandidateRoots final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentCandidateRoots)

public:
    TestBittorrentCandidateRoots() = default;

private slots:
    void testPathListComparisonDiscriminates() const
    {
        QCOMPARE((PathList {Path(u"a/b"_s), Path(u"c"_s)}), (PathList {Path(u"a/b"_s), Path(u"c"_s)}));
        QCOMPARE_NE((PathList {Path(u"a/b"_s), Path(u"c"_s)}), (PathList {Path(u"c"_s), Path(u"a/b"_s)}));
        QCOMPARE_NE((PathList {Path(u"a/b"_s), Path(u"c"_s)}), (PathList {Path(u"a/b"_s), Path(u"d"_s)}));
    }

    void testOrderFollowsSearchRoots() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s), Path(u"/other"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s), Path(u"/watch/Album Rip"_s)
                , Path(u"/other"_s), Path(u"/other/Album"_s), Path(u"/other/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testOwnPathsAreExcluded() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/save"_s), Path(u"/download"_s), Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/save/Album"_s), Path(u"/save/Album Rip"_s)
                , Path(u"/download/Album"_s), Path(u"/download/Album Rip"_s)
                , Path(u"/watch"_s), Path(u"/watch/Album"_s), Path(u"/watch/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testEmptyDownloadPathExcludesNothing() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path()
                , {Path(u"/download"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/download"_s), Path(u"/download/Album"_s), Path(u"/download/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testEmptySearchRootTakesDefault() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path()}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/default"_s), Path(u"/default/Album"_s), Path(u"/default/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testEmptySearchRootWithoutDefaultContributesNothing() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(), Path(u"/watch"_s)}, Path(), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s), Path(u"/watch/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testDuplicateKeepsEarliestPosition() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s), Path(u"/other"_s), Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s), Path(u"/watch/Album Rip"_s)
                , Path(u"/other"_s), Path(u"/other/Album"_s), Path(u"/other/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testAbsentNameOmitsNameForm() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u""_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testAbsentSourceOmitsSourceForm() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u""_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s)};
        QCOMPARE(result, expected);
    }

    void testSourceExtensionStrippedCaseInsensitively() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.TORRENT"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s), Path(u"/watch/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testSourceFormMatchingNameFormCollapses() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"Album.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s)};
        QCOMPARE(result, expected);
    }

    void testEscapingNameIsDropped() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u"../escape"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album Rip"_s)};
        QCOMPARE(result, expected);
    }

    void testEscapingSourceIsDropped() const
    {
        const PathList result = candidateRoots(Path(u"/save"_s), Path(u"/download"_s)
                , {Path(u"/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"...torrent"_s);
        const PathList expected {Path(u"/watch"_s), Path(u"/watch/Album"_s)};
        QCOMPARE(result, expected);
    }

    void testDuplicatesFoldCaseOnWindows() const
    {
#ifdef Q_OS_WIN
        const PathList result = candidateRoots(Path(u"C:/Save"_s), Path()
                , {Path(u"c:/save"_s), Path(u"D:/Watch"_s), Path(u"d:/watch"_s)}, Path(u"/default"_s), u"Album"_s, u"Album Rip.torrent"_s);
        const PathList expected {Path(u"c:/save/Album"_s), Path(u"c:/save/Album Rip"_s)
                , Path(u"D:/Watch"_s), Path(u"D:/Watch/Album"_s), Path(u"D:/Watch/Album Rip"_s)};
        QCOMPARE(result, expected);
#else
        QSKIP("Requires case-insensitive path comparison.");
#endif
    }
};

QTEST_APPLESS_MAIN(TestBittorrentCandidateRoots)
#include "testbittorrentcandidateroots.moc"
