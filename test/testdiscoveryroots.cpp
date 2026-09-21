/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTest>

#include "base/discoveryroots.h"
#include "base/global.h"
#include "base/path.h"

class TestDiscoveryRoots final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestDiscoveryRoots)

public:
    TestDiscoveryRoots() = default;

private slots:
    void testRoundTripPreservesOrderAndOptions() const
    {
        const QList<DiscoveryRoot> roots
        {
            {.path = Path(u"/video"_s), .options = {.recursive = true}},
            {.path = Path(u"/music"_s), .options = {.recursive = false}}
        };

        QCOMPARE(parseDiscoveryRoots(serializeDiscoveryRoots(roots)), roots);
    }

    void testSerializedShape() const
    {
        const QList<DiscoveryRoot> roots
        {
            {.path = Path(u"/music"_s), .options = {.recursive = true}}
        };

        const QJsonArray jsonArray = serializeDiscoveryRoots(roots);
        QCOMPARE(jsonArray.size(), 1);

        const QJsonObject jsonObj = jsonArray.at(0).toObject();
        QCOMPARE(jsonObj.keys(), (QStringList {u"path"_s, u"recursive"_s}));
        QCOMPARE(jsonObj.value(u"path"_s).toString(), u"/music"_s);
        QCOMPARE(jsonObj.value(u"recursive"_s).toBool(), true);
    }

    void testRecursiveDefaultsFalse() const
    {
        const QJsonArray jsonArray {QJsonObject {{u"path"_s, u"/music"_s}}};

        const QList<DiscoveryRoot> roots = parseDiscoveryRoots(jsonArray);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.at(0).path, Path(u"/music"_s));
        QCOMPARE(roots.at(0).options.recursive, false);
    }

    void testEmptyPathDropped() const
    {
        const QJsonArray jsonArray
        {
            QJsonObject {{u"path"_s, u""_s}},
            QJsonObject {{u"path"_s, u"/music"_s}}
        };

        const QList<DiscoveryRoot> roots = parseDiscoveryRoots(jsonArray);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.at(0).path, Path(u"/music"_s));
    }

    void testRelativePathDropped() const
    {
        const QJsonArray jsonArray
        {
            QJsonObject {{u"path"_s, u"music"_s}},
            QJsonObject {{u"path"_s, u"/video"_s}}
        };

        const QList<DiscoveryRoot> roots = parseDiscoveryRoots(jsonArray);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.at(0).path, Path(u"/video"_s));
    }

    void testNonObjectDropped() const
    {
        const QJsonArray jsonArray
        {
            u"/music"_s,
            QJsonObject {{u"path"_s, u"/video"_s}}
        };

        const QList<DiscoveryRoot> roots = parseDiscoveryRoots(jsonArray);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.at(0).path, Path(u"/video"_s));
    }

    void testDuplicateKeepsFirst() const
    {
        const QJsonArray jsonArray
        {
            QJsonObject {{u"path"_s, u"/music"_s}, {u"recursive"_s, true}},
            QJsonObject {{u"path"_s, u"/music"_s}, {u"recursive"_s, false}}
        };

        const QList<DiscoveryRoot> roots = parseDiscoveryRoots(jsonArray);
        QCOMPARE(roots.size(), 1);
        QCOMPARE(roots.at(0).options.recursive, true);
    }

    void testValueEquality() const
    {
        const DiscoveryRoot root {.path = Path(u"/music"_s), .options = {.recursive = true}};

        const DiscoveryRoot same {.path = Path(u"/music"_s), .options = {.recursive = true}};
        const DiscoveryRoot differentPath {.path = Path(u"/video"_s), .options = {.recursive = true}};
        const DiscoveryRoot differentOptions {.path = Path(u"/music"_s), .options = {.recursive = false}};

        QCOMPARE(root, same);
        QVERIFY(root != differentPath);
        QVERIFY(root != differentOptions);
    }
};

QTEST_APPLESS_MAIN(TestDiscoveryRoots)
#include "testdiscoveryroots.moc"
