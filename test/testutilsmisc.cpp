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
        // numbers are formatted with the system locale, so the expected values must be too
        const auto expected = [](const double number, const int precision, const QChar unit) -> QString
        {
            return QLocale::system().toString(number, 'f', precision) + QChar::Nbsp + unit;
        };

        QCOMPARE(Utils::Misc::friendlyUnitCompact(500), expected(500, 0, u'B'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000), expected(0.97, 2, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(5000), expected(4.88, 2, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000), expected(9.76, 2, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000), expected(14.6, 1, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(100000), expected(97.6, 1, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(150000), expected(146, 0, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(1000000), expected(976, 0, u'K'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000), expected(9.53, 2, u'M'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(15000000), expected(14.3, 1, u'M'));
        QCOMPARE(Utils::Misc::friendlyUnitCompact(10000000000), expected(9.31, 2, u'G'));
    }
};

QTEST_APPLESS_MAIN(TestUtilsMisc)
#include "testutilsmisc.moc"
