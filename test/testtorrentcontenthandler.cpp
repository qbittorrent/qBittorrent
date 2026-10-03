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

#include <QFuture>
#include <QTest>

#include "base/bittorrent/torrentcontenthandler.h"
#include "base/exceptions.h"
#include "base/path.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    class StubContentHandler final : public BitTorrent::TorrentContentHandler
    {
    public:
        using TorrentContentHandler::renameFile;

        PathList filePaths;

        bool hasMetadata() const override { return true; }
        int filesCount() const override { return filePaths.size(); }
        Path filePath(const int index) const override { return filePaths.at(index); }
        qlonglong fileSize([[maybe_unused]] const int index) const override { return 0; }
        Path actualStorageLocation() const override { return {}; }
        Path actualFilePath([[maybe_unused]] const int fileIndex) const override { return {}; }
        QList<BitTorrent::DownloadPriority> filePriorities() const override { return {}; }
        QList<qreal> filesProgress() const override { return {}; }
        QFuture<QList<qreal>> fetchAvailableFileFractions() const override { return {}; }

        void renameFile(const int index, const Path &newPath) override
        {
            filePaths[index] = newPath;
        }

        void prioritizeFiles([[maybe_unused]] const QList<BitTorrent::DownloadPriority> &priorities) override {}
        void flushCache() const override {}

    protected:
        void doRenameFolder(const Path &oldFolderPath, const Path &newFolderPath) override
        {
            for (qsizetype i = 0; i < filePaths.size(); ++i)
            {
                const Path &path = filePaths.at(i);
                if (path.hasAncestor(oldFolderPath))
                    filePaths[i] = (newFolderPath / oldFolderPath.relativePathOf(path));
            }
        }
    };
}

class TestTorrentContentHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTorrentContentHandler)

public:
    TestTorrentContentHandler() = default;

private slots:
    // A path that is stored in the torrent may contain characters that are
    // reserved on the host filesystem (e.g. ':' on macOS). It must still be
    // possible to rename such files and folders to a valid name.
    void testRenameFileFromInvalidOldPath() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"picture: sample.txt"_s), Path(u"other.txt"_s)};

        handler.renameFile(Path(u"picture: sample.txt"_s), Path(u"picture - sample.txt"_s));

        QCOMPARE(handler.filePaths.at(0), Path(u"picture - sample.txt"_s));
        QCOMPARE(handler.filePaths.at(1), Path(u"other.txt"_s));
    }

    void testRenameFolderFromInvalidOldPath() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"my: folder/a.txt"_s), Path(u"my: folder/sub/b.txt"_s), Path(u"other/c.txt"_s)};

        handler.renameFolder(Path(u"my: folder"_s), Path(u"my folder"_s));

        QCOMPARE(handler.filePaths.at(0), Path(u"my folder/a.txt"_s));
        QCOMPARE(handler.filePaths.at(1), Path(u"my folder/sub/b.txt"_s));
        QCOMPARE(handler.filePaths.at(2), Path(u"other/c.txt"_s));
    }

    void testRenameFileToInvalidPath() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"a.txt"_s)};

        // a control character is reserved on every platform
        QVERIFY_THROWS_EXCEPTION(RuntimeError
            , handler.renameFile(Path(u"a.txt"_s), Path(u"a\vb.txt"_s)));
    }

    void testRenameFileToExistingPath() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"a.txt"_s), Path(u"b.txt"_s)};

        QVERIFY_THROWS_EXCEPTION(RuntimeError
            , handler.renameFile(Path(u"a.txt"_s), Path(u"b.txt"_s)));
    }

    void testRenameNonexistentFile() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"a.txt"_s)};

        QVERIFY_THROWS_EXCEPTION(RuntimeError
            , handler.renameFile(Path(u"missing.txt"_s), Path(u"b.txt"_s)));
    }

    void testRenameNonexistentFolder() const
    {
        StubContentHandler handler;
        handler.filePaths = {Path(u"a/f.txt"_s)};

        QVERIFY_THROWS_EXCEPTION(RuntimeError
            , handler.renameFolder(Path(u"missing"_s), Path(u"b"_s)));
    }
};

QTEST_APPLESS_MAIN(TestTorrentContentHandler)
#include "testtorrentcontenthandler.moc"
