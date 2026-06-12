/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent project
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
#include <QFuture>
#include <QObject>
#include <QPromise>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include "base/bittorrent/filesearcher.h"
#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"

class TestBitTorrentFileSearcher final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBitTorrentFileSearcher)

public:
    TestBitTorrentFileSearcher() = default;

private:
    FileSearchResult search(const PathList &fileNames, const Path &savePath
            , const Path &downloadPath = {}, const bool forceAppendExt = false
            , const QSet<Path> &occupiedRootPaths = {}) const
    {
        QPromise<FileSearchResult> promise;
        QFuture<FileSearchResult> future = promise.future();
        promise.start();

        FileSearcher fileSearcher;
        fileSearcher.search(fileNames, savePath, downloadPath, forceAppendExt, occupiedRootPaths, promise);
        promise.finish();

        future.waitForFinished();
        return future.result();
    }

private slots:
    void testPreserveExistingRootWhenItIsNotOccupied() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path savePath {tempDir.path()};
        QVERIFY(Utils::Fs::mkpath(savePath / Path(u"Torrent"_s)));
        QVERIFY(QFile((savePath / Path(u"Torrent/file.txt"_s)).data()).open(QIODevice::WriteOnly));

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath);

        QCOMPARE_EQ(result.savePath, savePath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent/file.txt"_s)}));
    }

    void testAvoidOccupiedRoot() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path savePath {tempDir.path()};
        const QSet<Path> occupiedRootPaths {savePath / Path(u"Torrent"_s)};

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath, {}, false, occupiedRootPaths);

        QCOMPARE_EQ(result.savePath, savePath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent (1)/file.txt"_s)}));
    }

    void testAvoidMultipleOccupiedRoots() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path savePath {tempDir.path()};
        const QSet<Path> occupiedRootPaths
        {
            savePath / Path(u"Torrent"_s),
            savePath / Path(u"Torrent (1)"_s)
        };

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath, {}, false, occupiedRootPaths);

        QCOMPARE_EQ(result.savePath, savePath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent (2)/file.txt"_s)}));
    }

    void testAvoidOccupiedRootInDownloadPath() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path basePath {tempDir.path()};
        const Path savePath = basePath / Path(u"save"_s);
        const Path downloadPath = basePath / Path(u"incomplete"_s);
        const QSet<Path> occupiedRootPaths {downloadPath / Path(u"Torrent"_s)};

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath, downloadPath, false, occupiedRootPaths);

        QCOMPARE_EQ(result.savePath, downloadPath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent (1)/file.txt"_s)}));
    }

    void testAvoidOccupiedRootInFinalSavePath() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path basePath {tempDir.path()};
        const Path savePath = basePath / Path(u"save"_s);
        const Path downloadPath = basePath / Path(u"incomplete"_s);
        const QSet<Path> occupiedRootPaths {savePath / Path(u"Torrent"_s)};

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath, downloadPath, false, occupiedRootPaths);

        QCOMPARE_EQ(result.savePath, downloadPath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent (1)/file.txt"_s)}));
    }

    void testAvoidOccupiedRootsAcrossStoragePaths() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path basePath {tempDir.path()};
        const Path savePath = basePath / Path(u"save"_s);
        const Path downloadPath = basePath / Path(u"incomplete"_s);
        const QSet<Path> occupiedRootPaths
        {
            downloadPath / Path(u"Torrent"_s),
            savePath / Path(u"Torrent (1)"_s)
        };

        const FileSearchResult result = search({Path(u"Torrent/file.txt"_s)}, savePath, downloadPath, false, occupiedRootPaths);

        QCOMPARE_EQ(result.savePath, downloadPath);
        QCOMPARE_EQ(result.fileNames, PathList({Path(u"Torrent (2)/file.txt"_s)}));
    }

    void testKeepOriginalRootForSecondCollisionPass() const
    {
        PathList fileNames {Path(u"Torrent (1)/file.txt"_s)};
        const QSet<Path> occupiedRootPaths
        {
            Path(u"/downloads/Torrent"_s),
            Path(u"/downloads/Torrent (1)"_s)
        };

        const Path rootFolder = FileSearcher::makeRootFolderUnique(
                fileNames, Path(u"Torrent"_s), {Path(u"/downloads"_s)}, occupiedRootPaths);

        QCOMPARE_EQ(rootFolder, Path(u"Torrent (2)"_s));
        QCOMPARE_EQ(fileNames, PathList({Path(u"Torrent (2)/file.txt"_s)}));
    }
};

QTEST_APPLESS_MAIN(TestBitTorrentFileSearcher)
#include "testbittorrentfilesearcher.moc"
