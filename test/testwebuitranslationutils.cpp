/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Thomas Piccirello <thomas@piccirello.com>
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

#include "webui/translationutils.h"

using namespace Qt::Literals::StringLiterals;

class TestWebUITranslationUtils final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestWebUITranslationUtils)

public:
    TestWebUITranslationUtils() = default;

private slots:
    void testEscapeHTML() const
    {
        QCOMPARE(TranslationUtils::escapeHTML(u""_s), u""_s);
        QCOMPARE(TranslationUtils::escapeHTML(u"Hello world"_s), u"Hello world"_s);
        QCOMPARE(TranslationUtils::escapeHTML(u"line 1\nline 2"_s), u"line 1\nline 2"_s);

        QCOMPARE(TranslationUtils::escapeHTML(u"<script>alert(1)</script>"_s), u"&lt;script&gt;alert(1)&lt;/script&gt;"_s);
        QCOMPARE(TranslationUtils::escapeHTML(uR"(" onmouseover="alert(1))"_s), u"&quot; onmouseover=&quot;alert(1)"_s);
        QCOMPARE(TranslationUtils::escapeHTML(u"' onmouseover='alert(1)"_s), u"&#39; onmouseover=&#39;alert(1)"_s);

        // character references in source strings (e.g. `&amp;`) must still render
        QCOMPARE(TranslationUtils::escapeHTML(u"Coalesce reads &amp; writes"_s), u"Coalesce reads &amp; writes"_s);
        QCOMPARE(TranslationUtils::escapeHTML(u"Q&A"_s), u"Q&A"_s);
    }

    void testEscapeJSString() const
    {
        QCOMPARE(TranslationUtils::escapeJSString(u""_s), u""_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"Hello world"_s), u"Hello world"_s);

        // escape sequences in source strings are kept, including those added by `check_webui_translation_tag.py`
        QCOMPARE(TranslationUtils::escapeJSString(uR"(line 1\nline 2\tend)"_s), uR"(line 1\nline 2\tend)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(uR"(a \" b \' c \` d \${e} f \\ g)"_s), uR"(a \" b \' c \` d \${e} f \\ g)"_s);

        QCOMPARE(TranslationUtils::escapeJSString(uR"(say "hi")"_s), uR"(say \"hi\")"_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"it's"_s), uR"(it\'s)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"`${alert(1)}`"_s), uR"(\`\${alert(1)}\`)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"$5"_s), uR"(\$5)"_s);

        // a backslash that isn't a kept escape sequence must not escape the closing quote
        QCOMPARE(TranslationUtils::escapeJSString(uR"(trailing\)"_s), uR"(trailing\\)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(uR"(C:\path)"_s), uR"(C:\\path)"_s);

        QCOMPARE(TranslationUtils::escapeJSString(u"a\nb\rc\td"_s), uR"(a\nb\rc\td)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"a\u2028b\u2029c"_s), uR"(a\u2028b\u2029c)"_s);
        QCOMPARE(TranslationUtils::escapeJSString(u"a\u0001b"_s), uR"(a\u0001b)"_s);

        // `</script>` and `<!--` must not reach the HTML tokenizer of an inline script
        QCOMPARE(TranslationUtils::escapeJSString(u"</script><!--"_s), uR"(\u003C/script>\u003C!--)"_s);
    }
};

QTEST_APPLESS_MAIN(TestWebUITranslationUtils)
#include "testwebuitranslationutils.moc"
