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

#include <QLocale>
#include <QObject>
#include <QTest>

#include "base/global.h"

// only test qbt own implementation, not QCollator
#define QBT_USE_QCOLLATOR 0
#include "base/utils/compare.h"

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
        {u""_s, u""_s, CompareResult::Equal, CompareResult::Equal},
        {u""_s, u"a"_s, CompareResult::Less, CompareResult::Less},
        {u"a"_s, u""_s, CompareResult::Greater, CompareResult::Greater},

        {u"a"_s, u"a"_s, CompareResult::Equal, CompareResult::Equal},
        {u"A"_s, u"a"_s, CompareResult::Equal, CompareResult::Greater},
        {u"a"_s, u"A"_s, CompareResult::Equal, CompareResult::Less},

        {u"0"_s, u"0"_s, CompareResult::Equal, CompareResult::Equal},
        {u"1"_s, u"0"_s, CompareResult::Greater, CompareResult::Greater},
        {u"0"_s, u"1"_s, CompareResult::Less, CompareResult::Less},

        {u"😀"_s, u"😀"_s, CompareResult::Equal, CompareResult::Equal},
        {u"😀"_s, u"😁"_s, CompareResult::Less, CompareResult::Less},
        {u"😁"_s, u"😀"_s, CompareResult::Greater, CompareResult::Greater},

        {u"a1"_s, u"a1"_s, CompareResult::Equal, CompareResult::Equal},
        {u"A1"_s, u"a1"_s, CompareResult::Equal, CompareResult::Greater},
        {u"a1"_s, u"A1"_s, CompareResult::Equal, CompareResult::Less},

        {u"a1"_s, u"a2"_s, CompareResult::Less, CompareResult::Less},
        {u"A1"_s, u"a2"_s, CompareResult::Less, CompareResult::Greater},
        {u"a1"_s, u"A2"_s, CompareResult::Less, CompareResult::Less},
        {u"A1"_s, u"A2"_s, CompareResult::Less, CompareResult::Less},

        {u"abc100"_s, u"abc99"_s, CompareResult::Greater, CompareResult::Greater},
        {u"ABC100"_s, u"abc99"_s, CompareResult::Greater, CompareResult::Greater},
        {u"abc100"_s, u"ABC99"_s, CompareResult::Greater, CompareResult::Less},
        {u"ABC100"_s, u"ABC99"_s, CompareResult::Greater, CompareResult::Greater},

        {u"100abc"_s, u"99abc"_s, CompareResult::Greater, CompareResult::Greater},
        {u"100ABC"_s, u"99abc"_s, CompareResult::Greater, CompareResult::Greater},
        {u"100abc"_s, u"99ABC"_s, CompareResult::Greater, CompareResult::Greater},
        {u"100ABC"_s, u"99ABC"_s, CompareResult::Greater, CompareResult::Greater},

        {u"😀😀😀99"_s, u"😀😀😀100"_s, CompareResult::Less, CompareResult::Less},
        {u"😀😀😀100"_s, u"😀😀😀99"_s, CompareResult::Greater, CompareResult::Greater}
    };

    void testCompare(const TestData &data, const int actual, const CompareResult expected)
    {
        const auto errorMessage = u"Wrong result. LHS: \"%1\". RHS: \"%2\". Result: %3"_s
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
        const auto errorMessage = u"Wrong result. LHS: \"%1\". RHS: \"%2\". Result: %3"_s
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

class TestUtilsCompare final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsCompare)

public:
    TestUtilsCompare() = default;

private slots:
    void initTestCase() const
    {
        // Test will fail if ran with `C` locale. This is because `C` locale compare chars by code points
        // and doesn't take account of human expectations
        QLocale::setDefault(QLocale::English);
    }

    void cleanupTestCase() const
    {
        // restore global state
        QLocale::setDefault(QLocale::system());
    }

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
};

QTEST_APPLESS_MAIN(TestUtilsCompare)
#include "testutilscompare.moc"
