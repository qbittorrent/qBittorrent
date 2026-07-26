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
#include "base/global.h"
#include "base/path.h"

using namespace BitTorrent;

class TestTopLevelPayload final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTopLevelPayload)

public:
    TestTopLevelPayload() = default;

private slots:
    void testHashTagFormat() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        QCOMPARE(payloadHashTag(id), u" [qb-a19f83c275d1]"_s);
    }

    void testHashDirectoryName() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        QCOMPARE(payloadHashDirectoryName(id, u"Show"_s), u"Show [qb-a19f83c275d1]"_s);
    }

    void testFolderBecomesHashDirectory() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList out = applyPayloadHashNaming(
                {Path(u"Show/ep1.mkv"_s), Path(u"Show/ep2.mkv"_s)}, id, u"Show"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"Show [qb-a19f83c275d1]"_s);
        QCOMPARE(out.at(0), Path(u"Show [qb-a19f83c275d1]/ep1.mkv"_s));
    }

    void testSingleFileWrapsInHashDirectory() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList out = applyPayloadHashNaming({Path(u"movie.mkv"_s)}, id, u"movie"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"movie [qb-a19f83c275d1]"_s);
        QCOMPARE(out.at(0), Path(u"movie [qb-a19f83c275d1]/movie.mkv"_s));
    }

    void testRootlessWrapsInHashDirectory() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList out = applyPayloadHashNaming(
                {Path(u"a.mkv"_s), Path(u"b.srt"_s)}, id, u"Torrent Name"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"Torrent Name [qb-a19f83c275d1]"_s);
        QCOMPARE(out.at(0), Path(u"Torrent Name [qb-a19f83c275d1]/a.mkv"_s));
    }

    void testDifferentTorrentsDifferentDirectories() const
    {
        const TorrentID idA = TorrentID::fromString(u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"_s);
        const TorrentID idB = TorrentID::fromString(u"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"_s);
        const PathList layout {Path(u"Show/ep.mkv"_s)};
        const PathList outA = applyPayloadHashNaming(layout, idA, u"Show"_s);
        const PathList outB = applyPayloadHashNaming(layout, idB, u"Show"_s);
        QVERIFY(Path::findRootFolder(outA) != Path::findRootFolder(outB));
    }

    void testIdempotentNoNestedHashDirectory() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList once = applyPayloadHashNaming({Path(u"Show/ep.mkv"_s)}, id, u"Show"_s);
        const PathList twice = applyPayloadHashNaming(once, id, u"Show"_s);
        QCOMPARE(twice, once);
        QCOMPARE(Path::findRootFolder(twice).toString(), u"Show [qb-a19f83c275d1]"_s);
    }

    void testIdempotentSingleFile() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList once = applyPayloadHashNaming({Path(u"movie.mkv"_s)}, id, u"movie"_s);
        const PathList twice = applyPayloadHashNaming(once, id, u"movie"_s);
        QCOMPARE(twice, once);
        QCOMPARE(twice.at(0), Path(u"movie [qb-a19f83c275d1]/movie.mkv"_s));
    }

    void testIdempotentRootless() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList once = applyPayloadHashNaming(
                {Path(u"a.mkv"_s), Path(u"b.srt"_s)}, id, u"Pack"_s);
        const PathList twice = applyPayloadHashNaming(once, id, u"Pack"_s);
        QCOMPARE(twice, once);
        QVERIFY(!twice.at(0).toString().contains(u"[qb-a19f83c275d1]/Pack [qb-"_s));
    }

    void testUnicodeName() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList out = applyPayloadHashNaming(
                {Path(u"映画/ep.mkv"_s)}, id, u"映画"_s);
        QCOMPARE(Path::findRootFolder(out).toString(), u"映画 [qb-a19f83c275d1]"_s);
    }

    void testLongNameTruncated() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const QString longName = QString(300, u'a');
        const PathList out = applyPayloadHashNaming({Path(u"f.mkv"_s)}, id, longName);
        const QString root = Path::findRootFolder(out).toString();
        QVERIFY(root.size() <= 255);
        QVERIFY(root.endsWith(u" [qb-a19f83c275d1]"_s));
    }

    void testAlreadyHashedRootUnchanged() const
    {
        const TorrentID id = TorrentID::fromString(u"a19f83c275d1aabbccddeeff0011223344556677"_s);
        const PathList partial {Path(u"Show [qb-a19f83c275d1]/ep.mkv"_s)};
        const PathList out = applyPayloadHashNaming(partial, id, u"Show"_s);
        QCOMPARE(out, partial);
    }
};

QTEST_APPLESS_MAIN(TestTopLevelPayload)
#include "testtoplevelpayload.moc"
