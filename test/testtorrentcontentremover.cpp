/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent Contributors
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

// RED test for: "5.3.0 does not delete content even though it's checked"
// (completed single-file torrent, TorrentContentRemoveOption::Delete).
//
// Observed symptom (local logs, 2026-11-14):
//   (N) Torrent usunięto. Torrent: "[...].mkv"          <- "Torrent removed"
//   -- missing: (N) Usunięto zawartość torrenta. ...    <- "Torrent content removed"
//   -- missing: Failed to remove torrent content ...
//   + file still present under D:/download/torrent/
// Related upstream: #21993, #21457, #21813 (open); #21642, #22183 (closed dupes).
//
// Root cause candidates in release-5.3.0:
//   1. SessionImpl::handleRemovedTorrent() silently skips the remover job when
//      contentStoragePath.isEmpty(), with no warning
//      (src/base/bittorrent/sessionimpl.cpp).
//   2. TorrentContentRemover::performJob() emits jobFinished(torrentName, "")
//      (success) when fileNames.isEmpty(), so an empty file list is logged as
//      "Torrent content removed" while nothing was deleted
//      (src/base/bittorrent/torrentcontentremover.cpp:40-66).
// This test pins down (2): an empty file list must NOT report success.

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "base/bittorrent/torrentcontentremover.h"
#include "base/path.h"

using namespace Qt::Literals::StringLiterals;

class TestTorrentContentRemover final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTorrentContentRemover)

public:
    TestTorrentContentRemover() = default;

private slots:
    // Sanity: deleting a real completed single file works and reports no error.
    void testDeleteSingleCompletedFile() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const Path basePath {tempDir.path()};
        const Path fileName {u"[SubsPlease] completed episode - 04 (1080p) [D69FCE39].mkv"_s};
        QFile file {(basePath / fileName).data()};
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("fake-video-bytes");
        file.close();
        QVERIFY(file.exists());

        BitTorrent::TorrentContentRemover remover;
        QSignalSpy spy(&remover, &BitTorrent::TorrentContentRemover::jobFinished);
        remover.performJob(u"completed-torrent"_s, basePath
            , PathList {fileName}, BitTorrent::TorrentContentRemoveOption::Delete);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QString()); // no error
        QVERIFY(!QFile::exists((basePath / fileName).data())); // file actually gone
    }

    // RED: empty file list (e.g. actualFilePaths() == {} for a completed
    // torrent that lost metadata / shutdown race) must NOT be reported as
    // success. Current code emits errorMessage == "" -> logged as
    // "Torrent content removed" while nothing was deleted.
    void testEmptyFileListReportsError() const
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // Simulate a completed file still sitting on disk.
        const Path basePath {tempDir.path()};
        const Path leftover {u"[Erai-raws] leftover - 18 [03D24DBA].mkv"_s};
        QFile file {(basePath / leftover).data()};
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("leftover-bytes");
        file.close();

        BitTorrent::TorrentContentRemover remover;
        QSignalSpy spy(&remover, &BitTorrent::TorrentContentRemover::jobFinished);
        remover.performJob(u"completed-torrent"_s, basePath
            , PathList {}, BitTorrent::TorrentContentRemoveOption::Delete);

        QCOMPARE(spy.count(), 1);
        // BUG (red): currently "" (success). Expect non-empty error so the GUI
        // logs "Failed to remove torrent content" instead of silently (or
        // falsely) succeeding while the file remains.
        QVERIFY2(!spy.at(0).at(1).toString().isEmpty()
            , qPrintable(u"Empty file list reported success; leftover file kept: %1"_s.arg((basePath / leftover).toString())));
        QVERIFY(QFile::exists((basePath / leftover).data()));
    }

    // Empty storage path must also report failure, never touch the filesystem.
    // This is what lets SessionImpl route unconditionally: validity is owned
    // by the remover, the layer that deletes files.
    void testEmptyBasePathReportsError() const
    {
        BitTorrent::TorrentContentRemover remover;
        QSignalSpy spy(&remover, &BitTorrent::TorrentContentRemover::jobFinished);
        remover.performJob(u"completed-torrent"_s, Path {}
            , PathList {Path {u"some episode - 04 (1080p) [D69FCE39].mkv"_s}}
            , BitTorrent::TorrentContentRemoveOption::Delete);

        QCOMPARE(spy.count(), 1);
        QVERIFY2(!spy.at(0).at(1).toString().isEmpty()
            , qPrintable(u"Empty base path reported success"_s));
    }
};

QTEST_APPLESS_MAIN(TestTorrentContentRemover)
#include "testtorrentcontentremover.moc"
