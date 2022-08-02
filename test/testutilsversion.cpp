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
        using TwoDigits = Utils::Version<unsigned char, 2, 1>;
        TwoDigits();
        TwoDigits(0);
        TwoDigits(0, 1);

        using ThreeDigits = Utils::Version<int, 3>;
        // should not compile:
        // ThreeDigits(1);
        // ThreeDigits(1, 2);
        // ThreeDigits(1, 2, 3, 4);
        QCOMPARE(ThreeDigits(u"1.2.3"_qs), ThreeDigits(1, 2, 3));
        QCOMPARE(ThreeDigits(QByteArrayLiteral("1.2.3")), ThreeDigits(1, 2, 3));

#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(u""_qs));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(u"1"_qs));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(u"1.0"_qs));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(u"1.0.1.1"_qs));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(u"random_string"_qs));

        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(QByteArrayLiteral("1")));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(QByteArrayLiteral("1.0")));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(QByteArrayLiteral("1.0.1.1")));
        QVERIFY_THROWS_EXCEPTION(RuntimeError, ThreeDigits(QByteArrayLiteral("random_string")));
#endif
    }

    void testVersionComponents() const
    {
        const Utils::Version<int, 1> version1 {1};
        QCOMPARE(version1[0], 1);
        QCOMPARE(version1.majorNumber(), 1);
        // should not compile:
        // version1.minorNumber();
        // version1.revisionNumber();
        // version1.patchNumber();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version1[1]);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range, version1[2]);
#endif

        const Utils::Version<int, 4> version2 {10, 11, 12, 13};
        QCOMPARE(version2[0], 10);
        QCOMPARE(version2[1], 11);
        QCOMPARE(version2[2], 12);
        QCOMPARE(version2[3], 13);
        QCOMPARE(version2.majorNumber(), 10);
        QCOMPARE(version2.minorNumber(), 11);
        QCOMPARE(version2.revisionNumber(), 12);
        QCOMPARE(version2.patchNumber(), 13);
    }

    void testToString() const
    {
        using OneMandatory = Utils::Version<int, 2, 1>;
        QCOMPARE(OneMandatory(u"10"_qs).toString(), u"10"_qs);

        using FourDigits = Utils::Version<int, 4>;
        QCOMPARE(FourDigits().toString(), u"0.0.0.0"_qs);
        QCOMPARE(FourDigits(10, 11, 12, 13).toString(), u"10.11.12.13"_qs);
    }

    void testIsValid() const
    {
        using ThreeDigits = Utils::Version<int, 3>;
        QCOMPARE(ThreeDigits().isValid(), false);
        QCOMPARE(ThreeDigits(10, 11, 12).isValid(), true);
    }

    void testTryParse() const
    {
        using OneMandatory = Utils::Version<int, 2, 1>;
        const OneMandatory default1 {10, 11};
        QCOMPARE(OneMandatory::tryParse(u"1"_qs, default1), OneMandatory(1));
        QCOMPARE(OneMandatory::tryParse(u"1.2"_qs, default1), OneMandatory(1, 2));
        QCOMPARE(OneMandatory::tryParse(u"1,2"_qs, default1), default1);

        QCOMPARE(OneMandatory::tryParse(u""_qs, default1), default1);
        QCOMPARE(OneMandatory::tryParse(u"random_string"_qs, default1), default1);

        using FourDigits = Utils::Version<int, 4>;
        const FourDigits default4 {10, 11, 12, 13};
        QCOMPARE(FourDigits::tryParse(u"1"_qs, default4), default4);
        QCOMPARE(FourDigits::tryParse(u"1.2.3.4"_qs, default4), FourDigits(1, 2, 3, 4));
        QCOMPARE(FourDigits::tryParse(u"1,2.3.4"_qs, default4), default4);
    }

    void testComparisons() const
    {
        using ThreeDigits = Utils::Version<int, 3>;

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
