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

#include <QtSystemDetection>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QObject>
#include <QPromise>
#include <QScopeGuard>
#include <QTemporaryDir>
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

    SearchRootsResult runSearchRoots(const PathList &originalFileNames, const Path &savePath
            , const Path &downloadPath, const PathList &candidates, const bool forceAppendExt)
    {
        FileSearcher searcher;
        QPromise<SearchRootsResult> promise;
        promise.start();
        searcher.searchRoots(originalFileNames, savePath, downloadPath, candidates, forceAppendExt, promise);
        promise.finish();
        return promise.future().result();
    }
}

class TestBittorrentFileSearcherMultiRoot final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentFileSearcherMultiRoot)

public:
    TestBittorrentFileSearcherMultiRoot() = default;

private slots:
    void testSearchRootWithMoreBeatsSavePath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath;
        const PathList candidates {fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"complete"_s));
        QCOMPARE(result.fileNames, fixtureFileNames());
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.searchedCandidates, true);
        QCOMPARE(result.foundAtOwnPath, false);
    }

    void testSearchRootWithMoreBeatsDownloadPath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"partial"_s);
        const PathList candidates {fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"complete"_s));
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.searchedCandidates, true);
        QCOMPARE(result.foundAtOwnPath, false);
    }

    void testCompleteSearchRootBeatsIncompleteSavePath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"incomplete"_s);
        const Path downloadPath;
        const PathList candidates {fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, true);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"complete"_s));
        QCOMPARE(result.fileNames, fixtureFileNames());
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.foundAtOwnPath, false);
    }

    void testSavePathWinsTieWithSearchRoot() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath;
        const PathList candidates {fixtureRoot / Path(u"partialtwin"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, savePath);
        QCOMPARE(result.matchCount, 1);
        QCOMPARE(result.searchedCandidates, true);
        QCOMPARE(result.foundAtOwnPath, true);
    }

    void testForceAppliedAtSavePathWithoutDownloadPath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath;
        const PathList candidates {fixtureRoot / Path(u"partialtwin"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, true);
        QCOMPARE(result.savePath, savePath);
        const PathList expectedFileNames {Path(u"alpha.txt"_s), Path(u"beta.txt"_s) + QB_EXT};
        QCOMPARE(result.fileNames, expectedFileNames);
        QCOMPARE(result.matchCount, 1);
        QCOMPARE(result.foundAtOwnPath, true);
    }

    void testDownloadPathWithMoreBeatsSavePath() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"partial"_s);
        const Path downloadPath = fixtureRoot / Path(u"complete"_s);

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, {}, false);
        QCOMPARE(result.savePath, downloadPath);
        QCOMPARE(result.fileNames, fixtureFileNames());
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.searchedCandidates, false);
        QCOMPARE(result.foundAtOwnPath, true);
    }

    void testOwnPathFullMatchSkipsSearchRoots() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"complete"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"partial"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, savePath);
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.searchedCandidates, false);
        QCOMPARE(result.foundAtOwnPath, true);
    }

    void testOwnPathsMatchSearchWhenNoSearchRoots() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const PathList noCandidates;

        {
            const Path savePath = fixtureRoot / Path(u"partial"_s);
            const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);

            const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, noCandidates, true);
            QCOMPARE(result.savePath, savePath);
            QCOMPARE(result.fileNames, fixtureFileNames());
            QCOMPARE(result.matchCount, 1);
            QCOMPARE(result.searchedCandidates, false);
            QCOMPARE(result.foundAtOwnPath, true);
        }
        {
            const Path savePath = fixtureRoot / Path(u"absent"_s);
            const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);

            const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, noCandidates, true);
            QCOMPARE(result.savePath, downloadPath);
            const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s) + QB_EXT};
            QCOMPARE(result.fileNames, expectedFileNames);
            QCOMPARE(result.matchCount, 0);
            QCOMPARE(result.searchedCandidates, false);
            QCOMPARE(result.foundAtOwnPath, false);
        }
    }

    void testMostFilesWins() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"partial"_s), fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"complete"_s));
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.searchedCandidates, true);
    }

    void testEarlierRootWinsTie() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"partial"_s), fixtureRoot / Path(u"partialtwin"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"partial"_s));
        QCOMPARE(result.matchCount, 1);
    }

    void testIncompleteVariantCounts() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"incomplete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"incomplete"_s));
        QCOMPARE(result.matchCount, 1);
        const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s)};
        QCOMPARE(result.fileNames, expectedFileNames);
    }

    void testScoringDoesNotCarryRewrittenNames() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"incomplete"_s), fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"complete"_s));
        QCOMPARE(result.matchCount, 2);
        QCOMPARE(result.fileNames, fixtureFileNames());
    }

    void testMissFallsBackToDestination() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"absent"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, true);
        QCOMPARE(result.savePath, downloadPath);
        QCOMPARE(result.matchCount, 0);
        QCOMPARE(result.searchedCandidates, true);
        QCOMPARE(result.foundAtOwnPath, false);
        const PathList expectedFileNames {Path(u"alpha.txt"_s) + QB_EXT, Path(u"beta.txt"_s) + QB_EXT};
        QCOMPARE(result.fileNames, expectedFileNames);
    }

    void testForceNotAppliedAtSearchOnlyWinner() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"partial"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, true);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"partial"_s));
        QCOMPARE(result.fileNames, fixtureFileNames());
    }

    void testUnavailableRootContributesNothing() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {fixtureRoot / Path(u"absent"_s), fixtureRoot / Path(u"partial"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"partial"_s));
        QCOMPARE(result.matchCount, 1);
    }

    void testUnreadableRootContributesNothing() const
    {
#ifdef Q_OS_UNIX
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const Path child = Path(tempDir.path()) / Path(u"child"_s);
        QVERIFY(QDir().mkpath(child.data()));

        const Path alphaFile = child / Path(u"alpha.txt"_s);
        const Path betaFile = child / Path(u"beta.txt"_s);
        {
            QFile f(alphaFile.data());
            QVERIFY(f.open(QFile::WriteOnly));
            QVERIFY(f.write("fixture\n") >= 0);
        }
        {
            QFile f(betaFile.data());
            QVERIFY(f.open(QFile::WriteOnly));
            QVERIFY(f.write("fixture\n") >= 0);
        }

        QVERIFY(QFile::setPermissions(child.data(), {}));
        auto guard = qScopeGuard([child]
        {
            QFile::setPermissions(child.data(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        });

        if (QFileInfo::exists(alphaFile.data()))
            QSKIP("Account can traverse the child; precondition cannot be established.");

        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList candidates {child, fixtureRoot / Path(u"partial"_s)};

        const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"partial"_s));
        QCOMPARE(result.matchCount, 1);
#else
        QSKIP("Requires Unix permission semantics.");
#endif
    }

    void testEmptyRootIsSkipped() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        QVERIFY(fixtureRoot.isAbsolute());

        const QString savedCurrentPath = QDir::currentPath();
        {
            auto guard = qScopeGuard([&savedCurrentPath]
            {
                QDir::setCurrent(savedCurrentPath);
            });

            QVERIFY(QDir::setCurrent((fixtureRoot / Path(u"complete"_s)).data()));

            const Path savePath = fixtureRoot / Path(u"absent"_s);
            const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
            const PathList candidates {Path(), fixtureRoot / Path(u"partial"_s)};

            const SearchRootsResult result = runSearchRoots(fixtureFileNames(), savePath, downloadPath, candidates, false);
            QCOMPARE(result.savePath, fixtureRoot / Path(u"partial"_s));
            QCOMPARE(result.matchCount, 1);
        }
        QCOMPARE(QDir::currentPath(), savedCurrentPath);
    }

    void testNamesUnderPresentFolderCountedEach() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList fileNames {Path(u"folder/alpha.txt"_s), Path(u"folder/beta.txt"_s)};
        const PathList candidates {fixtureRoot / Path(u"complete"_s), fixtureRoot / Path(u"nested"_s)};

        const SearchRootsResult result = runSearchRoots(fileNames, savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, fixtureRoot / Path(u"nested"_s));
        QCOMPARE(result.matchCount, 1);
    }

    void testAbsentFolderCountsNone() const
    {
        const Path fixtureRoot = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
        const Path savePath = fixtureRoot / Path(u"absent"_s);
        const Path downloadPath = fixtureRoot / Path(u"absentdownload"_s);
        const PathList fileNames {Path(u"folder/alpha.txt"_s), Path(u"folder/beta.txt"_s)};
        const PathList candidates {fixtureRoot / Path(u"complete"_s)};

        const SearchRootsResult result = runSearchRoots(fileNames, savePath, downloadPath, candidates, false);
        QCOMPARE(result.savePath, downloadPath);
        QCOMPARE(result.matchCount, 0);
        QCOMPARE(result.searchedCandidates, true);
    }
};

QTEST_APPLESS_MAIN(TestBittorrentFileSearcherMultiRoot)
#include "testbittorrentfilesearchermultiroot.moc"
