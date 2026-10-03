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

    void testEscapeJSStringHazards() const
    {
        using Utils::String::escapeJSStringHazards;

        // unchanged: plain text, non-ASCII, raw tab (legal in JS)
        QCOMPARE(escapeJSStringHazards(u""_s), u""_s);
        QCOMPARE(escapeJSStringHazards(u"Hello world"_s), u"Hello world"_s);
        QCOMPARE(escapeJSStringHazards(u"café 中文"_s), u"café 中文"_s);
        QCOMPARE(escapeJSStringHazards(u"a\tb"_s), u"a\tb"_s);

        // raw line terminators escaped (the #23488 fix)
        QCOMPARE(escapeJSStringHazards(u"a\nb"_s), u"a\\nb"_s);
        QCOMPARE(escapeJSStringHazards(u"a\rb"_s), u"a\\rb"_s);
        QCOMPARE(escapeJSStringHazards(u"a\r\nb"_s), u"a\\r\\nb"_s);
        QCOMPARE(escapeJSStringHazards(u"trailing\n"_s), u"trailing\\n"_s);
        QCOMPARE(escapeJSStringHazards(QString(QChar(0x2028))), u"\\u2028"_s);
        QCOMPARE(escapeJSStringHazards(QString(QChar(0x2029))), u"\\u2029"_s);

        // `<` escaped, so a "</script>" can't end the element
        QCOMPARE(escapeJSStringHazards(u"</script>"_s), u"\\u003C/script>"_s);
        QCOMPARE(escapeJSStringHazards(u"a<br>b"_s), u"a\\u003Cbr>b"_s);
        QCOMPARE(escapeJSStringHazards(u"</script><script>x</script>"_s),
            u"\\u003C/script>\\u003Cscript>x\\u003C/script>"_s);

        // template literals: backtick and "${" escaped; lone '$' not
        QCOMPARE(escapeJSStringHazards(u"a`b"_s), u"a\\`b"_s);
        QCOMPARE(escapeJSStringHazards(u"`"_s), u"\\`"_s);
        QCOMPARE(escapeJSStringHazards(u"${x}"_s), u"\\${x}"_s);
        QCOMPARE(escapeJSStringHazards(u"a${b}c${d}"_s), u"a\\${b}c\\${d}"_s);
        QCOMPARE(escapeJSStringHazards(u"5$ each"_s), u"5$ each"_s);
        QCOMPARE(escapeJSStringHazards(u"ends$"_s), u"ends$"_s);
        QCOMPARE(escapeJSStringHazards(u"$var"_s), u"$var"_s);

        // single quote escaped
        QCOMPARE(escapeJSStringHazards(u"it's"_s), u"it\\'s"_s);
        QCOMPARE(escapeJSStringHazards(u"''"_s), u"\\'\\'"_s);

        // `\n`/`\t` escapes pass through
        QCOMPARE(escapeJSStringHazards(uR"(line1\nline2)"_s), uR"(line1\nline2)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(col1\tcol2)"_s), uR"(col1\tcol2)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(\n\t)"_s), uR"(\n\t)"_s);

        // any other backslash escaped (trailing, or invalid \x/\u)
        QCOMPARE(escapeJSStringHazards(uR"(path\)"_s), uR"(path\\)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(\\)"_s), uR"(\\\\)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(\x41)"_s), uR"(\\x41)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(\u12)"_s), uR"(\\u12)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(a\rb)"_s), uR"(a\\rb)"_s);
        QCOMPARE(escapeJSStringHazards(uR"(\0)"_s), uR"(\\0)"_s);

        // double quote is left to the caller
        QCOMPARE(escapeJSStringHazards(uR"(say "hi")"_s), uR"(say "hi")"_s);

        // combinations
        QCOMPARE(escapeJSStringHazards(u"a\n\\nb"_s), u"a\\n\\nb"_s); // raw newline next to a `\n` escape
        QCOMPARE(escapeJSStringHazards(u"c'est\nfini"_s), u"c\\'est\\nfini"_s);
        QCOMPARE(escapeJSStringHazards(u"${fetch('/x')}"_s), u"\\${fetch(\\'/x\\')}"_s);
        QCOMPARE(escapeJSStringHazards(u"'`<${\n"_s), u"\\'\\`\\u003C\\${\\n"_s);
    }
};

QTEST_APPLESS_MAIN(TestUtilsString)
#include "testutilsstring.moc"
