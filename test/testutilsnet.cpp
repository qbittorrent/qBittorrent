/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Mike Tzou (Chocobo1)
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

#include "base/utils/net.h"

using namespace Qt::Literals::StringLiterals;

class TestUtilsNet final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsNet)

public:
    TestUtilsNet() = default;

private slots:
    void testQHostAddressLessThan() const
    {
        const auto ip1 = QHostAddress(u"127.0.0.1"_s);
        const auto ip2 = QHostAddress(u"127.0.0.2"_s);
        const auto ip3 = QHostAddress(u"2001::1"_s);
        const auto ip4 = QHostAddress(u"2001::2"_s);

        QCOMPARE(Utils::Net::lessThan({}, {}), false);
        QCOMPARE(Utils::Net::lessThan({}, ip1), true);
        QCOMPARE(Utils::Net::lessThan(ip1, {}), false);

        QCOMPARE(Utils::Net::lessThan(ip1, ip1), false);
        QCOMPARE(Utils::Net::lessThan(ip3, ip3), false);

        QCOMPARE(Utils::Net::lessThan(ip1, ip2), true);
        QCOMPARE(Utils::Net::lessThan(ip2, ip1), false);

        QCOMPARE(Utils::Net::lessThan(ip2, ip3), true);
        QCOMPARE(Utils::Net::lessThan(ip3, ip2), false);

        QCOMPARE(Utils::Net::lessThan(ip3, ip4), true);
        QCOMPARE(Utils::Net::lessThan(ip4, ip3), false);
    }
};

QTEST_APPLESS_MAIN(TestUtilsNet)
#include "testutilsnet.moc"
