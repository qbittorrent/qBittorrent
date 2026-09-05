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
#include <QList>
#include <QObject>
#include <QTest>

#include <utility>

#include "base/bittorrent/torrentcontenthandler.h"
#include "base/exceptions.h"
#include "base/path.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    Path invalidPath()
    {
        return Path(QString(QChar(127)) + u"invalid"_s);
    }

    class TorrentContentHandler final : public BitTorrent::TorrentContentHandler
    {
    public:
        using BitTorrent::TorrentContentHandler::renameFile;

        explicit TorrentContentHandler(PathList filePaths)
            : m_filePaths {std::move(filePaths)}
        {
        }

        bool hasMetadata() const override
        {
            return true;
        }

        int filesCount() const override
        {
            return m_filePaths.size();
        }

        Path filePath(const int index) const override
        {
            return m_filePaths[index];
        }

        qlonglong fileSize(int) const override
        {
            return 0;
        }

        Path actualStorageLocation() const override
        {
            return {};
        }

        Path actualFilePath(const int fileIndex) const override
        {
            return filePath(fileIndex);
        }

        QList<BitTorrent::DownloadPriority> filePriorities() const override
        {
            return {};
        }

        QList<qreal> filesProgress() const override
        {
            return {};
        }

        QFuture<QList<qreal>> fetchAvailableFileFractions() const override
        {
            return {};
        }

        void renameFile(const int index, const Path &newPath) override
        {
            m_renamedFileIndex = index;
            m_renamedFilePath = newPath;
            m_filePaths[index] = newPath;
        }

        void prioritizeFiles(const QList<BitTorrent::DownloadPriority> &) override
        {
        }

        void flushCache() const override
        {
        }

        int renamedFileIndex() const
        {
            return m_renamedFileIndex;
        }

        Path renamedFilePath() const
        {
            return m_renamedFilePath;
        }

        Path renamedFolderOldPath() const
        {
            return m_renamedFolderOldPath;
        }

        Path renamedFolderNewPath() const
        {
            return m_renamedFolderNewPath;
        }

    private:
        void doRenameFolder(const Path &oldFolderPath, const Path &newFolderPath) override
        {
            m_renamedFolderOldPath = oldFolderPath;
            m_renamedFolderNewPath = newFolderPath;
        }

        PathList m_filePaths;
        int m_renamedFileIndex = -1;
        Path m_renamedFilePath;
        Path m_renamedFolderOldPath;
        Path m_renamedFolderNewPath;
    };
}

class TestBittorrentTorrentContentHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentTorrentContentHandler)

public:
    TestBittorrentTorrentContentHandler() = default;

private slots:
    void testRenameFileWithInvalidOldPath() const
    {
        const Path oldPath = invalidPath();
        QVERIFY(!oldPath.isValid());

        TorrentContentHandler handler {{oldPath}};

        const Path newPath {u"valid"_s};
        handler.renameFile(oldPath, newPath);

        QCOMPARE(handler.renamedFileIndex(), 0);
        QCOMPARE(handler.renamedFilePath(), newPath);
    }

    void testRenameFileWithInvalidNewPath() const
    {
        const Path oldPath {u"valid"_s};
        const Path newPath = invalidPath();
        QVERIFY(!newPath.isValid());

        TorrentContentHandler handler {{oldPath}};

        bool thrown = false;
        try
        {
            handler.renameFile(oldPath, newPath);
        }
        catch (const RuntimeError &)
        {
            thrown = true;
        }
        QVERIFY(thrown);
        QCOMPARE(handler.renamedFileIndex(), -1);
    }

    void testRenameFileWithEmptyOldPath() const
    {
        TorrentContentHandler handler {{Path(u"valid"_s)}};

        bool thrown = false;
        try
        {
            handler.renameFile(Path(), Path(u"new"_s));
        }
        catch (const RuntimeError &)
        {
            thrown = true;
        }
        QVERIFY(thrown);
        QCOMPARE(handler.renamedFileIndex(), -1);
    }

    void testRenameFolderWithInvalidOldPath() const
    {
        const Path oldFolderPath = invalidPath();
        QVERIFY(!oldFolderPath.isValid());

        TorrentContentHandler handler {{oldFolderPath / Path(u"file"_s)}};

        const Path newFolderPath {u"valid"_s};
        handler.renameFolder(oldFolderPath, newFolderPath);

        QCOMPARE(handler.renamedFolderOldPath(), oldFolderPath);
        QCOMPARE(handler.renamedFolderNewPath(), newFolderPath);
    }
};

QTEST_APPLESS_MAIN(TestBittorrentTorrentContentHandler)
#include "testbittorrenttorrentcontenthandler.moc"
