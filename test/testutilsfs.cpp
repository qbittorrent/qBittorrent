/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye
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

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"

class TestUtilsFs final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsFs)

public:
    TestUtilsFs() = default;

private slots:
    void testToValidFileName() const
    {
        QCOMPARE(Utils::Fs::toValidFileName(u"///"_s), u"_"_s);
        QCOMPARE(Utils::Fs::toValidFileName(u"///"_s, {}), QString());
        QCOMPARE(Utils::Fs::toValidFileName(u"."_s, {}), QString());
        QCOMPARE(Utils::Fs::toValidFileName(u" a/b "_s, {}), u"ab"_s);
    }

    void testDeallocateFileRange() const
    {
        const QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const Path filePath = Path(tmpDir.path()) / Path(u"sparse.bin"_s);

        const qint64 chunkSize = 1024 * 1024;
        const QByteArray chunk(chunkSize, 'x');
        {
            QFile file {filePath.data()};
            QVERIFY(file.open(QIODevice::WriteOnly));
            for (int i = 0; i < 4; ++i)
                QCOMPARE(file.write(chunk), chunkSize);
        }

        // deallocate the 2nd and the 3rd chunk
        const auto result = Utils::Fs::deallocateFileRange(filePath, chunkSize, (2 * chunkSize));
        if (!result)
            QSKIP(qUtf8Printable(u"Freeing file range isn't supported here: "_s + result.error()));

        QFile file {filePath.data()};
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.size(), (4 * chunkSize));  // logical size is kept
        QCOMPARE(file.read(chunkSize), chunk);
        QCOMPARE(file.read(2 * chunkSize), QByteArray((2 * chunkSize), '\0'));
        QCOMPARE(file.read(chunkSize), chunk);

        // nothing to do for empty ranges
        QVERIFY(Utils::Fs::deallocateFileRange(filePath, 0, 0));
    }

    void testTruncateFile() const
    {
        const QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const Path filePath = Path(tmpDir.path()) / Path(u"file.bin"_s);
        {
            QFile file {filePath.data()};
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(QByteArray(1000, 'x')), 1000);
        }

        QVERIFY(Utils::Fs::truncateFile(filePath, 100));
        QFile file {filePath.data()};
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray(100, 'x'));

        QVERIFY(!Utils::Fs::truncateFile((Path(tmpDir.path()) / Path(u"missing/file.bin"_s)), 10));
    }
};

QTEST_APPLESS_MAIN(TestUtilsFs)
#include "testutilsfs.moc"
