/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QByteArray>
#include <QByteArrayView>
#include <QLatin1StringView>
#include <QObject>
#include <QTest>

#include "base/utils/bytearray.h"

class TestUtilsByteArray final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsByteArray)

public:
    TestUtilsByteArray() = default;

private slots:
    void testSplitToViews() const
    {
        using BAViews = QList<QByteArrayView>;

        const auto checkSkipEmptyParts = [](const QByteArrayView in, const QByteArrayView sep, const BAViews expected)
        {
            // verify it works
            QCOMPARE(Utils::ByteArray::splitToViews(in, sep), expected);

            // verify it has the same behavior as `split(Qt::SkipEmptyParts)`
            using Latin1Views = QList<QLatin1StringView>;

            const Latin1Views reference = QLatin1StringView(in)
                    .tokenize(QLatin1StringView(sep), Qt::SkipEmptyParts).toContainer();
            Latin1Views expectedStrings;
            for (const auto &string : expected)
                expectedStrings.append(QLatin1StringView(string));
            QCOMPARE(reference, expectedStrings);
        };

        checkSkipEmptyParts({}, {}, {});
        checkSkipEmptyParts({}, "/", {});
        checkSkipEmptyParts("/", "/", {});
        checkSkipEmptyParts("/a", "/", {"a"});
        checkSkipEmptyParts("/a/", "/", {"a"});
        checkSkipEmptyParts("/a/b", "/", (BAViews {"a", "b"}));
        checkSkipEmptyParts("/a/b/", "/", (BAViews {"a", "b"}));
        checkSkipEmptyParts("/a/b", "//", {"/a/b"});
        checkSkipEmptyParts("//a/b", "//", {"a/b"});
        checkSkipEmptyParts("//a//b", "//", (BAViews {"a", "b"}));
        checkSkipEmptyParts("//a//b/", "//", (BAViews {"a", "b/"}));
        checkSkipEmptyParts("//a//b//", "//", (BAViews {"a", "b"}));
        checkSkipEmptyParts("///a//b//", "//", (BAViews {"/a", "b"}));

        const auto checkKeepEmptyParts = [](const QByteArrayView in, const QByteArrayView sep, const BAViews expected)
        {
            // verify it works
            QCOMPARE(Utils::ByteArray::splitToViews(in, sep, Qt::KeepEmptyParts), expected);

            // verify it has the same behavior as `split(Qt::KeepEmptyParts)`
            using Latin1Views = QList<QLatin1StringView>;

            const Latin1Views reference = QLatin1StringView(in)
                    .tokenize(QLatin1StringView(sep), Qt::KeepEmptyParts).toContainer();
            Latin1Views expectedStrings;
            for (const auto &string : expected)
                expectedStrings.append(QLatin1StringView(string));
            QCOMPARE(reference, expectedStrings);
        };

        checkKeepEmptyParts({}, {}, {{}, {}});
        checkKeepEmptyParts({}, "/", {{}});
        checkKeepEmptyParts("/", "/", {"", ""});
        checkKeepEmptyParts("/a", "/", {"", "a"});
        checkKeepEmptyParts("/a/", "/", {"", "a", ""});
        checkKeepEmptyParts("/a/b", "/", (BAViews {"", "a", "b"}));
        checkKeepEmptyParts("/a/b/", "/", (BAViews {"", "a", "b", ""}));
        checkKeepEmptyParts("/a/b", "//", {"/a/b"});
        checkKeepEmptyParts("//a/b", "//", {"", "a/b"});
        checkKeepEmptyParts("//a//b", "//", (BAViews {"", "a", "b"}));
        checkKeepEmptyParts("//a//b/", "//", (BAViews {"", "a", "b/"}));
        checkKeepEmptyParts("//a//b//", "//", (BAViews {"", "a", "b", ""}));
        checkKeepEmptyParts("///a//b//", "//", (BAViews {"", "/a", "b", ""}));
    }

    void testAsQByteArray() const
    {
        QCOMPARE(Utils::ByteArray::asQByteArray(""), "");
        QCOMPARE(Utils::ByteArray::asQByteArray("12345"), "12345");
    }

    void testToBase32() const
    {
        QCOMPARE(Utils::ByteArray::toBase32({}), QByteArray());
        QCOMPARE(Utils::ByteArray::toBase32(""), "");
        QCOMPARE(Utils::ByteArray::toBase32("0123456789"), "GAYTEMZUGU3DOOBZ");
        QCOMPARE(Utils::ByteArray::toBase32("ABCDE"), "IFBEGRCF");
        QCOMPARE(Utils::ByteArray::toBase32("0000000000"), "GAYDAMBQGAYDAMBQ");
        QCOMPARE(Utils::ByteArray::toBase32("1"), "GE======");
    }

    void testUnquote() const
    {
        const auto test = []<typename T>()
        {
            QCOMPARE(Utils::ByteArray::unquote<T>({}), {});
            QCOMPARE(Utils::ByteArray::unquote<T>("abc"), "abc");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"abc\""), "abc");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"a b c\""), "a b c");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"abc"), "\"abc");
            QCOMPARE(Utils::ByteArray::unquote<T>("abc\""), "abc\"");
            QCOMPARE(Utils::ByteArray::unquote<T>(" \"abc\" "), " \"abc\" ");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"a\"bc\""), "a\"bc");
            QCOMPARE(Utils::ByteArray::unquote<T>("'abc'", "'"), "abc");
            QCOMPARE(Utils::ByteArray::unquote<T>("'abc'", "\"'"), "abc");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"'abc'\"", "\"'"), "'abc'");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"'abc'\"", "'\""), "'abc'");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"'abc'\"", "'"), "\"'abc'\"");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"abc'", "'"), "\"abc'");
            QCOMPARE(Utils::ByteArray::unquote<T>("'abc\"", "'"), "'abc\"");
            QCOMPARE(Utils::ByteArray::unquote<T>("\"\""), "");
            QCOMPARE(Utils::ByteArray::unquote<T>("\""), "\"");
        };

        test.template operator()<QByteArray>();
        test.template operator()<QByteArrayView>();
    }
};

QTEST_APPLESS_MAIN(TestUtilsByteArray)
#include "testutilsbytearray.moc"
