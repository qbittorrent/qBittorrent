/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Mark Yu (vafada)
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

#include <QLocale>
#include <QObject>
#include <QTest>

#include "base/global.h"
#include "base/utils/misc.h"

class TestUtilsMisc final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsMisc)

public:
    TestUtilsMisc() = default;

private slots:
    void testfriendlyUnitCompact() const
    {
        // numbers are formatted with the system locale
        const QString decimalPoint = QLocale::system().decimalPoint();
        const auto toNative = [&decimalPoint](QString string) -> QString
        {
            return string.replace(u' ', QChar::Nbsp).replace(u'.', decimalPoint);
        };

        QCOMPARE(Utils::Misc::friendlyUnitCompact(500), toNative(u"500 B"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000), toNative(u"0.97 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(5000), toNative(u"4.88 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000), toNative(u"9.76 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000), toNative(u"14.6 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(100000), toNative(u"97.6 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(150000), toNative(u"146 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000000), toNative(u"976 K"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000), toNative(u"9.53 M"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000000), toNative(u"14.3 M"_s));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000000), toNative(u"9.31 G"_s));
    }
};

QTEST_APPLESS_MAIN(TestUtilsMisc)
#include "testutilsmisc.moc"
