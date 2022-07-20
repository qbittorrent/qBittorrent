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
#include "base/orderedset.h"

class TestOrderedSet final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestOrderedSet)

public:
    TestOrderedSet() = default;

private slots:
#if __cplusplus < 202002L
    void testContains() const
    {
        const OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        QVERIFY(set.contains(u"a"_qs));
        QVERIFY(set.contains(u"b"_qs));
        QVERIFY(set.contains(u"c"_qs));
        QVERIFY(!set.contains(u"z"_qs));

        const OrderedSet<QString> emptySet;
        QVERIFY(!emptySet.contains(u"a"_qs));
    }
#endif

    void testCount() const
    {
        const OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs, u"c"_qs};
        QCOMPARE(set.count(), 3);

        const OrderedSet<QString> emptySet;
        QCOMPARE(emptySet.count(), 0);
    }

    void testIntersect() const
    {
        OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        set.intersect({u"c"_qs, u"a"_qs});
        QCOMPARE(set.size(), 2);
        QCOMPARE(set.join(u","_qs), u"a,c"_qs);

        OrderedSet<QString> emptySet;
        emptySet.intersect({u"a"_qs}).intersect({u"c"_qs});;
        QVERIFY(emptySet.isEmpty());
    }

    void testIsEmpty() const
    {
        const OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        QVERIFY(!set.isEmpty());

        const OrderedSet<QString> emptySet;
        QVERIFY(emptySet.isEmpty());
    }

    void testJoin() const
    {
        const OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        QCOMPARE(set.join(u","_qs), u"a,b,c"_qs);

        const OrderedSet<QString> emptySet;
        QCOMPARE(emptySet.join(u","_qs), u""_qs);
    }

    void testRemove() const
    {
        OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        QVERIFY(!set.remove(u"z"_qs));
        QCOMPARE(set.join(u","_qs), u"a,b,c"_qs);
        QVERIFY(set.remove(u"b"_qs));
        QCOMPARE(set.join(u","_qs), u"a,c"_qs);
        QVERIFY(set.remove(u"a"_qs));
        QCOMPARE(set.join(u","_qs), u"c"_qs);
        QVERIFY(set.remove(u"c"_qs));
        QVERIFY(set.isEmpty());

        OrderedSet<QString> emptySet;
        QVERIFY(!emptySet.remove(u"a"_qs));
        QVERIFY(emptySet.isEmpty());
    }

    void testUnite() const
    {
        const OrderedSet<QString> newData1 {u"z"_qs};
        const OrderedSet<QString> newData2 {u"y"_qs};

        OrderedSet<QString> set {u"a"_qs, u"b"_qs, u"c"_qs};
        set.unite(newData1);
        QCOMPARE(set.join(u","_qs), u"a,b,c,z"_qs);
        set.unite(newData2);
        QCOMPARE(set.join(u","_qs), u"a,b,c,y,z"_qs);

        OrderedSet<QString> emptySet;
        emptySet.unite(newData1).unite(newData2);
        QCOMPARE(emptySet.join(u","_qs), u"y,z"_qs);
    }
};

QTEST_APPLESS_MAIN(TestOrderedSet)
#include "testorderedset.moc"
