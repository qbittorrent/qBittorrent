/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QObject>
#include <QSet>
#include <QTest>

#include "base/global.h"
#include "base/orderedset.h"
#include "base/utils/string.h"

class TestOrderedSet final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestOrderedSet)

public:
    TestOrderedSet() = default;

private slots:
    void testCount() const
    {
        const OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s, u"c"_s};
        QCOMPARE(set.count(), 3);

        const OrderedSet<QString> emptySet;
        QCOMPARE(emptySet.count(), 0);
    }

    void testIntersect() const
    {
        OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s};
        set.intersect({u"c"_s, u"a"_s});
        QCOMPARE(set.size(), 2);
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,c"_s);

        OrderedSet<QString> emptySet;
        emptySet.intersect({u"a"_s}).intersect({u"c"_s});;
        QVERIFY(emptySet.isEmpty());
    }

    void testIsEmpty() const
    {
        const OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s};
        QVERIFY(!set.isEmpty());

        const OrderedSet<QString> emptySet;
        QVERIFY(emptySet.isEmpty());
    }

    void testRemove() const
    {
        OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s};
        QVERIFY(!set.remove(u"z"_s));
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,b,c"_s);
        QVERIFY(set.remove(u"b"_s));
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,c"_s);
        QVERIFY(set.remove(u"a"_s));
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"c"_s);
        QVERIFY(set.remove(u"c"_s));
        QVERIFY(set.isEmpty());

        OrderedSet<QString> emptySet;
        QVERIFY(!emptySet.remove(u"a"_s));
        QVERIFY(emptySet.isEmpty());
    }

    void testUnite() const
    {
        const OrderedSet<QString> newData1 {u"z"_s};
        const OrderedSet<QString> newData2 {u"y"_s};
        const QSet<QString> newData3 {u"c"_s, u"d"_s, u"e"_s};

        OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s};
        set.unite(newData1);
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,b,c,z"_s);
        set.unite(newData2);
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,b,c,y,z"_s);
        set.unite(newData3);
        QCOMPARE(Utils::String::joinIntoString(set, u","_s), u"a,b,c,d,e,y,z"_s);

        OrderedSet<QString> emptySet;
        emptySet.unite(newData1).unite(newData2).unite(newData3);
        QCOMPARE(Utils::String::joinIntoString(emptySet, u","_s), u"c,d,e,y,z"_s);
    }

    void testUnited() const
    {
        const OrderedSet<QString> newData1 {u"z"_s};
        const OrderedSet<QString> newData2 {u"y"_s};
        const QSet<QString> newData3 {u"c"_s, u"d"_s, u"e"_s};

        OrderedSet<QString> set {u"a"_s, u"b"_s, u"c"_s};

        QCOMPARE(Utils::String::joinIntoString(set.united(newData1), u","_s), u"a,b,c,z"_s);
        QCOMPARE(Utils::String::joinIntoString(set.united(newData2), u","_s), u"a,b,c,y"_s);
        QCOMPARE(Utils::String::joinIntoString(set.united(newData3), u","_s), u"a,b,c,d,e"_s);

        QCOMPARE(Utils::String::joinIntoString(OrderedSet<QString>().united(newData1).united(newData2).united(newData3), u","_s), u"c,d,e,y,z"_s);
    }
};

QTEST_APPLESS_MAIN(TestOrderedSet)
#include "testorderedset.moc"
