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

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QtSystemDetection>

#include "base/bittorrent/filesearcher.h"
#include "base/global.h"
#include "base/path.h"

namespace
{
    QString folded(const QString &name)
    {
        return (Path::CASE_SENSITIVITY == Qt::CaseInsensitive) ? name.toCaseFolded() : name;
    }

    Path fixtureRoot()
    {
        return Path(QString::fromUtf8(__FILE__)).parentPath() / Path(u"testdata/filesearcher"_s);
    }
}

class TestBittorrentSubdirectories final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestBittorrentSubdirectories)

public:
    TestBittorrentSubdirectories() = default;

private slots:
    void testListsEveryDirectoryInTree() const
    {
        const Path root = fixtureRoot() / Path(u"library"_s);
        const SubdirectoryMap map = enumerateSubdirectories(root);

        const QList<QString> expectedKeys {folded(u"Album One"_s), folded(u"Album Three"_s)
                , folded(u"album two"_s), folded(u"Artist"_s), folded(u"Disc 1"_s)
                , folded(u"Genre"_s), folded(u"Other"_s)};
        QCOMPARE(map.keys().size(), expectedKeys.size());
        for (const QString &key : expectedKeys)
            QVERIFY2(map.contains(key), qPrintable(key));

        QCOMPARE(map.value(folded(u"Album One"_s)), (PathList {root / Path(u"Album One"_s)}));
        QCOMPARE(map.value(folded(u"album two"_s)), (PathList {root / Path(u"album two"_s)}));
        QCOMPARE(map.value(folded(u"Genre"_s)), (PathList {root / Path(u"Genre"_s)}));
        QCOMPARE(map.value(folded(u"Artist"_s)), (PathList {root / Path(u"Genre/Artist"_s)}));
        QCOMPARE(map.value(folded(u"Album Three"_s)), (PathList {root / Path(u"Genre/Artist/Album Three"_s)}));
        QCOMPARE(map.value(folded(u"Other"_s)), (PathList {root / Path(u"Genre/Other"_s)}));
    }

    void testRepeatedNameListsEachInPathOrder() const
    {
        const Path root = fixtureRoot() / Path(u"library"_s);
        const SubdirectoryMap map = enumerateSubdirectories(root);

        QCOMPARE(map.value(folded(u"Disc 1"_s)), (PathList {
                root / Path(u"Album One/Disc 1"_s), root / Path(u"Genre/Other/Disc 1"_s)}));
    }

    void testFilesAndRootAreNotListed() const
    {
        const Path root = fixtureRoot() / Path(u"library"_s);
        const SubdirectoryMap map = enumerateSubdirectories(root);

        QVERIFY(!map.contains(folded(u"loose.txt"_s)));
        QVERIFY(!map.contains(folded(u"alpha.txt"_s)));
        QVERIFY(!map.contains(folded(u"library"_s)));
    }

    void testHiddenDirectoriesAreNotEntered() const
    {
#ifdef Q_OS_UNIX
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const Path root {tempDir.path()};

        QDir dir {root.data()};
        QVERIFY(dir.mkdir(u"visible"_s));
        QVERIFY(dir.mkpath(u".hidden/inner"_s));

        const SubdirectoryMap map = enumerateSubdirectories(root);
        QCOMPARE(map.keys(), (QList<QString> {folded(u"visible"_s)}));
#else
        QSKIP("Requires dot-prefixed hidden directories.");
#endif
    }

    void testSymbolicLinksAreNotFollowed() const
    {
#ifdef Q_OS_UNIX
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const Path root {tempDir.path()};

        QDir dir {root.data()};
        QVERIFY(dir.mkpath(u"real/inner"_s));
        QVERIFY(QFile::link(root.data(), (root / Path(u"real/loop"_s)).data()));
        QVERIFY(QFile::link((root / Path(u"real"_s)).data(), (root / Path(u"linked"_s)).data()));

        const SubdirectoryMap map = enumerateSubdirectories(root);
        QList<QString> keys = map.keys();
        keys.sort();
        QCOMPARE(keys, (QList<QString> {folded(u"inner"_s), folded(u"real"_s)}));
        QCOMPARE(map.value(folded(u"real"_s)), (PathList {root / Path(u"real"_s)}));
#else
        QSKIP("Requires symbolic links.");
#endif
    }

    void testUnreadableBranchIsSkipped() const
    {
#ifdef Q_OS_UNIX
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const Path root {tempDir.path()};

        QDir dir {root.data()};
        QVERIFY(dir.mkpath(u"open/inside"_s));
        QVERIFY(dir.mkpath(u"closed/child"_s));
        const Path closed = root / Path(u"closed"_s);
        QVERIFY(QFile::setPermissions(closed.data(), {}));
        const auto restore = qScopeGuard([closed]
        {
            QFile::setPermissions(closed.data()
                    , (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        });

        if (QFileInfo::exists((closed / Path(u"child"_s)).data()))
            QSKIP("The running account can traverse the unreadable directory.");

        const SubdirectoryMap map = enumerateSubdirectories(root);
        QList<QString> keys = map.keys();
        keys.sort();
        QCOMPARE(keys, (QList<QString> {folded(u"closed"_s), folded(u"inside"_s), folded(u"open"_s)}));
#else
        QSKIP("Requires Unix permission semantics.");
#endif
    }

    void testAbsentRootYieldsEmptyMap() const
    {
        const SubdirectoryMap map = enumerateSubdirectories(fixtureRoot() / Path(u"absent"_s));
        QVERIFY(map.isEmpty());
    }

    void testEmptyRootYieldsEmptyMap() const
    {
        const SubdirectoryMap map = enumerateSubdirectories(Path());
        QVERIFY(map.isEmpty());
    }

#ifdef Q_OS_WIN
    void testKeysFoldCaseOnWindows() const
    {
        const Path root = fixtureRoot() / Path(u"library"_s);
        const SubdirectoryMap map = enumerateSubdirectories(root);

        QVERIFY(map.contains(u"album one"_s));
        QVERIFY(map.contains(u"album three"_s));
    }
#endif
};

QTEST_APPLESS_MAIN(TestBittorrentSubdirectories)
#include "testbittorrentsubdirectories.moc"
