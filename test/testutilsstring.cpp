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
};

QTEST_APPLESS_MAIN(TestUtilsString)
#include "testutilsstring.moc"
