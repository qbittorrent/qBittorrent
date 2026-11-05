/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Alvee Ahnaf
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

#include <csignal>
#include <cstdio>
#include <unistd.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTest>
#include <QTimer>

#include "app/signalhandler.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    QByteArray readLine(QProcess &process)
    {
        QElapsedTimer timer;
        timer.start();
        while (!process.canReadLine())
        {
            const int remaining = 5000 - static_cast<int>(timer.elapsed());
            if ((remaining <= 0) || !process.waitForReadyRead(remaining))
                return {};
        }
        return process.readLine();
    }
}

class TestSignalHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestSignalHandler)

public:
    TestSignalHandler() = default;

private slots:
    void testNormalExit_data() const
    {
        QTest::addColumn<int>("firstSignal");
        QTest::addColumn<int>("shutdownSignal");
        QTest::addColumn<bool>("ignoreSighup");

        QTest::newRow("SIGINT") << SIGINT << 0 << false;
        QTest::newRow("SIGTERM") << SIGTERM << 0 << false;
        QTest::newRow("SIGHUP") << SIGHUP << 0 << false;
        QTest::newRow("SIGTERM followed by SIGHUP") << SIGTERM << SIGHUP << false;
        QTest::newRow("SIGHUP followed by SIGTERM") << SIGHUP << SIGTERM << false;
        QTest::newRow("Inherited ignored SIGHUP") << SIGTERM << 0 << true;
    }

    void testNormalExit() const
    {
        QFETCH(const int, firstSignal);
        QFETCH(const int, shutdownSignal);
        QFETCH(const bool, ignoreSighup);

        QProcess process;
        QStringList arguments {u"--signal-handler-helper"_s};
        if (ignoreSighup)
        {
            process.setChildProcessModifier([] { signal(SIGHUP, SIG_IGN); });
            arguments.append(u"--inherited-ignored-sighup"_s);
        }
        process.start(QCoreApplication::applicationFilePath(), arguments);
        QVERIFY(process.waitForStarted());
        QCOMPARE(readLine(process), "ready\n");

        QCOMPARE(kill(static_cast<pid_t>(process.processId()), firstSignal), 0);
        QCOMPARE(readLine(process), "saving\n");

        if (shutdownSignal != 0)
        {
            process.readAllStandardError();
            QCOMPARE(kill(static_cast<pid_t>(process.processId()), shutdownSignal), 0);
            process.setReadChannel(QProcess::StandardError);
            QVERIFY(readLine(process).startsWith("Catching signal: "));
            process.setReadChannel(QProcess::StandardOutput);
        }

        // Let the helper finish only after any second signal was handled.
        QCOMPARE(process.write("\n"), 1);
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QCOMPARE(process.readAllStandardOutput(), "saved\n");
        if (ignoreSighup)
            QVERIFY(!process.readAllStandardError().contains("Catching signal: SIGHUP"));
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(u"--signal-handler-helper"_s))
    {
        registerSignalHandlers();
        if (app.arguments().contains(u"--inherited-ignored-sighup"_s))
            raise(SIGHUP);
        QTimer::singleShot(0, &app, []
        {
            std::puts("ready");
            std::fflush(stdout);
        });
        const int result = app.exec();

        // Model cleanup after the event loop exits, such as saving resume data.
        std::puts("saving");
        std::fflush(stdout);
        if (std::getchar() != '\n')
            return 1;
        std::puts("saved");
        return result;
    }

    TestSignalHandler test;
    return QTest::qExec(&test, argc, argv);
}

#include "testsignalhandler.moc"
