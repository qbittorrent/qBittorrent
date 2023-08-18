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

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <QObject>
#include <QTest>

#include "base/algorithm.h"
#include "base/global.h"

class TestAlgorithm final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestAlgorithm)

public:
    TestAlgorithm() = default;

private slots:
    void testHasMappedType() const
    {
        static_assert(Algorithm::HasMappedType<std::map<bool, bool>>);
        static_assert(Algorithm::HasMappedType<std::unordered_map<bool, bool>>);
        static_assert(Algorithm::HasMappedType<QHash<bool, bool>>);
        static_assert(Algorithm::HasMappedType<QMap<bool, bool>>);

        static_assert(!Algorithm::HasMappedType<std::set<bool>>);
        static_assert(!Algorithm::HasMappedType<std::unordered_set<bool>>);
        static_assert(!Algorithm::HasMappedType<QSet<bool>>);
    }

    void testMappedTypeRemoveIf() const
    {
        {
            QMap<int, char> data =
            {
                {0, 'a'},
                {1, 'b'},
                {2, 'c'},
                {3, 'b'},
                {4, 'd'}
            };
            Algorithm::removeIf(data, []([[maybe_unused]] const int key, const char value)
            {
                return (value == 'b');
            });
            QCOMPARE(data.size(), 3);
            QCOMPARE(data.value(0), 'a');
            QVERIFY(!data.contains(1));
            QCOMPARE(data.value(2), 'c');
            QVERIFY(!data.contains(3));
            QCOMPARE(data.value(4), 'd');
        }
        {
            QHash<int, char> data;
            Algorithm::removeIf(data, []([[maybe_unused]] const int key, const char value)
            {
                return (value == 'b');
            });
            QVERIFY(data.empty());
        }
    }

    void testSorted() const
    {
        const QStringList list = {u"c"_s, u"b"_s, u"a"_s};
        const QStringList sortedList = {u"a"_s, u"b"_s, u"c"_s};
        QCOMPARE(Algorithm::sorted(list), sortedList);
    }
};

QTEST_APPLESS_MAIN(TestAlgorithm)
#include "testalgorithm.moc"
