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
#include "base/utils/version.h"

class TestUtilsVersion final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsVersion)

public:
    TestUtilsVersion() = default;

private slots:
    void testConstructors() const
    {
        // should not compile:
        // Utils::Version<-1>();
        // Utils::Version<0>();
        // Utils::Version<2, 3>();
        // Utils::Version<2, -1>();
        // Utils::Version<2, 0>();

        using TwoDigits = Utils::Version<2, 1>;
        QCOMPARE(TwoDigits(0), TwoDigits(u"0"_s));
        QCOMPARE(TwoDigits(50), TwoDigits(u"50"_s));
        QCOMPARE(TwoDigits(0, 1), TwoDigits(u"0.1"_s));

        using ThreeDigits = Utils::Version<3, 3>;
        // should not compile:
        // ThreeDigits(1);
        // ThreeDigits(1, 2);
        // ThreeDigits(1.0, 2, 3);
        // ThreeDigits(1, 2, 3, 4);
        QCOMPARE(ThreeDigits(1, 2, 3), ThreeDigits(u"1.2.3"_s));
    }

    void testIsValid() const
    {
        using ThreeDigits = Utils::Version<3>;
        QCOMPARE(ThreeDigits().isValid(), false);
        QCOMPARE(ThreeDigits(0, 0, 0).isValid(), false);
        QCOMPARE(ThreeDigits(0, 0, -1).isValid(), false);
        QCOMPARE(ThreeDigits(0, 0, 1).isValid(), true);
        QCOMPARE(ThreeDigits(0, 1, 0).isValid(), true);
        QCOMPARE(ThreeDigits(1, 0, 0).isValid(), true);
        QCOMPARE(ThreeDigits(10, 11, 12).isValid(), true);
    }

    void testVersionComponents() const
    {
        const Utils::Version<1> version1 {1};
        QCOMPARE(version1[0], 1);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version1[1]);
#endif
        QCOMPARE(version1.majorNumber(), 1);
        // should not compile:
        // version1.minorNumber();
        // version1.revisionNumber();
        // version1.patchNumber();

        const Utils::Version<2, 1> version2 {2};
        QCOMPARE(version2[0], 2);
        QCOMPARE(version2[1], 0);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version2[2]);
#endif
        QCOMPARE(version2.majorNumber(), 2);
        QCOMPARE(version2.minorNumber(), 0);
        // should not compile:
        // version2.revisionNumber();
        // version2.patchNumber();

        const Utils::Version<3, 2> version3 {3, 2};
        QCOMPARE(version3[0], 3);
        QCOMPARE(version3[1], 2);
        QCOMPARE(version3[2], 0);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version3[3]);
#endif
        QCOMPARE(version3.majorNumber(), 3);
        QCOMPARE(version3.minorNumber(), 2);
        QCOMPARE(version3.revisionNumber(), 0);
        // should not compile:
        // version3.patchNumber();

        const Utils::Version<4> version4 {10, 11, 12, 13};
        QCOMPARE(version4[0], 10);
        QCOMPARE(version4[1], 11);
        QCOMPARE(version4[2], 12);
        QCOMPARE(version4[3], 13);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version4[4]);
#endif
        QCOMPARE(version4.majorNumber(), 10);
        QCOMPARE(version4.minorNumber(), 11);
        QCOMPARE(version4.revisionNumber(), 12);
        QCOMPARE(version4.patchNumber(), 13);
    }

    void testToString() const
    {
        using OneMandatory = Utils::Version<2, 1>;
        QCOMPARE(OneMandatory(10).toString(), u"10"_s);
        QCOMPARE(OneMandatory(2).toString(), u"2"_s);
        QCOMPARE(OneMandatory(2, 0).toString(), u"2"_s);
        QCOMPARE(OneMandatory(2, 2).toString(), u"2.2"_s);

        using FourDigits = Utils::Version<4>;
        QCOMPARE(FourDigits(10, 11, 12, 13).toString(), u"10.11.12.13"_s);
    }

    void testFromString() const
    {
        using OneMandatory = Utils::Version<2, 1>;
        const OneMandatory default1 {10, 11};
        QCOMPARE(OneMandatory::fromString(u"1"_s, default1), OneMandatory(1));
        QCOMPARE(OneMandatory::fromString(u"1.2"_s, default1), OneMandatory(1, 2));
        QCOMPARE(OneMandatory::fromString(u"100.2000"_s, default1), OneMandatory(100, 2000));
        QCOMPARE(OneMandatory::fromString(u"1,2"_s), OneMandatory());
        QCOMPARE(OneMandatory::fromString(u"1,2"_s, default1), default1);
        QCOMPARE(OneMandatory::fromString(u"1.2a"_s), OneMandatory());
        QCOMPARE(OneMandatory::fromString(u"1.2.a"_s), OneMandatory());
        QCOMPARE(OneMandatory::fromString(u""_s), OneMandatory());
        QCOMPARE(OneMandatory::fromString(u""_s, default1), default1);
        QCOMPARE(OneMandatory::fromString(u"random_string"_s), OneMandatory());
        QCOMPARE(OneMandatory::fromString(u"random_string"_s, default1), default1);

        using FourDigits = Utils::Version<4, 3>;
        const FourDigits default2 {10, 11, 12, 13};
        QCOMPARE(FourDigits::fromString(u"1"_s, default2), default2);
        QCOMPARE(FourDigits::fromString(u"1.2"_s), FourDigits());
        QCOMPARE(FourDigits::fromString(u"1.2.3"_s), FourDigits(1, 2, 3));
        QCOMPARE(FourDigits::fromString(u"1.2.3.0"_s), FourDigits(1, 2, 3));
        QCOMPARE(FourDigits::fromString(u"1.2.3.4"_s), FourDigits(1, 2, 3, 4));
    }

    void testComparisons() const
    {
        using ThreeDigits = Utils::Version<3>;

        QVERIFY(ThreeDigits() == ThreeDigits());
        QVERIFY(!(ThreeDigits() != ThreeDigits()));
        QVERIFY(ThreeDigits() <= ThreeDigits());
        QVERIFY(ThreeDigits() >= ThreeDigits());

        QVERIFY(ThreeDigits() != ThreeDigits(1, 2, 3));
        QVERIFY(ThreeDigits() < ThreeDigits(1, 2, 3));
        QVERIFY(ThreeDigits() <= ThreeDigits(1, 2, 3));

        QVERIFY(ThreeDigits(1, 2, 3) != ThreeDigits());
        QVERIFY(ThreeDigits(1, 2, 3) > ThreeDigits());
        QVERIFY(ThreeDigits(1, 2, 3) >= ThreeDigits());

        QVERIFY(ThreeDigits(1, 3, 3) != ThreeDigits(2, 2, 3));
        QVERIFY(ThreeDigits(1, 3, 3) < ThreeDigits(2, 2, 3));
        QVERIFY(ThreeDigits(1, 3, 3) <= ThreeDigits(2, 2, 3));

        QVERIFY(ThreeDigits(1, 2, 3) != ThreeDigits(1, 2, 4));
        QVERIFY(ThreeDigits(1, 2, 3) < ThreeDigits(1, 2, 4));
        QVERIFY(ThreeDigits(1, 2, 3) <= ThreeDigits(1, 2, 4));
    }
};

QTEST_APPLESS_MAIN(TestUtilsVersion)
#include "testutilsversion.moc"
