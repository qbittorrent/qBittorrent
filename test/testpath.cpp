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

#include <QtGlobal>
#include <QTest>

#include "base/global.h"
#include "base/path.h"

class TestPath final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestPath)

public:
    TestPath() = default;

private slots:
    void testConstructors() const
    {
        QVERIFY(Path(u""_qs) == Path(std::string("")));
        QVERIFY(Path(u"abc"_qs) == Path(std::string("abc")));
        QVERIFY(Path(u"/abc"_qs) == Path(std::string("/abc")));
        QVERIFY(Path(uR"(\abc)"_qs) == Path(std::string(R"(\abc)")));

#ifdef Q_OS_WIN
        QVERIFY(Path(uR"(c:)"_qs) == Path(std::string(R"(c:)")));
        QVERIFY(Path(uR"(c:/)"_qs) == Path(std::string(R"(c:/)")));
        QVERIFY(Path(uR"(c:/)"_qs) == Path(std::string(R"(c:\)")));
        QVERIFY(Path(uR"(c:\)"_qs) == Path(std::string(R"(c:/)")));
        QVERIFY(Path(uR"(c:\)"_qs) == Path(std::string(R"(c:\)")));

        QVERIFY(Path(uR"(\\?\C:)"_qs) == Path(std::string(R"(\\?\C:)")));
        QVERIFY(Path(uR"(\\?\C:/)"_qs) == Path(std::string(R"(\\?\C:/)")));
        QVERIFY(Path(uR"(\\?\C:/)"_qs) == Path(std::string(R"(\\?\C:\)")));
        QVERIFY(Path(uR"(\\?\C:\)"_qs) == Path(std::string(R"(\\?\C:/)")));
        QVERIFY(Path(uR"(\\?\C:\)"_qs) == Path(std::string(R"(\\?\C:\)")));

        QVERIFY(Path(uR"(\\?\C:\abc)"_qs) == Path(std::string(R"(\\?\C:\abc)")));
#endif
    }

    void testIsValid() const
    {
        QCOMPARE(Path().isValid(), false);
        QCOMPARE(Path(u""_qs).isValid(), false);

        QCOMPARE(Path(u"/"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\)"_qs).isValid(), true);

        QCOMPARE(Path(u"a"_qs).isValid(), true);
        QCOMPARE(Path(u"/a"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\a)"_qs).isValid(), true);

        QCOMPARE(Path(u"a/b"_qs).isValid(), true);
        QCOMPARE(Path(uR"(a\b)"_qs).isValid(), true);
        QCOMPARE(Path(u"/a/b"_qs).isValid(), true);
        QCOMPARE(Path(uR"(/a\b)"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\a/b)"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\a\b)"_qs).isValid(), true);

        QCOMPARE(Path(u"//"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\\)"_qs).isValid(), true);
        QCOMPARE(Path(u"//a"_qs).isValid(), true);
        QCOMPARE(Path(uR"(\\a)"_qs).isValid(), true);

#if defined Q_OS_MACOS
        QCOMPARE(Path(u"\0"_qs).isValid(), false);
        QCOMPARE(Path(u":"_qs).isValid(), false);
#elif defined Q_OS_WIN
        QCOMPARE(Path(u"c:"_qs).isValid(), false);
        QCOMPARE(Path(u"c:/"_qs).isValid(), true);
        QCOMPARE(Path(uR"(c:\)"_qs).isValid(), true);
        QCOMPARE(Path(uR"(c:\a)"_qs).isValid(), true);
        QCOMPARE(Path(uR"(c:\a\b)"_qs).isValid(), true);

        for (int i = 0; i <= 31; ++i)
            QCOMPARE(Path(QChar(i)).isValid(), false);
        QCOMPARE(Path(u":"_qs).isValid(), false);
        QCOMPARE(Path(u"?"_qs).isValid(), false);
        QCOMPARE(Path(u"\""_qs).isValid(), false);
        QCOMPARE(Path(u"*"_qs).isValid(), false);
        QCOMPARE(Path(u"<"_qs).isValid(), false);
        QCOMPARE(Path(u">"_qs).isValid(), false);
        QCOMPARE(Path(u"|"_qs).isValid(), false);
#else
        QCOMPARE(Path(u"\0"_qs).isValid(), false);
#endif
    }

    void testIsEmpty() const
    {
        QCOMPARE(Path().isEmpty(), true);
        QCOMPARE(Path(u""_qs).isEmpty(), true);

        QCOMPARE(Path(u"\0"_qs).isEmpty(), false);

        QCOMPARE(Path(u"/"_qs).isEmpty(), false);
        QCOMPARE(Path(uR"(\)"_qs).isEmpty(), false);

        QCOMPARE(Path(u"a"_qs).isEmpty(), false);
        QCOMPARE(Path(u"/a"_qs).isEmpty(), false);
        QCOMPARE(Path(uR"(\a)"_qs).isEmpty(), false);

        QCOMPARE(Path(uR"(c:)"_qs).isEmpty(), false);
        QCOMPARE(Path(uR"(c:/)"_qs).isEmpty(), false);
        QCOMPARE(Path(uR"(c:\)"_qs).isEmpty(), false);
    }

    void testIsAbsolute() const
    {
        QCOMPARE(Path().isAbsolute(), false);
        QCOMPARE(Path(u""_qs).isAbsolute(), false);

#ifdef Q_OS_WIN
        QCOMPARE(Path(u"/"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\)"_qs).isAbsolute(), true);

        QCOMPARE(Path(u"a"_qs).isAbsolute(), false);
        QCOMPARE(Path(u"/a"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\a)"_qs).isAbsolute(), true);

        QCOMPARE(Path(u"//"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\)"_qs).isAbsolute(), true);
        QCOMPARE(Path(u"//a"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\a)"_qs).isAbsolute(), true);

        QCOMPARE(Path(uR"(c:)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:/)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\a)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\a\b)"_qs).isAbsolute(), true);

        QCOMPARE(Path(uR"(\\?\C:)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:/)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\a)"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\a\b)"_qs).isAbsolute(), true);
#else
        QCOMPARE(Path(u"/"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\)"_qs).isAbsolute(), false);

        QCOMPARE(Path(u"a"_qs).isAbsolute(), false);
        QCOMPARE(Path(u"/a"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\a)"_qs).isAbsolute(), false);

        QCOMPARE(Path(u"//"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\)"_qs).isAbsolute(), false);
        QCOMPARE(Path(u"//a"_qs).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\a)"_qs).isAbsolute(), false);
#endif
    }

    void testIsRelative() const
    {
        QCOMPARE(Path().isRelative(), true);
        QCOMPARE(Path(u""_qs).isRelative(), true);

#if defined Q_OS_WIN
        QCOMPARE(Path(u"/"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\)"_qs).isRelative(), false);

        QCOMPARE(Path(u"a"_qs).isRelative(), true);
        QCOMPARE(Path(u"/a"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\a)"_qs).isRelative(), false);

        QCOMPARE(Path(u"//"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\)"_qs).isRelative(), false);
        QCOMPARE(Path(u"//a"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\a)"_qs).isRelative(), false);

        QCOMPARE(Path(uR"(c:)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(c:/)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(c:\)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(c:\a)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(c:\a\b)"_qs).isRelative(), false);

        QCOMPARE(Path(uR"(\\?\C:)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:/)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\a)"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\a\b)"_qs).isRelative(), false);
#else
        QCOMPARE(Path(u"/"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\)"_qs).isRelative(), true);

        QCOMPARE(Path(u"a"_qs).isRelative(), true);
        QCOMPARE(Path(u"/a"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\a)"_qs).isRelative(), true);

        QCOMPARE(Path(u"//"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\)"_qs).isRelative(), true);
        QCOMPARE(Path(u"//a"_qs).isRelative(), false);
        QCOMPARE(Path(uR"(\\a)"_qs).isRelative(), true);
#endif
    }

    void testRootItem() const
    {
        QCOMPARE(Path().rootItem(), Path());
        QCOMPARE(Path(u""_qs).rootItem(), Path());

        QCOMPARE(Path(u"/"_qs).rootItem(), Path(u"/"_qs));
        QCOMPARE(Path(uR"(\)"_qs).rootItem(), Path(uR"(\)"_qs));

        QCOMPARE(Path(u"a"_qs).rootItem(), Path(u"a"_qs));
        QCOMPARE(Path(u"/a"_qs).rootItem(), Path(u"/"_qs));
        QCOMPARE(Path(u"/a/b"_qs).rootItem(), Path(u"/"_qs));

        QCOMPARE(Path(u"//"_qs).rootItem(), Path(u"/"_qs));
        QCOMPARE(Path(uR"(\\)"_qs).rootItem(), Path(uR"(\\)"_qs));
        QCOMPARE(Path(u"//a"_qs).rootItem(), Path(u"/"_qs));

#ifdef Q_OS_WIN
        QCOMPARE(Path(uR"(\a)"_qs).rootItem(), Path(uR"(\)"_qs));
        QCOMPARE(Path(uR"(\\a)"_qs).rootItem(), Path(uR"(\)"_qs));

        QCOMPARE(Path(uR"(c:)"_qs).rootItem(), Path(uR"(c:)"_qs));
        QCOMPARE(Path(uR"(c:/)"_qs).rootItem(), Path(uR"(c:/)"_qs));
        QCOMPARE(Path(uR"(c:\)"_qs).rootItem(), Path(uR"(c:\)"_qs));
        QCOMPARE(Path(uR"(c:\)"_qs).rootItem(), Path(uR"(c:/)"_qs));
        QCOMPARE(Path(uR"(c:\a)"_qs).rootItem(), Path(uR"(c:\)"_qs));
        QCOMPARE(Path(uR"(c:\a\b)"_qs).rootItem(), Path(uR"(c:\)"_qs));
#else
        QCOMPARE(Path(uR"(\a)"_qs).rootItem(), Path(uR"(\a)"_qs));
        QCOMPARE(Path(uR"(\\a)"_qs).rootItem(), Path(uR"(\\a)"_qs));
#endif
    }

    void testParentPath() const
    {
        QCOMPARE(Path().parentPath(), Path());
        QCOMPARE(Path(u""_qs).parentPath(), Path());

        QCOMPARE(Path(u"/"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(\)"_qs).parentPath(), Path());

        QCOMPARE(Path(u"a"_qs).parentPath(), Path());
        QCOMPARE(Path(u"/a"_qs).parentPath(), Path(u"/"_qs));

        QCOMPARE(Path(u"//"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(\\)"_qs).parentPath(), Path());
        QCOMPARE(Path(u"//a"_qs).parentPath(), Path(u"/"_qs));

        QCOMPARE(Path(u"a/b"_qs).parentPath(), Path(u"a"_qs));

#ifdef Q_OS_WIN
        QCOMPARE(Path(uR"(\a)"_qs).parentPath(), Path(uR"(\)"_qs));
        QCOMPARE(Path(uR"(\\a)"_qs).parentPath(), Path(uR"(\)"_qs));
        QCOMPARE(Path(uR"(a\b)"_qs).parentPath(), Path(u"a"_qs));

        QCOMPARE(Path(uR"(c:)"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(c:/)"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(c:\)"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(c:\a)"_qs).parentPath(), Path(uR"(c:\)"_qs));
        QCOMPARE(Path(uR"(c:\a\b)"_qs).parentPath(), Path(uR"(c:\a)"_qs));
#else
        QCOMPARE(Path(uR"(\a)"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(\\a)"_qs).parentPath(), Path());
        QCOMPARE(Path(uR"(a\b)"_qs).parentPath(), Path());
#endif
    }

    // TODO: add tests for remaining methods
};

QTEST_APPLESS_MAIN(TestPath)
#include "testpath.moc"
