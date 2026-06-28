/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye <35905412+TurboTheTurtle@users.noreply.github.com>
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

#include <QHostAddress>
#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/utils/net.h"

class TestUtilsNet final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsNet)

public:
    TestUtilsNet() = default;

private slots:
    void testIsIPAddressLessThan() const
    {
        QVERIFY(Utils::Net::isIPAddressLessThan(QHostAddress(u"127.0.0.1"_s), QHostAddress(u"127.0.0.2"_s)));
        QVERIFY(!Utils::Net::isIPAddressLessThan(QHostAddress(u"127.0.0.1"_s), QHostAddress(u"127.0.0.1"_s)));
        QVERIFY(!Utils::Net::isIPAddressLessThan(QHostAddress(u"127.0.0.2"_s), QHostAddress(u"127.0.0.1"_s)));

        QVERIFY(Utils::Net::isIPAddressLessThan(QHostAddress(u"2001:db8::f"_s), QHostAddress(u"2001:db8::10"_s)));
        QVERIFY(!Utils::Net::isIPAddressLessThan(QHostAddress(u"2001:db8::10"_s), QHostAddress(u"2001:db8::f"_s)));

        QVERIFY(Utils::Net::isIPAddressLessThan(QHostAddress(u"255.255.255.255"_s), QHostAddress(u"::1"_s)));
        QVERIFY(!Utils::Net::isIPAddressLessThan(QHostAddress(u"::1"_s), QHostAddress(u"255.255.255.255"_s)));
    }
};

QTEST_APPLESS_MAIN(TestUtilsNet)
#include "testutilsnet.moc"
