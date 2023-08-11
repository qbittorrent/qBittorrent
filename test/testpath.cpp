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

#include <QtSystemDetection>
#include <QObject>
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
        QVERIFY(Path(u""_s) == Path(std::string("")));
        QVERIFY(Path(u"abc"_s) == Path(std::string("abc")));
        QVERIFY(Path(u"/abc"_s) == Path(std::string("/abc")));
        QVERIFY(Path(uR"(\abc)"_s) == Path(std::string(R"(\abc)")));

#ifdef Q_OS_WIN
        QVERIFY(Path(uR"(c:)"_s) == Path(std::string(R"(c:)")));
        QVERIFY(Path(uR"(c:/)"_s) == Path(std::string(R"(c:/)")));
        QVERIFY(Path(uR"(c:/)"_s) == Path(std::string(R"(c:\)")));
        QVERIFY(Path(uR"(c:\)"_s) == Path(std::string(R"(c:/)")));
        QVERIFY(Path(uR"(c:\)"_s) == Path(std::string(R"(c:\)")));

        QVERIFY(Path(uR"(\\?\C:)"_s) == Path(std::string(R"(\\?\C:)")));
        QVERIFY(Path(uR"(\\?\C:/)"_s) == Path(std::string(R"(\\?\C:/)")));
        QVERIFY(Path(uR"(\\?\C:/)"_s) == Path(std::string(R"(\\?\C:\)")));
        QVERIFY(Path(uR"(\\?\C:\)"_s) == Path(std::string(R"(\\?\C:/)")));
        QVERIFY(Path(uR"(\\?\C:\)"_s) == Path(std::string(R"(\\?\C:\)")));

        QVERIFY(Path(uR"(\\?\C:\abc)"_s) == Path(std::string(R"(\\?\C:\abc)")));
#endif
    }

    void testIsValid() const
    {
        QCOMPARE(Path().isValid(), false);
        QCOMPARE(Path(u""_s).isValid(), false);

        QCOMPARE(Path(u"/"_s).isValid(), true);
        QCOMPARE(Path(uR"(\)"_s).isValid(), true);

        QCOMPARE(Path(u"a"_s).isValid(), true);
        QCOMPARE(Path(u"/a"_s).isValid(), true);
        QCOMPARE(Path(uR"(\a)"_s).isValid(), true);

        QCOMPARE(Path(u"a/b"_s).isValid(), true);
        QCOMPARE(Path(uR"(a\b)"_s).isValid(), true);
        QCOMPARE(Path(u"/a/b"_s).isValid(), true);
        QCOMPARE(Path(uR"(/a\b)"_s).isValid(), true);
        QCOMPARE(Path(uR"(\a/b)"_s).isValid(), true);
        QCOMPARE(Path(uR"(\a\b)"_s).isValid(), true);

        QCOMPARE(Path(u"//"_s).isValid(), true);
        QCOMPARE(Path(uR"(\\)"_s).isValid(), true);
        QCOMPARE(Path(u"//a"_s).isValid(), true);
        QCOMPARE(Path(uR"(\\a)"_s).isValid(), true);

#if defined Q_OS_MACOS
        QCOMPARE(Path(u"\0"_s).isValid(), false);
        QCOMPARE(Path(u":"_s).isValid(), false);
#elif defined Q_OS_WIN
        QCOMPARE(Path(u"c:"_s).isValid(), false);
        QCOMPARE(Path(u"c:/"_s).isValid(), true);
        QCOMPARE(Path(uR"(c:\)"_s).isValid(), true);
        QCOMPARE(Path(uR"(c:\a)"_s).isValid(), true);
        QCOMPARE(Path(uR"(c:\a\b)"_s).isValid(), true);

        for (int i = 0; i <= 31; ++i)
            QCOMPARE(Path(QChar(i)).isValid(), false);
        QCOMPARE(Path(u":"_s).isValid(), false);
        QCOMPARE(Path(u"?"_s).isValid(), false);
        QCOMPARE(Path(u"\""_s).isValid(), false);
        QCOMPARE(Path(u"*"_s).isValid(), false);
        QCOMPARE(Path(u"<"_s).isValid(), false);
        QCOMPARE(Path(u">"_s).isValid(), false);
        QCOMPARE(Path(u"|"_s).isValid(), false);
#else
        QCOMPARE(Path(u"\0"_s).isValid(), false);
#endif
    }

    void testIsEmpty() const
    {
        QCOMPARE(Path().isEmpty(), true);
        QCOMPARE(Path(u""_s).isEmpty(), true);

        QCOMPARE(Path(u"\0"_s).isEmpty(), false);

        QCOMPARE(Path(u"/"_s).isEmpty(), false);
        QCOMPARE(Path(uR"(\)"_s).isEmpty(), false);

        QCOMPARE(Path(u"a"_s).isEmpty(), false);
        QCOMPARE(Path(u"/a"_s).isEmpty(), false);
        QCOMPARE(Path(uR"(\a)"_s).isEmpty(), false);

        QCOMPARE(Path(uR"(c:)"_s).isEmpty(), false);
        QCOMPARE(Path(uR"(c:/)"_s).isEmpty(), false);
        QCOMPARE(Path(uR"(c:\)"_s).isEmpty(), false);
    }

    void testIsAbsolute() const
    {
        QCOMPARE(Path().isAbsolute(), false);
        QCOMPARE(Path(u""_s).isAbsolute(), false);

#ifdef Q_OS_WIN
        QCOMPARE(Path(u"/"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\)"_s).isAbsolute(), true);

        QCOMPARE(Path(u"a"_s).isAbsolute(), false);
        QCOMPARE(Path(u"/a"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\a)"_s).isAbsolute(), true);

        QCOMPARE(Path(u"//"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\)"_s).isAbsolute(), true);
        QCOMPARE(Path(u"//a"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\a)"_s).isAbsolute(), true);

        QCOMPARE(Path(uR"(c:)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:/)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\a)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(c:\a\b)"_s).isAbsolute(), true);

        QCOMPARE(Path(uR"(\\?\C:)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:/)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\a)"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\?\C:\a\b)"_s).isAbsolute(), true);
#else
        QCOMPARE(Path(u"/"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\)"_s).isAbsolute(), false);

        QCOMPARE(Path(u"a"_s).isAbsolute(), false);
        QCOMPARE(Path(u"/a"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\a)"_s).isAbsolute(), false);

        QCOMPARE(Path(u"//"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\)"_s).isAbsolute(), false);
        QCOMPARE(Path(u"//a"_s).isAbsolute(), true);
        QCOMPARE(Path(uR"(\\a)"_s).isAbsolute(), false);
#endif
    }

    void testIsRelative() const
    {
        QCOMPARE(Path().isRelative(), true);
        QCOMPARE(Path(u""_s).isRelative(), true);

#if defined Q_OS_WIN
        QCOMPARE(Path(u"/"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\)"_s).isRelative(), false);

        QCOMPARE(Path(u"a"_s).isRelative(), true);
        QCOMPARE(Path(u"/a"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\a)"_s).isRelative(), false);

        QCOMPARE(Path(u"//"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\)"_s).isRelative(), false);
        QCOMPARE(Path(u"//a"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\a)"_s).isRelative(), false);

        QCOMPARE(Path(uR"(c:)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(c:/)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(c:\)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(c:\a)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(c:\a\b)"_s).isRelative(), false);

        QCOMPARE(Path(uR"(\\?\C:)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:/)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\a)"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\?\C:\a\b)"_s).isRelative(), false);
#else
        QCOMPARE(Path(u"/"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\)"_s).isRelative(), true);

        QCOMPARE(Path(u"a"_s).isRelative(), true);
        QCOMPARE(Path(u"/a"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\a)"_s).isRelative(), true);

        QCOMPARE(Path(u"//"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\)"_s).isRelative(), true);
        QCOMPARE(Path(u"//a"_s).isRelative(), false);
        QCOMPARE(Path(uR"(\\a)"_s).isRelative(), true);
#endif
    }

    void testRootItem() const
    {
        QCOMPARE(Path().rootItem(), Path());
        QCOMPARE(Path(u""_s).rootItem(), Path());

        QCOMPARE(Path(u"/"_s).rootItem(), Path(u"/"_s));
        QCOMPARE(Path(uR"(\)"_s).rootItem(), Path(uR"(\)"_s));

        QCOMPARE(Path(u"a"_s).rootItem(), Path(u"a"_s));
        QCOMPARE(Path(u"/a"_s).rootItem(), Path(u"/"_s));
        QCOMPARE(Path(u"/a/b"_s).rootItem(), Path(u"/"_s));

        QCOMPARE(Path(u"//"_s).rootItem(), Path(u"/"_s));
        QCOMPARE(Path(uR"(\\)"_s).rootItem(), Path(uR"(\\)"_s));
        QCOMPARE(Path(u"//a"_s).rootItem(), Path(u"/"_s));

#ifdef Q_OS_WIN
        QCOMPARE(Path(uR"(\a)"_s).rootItem(), Path(uR"(\)"_s));
        QCOMPARE(Path(uR"(\\a)"_s).rootItem(), Path(uR"(\)"_s));

        QCOMPARE(Path(uR"(c:)"_s).rootItem(), Path(uR"(c:)"_s));
        QCOMPARE(Path(uR"(c:/)"_s).rootItem(), Path(uR"(c:/)"_s));
        QCOMPARE(Path(uR"(c:\)"_s).rootItem(), Path(uR"(c:\)"_s));
        QCOMPARE(Path(uR"(c:\)"_s).rootItem(), Path(uR"(c:/)"_s));
        QCOMPARE(Path(uR"(c:\a)"_s).rootItem(), Path(uR"(c:\)"_s));
        QCOMPARE(Path(uR"(c:\a\b)"_s).rootItem(), Path(uR"(c:\)"_s));
#else
        QCOMPARE(Path(uR"(\a)"_s).rootItem(), Path(uR"(\a)"_s));
        QCOMPARE(Path(uR"(\\a)"_s).rootItem(), Path(uR"(\\a)"_s));
#endif
    }

    void testParentPath() const
    {
        QCOMPARE(Path().parentPath(), Path());
        QCOMPARE(Path(u""_s).parentPath(), Path());

        QCOMPARE(Path(u"/"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(\)"_s).parentPath(), Path());

        QCOMPARE(Path(u"a"_s).parentPath(), Path());
        QCOMPARE(Path(u"/a"_s).parentPath(), Path(u"/"_s));

        QCOMPARE(Path(u"//"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(\\)"_s).parentPath(), Path());
        QCOMPARE(Path(u"//a"_s).parentPath(), Path(u"/"_s));

        QCOMPARE(Path(u"a/b"_s).parentPath(), Path(u"a"_s));

#ifdef Q_OS_WIN
        QCOMPARE(Path(uR"(\a)"_s).parentPath(), Path(uR"(\)"_s));
        QCOMPARE(Path(uR"(\\a)"_s).parentPath(), Path(uR"(\)"_s));
        QCOMPARE(Path(uR"(a\b)"_s).parentPath(), Path(u"a"_s));

        QCOMPARE(Path(uR"(c:)"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(c:/)"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(c:\)"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(c:\a)"_s).parentPath(), Path(uR"(c:\)"_s));
        QCOMPARE(Path(uR"(c:\a\b)"_s).parentPath(), Path(uR"(c:\a)"_s));
#else
        QCOMPARE(Path(uR"(\a)"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(\\a)"_s).parentPath(), Path());
        QCOMPARE(Path(uR"(a\b)"_s).parentPath(), Path());
#endif
    }

    // TODO: add tests for remaining methods
};

QTEST_APPLESS_MAIN(TestPath)
#include "testpath.moc"
