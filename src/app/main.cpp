/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QDebug>
#include <QScopedPointer>

#ifndef DISABLE_GUI
// GUI-only includes
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSplashScreen>
#ifdef QBT_STATIC_QT
#include <QtPlugin>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
Q_IMPORT_PLUGIN(QICOPlugin)
#else
Q_IMPORT_PLUGIN(qico)
#endif
#endif // QBT_STATIC_QT

#else
// NoGUI-only includes
#include <cstdio>
#ifdef Q_OS_UNIX
#include "unistd.h"
#endif
#endif // DISABLE_GUI

#ifdef Q_OS_UNIX
#include <signal.h>
#include <execinfo.h>
#include "stacktrace.h"
#endif // Q_OS_UNIX

#ifdef STACKTRACE_WIN
#include <signal.h>
#include "stacktrace_win.h"
#include "stacktrace_win_dlg.h"
#endif //STACKTRACE_WIN

#include <cstdlib>
#include <iostream>
#include "application.h"
#include "core/utils/misc.h"
#include "core/preferences.h"

#include "upgrade.h"

// Signal handlers
#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
void sigintHandler(int);
void sigtermHandler(int);
void sigsegvHandler(int);
void sigabrtHandler(int);
#endif

struct QBtCommandLineParameters
{
    bool showHelp;
#ifndef Q_OS_WIN
    bool showVersion;
#endif
#ifndef DISABLE_GUI
    bool noSplash;
#else
    bool shouldDaemonize;
#endif
    int webUiPort;
    QStringList torrents;
    QString unknownParameter;

    QBtCommandLineParameters()
        : showHelp(false)
#ifndef Q_OS_WIN
        , showVersion(false)
#endif
#ifndef DISABLE_GUI
        , noSplash(Preferences::instance()->isSplashScreenDisabled())
#else
        , shouldDaemonize(false)
#endif
        , webUiPort(Preferences::instance()->getWebUiPort())
    {
    }
};

#ifndef DISABLE_GUI
void showSplashScreen();
#endif
void displayVersion();
void displayUsage(const QString &prg_name);
bool userAgreesWithLegalNotice();
void displayBadArgMessage(const QString &message);
QBtCommandLineParameters parseCommandLine();

// Main
int main(int argc, char *argv[])
{
    // We must save it here because QApplication constructor may change it
    bool isOneArg = (argc == 2);

    // Create Application
    QString appId = QLatin1String("qBittorrent-") + Utils::Misc::getUserIDString();
    QScopedPointer<Application> app(new Application(appId, argc, argv));

    const QBtCommandLineParameters params = parseCommandLine();

    if (!params.unknownParameter.isEmpty()) {
        displayBadArgMessage(QObject::tr("%1 is an unknown command line parameter.", "--random-parameter is an unknown command line parameter.")
                             .arg(params.unknownParameter));
        return EXIT_FAILURE;
    }

#ifndef Q_OS_WIN
    if (params.showVersion) {
        if (isOneArg) {
            displayVersion();
            return EXIT_SUCCESS;
        }
        else {
            displayBadArgMessage(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(QLatin1String("-v (or --version)")));
            return EXIT_FAILURE;
        }
    }
#endif

    if (params.showHelp) {
        if (isOneArg) {
            displayUsage(argv[0]);
            return EXIT_SUCCESS;
        }
        else {
            displayBadArgMessage(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(QLatin1String("-h (or --help)")));
            return EXIT_FAILURE;
        }
    }

    if ((params.webUiPort > 0) && (params.webUiPort <= 65535)) {
        Preferences::instance()->setWebUiPort(params.webUiPort);
    }
    else {
        displayBadArgMessage(QObject::tr("%1 must specify the correct port (1 to 65535).")
                             .arg(QLatin1String("--webui-port")));
        return EXIT_FAILURE;
    }

    // Set environment variable
    if (!qputenv("QBITTORRENT", QByteArray(VERSION)))
        std::cerr << "Couldn't set environment variable...\n";

    if (!userAgreesWithLegalNotice())
        return EXIT_SUCCESS;

    // Check if qBittorrent is already running for this user
    if (app->isRunning()) {
#ifdef DISABLE_GUI
        if (params.shouldDaemonize) {
            displayBadArgMessage(QObject::tr("You cannot use %1: qBittorrent is already running for this user.")
                                 .arg(QLatin1String("-d (or --daemon)")));
            return EXIT_FAILURE;
        }
        else
#endif
        qDebug("qBittorrent is already running for this user.");

        Utils::Misc::msleep(300);
        app->sendParams(params.torrents);

        return EXIT_SUCCESS;
    }

    if (!upgrade()) return EXIT_FAILURE;

    srand(time(0));
#ifdef DISABLE_GUI
    if (params.shouldDaemonize) {
        app.reset(); // Destroy current application
        if ((daemon(1, 0) == 0)) {
            app.reset(new Application(appId, argc, argv));
            if (app->isRunning()) {
                // Another instance had time to start.
                return EXIT_FAILURE;
            }
        }
        else {
            qCritical("Something went wrong while daemonizing, exiting...");
            return EXIT_FAILURE;
        }
    }
#else
    if (!params.noSplash)
        showSplashScreen();
#endif

#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
    signal(SIGABRT, sigabrtHandler);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, sigintHandler);
    signal(SIGSEGV, sigsegvHandler);
#endif

    return app->exec(params.torrents);
}

QBtCommandLineParameters parseCommandLine()
{
    QBtCommandLineParameters result;
    QStringList appArguments = qApp->arguments();

    for (int i = 1; i < appArguments.size(); ++i) {
        const QString& arg = appArguments[i];

        if ((arg.startsWith("--") && !arg.endsWith(".torrent")) ||
            (arg.startsWith("-") && arg.size() == 2)) {
            //Parse known parameters
            if ((arg == QLatin1String("-h")) || (arg == QLatin1String("--help"))) {
                result.showHelp = true;
            }
#ifndef Q_OS_WIN
            else if ((arg == QLatin1String("-v")) || (arg == QLatin1String("--version"))) {
                result.showVersion = true;
            }
#endif
            else if (arg.startsWith(QLatin1String("--webui-port="))) {
                QStringList parts = arg.split(QLatin1Char('='));
                if (parts.size() == 2)
                    result.webUiPort = parts.last().toInt();
            }
#ifndef DISABLE_GUI
            else if (arg == QLatin1String("--no-splash")) {
                result.noSplash = true;
            }
#else
            else if ((arg == QLatin1String("-d")) || (arg == QLatin1String("--daemon"))) {
                result.shouldDaemonize = true;
            }
#endif
            else {
                //Unknown argument
                result.unknownParameter = arg;
                break;
            }
        }
        else {
            QFileInfo torrentPath;
            torrentPath.setFile(arg);

            if (torrentPath.exists())
                result.torrents += torrentPath.absoluteFilePath();
            else
                result.torrents += arg;
        }
    }

    return result;
}

#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
void sigintHandler(int)
{
    signal(SIGINT, 0);
    qDebug("Catching SIGINT, exiting cleanly");
    qApp->exit();
}

void sigtermHandler(int)
{
    signal(SIGTERM, 0);
    qDebug("Catching SIGTERM, exiting cleanly");
    qApp->exit();
}

void sigsegvHandler(int)
{
    signal(SIGABRT, 0);
    signal(SIGSEGV, 0);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    std::cerr << "\n\n*************************************************************\n";
    std::cerr << "Catching SIGSEGV, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
    std::cerr << "qBittorrent version: " << VERSION << std::endl;
    print_stacktrace();
#else
#ifdef STACKTRACE_WIN
    StraceDlg dlg;
    dlg.setStacktraceString(straceWin::getBacktrace());
    dlg.exec();
#endif
#endif
    raise(SIGSEGV);
}

void sigabrtHandler(int)
{
    signal(SIGABRT, 0);
    signal(SIGSEGV, 0);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    std::cerr << "\n\n*************************************************************\n";
    std::cerr << "Catching SIGABRT, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
    std::cerr << "qBittorrent version: " << VERSION << std::endl;
    print_stacktrace();
#else
#ifdef STACKTRACE_WIN
    StraceDlg dlg;
    dlg.setStacktraceString(straceWin::getBacktrace());
    dlg.exec();
#endif
#endif
    raise(SIGABRT);
}
#endif

#ifndef DISABLE_GUI
void showSplashScreen()
{
    QPixmap splash_img(":/icons/skin/splash.png");
    QPainter painter(&splash_img);
    QString version = VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
    QSplashScreen *splash = new QSplashScreen(splash_img, Qt::WindowStaysOnTopHint);
    QTimer::singleShot(1500, splash, SLOT(deleteLater()));
    splash->show();
    qApp->processEvents();
}
#endif

void displayVersion()
{
    std::cout << qPrintable(qApp->applicationName()) << " " << VERSION << std::endl;
}

QString makeUsage(const QString &prg_name)
{
    QString text;

    text += QObject::tr("Usage:") + QLatin1Char('\n');
#ifndef Q_OS_WIN
    text += QLatin1Char('\t') + prg_name + QLatin1String(" (-v | --version)") + QLatin1Char('\n');
#endif
    text += QLatin1Char('\t') + prg_name + QLatin1String(" (-h | --help)") + QLatin1Char('\n');
    text += QLatin1Char('\t') + prg_name
            + QLatin1String(" [--webui-port=<port>]")
#ifndef DISABLE_GUI
            + QLatin1String(" [--no-splash]")
#else
            + QLatin1String(" [-d | --daemon]")
#endif
            + QLatin1String("[(<filename> | <url>)...]") + QLatin1Char('\n');
    text += QObject::tr("Options:") + QLatin1Char('\n');
#ifndef Q_OS_WIN
    text += QLatin1String("\t-v | --version\t\t") + QObject::tr("Displays program version") + QLatin1Char('\n');
#endif
    text += QLatin1String("\t-h | --help\t\t") + QObject::tr("Displays this help message") + QLatin1Char('\n');
    text += QLatin1String("\t--webui-port=<port>\t")
            + QObject::tr("Changes the Web UI port (current: %1)").arg(QString::number(Preferences::instance()->getWebUiPort()))
            + QLatin1Char('\n');
#ifndef DISABLE_GUI
    text += QLatin1String("\t--no-splash\t\t") + QObject::tr("Disable splash screen") + QLatin1Char('\n');
#else
    text += QLatin1String("\t-d | --daemon\t\t") + QObject::tr("Run in daemon-mode (background)") + QLatin1Char('\n');
#endif
    text += QLatin1String("\tfiles or urls\t\t") + QObject::tr("Downloads the torrents passed by the user");

    return text;
}

void displayUsage(const QString& prg_name)
{
#ifndef Q_OS_WIN
    std::cout << qPrintable(makeUsage(prg_name)) << std::endl;
#else
    QMessageBox msgBox(QMessageBox::Information, QObject::tr("Help"), makeUsage(prg_name), QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
#endif
}

void displayBadArgMessage(const QString& message)
{
    QString help = QObject::tr("Run application with -h option to read about command line parameters.");
#ifdef Q_OS_WIN
    QMessageBox msgBox(QMessageBox::Critical, QObject::tr("Bad command line"),
                       message + QLatin1Char('\n') + help, QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
#else
    std::cerr << qPrintable(QObject::tr("Bad command line: "));
    std::cerr << qPrintable(message) << std::endl;
    std::cerr << qPrintable(help) << std::endl;
#endif
}

bool userAgreesWithLegalNotice()
{
    Preferences* const pref = Preferences::instance();
    if (pref->getAcceptedLegal()) // Already accepted once
        return true;

#ifdef DISABLE_GUI
    std::cout << std::endl << "*** " << qPrintable(QObject::tr("Legal Notice")) << " ***" << std::endl;
    std::cout << qPrintable(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued.")) << std::endl << std::endl;
    std::cout << qPrintable(QObject::tr("Press %1 key to accept and continue...").arg("'y'")) << std::endl;
    char ret = getchar(); // Read pressed key
    if (ret == 'y' || ret == 'Y') {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#else
    QMessageBox msgBox;
    msgBox.setText(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(QObject::tr("Legal notice"));
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
    QAbstractButton *agree_button = msgBox.addButton(QObject::tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
    if (msgBox.clickedButton() == agree_button) {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#endif

    return false;
}
