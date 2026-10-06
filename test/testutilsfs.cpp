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

#include <QtSystemDetection>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include <QDir>
#include <QFile>
#include <QObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

#include "base/path.h"
#include "base/utils/fs.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    class TestUtilsFs final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TestUtilsFs)

    public:
        TestUtilsFs() = default;

    private slots:
        void testExistingPaths() const
        {
            const QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(tempDir.path())) >= 0);

            QFile file {tempDir.filePath(u"file"_s)};
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(file.fileName())) >= 0);

            const QString unicodeDir = tempDir.filePath(u"é-目录"_s);
            QVERIFY(QDir().mkdir(unicodeDir));
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(unicodeDir)) >= 0);
        }

        void testMissingPaths() const
        {
            const QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());

            const Path missingPath {tempDir.filePath(u"missing/child"_s)};
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(missingPath) >= 0);
            QVERIFY(!QDir(tempDir.path()).exists(u"missing"_s));

            const QString originalDir = QDir::currentPath();
            [[maybe_unused]] const auto restoreCurrentDir = qScopeGuard([&originalDir] { QDir::setCurrent(originalDir); });
            QVERIFY(QDir::setCurrent(tempDir.path()));
            QVERIFY(QDir().mkdir(u"existing"_s));

            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(u"existing"_s)) >= 0);
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(u"missing"_s)) >= 0);
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(u"missing/child"_s)) >= 0);
            QVERIFY(!QDir().exists(u"missing"_s));
        }

        void testInvalidPaths() const
        {
            QCOMPARE(Utils::Fs::freeDiskSpaceOnPath({}), -1);

            const QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            QFile file {tempDir.filePath(u"file"_s)};
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();

            QCOMPARE(Utils::Fs::freeDiskSpaceOnPath(Path(file.fileName() + u"/child")), -1);
        }

        void testSymlinks() const
        {
    #ifdef Q_OS_UNIX
            const QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString targetDir = tempDir.filePath(u"target"_s);
            const QString linkPath = tempDir.filePath(u"link"_s);
            QVERIFY(QDir().mkdir(targetDir));
            QVERIFY(QFile::link(targetDir, linkPath));

            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(linkPath)) >= 0);
            QVERIFY(Utils::Fs::freeDiskSpaceOnPath(Path(linkPath + u"/missing/child")) >= 0);
            QVERIFY(!QDir(targetDir).exists(u"missing"_s));

            QVERIFY(QDir().rmdir(targetDir));
            QCOMPARE(Utils::Fs::freeDiskSpaceOnPath(Path(linkPath)), -1);
            QCOMPARE(Utils::Fs::freeDiskSpaceOnPath(Path(linkPath + u"/child")), -1);
    #else
            QSKIP("QFile::link creates shortcuts on Windows rather than symbolic links.");
    #endif
        }

        void testInaccessiblePath() const
        {
    #ifdef Q_OS_UNIX
            if (::geteuid() == 0)
                QSKIP("Root can access directories without search permission.");

            const QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString restrictedDir = tempDir.filePath(u"restricted"_s);
            QVERIFY(QDir().mkdir(restrictedDir));
            const QFile::Permissions originalPermissions = QFile::permissions(restrictedDir);
            [[maybe_unused]] const auto restorePermissions = qScopeGuard([&restrictedDir, originalPermissions]
            {
                QFile::setPermissions(restrictedDir, originalPermissions);
            });
            QVERIFY(QFile::setPermissions(restrictedDir, {}));

            QCOMPARE(Utils::Fs::freeDiskSpaceOnPath(Path(restrictedDir + u"/child")), -1);
    #else
            QSKIP("Directory search permissions are specific to POSIX file systems.");
    #endif
        }
    };
}

QTEST_APPLESS_MAIN(TestUtilsFs)
#include "testutilsfs.moc"
