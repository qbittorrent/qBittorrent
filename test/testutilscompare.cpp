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

#include <tuple>

#include <QTest>

#include "base/global.h"
#include "base/utils/compare.h"

#ifndef QBT_USE_QCOLLATOR  // only test qbt own implementation, not QCollator
namespace
{
    enum class CompareResult
    {
        Equal,
        Greater,
        Less
    };

    struct TestData
    {
        QString lhs;
        QString rhs;
        CompareResult caseInsensitiveResult = CompareResult::Equal;
        CompareResult caseSensitiveResult = CompareResult::Equal;
    };

    const TestData testData[] =
    {
        {u""_qs, u""_qs, CompareResult::Equal, CompareResult::Equal},
        {u""_qs, u"a"_qs, CompareResult::Less, CompareResult::Less},
        {u"a"_qs, u""_qs, CompareResult::Greater, CompareResult::Greater},

        {u"a"_qs, u"a"_qs, CompareResult::Equal, CompareResult::Equal},
        {u"A"_qs, u"a"_qs, CompareResult::Equal, CompareResult::Less},  // ascii code of 'A' is smaller than 'a'
        {u"a"_qs, u"A"_qs, CompareResult::Equal, CompareResult::Greater},

        {u"0"_qs, u"0"_qs, CompareResult::Equal, CompareResult::Equal},
        {u"1"_qs, u"0"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"0"_qs, u"1"_qs, CompareResult::Less, CompareResult::Less},

        {u"😀"_qs, u"😀"_qs, CompareResult::Equal, CompareResult::Equal},
        {u"😀"_qs, u"😁"_qs, CompareResult::Less, CompareResult::Less},
        {u"😁"_qs, u"😀"_qs, CompareResult::Greater, CompareResult::Greater},

        {u"a1"_qs, u"a1"_qs, CompareResult::Equal, CompareResult::Equal},
        {u"A1"_qs, u"a1"_qs, CompareResult::Equal, CompareResult::Less},
        {u"a1"_qs, u"A1"_qs, CompareResult::Equal, CompareResult::Greater},

        {u"a1"_qs, u"a2"_qs, CompareResult::Less, CompareResult::Less},
        {u"A1"_qs, u"a2"_qs, CompareResult::Less, CompareResult::Less},
        {u"a1"_qs, u"A2"_qs, CompareResult::Less, CompareResult::Greater},
        {u"A1"_qs, u"A2"_qs, CompareResult::Less, CompareResult::Less},

        {u"abc100"_qs, u"abc99"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"ABC100"_qs, u"abc99"_qs, CompareResult::Greater, CompareResult::Less},
        {u"abc100"_qs, u"ABC99"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"ABC100"_qs, u"ABC99"_qs, CompareResult::Greater, CompareResult::Greater},

        {u"100abc"_qs, u"99abc"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"100ABC"_qs, u"99abc"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"100abc"_qs, u"99ABC"_qs, CompareResult::Greater, CompareResult::Greater},
        {u"100ABC"_qs, u"99ABC"_qs, CompareResult::Greater, CompareResult::Greater},

        {u"😀😀😀99"_qs, u"😀😀😀100"_qs, CompareResult::Less, CompareResult::Less},
        {u"😀😀😀100"_qs, u"😀😀😀99"_qs, CompareResult::Greater, CompareResult::Greater}
    };

    void testCompare(const TestData &data, const int actual, const CompareResult expected)
    {
        const auto errorMessage = u"Wrong result. LHS: \"%1\". RHS: \"%2\". Result: %3"_qs
            .arg(data.lhs, data.rhs, QString::number(actual));

        switch (expected)
        {
        case CompareResult::Equal:
            QVERIFY2((actual == 0), qPrintable(errorMessage));
            break;
        case CompareResult::Greater:
            QVERIFY2((actual > 0), qPrintable(errorMessage));
            break;
        case CompareResult::Less:
            QVERIFY2((actual < 0), qPrintable(errorMessage));
            break;
        default:
            QFAIL("Unhandled case");
            break;
        }
    }

    void testLessThan(const TestData &data, const bool actual, const CompareResult expected)
    {
        const auto errorMessage = u"Wrong result. LHS: \"%1\". RHS: \"%2\". Result: %3"_qs
            .arg(data.lhs, data.rhs, QString::number(actual));

        switch (expected)
        {
        case CompareResult::Equal:
        case CompareResult::Greater:
            QVERIFY2(!actual, qPrintable(errorMessage));
            break;
        case CompareResult::Less:
            QVERIFY2(actual, qPrintable(errorMessage));
            break;
        default:
            QFAIL("Unhandled case");
            break;
        }
    }
}
#endif

class TestUtilsCompare final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsCompare)

public:
    TestUtilsCompare() = default;

#ifndef QBT_USE_QCOLLATOR  // only test qbt own implementation, not QCollator
private slots:
    void testNaturalCompareCaseInsensitive() const
    {
        const Utils::Compare::NaturalCompare<Qt::CaseInsensitive> cmp;

        for (const TestData &data : testData)
            testCompare(data, cmp(data.lhs, data.rhs), data.caseInsensitiveResult);
    }

    void testNaturalCompareCaseSensitive() const
    {
        const Utils::Compare::NaturalCompare<Qt::CaseSensitive> cmp;

        for (const TestData &data : testData)
            testCompare(data, cmp(data.lhs, data.rhs), data.caseSensitiveResult);
    }

    void testNaturalLessThanCaseInsensitive() const
    {
        const Utils::Compare::NaturalLessThan<Qt::CaseInsensitive> cmp {};

        for (const TestData &data : testData)
            testLessThan(data, cmp(data.lhs, data.rhs), data.caseInsensitiveResult);
    }

    void testNaturalLessThanCaseSensitive() const
    {
        const Utils::Compare::NaturalLessThan<Qt::CaseSensitive> cmp {};

        for (const TestData &data : testData)
            testLessThan(data, cmp(data.lhs, data.rhs), data.caseSensitiveResult);
    }
#endif
};

QTEST_APPLESS_MAIN(TestUtilsCompare)
#include "testutilscompare.moc"
