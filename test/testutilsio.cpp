/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Mike Tzou (Chocobo1)
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
#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/io.h"

class TestUtilsIO final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsIO)

public:
    TestUtilsIO() = default;

private slots:
    void testReadFile() const
    {
        const Path testFolder = Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata"_s);

        const Path size10File = testFolder / Path(u"size10.txt"_s);
        const QByteArray size10Data = QByteArrayLiteral("123456789\n");

        {
            const auto readResult = Utils::IO::readFile(size10File, 0);
            QCOMPARE(readResult.has_value(), false);
            QCOMPARE(readResult.error().status, Utils::IO::ReadError::ExceedSize);
            QCOMPARE(readResult.error().message.isEmpty(), false);
        }
        {
            const auto readResult = Utils::IO::readFile(size10File, 9);
            QCOMPARE(readResult.has_value(), false);
            QCOMPARE(readResult.error().status, Utils::IO::ReadError::ExceedSize);
            QCOMPARE(readResult.error().message.isEmpty(), false);
        }
        {
            const auto readResult = Utils::IO::readFile(size10File, 10);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value(), size10Data);
        }
        {
            const auto readResult = Utils::IO::readFile(size10File, 11);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value(), size10Data);
        }
        {
            const auto readResult = Utils::IO::readFile(size10File, -1);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value(), size10Data);
        }

        {
            const Path crlfFile = testFolder / Path(u"crlf.txt"_s);
            const auto readResult = Utils::IO::readFile(crlfFile, -1, QIODevice::Text);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value(), "\n\n");
        }

        {
            const Path nonExistFile = testFolder / Path(u".non_existent_file_1234"_s);
            const auto readResult = Utils::IO::readFile(nonExistFile, 1);
            QCOMPARE(readResult.has_value(), false);
            QCOMPARE(readResult.error().status, Utils::IO::ReadError::NotExist);
            QCOMPARE(readResult.error().message.isEmpty(), false);
        }

#ifdef Q_OS_UNIX
        {
            const auto readResult = Utils::IO::readFile(Path(u"/dev/null"_s), 10);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value().length(), 0);
        }
        {
            const auto readResult = Utils::IO::readFile(Path(u"/dev/null"_s), -1);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value().length(), 0);
        }

        {
            const auto readResult = Utils::IO::readFile(Path(u"/dev/zero"_s), 10);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value().length(), 0);
        }
        {
            const auto readResult = Utils::IO::readFile(Path(u"/dev/zero"_s), -1);
            QCOMPARE(readResult.has_value(), true);
            QCOMPARE(readResult.value().length(), 0);
        }
#endif
    }
};

QTEST_APPLESS_MAIN(TestUtilsIO)
#include "testutilsio.moc"
