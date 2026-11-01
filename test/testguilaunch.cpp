/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Andy Ye
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "gui/utils/launch.h"

using namespace Qt::Literals::StringLiterals;

class TestGuiLaunch final : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    bool writeFile(const QString &name, const QByteArray &data) const
    {
        QFile file {m_dir.filePath(name)};
        return file.open(QIODevice::WriteOnly) && (file.write(data) == data.size());
    }

    QString url(const QString &name) const
    {
        return QUrl::fromLocalFile(m_dir.filePath(name)).toString(QUrl::FullyEncoded);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(QDir().mkpath(m_dir.filePath(u"data/applications"_s)));
        QVERIFY(QDir().mkpath(m_dir.filePath(u"config"_s)));
        QVERIFY(QDir().mkpath(m_dir.filePath(u"empty"_s)));
        qputenv("XDG_DATA_HOME", m_dir.filePath(u"data"_s).toUtf8());
        qputenv("XDG_CONFIG_HOME", m_dir.filePath(u"config"_s).toUtf8());
        qputenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
        qputenv("XDG_CONFIG_DIRS", m_dir.filePath(u"empty"_s).toUtf8());
        qputenv("GIO_USE_VFS", "local");

        QString executable = QCoreApplication::applicationFilePath();
        executable.replace(u'\\', u"\\\\"_s).replace(u'"', u"\\\""_s)
            .replace(u'`', u"\\`"_s).replace(u'$', u"\\$"_s).replace(u'%', u"%%"_s);
        for (const QString &id : {u"text"_s, u"image"_s})
        {
            const QString desktop = u"[Desktop Entry]\nType=Application\nName=Launch test\nExec=\"%1\" --record %2 %3 %U\n"_s
                .arg(executable, m_dir.filePath(u"launches"_s), id);
            QVERIFY(writeFile(u"data/applications/"_s + id + u".desktop"_s, desktop.toUtf8()));
        }
        QVERIFY(writeFile(u"config/mimeapps.list"_s
            , "[Default Applications]\ntext/plain=text.desktop;\nimage/png=image.desktop;\napplication/pdf=broken.desktop;\n"));
        QVERIFY(writeFile(u"data/applications/broken.desktop"_s
            , "[Desktop Entry]\nType=Application\nName=Broken player\nExec=/bin/true %U\nPath=/qbt-nonexistent-directory\n"));
        QVERIFY(writeFile(u"broken.pdf"_s, "%PDF-1.7\n"));
        QVERIFY(writeFile(u"one.txt"_s, "first\n"));
        QVERIFY(writeFile(u"two spaces ü.txt"_s, "second\n"));
        QVERIFY(writeFile(u"picture.png"_s, QByteArray::fromBase64(
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/l9sAAAAASUVORK5CYII=")));
    }

    void testBatchByApplication()
    {
        const QStringList urls {url(u"one.txt"_s), url(u"picture.png"_s), url(u"two spaces ü.txt"_s)};
        QVERIFY(Utils::Gui::launchWithDefaultApplications(urls).isEmpty());

        const auto readLaunches = [this]()
        {
            QFile file {m_dir.filePath(u"launches"_s)};
            if (!file.open(QIODevice::ReadOnly))
                return QList<QByteArray> {};
            return file.readAll().trimmed().split('\n');
        };
        QTRY_COMPARE(readLaunches().size(), 2);
        for (const QByteArray &line : readLaunches())
        {
            const QJsonArray args = QJsonDocument::fromJson(line).array();
            if (args[0].toString() == u"text")
            {
                QCOMPARE(args.size(), 3);
                QCOMPARE(args[1].toString(), m_dir.filePath(u"one.txt"_s));
                QCOMPARE(args[2].toString(), m_dir.filePath(u"two spaces ü.txt"_s));
            }
            else
            {
                QCOMPARE(args[0].toString(), u"image"_s);
                QCOMPARE(args.size(), 2);
                QCOMPARE(args[1].toString(), m_dir.filePath(u"picture.png"_s));
            }
        }
    }

    void testLaunchFailure()
    {
        const QStringList urls {url(u"broken.pdf"_s)};
        QCOMPARE(Utils::Gui::launchWithDefaultApplications(urls), urls);
    }

    void testUnhandled()
    {
        const QStringList urls {u"qbt-unhandled-test://file"_s};
        QCOMPARE(Utils::Gui::launchWithDefaultApplications(urls), urls);
        QVERIFY(Utils::Gui::launchWithDefaultApplications({}).isEmpty());
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app {argc, argv};
    const QStringList args = app.arguments();
    if ((args.size() >= 4) && (args[1] == u"--record"))
    {
        QFile output {args[2]};
        if (!output.open(QIODevice::WriteOnly | QIODevice::Append))
            return 1;
        const QByteArray record = QJsonDocument(QJsonArray::fromStringList(args.mid(3))).toJson(QJsonDocument::Compact) + '\n';
        return (output.write(record) == record.size()) ? 0 : 1;
    }

    TestGuiLaunch test;
    return QTest::qExec(&test, argc, argv);
}

#include "testguilaunch.moc"
