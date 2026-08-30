/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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
#include <unordered_map>

#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTest>

#include "base/utils/dict.h"

using namespace Qt::Literals::StringLiterals;

class TestUtilsDict final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsDict)

public:
    TestUtilsDict() = default;

private slots:
    void testDictConcept() const
    {
        static_assert(Utils::Dict::Dict<QHash<QString, int>>);
        static_assert(Utils::Dict::Dict<QMap<QString, int>>);

        static_assert(!Utils::Dict::Dict<QSet<QString>>); // not an associative container

        static_assert(!Utils::Dict::Dict<std::map<QString, int>>); // not a Qt associative container
        static_assert(!Utils::Dict::Dict<std::unordered_map<QString, int>>); // not a Qt associative container
    }

    void testDictGet() const
    {
        {
            const QHash<QString, int> data =
            {
                {u"key1"_s, 1},
                {u"key2"_s, 2},
                {u"key3"_s, 3},
                {u"key4"_s, 4},
                {u"key5"_s, 5}
            };

            QVERIFY(data.contains(u"key1"_s));
            const std::optional<int> val1 = Utils::Dict::get(data, u"key1"_s);
            QVERIFY(val1.has_value());
            QCOMPARE(*val1, 1);

            QVERIFY(data.contains(u"key2"_s));
            const std::optional<int> val2 = Utils::Dict::get(data, u"key2"_s);
            QVERIFY(val2.has_value());
            QCOMPARE(*val2, 2);

            QVERIFY(!data.contains(u"key0"_s));
            const std::optional<int> val0 = Utils::Dict::get(data, u"key0"_s);
            QVERIFY(!val0.has_value());
        }

        {
            const QMap<QString, int> data =
            {
                {u"key1"_s, 1},
                {u"key2"_s, 2},
                {u"key3"_s, 3},
                {u"key4"_s, 4},
                {u"key5"_s, 5}
            };

            QVERIFY(data.contains(u"key1"_s));
            const std::optional<int> val1 = Utils::Dict::get(data, u"key1"_s);
            QVERIFY(val1.has_value());
            QCOMPARE(*val1, 1);

            QVERIFY(data.contains(u"key2"_s));
            const std::optional<int> val2 = Utils::Dict::get(data, u"key2"_s);
            QVERIFY(val2.has_value());
            QCOMPARE(*val2, 2);

            QVERIFY(!data.contains(u"key0"_s));
            const std::optional<int> val0 = Utils::Dict::get(data, u"key0"_s);
            QVERIFY(!val0.has_value());
        }
    }

    void testDictTake() const
    {
        {
            QHash<QString, int> data =
            {
                {u"key1"_s, 1},
                {u"key2"_s, 2},
                {u"key3"_s, 3},
                {u"key4"_s, 4},
                {u"key5"_s, 5}
            };

            QVERIFY(data.contains(u"key1"_s));
            const std::optional<int> val1 = Utils::Dict::take(data, u"key1"_s);
            QVERIFY(val1.has_value());
            QCOMPARE(*val1, 1);
            QVERIFY(!data.contains(u"key1"_s));

            QVERIFY(data.contains(u"key2"_s));
            const std::optional<int> val2 = Utils::Dict::take(data, u"key2"_s);
            QVERIFY(val2.has_value());
            QCOMPARE(*val2, 2);
            QVERIFY(!data.contains(u"key2"_s));

            QVERIFY(!data.contains(u"key0"_s));
            const std::optional<int> val0 = Utils::Dict::take(data, u"key0"_s);
            QVERIFY(!val0.has_value());
            QVERIFY(!data.contains(u"key0"_s));
        }

        {
            QMap<QString, int> data =
            {
                {u"key1"_s, 1},
                {u"key2"_s, 2},
                {u"key3"_s, 3},
                {u"key4"_s, 4},
                {u"key5"_s, 5}
            };

            QVERIFY(data.contains(u"key1"_s));
            const std::optional<int> val1 = Utils::Dict::take(data, u"key1"_s);
            QVERIFY(val1.has_value());
            QCOMPARE(*val1, 1);
            QVERIFY(!data.contains(u"key1"_s));

            QVERIFY(data.contains(u"key2"_s));
            const std::optional<int> val2 = Utils::Dict::take(data, u"key2"_s);
            QVERIFY(val2.has_value());
            QCOMPARE(*val2, 2);
            QVERIFY(!data.contains(u"key2"_s));

            QVERIFY(!data.contains(u"key0"_s));
            const std::optional<int> val0 = Utils::Dict::take(data, u"key0"_s);
            QVERIFY(!val0.has_value());
            QVERIFY(!data.contains(u"key0"_s));
        }
    }
};

QTEST_APPLESS_MAIN(TestUtilsDict)
#include "testutilsdict.moc"
