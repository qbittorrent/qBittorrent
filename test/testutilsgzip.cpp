/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
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

#include "base/global.h"
#include "base/utils/gzip.h"

class TestUtilsGzip final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsGzip)

public:
    TestUtilsGzip() = default;

private slots:
    void testCompress() const
    {
        // compressed data is not reproducible, see: https://stackoverflow.com/questions/30150972/are-there-test-vectors-available-for-gzip
        const QByteArray data = QByteArrayLiteral("abc");

        bool ok = false;
        const QByteArray compressedData = Utils::Gzip::compress(data, 6, &ok);
        QVERIFY(ok);
    }

    void testDecompress() const
    {
        const QByteArray data = QByteArrayLiteral("abc");

        bool ok = false;
        const QByteArray compressedData = Utils::Gzip::compress(data, 6, &ok);
        QVERIFY(ok);
        QVERIFY(compressedData != data);

        ok = false;
        const QByteArray decompressedData = Utils::Gzip::decompress(compressedData, &ok);
        QVERIFY(ok);
        QCOMPARE(decompressedData, data);
    }
};

QTEST_APPLESS_MAIN(TestUtilsGzip)
#include "testutilsgzip.moc"
