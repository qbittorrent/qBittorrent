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

#include <QList>
#include <QObject>
#include <QRegularExpression>
#include <QTest>

#include "base/global.h"
#include "base/utils/string.h"

namespace
{
    class MyString
    {
    public:
        MyString(const QString &str)
            : m_str {str}
        {
        }

        explicit operator QString() const
        {
            return m_str;
        }

    private:
        QString m_str;
    };
}

class TestUtilsString final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsString)

public:
    TestUtilsString() = default;

private slots:
    void testJoinIntoString() const
    {
        const QList<QString> list1;
        QCOMPARE(Utils::String::joinIntoString(list1, u","_s), u""_s);

        const QList<QString> list2 {u"a"_s};
        QCOMPARE(Utils::String::joinIntoString(list2, u","_s), u"a"_s);

        const QList<QString> list3 {u"a"_s, u"b"_s};
        QCOMPARE(Utils::String::joinIntoString(list3, u" , "_s), u"a , b"_s);

        const QList<MyString> list4 {u"a"_s, u"b"_s, u"cd"_s};
        QCOMPARE(Utils::String::joinIntoString(list4, u"++"_s), u"a++b++cd"_s);
    }

    void testSplitCommand() const
    {
        QCOMPARE(Utils::String::splitCommand({}), {});
        QCOMPARE(Utils::String::splitCommand(u""_s), {});
        QCOMPARE(Utils::String::splitCommand(u"  "_s), {});
        QCOMPARE(Utils::String::splitCommand(uR"("")"_s), {uR"("")"_s});
        QCOMPARE(Utils::String::splitCommand(uR"(" ")"_s), {uR"(" ")"_s});
        QCOMPARE(Utils::String::splitCommand(u"\"\"\""_s), {u"\"\"\""_s});
        QCOMPARE(Utils::String::splitCommand(uR"(" """)"_s), {uR"(" """)"_s});
        QCOMPARE(Utils::String::splitCommand(u" app a b c  "_s), QStringList({u"app"_s, u"a"_s, u"b"_s, u"c"_s}));
        QCOMPARE(Utils::String::splitCommand(u"   cmd.exe /d --arg2 \"arg3\" \"\" arg5 \"\"arg6 \"arg7 "_s)
            , QStringList({u"cmd.exe"_s, u"/d"_s, u"--arg2"_s, u"\"arg3\""_s, u"\"\""_s, u"arg5"_s, u"\"\"arg6"_s, u"\"arg7 "_s}));
    }

    void testNormalizeSearchPattern() const
    {
        QCOMPARE(Utils::String::normalizeSearchPattern({}), u""_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"   "_s), u""_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"2026 h265 1080"_s), u"2026 h265 1080"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"2026 AND h265"_s), u"2026 h265"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"2026 AND h265 AND 1080"_s), u"2026 h265 1080"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"2026   AND   h265"_s), u"2026 h265"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"2026 and h265"_s), u"2026 h265"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"AND 2026 h265 AND"_s), u"2026 h265"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(u"AND"_s), u""_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(uR"("2026 AND h265")"_s), uR"("2026 AND h265")"_s);
        QCOMPARE(Utils::String::normalizeSearchPattern(uR"(2026 AND "h265 1080")"_s), uR"(2026 "h265 1080")"_s);
    }

    void testParseSearchTerms() const
    {
        const auto terms = [](const QString &pattern) -> QStringList
        {
            QStringList result;
            for (const Utils::String::SearchTerm &term : Utils::String::parseSearchTerms(pattern))
                result.append((term.excluded ? u"-"_s : u""_s) + term.text);
            return result;
        };

        QCOMPARE(terms({}), {});
        QCOMPARE(terms(u"   "_s), {});
        QCOMPARE(terms(u"1080 2026 265"_s), QStringList({u"1080"_s, u"2026"_s, u"265"_s}));
        QCOMPARE(terms(uR"("1080 2026" 265)"_s), QStringList({u"1080 2026"_s, u"265"_s}));
        QCOMPARE(terms(u"doe -john"_s), QStringList({u"doe"_s, u"-john"_s}));
        QCOMPARE(terms(u"doe +john"_s), QStringList({u"doe"_s, u"john"_s}));
        QCOMPARE(terms(u"doe -"_s), QStringList({u"doe"_s}));
        QCOMPARE(terms(uR"(doe -"john doe")"_s), QStringList({u"doe"_s, u"-john doe"_s}));
        QCOMPARE(terms(u"\"unterminated"_s), QStringList({u"\"unterminated"_s}));
    }

    void testSearchTermsToRegexPattern() const
    {
        // Compare behavior rather than the exact pattern: Qt is free to wrap
        // the wildcard conversion in extra groups (e.g. "(?s:...)").
        const auto matches = [](const QString &pattern, const QString &name, const bool regexTerms = false) -> bool
        {
            const QRegularExpression regex {Utils::String::searchTermsToRegexPattern(pattern, regexTerms)
                , QRegularExpression::CaseInsensitiveOption};
            return regex.isValid() && regex.match(name).hasMatch();
        };

        QVERIFY(matches({}, u"anything"_s));
        QVERIFY(matches(u"1080"_s, u"Movie.2026.1080p.H265"_s));
        QVERIFY(!matches(u"1080"_s, u"Movie.2026.720p.H265"_s));

        QVERIFY(matches(u"1080 2026 265"_s, u"Movie.2026.1080p.H265"_s));
        QVERIFY(matches(u"265 1080"_s, u"Movie.2026.1080p.H265"_s));
        QVERIFY(!matches(u"1080 2026 265"_s, u"Movie.2025.1080p.H265"_s));

        QVERIFY(matches(u"2026 x265"_s, u"Movie.2026.1080p.x265"_s));
        QVERIFY(matches(u"2026 "_s, u"Movie.2026.1080p.x265"_s));

        QVERIFY(matches(u"1080 AND 2026"_s, u"Movie.2026.1080p.H265"_s));
        QVERIFY(!matches(u"1080 AND 2026"_s, u"Movie.2025.1080p.H265"_s));

        QVERIFY(matches(uR"("1080 2026")"_s, u"Movie.1080 2026.H265"_s));
        QVERIFY(!matches(uR"("1080 2026")"_s, u"Movie.2026.1080p.H265"_s));

        QVERIFY(matches(u"1080*"_s, u"Movie.2026.1080p.H265"_s));
        QVERIFY(!matches(u"1080*"_s, u"Movie.2026.720p.H265"_s));

        QVERIFY(matches(u"doe -john"_s, u"jane doe"_s));
        QVERIFY(!matches(u"doe -john"_s, u"john doe"_s));
        QVERIFY(matches(u"doe +john"_s, u"john doe"_s));
        QVERIFY(matches(uR"(doe -"john doe")"_s, u"jane doe"_s));
        QVERIFY(!matches(uR"(doe -"john doe")"_s, u"john doe"_s));

        // In regex mode each term is a regular expression, still combined with AND
        QVERIFY(matches(u"2026 x265"_s, u"Movie.2026.1080p.x265"_s, true));
        QVERIFY(matches(u"2026 (x265|hevc)"_s, u"Movie.2026.1080p.x265"_s, true));
        QVERIFY(!matches(u"2026 av1"_s, u"Movie.2026.1080p.x265"_s, true));
        QVERIFY(matches(u"2026 "_s, u"Movie.2026.1080p.x265"_s, true));
        QVERIFY(matches(u"doe -john"_s, u"jane doe"_s, true));
        QVERIFY(!matches(u"doe -john"_s, u"john doe"_s, true));
    }
};

QTEST_APPLESS_MAIN(TestUtilsString)
#include "testutilsstring.moc"
