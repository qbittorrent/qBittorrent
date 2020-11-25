/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QtGlobal>

#include <csignal>
#include <cstdlib>
#include <memory>

#if defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
#include <unistd.h>
#elif defined Q_OS_WIN && defined DISABLE_GUI
#include <io.h>
#endif

#include <QDebug>
#include <QThread>

#ifndef DISABLE_GUI
// GUI-only includes
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSplashScreen>
#include <QTimer>

#ifdef QBT_STATIC_QT
#include <QtPlugin>
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // QBT_STATIC_QT

#else
// NoGUI-only includes
#include <cstdio>
#endif // DISABLE_GUI

#ifdef STACKTRACE
#ifdef Q_OS_UNIX
#include "stacktrace.h"
#else
#include "stacktrace_win.h"
#ifndef DISABLE_GUI
#include "stacktracedialog.h"
#endif // DISABLE_GUI
#endif // Q_OS_UNIX
#endif //STACKTRACE

#include "base/preferences.h"
#include "base/profile.h"
#include "application.h"
#include "cmdoptions.h"
#include "upgrade.h"

#ifndef DISABLE_GUI
#include "gui/utils.h"
#endif

// Signal handlers
void sigNormalHandler(int signum);
#ifdef STACKTRACE
void sigAbnormalHandler(int signum);
#endif
// sys_signame[] is only defined in BSD
const char *const sysSigName[] =
{
#if defined(Q_OS_WIN)
    "", "", "SIGINT", "", "SIGILL", "", "SIGABRT_COMPAT", "", "SIGFPE", "",
    "", "SIGSEGV", "", "", "", "SIGTERM", "", "", "", "",
    "", "SIGBREAK", "SIGABRT", "", "", "", "", "", "", "",
    "", ""
#else
    "", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
    "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
    "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
    "SIGPWR", "SIGUNUSED"
#endif
};

#if !(defined Q_OS_WIN && !defined DISABLE_GUI) && !defined Q_OS_HAIKU
void reportToUser(const char *str);
#endif

void displayVersion();
bool userAgreesWithLegalNotice();
void displayBadArgMessage(const QString &message);

#if !defined(DISABLE_GUI)
void showSplashScreen();
#endif  // DISABLE_GUI

#if defined(Q_OS_UNIX)
void adjustFileDescriptorLimit();
#endif

// Main
int main(int argc, char *argv[])
{
#if defined(Q_OS_UNIX)
    adjustFileDescriptorLimit();
#endif

    // We must save it here because QApplication constructor may change it
    bool isOneArg = (argc == 2);

#if !defined(DISABLE_GUI) && (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    // Attribute Qt::AA_EnableHighDpiScaling must be set before QCoreApplication is created
    if (qgetenv("QT_ENABLE_HIGHDPI_SCALING").isEmpty() && qgetenv("QT_AUTO_SCREEN_SCALE_FACTOR").isEmpty())
        Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    // HighDPI scale factor policy must be set before QGuiApplication is created
    if (qgetenv("QT_SCALE_FACTOR_ROUNDING_POLICY").isEmpty())
        Application::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    try
    {
        // Create Application
        auto app = std::make_unique<Application>(argc, argv);

        const QBtCommandLineParameters params = app->commandLineArgs();
        if (!params.unknownParameter.isEmpty())
        {
            throw CommandLineParameterError(QObject::tr("%1 is an unknown command line parameter.",
                                                        "--random-parameter is an unknown command line parameter.")
                                                        .arg(params.unknownParameter));
        }
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
        if (params.showVersion)
        {
            if (isOneArg)
            {
                displayVersion();
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                     .arg(QLatin1String("-v (or --version)")));
        }
#endif
        if (params.showHelp)
        {
            if (isOneArg)
            {
                displayUsage(argv[0]);
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(QLatin1String("-h (or --help)")));
        }

        // Set environment variable
        if (!qputenv("QBITTORRENT", QBT_VERSION))
            fprintf(stderr, "Couldn't set environment variable...\n");

        const bool firstTimeUser = !Preferences::instance()->getAcceptedLegal();
        if (firstTimeUser)
        {
#ifndef DISABLE_GUI
            if (!userAgreesWithLegalNotice())
                return EXIT_SUCCESS;

#elif defined(Q_OS_WIN)
            if (_isatty(_fileno(stdin))
                && _isatty(_fileno(stdout))
                && !userAgreesWithLegalNotice())
                return EXIT_SUCCESS;
#else
            if (!params.shouldDaemonize
                && isatty(fileno(stdin))
                && isatty(fileno(stdout))
                && !userAgreesWithLegalNotice())
                return EXIT_SUCCESS;
#endif
        }

        // Check if qBittorrent is already running for this user
        if (app->isRunning())
        {
#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
            if (params.shouldDaemonize)
            {
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)) && !defined(DISABLE_GUI)
        // this is the default in Qt6
        app->setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#endif // Q_OS_WIN

#if defined(Q_OS_MACOS)
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        QByteArray path = "/usr/local/bin:";
        path += qgetenv("PATH");
        qputenv("PATH", path.constData());

        // On OS X the standard is to not show icons in the menus
        app->setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

        if (!firstTimeUser)
        {
            handleChangedDefaults(DefaultPreferencesMode::Legacy);

#ifndef DISABLE_GUI
            if (!upgrade()) return EXIT_FAILURE;
#elif defined(Q_OS_WIN)
            if (!upgrade(_isatty(_fileno(stdin))
                         && _isatty(_fileno(stdout)))) return EXIT_FAILURE;
#else
            if (!upgrade(!params.shouldDaemonize
                         && isatty(fileno(stdin))
                         && isatty(fileno(stdout)))) return EXIT_FAILURE;
#endif
        }
        else
        {
            handleChangedDefaults(DefaultPreferencesMode::Current);
        }

#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
        if (params.shouldDaemonize)
        {
            app.reset(); // Destroy current application
            if (daemon(1, 0) == 0)
            {
                app = std::make_unique<Application>(argc, argv);
                if (app->isRunning())
                {
                    // Another instance had time to start.
                    return EXIT_FAILURE;
                }
            }
            else
            {
                qCritical("Something went wrong while daemonizing, exiting...");
                return EXIT_FAILURE;
            }
        }
#elif !defined(DISABLE_GUI)
        if (!(params.noSplash || Preferences::instance()->isSplashScreenDisabled()))
            showSplashScreen();
#endif

        signal(SIGINT, sigNormalHandler);
        signal(SIGTERM, sigNormalHandler);
#ifdef STACKTRACE
        signal(SIGABRT, sigAbnormalHandler);
        signal(SIGSEGV, sigAbnormalHandler);
#endif

        return app->exec(params.paramList());
    }
    catch (const CommandLineParameterError &er)
    {
        displayBadArgMessage(er.messageForUser());
        return EXIT_FAILURE;
    }
}

#if !(defined Q_OS_WIN && !defined DISABLE_GUI) && !defined Q_OS_HAIKU
void reportToUser(const char *str)
{
    const size_t strLen = strlen(str);
#ifndef Q_OS_WIN
    if (write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen))
    {
        const auto dummy = write(STDOUT_FILENO, str, strLen);
#else
    if (_write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen))
    {
        const auto dummy = _write(STDOUT_FILENO, str, strLen);
#endif
        Q_UNUSED(dummy);
    }
}
#endif

void sigNormalHandler(int signum)
{
#if !(defined Q_OS_WIN && !defined DISABLE_GUI) && !defined Q_OS_HAIKU
    const char msg1[] = "Catching signal: ";
    const char msg2[] = "\nExiting cleanly\n";
    reportToUser(msg1);
    reportToUser(sysSigName[signum]);
    reportToUser(msg2);
#endif // !defined Q_OS_WIN && !defined Q_OS_HAIKU
    signal(signum, SIG_DFL);
    qApp->exit();  // unsafe, but exit anyway
}

#ifdef STACKTRACE
void sigAbnormalHandler(int signum)
{
    const char *sigName = sysSigName[signum];
#if !(defined Q_OS_WIN && !defined DISABLE_GUI) && !defined Q_OS_HAIKU
    const char msg[] = "\n\n*************************************************************\n"
        "Please file a bug report at http://bug.qbittorrent.org and provide the following information:\n\n"
        "qBittorrent version: " QBT_VERSION "\n\n"
        "Caught signal: ";
    reportToUser(msg);
    reportToUser(sigName);
    reportToUser("\n");
    print_stacktrace();  // unsafe
#endif

#if defined Q_OS_WIN && !defined DISABLE_GUI
    StacktraceDialog dlg;  // unsafe
    dlg.setStacktraceString(QLatin1String(sigName), straceWin::getBacktrace());
    dlg.exec();
#endif

    signal(signum, SIG_DFL);
    raise(signum);
}
#endif // STACKTRACE

#if !defined(DISABLE_GUI)
void showSplashScreen()
{
    QPixmap splashImg(":/icons/splash.png");
    QPainter painter(&splashImg);
    const QString version = QBT_VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    painter.drawText(224 - painter.fontMetrics().horizontalAdvance(version), 270, version);
#else
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
#endif
    QSplashScreen *splash = new QSplashScreen(splashImg);
    splash->show();
    QTimer::singleShot(1500, splash, &QObject::deleteLater);
    qApp->processEvents();
}
#endif  // DISABLE_GUI

void displayVersion()
{
    printf("%s %s\n", qUtf8Printable(qApp->applicationName()), QBT_VERSION);
}

void displayBadArgMessage(const QString &message)
{
    const QString help = QObject::tr("Run application with -h option to read about command line parameters.");
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    QMessageBox msgBox(QMessageBox::Critical, QObject::tr("Bad command line"),
                       message + QLatin1Char('\n') + help, QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
#else
    const QString errMsg = QObject::tr("Bad command line: ") + '\n'
        + message + '\n'
        + help + '\n';
    fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
}

bool userAgreesWithLegalNotice()
{
    Preferences *const pref = Preferences::instance();
    Q_ASSERT(!pref->getAcceptedLegal());

#ifdef DISABLE_GUI
    const QString eula = QString::fromLatin1("\n*** %1 ***\n").arg(QObject::tr("Legal Notice"))
        + QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.") + "\n\n"
        + QObject::tr("No further notices will be issued.") + "\n\n"
        + QObject::tr("Press %1 key to accept and continue...").arg("'y'") + '\n';
    printf("%s", qUtf8Printable(eula));

    const char ret = getchar(); // Read pressed key
    if ((ret == 'y') || (ret == 'Y'))
    {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#else
    QMessageBox msgBox;
    msgBox.setText(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(QObject::tr("Legal notice"));
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
    const QAbstractButton *agreeButton = msgBox.addButton(QObject::tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
    if (msgBox.clickedButton() == agreeButton)
    {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
#endif // DISABLE_GUI

    return false;
}

#if defined(Q_OS_UNIX)
void adjustFileDescriptorLimit()
{
    rlimit limit {};

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return;

    limit.rlim_cur = limit.rlim_max;
    setrlimit(RLIMIT_NOFILE, &limit);
}
#endif
