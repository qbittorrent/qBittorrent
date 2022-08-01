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

#include <QTest>

#include "base/global.h"
#include "base/utils/string.h"

class TestUtilsString final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsString)

public:
    TestUtilsString() = default;

private slots:
    void testSplitCommand() const
    {
        QCOMPARE(Utils::String::splitCommand({}), {});
        QCOMPARE(Utils::String::splitCommand(u""_qs), {});
        QCOMPARE(Utils::String::splitCommand(u"  "_qs), {});
        QCOMPARE(Utils::String::splitCommand(uR"("")"_qs), {uR"("")"_qs});
        QCOMPARE(Utils::String::splitCommand(uR"(" ")"_qs), {uR"(" ")"_qs});
        QCOMPARE(Utils::String::splitCommand(u"\"\"\""_qs), {u"\"\"\""_qs});
        QCOMPARE(Utils::String::splitCommand(uR"(" """)"_qs), {uR"(" """)"_qs});
        QCOMPARE(Utils::String::splitCommand(u" app a b c  "_qs), QStringList({u"app"_qs, u"a"_qs, u"b"_qs, u"c"_qs}));
        QCOMPARE(Utils::String::splitCommand(u"   cmd.exe /d --arg2 \"arg3\" \"\" arg5 \"\"arg6 \"arg7 "_qs)
            , QStringList({u"cmd.exe"_qs, u"/d"_qs, u"--arg2"_qs, u"\"arg3\""_qs, u"\"\""_qs, u"arg5"_qs, u"\"\"arg6"_qs, u"\"arg7 "_qs}));
    }
};

QTEST_APPLESS_MAIN(TestUtilsString)
#include "testutilsstring.moc"
