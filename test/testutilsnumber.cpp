/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Mike Tzou (Chocobo1)
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

#include <limits>

#include <QTest>

#include "base/utils/number.h"

class TestUtilsNumber final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsNumber)

public:
    TestUtilsNumber() = default;

private slots:
    void testClampingAdd() const
    {
        const int intMin = std::numeric_limits<int>::min();
        const int intMax = std::numeric_limits<int>::max();

        QCOMPARE(Utils::Number::clampingAdd(1, 2), 3);
        QCOMPARE(Utils::Number::clampingAdd(-1, -2), -3);

        QCOMPARE(Utils::Number::clampingAdd((intMax - 1), 1), intMax);
        QCOMPARE(Utils::Number::clampingAdd(intMax, 1), intMax);
        QCOMPARE(Utils::Number::clampingAdd(intMax, intMax), intMax);

        QCOMPARE(Utils::Number::clampingAdd((intMin + 1), -1), intMin);
        QCOMPARE(Utils::Number::clampingAdd(intMin, -1), intMin);
        QCOMPARE(Utils::Number::clampingAdd(intMin, intMin), intMin);
    }

    void testRoundToPrecision() const
    {
        QCOMPARE(Utils::Number::roundToPrecision(1.234, 2), 1.23);
        QCOMPARE(Utils::Number::roundToPrecision(1.235, 2), 1.24);
        QCOMPARE(Utils::Number::roundToPrecision(-1.235, 2), -1.24);
        QCOMPARE(Utils::Number::roundToPrecision(1.5, 0), 2.0);
        QCOMPARE(Utils::Number::roundToPrecision(0.0, 2), 0.0);

        // values already having the requested precision are left alone
        QCOMPARE(Utils::Number::roundToPrecision(4.0, 2), 4.0);
        QCOMPARE(Utils::Number::roundToPrecision(2.3, 2), 2.3);

        // the error accumulated by repeatedly stepping a spin box is undone
        QCOMPARE(Utils::Number::roundToPrecision(3.9999999999999938, 2), 4.0);
        QCOMPARE(Utils::Number::roundToPrecision(2.9999999999999973, 2), 3.0);
    }
};

QTEST_APPLESS_MAIN(TestUtilsNumber)
#include "testutilsnumber.moc"
