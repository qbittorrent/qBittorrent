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

#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/http/utils.h"

class TestHttpUtils final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestHttpUtils)

public:
    TestHttpUtils() = default;

private slots:
    void testMakeContentDispositionHeader() const
    {
        QCOMPARE(Http::makeContentDispositionHeader(u"file.torrent"_s), u"attachment; filename=\"file.torrent\""_s);
        QCOMPARE(Http::makeContentDispositionHeader(u"a\"b\\c.torrent"_s), u"attachment; filename=\"a\\\"b\\\\c.torrent\""_s);

        const QString header = Http::makeContentDispositionHeader(u"evil\r\nset-cookie: value.torrent"_s);
        QVERIFY(!header.contains(u'\r'));
        QVERIFY(!header.contains(u'\n'));
        QCOMPARE(header, u"attachment; filename=\"evilset-cookie: value.torrent\""_s);

        QCOMPARE(Http::makeContentDispositionHeader(u"тест.torrent"_s)
            , u"attachment; filename=\"____.torrent\"; filename*=UTF-8''%D1%82%D0%B5%D1%81%D1%82.torrent"_s);
    }
};

QTEST_APPLESS_MAIN(TestHttpUtils)
#include "testhttputils.moc"
