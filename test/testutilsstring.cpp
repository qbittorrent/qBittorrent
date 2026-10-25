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

    void testEscapeHTML() const
    {
        QCOMPARE(Utils::String::escapeHTML(u""_s), u""_s);
        QCOMPARE(Utils::String::escapeHTML(u"Hello world"_s), u"Hello world"_s);
        QCOMPARE(Utils::String::escapeHTML(u"line 1\nline 2"_s), u"line 1\nline 2"_s);

        QCOMPARE(Utils::String::escapeHTML(u"<script>alert(1)</script>"_s), u"&lt;script&gt;alert(1)&lt;/script&gt;"_s);
        QCOMPARE(Utils::String::escapeHTML(uR"(" onmouseover="alert(1))"_s), u"&quot; onmouseover=&quot;alert(1)"_s);
        QCOMPARE(Utils::String::escapeHTML(u"' onmouseover='alert(1)"_s), u"&#39; onmouseover=&#39;alert(1)"_s);

        // character references in source strings (e.g. `&amp;`) must still render
        QCOMPARE(Utils::String::escapeHTML(u"Coalesce reads &amp; writes"_s), u"Coalesce reads &amp; writes"_s);
        QCOMPARE(Utils::String::escapeHTML(u"Q&A"_s), u"Q&A"_s);
    }

    void testEscapeJSString() const
    {
        QCOMPARE(Utils::String::escapeJSString(u""_s), u""_s);
        QCOMPARE(Utils::String::escapeJSString(u"Hello world"_s), u"Hello world"_s);

        // escape sequences in source strings are kept, including those added by `check_webui_translation_tag.py`
        QCOMPARE(Utils::String::escapeJSString(uR"(line 1\nline 2\tend)"_s), uR"(line 1\nline 2\tend)"_s);
        QCOMPARE(Utils::String::escapeJSString(uR"(a \" b \' c \` d \${e} f \\ g)"_s), uR"(a \" b \' c \` d \${e} f \\ g)"_s);

        QCOMPARE(Utils::String::escapeJSString(uR"(say "hi")"_s), uR"(say \"hi\")"_s);
        QCOMPARE(Utils::String::escapeJSString(u"it's"_s), uR"(it\'s)"_s);
        QCOMPARE(Utils::String::escapeJSString(u"`${alert(1)}`"_s), uR"(\`\${alert(1)}\`)"_s);
        QCOMPARE(Utils::String::escapeJSString(u"$5"_s), uR"(\$5)"_s);

        // a backslash that isn't a kept escape sequence must not escape the closing quote
        QCOMPARE(Utils::String::escapeJSString(uR"(trailing\)"_s), uR"(trailing\\)"_s);
        QCOMPARE(Utils::String::escapeJSString(uR"(C:\path)"_s), uR"(C:\\path)"_s);

        QCOMPARE(Utils::String::escapeJSString(u"a\nb\rc\td"_s), uR"(a\nb\rc\td)"_s);
        QCOMPARE(Utils::String::escapeJSString(u"a\u2028b\u2029c"_s), uR"(a\u2028b\u2029c)"_s);
        QCOMPARE(Utils::String::escapeJSString(u"a\u0001b"_s), uR"(a\u0001b)"_s);

        // `</script>` and `<!--` must not reach the HTML tokenizer of an inline script
        QCOMPARE(Utils::String::escapeJSString(u"</script><!--"_s), uR"(\u003C/script>\u003C!--)"_s);
    }
};

QTEST_APPLESS_MAIN(TestUtilsString)
#include "testutilsstring.moc"
