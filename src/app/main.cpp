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
#include <QThread>

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
Q_IMPORT_PLUGIN(QICOPlugin)
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
#include "base/profile.h"
#include "base/utils/misc.h"
#include "base/preferences.h"
#include "cmdoptions.h"

#include "upgrade.h"

// Signal handlers
#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
void sigNormalHandler(int signum);
void sigAbnormalHandler(int signum);
// sys_signame[] is only defined in BSD
const char *sysSigName[] = {
    "", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
    "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
    "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
    "SIGPWR", "SIGUNUSED"
};
#endif

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char* str);
#endif

void displayVersion();
bool userAgreesWithLegalNotice();
void displayBadArgMessage(const QString &message);

#if !defined(DISABLE_GUI)
void showSplashScreen();

#if defined(Q_OS_UNIX)
void setupDpi();
#endif  // Q_OS_UNIX
#endif  // DISABLE_GUI

// Main
int main(int argc, char *argv[])
{
    // We must save it here because QApplication constructor may change it
    bool isOneArg = (argc == 2);

#ifdef Q_OS_MAC
    // On macOS 10.12 Sierra, Apple changed the behaviour of CFPreferencesSetValue() https://bugreports.qt.io/browse/QTBUG-56344
    // Due to this, we have to move from native plist to IniFormat
    macMigratePlists();
#endif

#if !defined(DISABLE_GUI) && defined(Q_OS_UNIX)
    setupDpi();
#endif

    try {
        // Create Application
        QString appId = QLatin1String("qBittorrent-") + Utils::Misc::getUserIDString();
        QScopedPointer<Application> app(new Application(appId, argc, argv));

#ifndef DISABLE_GUI
        // after the application object creation because we need a profile to be set already
        // for the migration
        migrateRSS();
#endif

        const QBtCommandLineParameters params = app->commandLineArgs();

        if (!params.unknownParameter.isEmpty()) {
            throw CommandLineParameterError(QObject::tr("%1 is an unknown command line parameter.",
                                                        "--random-parameter is an unknown command line parameter.")
                                                        .arg(params.unknownParameter));
        }
#ifndef Q_OS_WIN
        if (params.showVersion) {
            if (isOneArg) {
                displayVersion();
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                     .arg(QLatin1String("-v (or --version)")));
        }
#endif
        if (params.showHelp) {
            if (isOneArg) {
                displayUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(QLatin1String("-h (or --help)")));
        }

        // Set environment variable
        if (!qputenv("QBITTORRENT", QBT_VERSION))
            std::cerr << "Couldn't set environment variable...\n";

#ifndef DISABLE_GUI
        if (!userAgreesWithLegalNotice())
            return EXIT_SUCCESS;
#else
        if (!params.shouldDaemonize
            && isatty(fileno(stdin))
            && isatty(fileno(stdout))
            && !userAgreesWithLegalNotice())
            return EXIT_SUCCESS;
#endif

        // Check if qBittorrent is already running for this user
        if (app->isRunning()) {
#ifdef DISABLE_GUI
            if (params.shouldDaemonize) {
                throw CommandLineParameterError(QObject::tr("You cannot use %1: qBittorrent is already running for this user.")
                                     .arg(QLatin1String("-d (or --daemon)")));
            }
            else
#endif
            qDebug("qBittorrent is already running for this user.");

            QThread::msleep(300);
            app->sendParams(params.paramList());

            return EXIT_SUCCESS;
        }

#if defined(Q_OS_WIN)
        // This affects only Windows apparently and Qt5.
        // When QNetworkAccessManager is instantiated it regularly starts polling
        // the network interfaces to see what's available and their status.
        // This polling creates jitter and high ping with wifi interfaces.
        // So here we disable it for lack of better measure.
        // It will also spew this message in the console: QObject::startTimer: Timers cannot have negative intervals
        // For more info see:
        // 1. https://github.com/qbittorrent/qBittorrent/issues/4209
        // 2. https://bugreports.qt.io/browse/QTBUG-40332
        // 3. https://bugreports.qt.io/browse/QTBUG-46015

        qputenv("QT_BEARER_POLL_TIMEOUT", QByteArray::number(-1));
#endif

#if defined(Q_OS_MAC)
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        QByteArray path = "/usr/local/bin:";
        path += qgetenv("PATH");
        qputenv("PATH", path.constData());
        
        // On OS X the standard is to not show icons in the menus
        app->setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

#ifndef DISABLE_GUI
        if (!upgrade()) return EXIT_FAILURE;
#else
        if (!upgrade(!params.shouldDaemonize
                     && isatty(fileno(stdin))
                     && isatty(fileno(stdout)))) return EXIT_FAILURE;
#endif
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
        if (!(params.noSplash || Preferences::instance()->isSplashScreenDisabled()))
            showSplashScreen();
#endif

#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
        signal(SIGINT, sigNormalHandler);
        signal(SIGTERM, sigNormalHandler);
        signal(SIGABRT, sigAbnormalHandler);
        signal(SIGSEGV, sigAbnormalHandler);
#endif

        return app->exec(params.paramList());
    }
    catch (CommandLineParameterError &er) {
        displayBadArgMessage(er.messageForUser());
        return EXIT_FAILURE;
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char* str)
{
    const size_t strLen = strlen(str);
    if (write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen)) {
        auto dummy = write(STDOUT_FILENO, str, strLen);
        Q_UNUSED(dummy);
    }
}
#endif

#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
void sigNormalHandler(int signum)
{
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char str1[] = "Catching signal: ";
    const char *sigName = sysSigName[signum];
    const char str2[] = "\nExiting cleanly\n";
    reportToUser(str1);
    reportToUser(sigName);
    reportToUser(str2);
#endif // !defined Q_OS_WIN && !defined Q_OS_HAIKU
    signal(signum, SIG_DFL);
    qApp->exit();  // unsafe, but exit anyway
}

void sigAbnormalHandler(int signum)
{
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char str1[] = "\n\n*************************************************************\nCatching signal: ";
    const char *sigName = sysSigName[signum];
    const char str2[] = "\nPlease file a bug report at http://bug.qbittorrent.org and provide the following information:\n\n"
    "qBittorrent version: " QBT_VERSION "\n";
    reportToUser(str1);
    reportToUser(sigName);
    reportToUser(str2);
    print_stacktrace();  // unsafe
#endif // !defined Q_OS_WIN && !defined Q_OS_HAIKU
#ifdef STACKTRACE_WIN
    StraceDlg dlg;  // unsafe
    dlg.setStacktraceString(straceWin::getBacktrace());
    dlg.exec();
#endif // STACKTRACE_WIN
    signal(signum, SIG_DFL);
    raise(signum);
}
#endif // defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)

#if !defined(DISABLE_GUI)
void showSplashScreen()
{
    QPixmap splash_img(":/icons/skin/splash.png");
    QPainter painter(&splash_img);
    QString version = QBT_VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
    QSplashScreen *splash = new QSplashScreen(splash_img);
    splash->show();
    QTimer::singleShot(1500, splash, SLOT(deleteLater()));
    qApp->processEvents();
}

#if defined(Q_OS_UNIX)
void setupDpi()
{
    if (qgetenv("QT_AUTO_SCREEN_SCALE_FACTOR").isEmpty())
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
}
#endif  // Q_OS_UNIX
#endif  // DISABLE_GUI

void displayVersion()
{
    std::cout << qPrintable(qApp->applicationName()) << " " << QBT_VERSION << std::endl;
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
