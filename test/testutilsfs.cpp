/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  ExcitedHumvee
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

#include <QDir>
#include <QObject>
#include <QString>
#include <QTest>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"

class TestUtilsFs final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestUtilsFs)

public:
    TestUtilsFs() = default;

private slots:
    void testExpandTilde_data() const;
    void testExpandTilde() const;
};

void TestUtilsFs::testExpandTilde_data() const
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    
    const QString homeDir = QDir::homePath();
    
    QTest::newRow("tilde only") << u"~"_s << homeDir;
    QTest::newRow("tilde with path") << u"~/Downloads"_s << QString(homeDir + u"/Downloads"_s);
    QTest::newRow("tilde with nested path") << u"~/Documents/qBittorrent"_s << QString(homeDir + u"/Documents/qBittorrent"_s);
    QTest::newRow("absolute path") << u"/absolute/path"_s << u"/absolute/path"_s;
    QTest::newRow("relative path") << u"relative/path"_s << u"relative/path"_s;
    QTest::newRow("empty path") << u""_s << u""_s;
}

void TestUtilsFs::testExpandTilde() const
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    
    const Path inputPath(input);
    const Path expandedPath = Utils::Fs::expandTilde(inputPath);
    
    QCOMPARE(expandedPath.data(), expected);
}

QTEST_APPLESS_MAIN(TestUtilsFs)
#include "testutilsfs.moc"
