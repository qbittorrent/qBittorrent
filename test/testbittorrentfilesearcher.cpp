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

#include <QFuture>
#include <QObject>
#include <QPromise>
#include <QTest>

#include "base/bittorrent/common.h"
#include "base/bittorrent/filesearcher.h"
#include "base/global.h"
#include "base/path.h"

namespace
{
    const PathList fixtureFileNames()
    {
        return {Path(u"alpha.txt"_s), Path(u"beta.txt"_s)};
    }

    FileSearchResult runSearch(const PathList &originalFileNames, const Path &savePath
            , const Path &downloadPath, const bool forceAppendExt)
    {
        FileSearcher searcher;
        QPromise<FileSearchResult> promise;
        promise.start();
        searcher.search(originalFileNames, savePath, downloadPath, forceAppendExt, promise);
        promise.finish();
        return promise.future().result();
    }
}

class TestBittorrentFileSearcher final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentFileSearcher)

public:
    TestBittorrentFileSearcher() = default;

private slots:
    void testSavePathHoldingAnyFileIsUsed() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"complete"_s);
        const Path downloadPath;

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, true);
        QCOMPARE(result.savePath, savePath);
        QCOMPARE(result.fileNames, fixtureFileNames());
    }

    void testForceAppendsAtSavePathWithoutDownloadPath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath;

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, true);
        QCOMPARE(result.savePath, savePath);
        const PathList expectedFileNames {Path(u"alpha.txt"_s), Path(u"beta.txt"_s) + QB_EXT};
        QCOMPARE(result.fileNames, expectedFileNames);
    }

    void testNoForceLeavesAbsentNamesUnchanged() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath;

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, false);
        QCOMPARE(result.fileNames, fixtureFileNames());
    }

    void testIncompleteVariantIsAdopted() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"incomplete"_s);
        const Path downloadPath;

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, false);
        QCOMPARE(result.savePath, savePath);
        const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s)};
        QCOMPARE(result.fileNames, expectedFileNames);
    }

    void testFirstDirectoryHoldingAnyFileWins() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath = fixtureRoot / Path(u"complete"_s);

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, false);
        QCOMPARE(result.savePath, savePath);
    }

    void testDownloadPathSuppressesForceAtSavePath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath = fixtureRoot / Path(u"complete"_s);

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, true);
        QCOMPARE(result.fileNames, fixtureFileNames());
    }

    void testDownloadPathUsedWhenSavePathHoldsNothing() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"complete"_s);

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, false);
        QCOMPARE(result.savePath, downloadPath);
    }

    void testMissAppendsAtDownloadPath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, true);
        QCOMPARE(result.savePath, downloadPath);
        const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s) + QB_EXT};
        QCOMPARE(result.fileNames, expectedFileNames);
    }

    void testMissWithoutDownloadPathStaysAtSavePath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath;

        const FileSearchResult result = runSearch(fixtureFileNames(), savePath, downloadPath, true);
        QCOMPARE(result.savePath, savePath);
        const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s) + QB_EXT};
        QCOMPARE(result.fileNames, expectedFileNames);
    }
};

QTEST_APPLESS_MAIN(TestBittorrentFileSearcher)
#include "testbittorrentfilesearcher.moc"
