/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtSystemDetection>

#include <chrono>
#include <cstdlib>
#include <memory>

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

#ifndef Q_OS_WIN
#ifndef Q_OS_HAIKU
#include <unistd.h>
#endif // Q_OS_HAIKU
#elif defined DISABLE_GUI
#include <io.h>
#endif

#include <QCoreApplication>
#include <QString>
#include <QThread>

#ifndef DISABLE_GUI
// GUI-only includes
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QSplashScreen>
#include <QTimer>

#ifdef QBT_STATIC_QT
#include <QtPlugin>
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // QBT_STATIC_QT

#else // DISABLE_GUI
#include <cstdio>
#endif // DISABLE_GUI

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingvalue.h"
#include "base/version.h"
#include "application.h"
#include "cmdoptions.h"
#include "legalnotice.h"
#include "signalhandler.h"

#ifndef DISABLE_GUI
#include "gui/utils.h"
#endif

using namespace std::chrono_literals;

namespace
{
    void displayBadArgMessage(const QString &message)
    {
        const QString help = QCoreApplication::translate("Main", "Run application with -h option to read about command line parameters.");
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
        QMessageBox msgBox(QMessageBox::Critical, QCoreApplication::translate("Main", "Bad command line"),
                           (message + u'\n' + help), QMessageBox::Ok);
        msgBox.show(); // Need to be shown or to moveToCenter does not work
        msgBox.move(Utils::Gui::screenCenter(&msgBox));
        msgBox.exec();
#else
        const QString errMsg = QCoreApplication::translate("Main", "Bad command line: ") + u'\n'
            + message + u'\n'
            + help + u'\n';
        fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
    }

    void displayErrorMessage(const QString &message)
    {
#ifndef DISABLE_GUI
        if (QApplication::instance())
        {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText(QCoreApplication::translate("Main", "An unrecoverable error occurred."));
            msgBox.setInformativeText(message);
            msgBox.show(); // Need to be shown or to moveToCenter does not work
            msgBox.move(Utils::Gui::screenCenter(&msgBox));
            msgBox.exec();
        }
        else
        {
            const QString errMsg = QCoreApplication::translate("Main", "qBittorrent has encountered an unrecoverable error.") + u'\n' + message + u'\n';
            fprintf(stderr, "%s", qUtf8Printable(errMsg));
        }
#else
        const QString errMsg = QCoreApplication::translate("Main", "qBittorrent has encountered an unrecoverable error.") + u'\n' + message + u'\n';
        fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
    }

    void displayVersion()
    {
        printf("%s %s\n", qUtf8Printable(qApp->applicationName()), QBT_VERSION);
    }

#ifndef DISABLE_GUI
    void showSplashScreen()
    {
        QPixmap splashImg(u":/icons/splash.png"_s);
        QPainter painter(&splashImg);
        const auto version = QStringLiteral(QBT_VERSION);
        painter.setPen(QPen(Qt::white));
        painter.setFont(QFont(u"Arial"_s, 22, QFont::Black));
        painter.drawText(224 - painter.fontMetrics().horizontalAdvance(version), 270, version);
        QSplashScreen *splash = new QSplashScreen(splashImg);
        splash->show();
        QTimer::singleShot(1500ms, Qt::CoarseTimer, splash, &QObject::deleteLater);
        qApp->processEvents();
    }
#endif  // DISABLE_GUI

#ifdef Q_OS_UNIX
    void adjustFileDescriptorLimit()
    {
        rlimit limit {};

        if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
            return;

        limit.rlim_cur = limit.rlim_max;
        setrlimit(RLIMIT_NOFILE, &limit);
    }

    void adjustLocale()
    {
        // specify the default locale just in case if user has not set any other locale
        // only `C` locale is available universally without installing locale packages
        if (qEnvironmentVariableIsEmpty("LANG"))
            qputenv("LANG", "C.UTF-8");
    }
#endif
}

// Main
int main(int argc, char *argv[])
{
#ifdef DISABLE_GUI
    setvbuf(stdout, nullptr, _IONBF, 0);
#endif

#ifdef Q_OS_UNIX
    adjustLocale();
    adjustFileDescriptorLimit();
#endif

    // We must save it here because QApplication constructor may change it
    const bool isOneArg = (argc == 2);

    // `app` must be declared out of try block to allow display message box in case of exception
    std::unique_ptr<Application> app;
    try
    {
        // Create Application
        app = std::make_unique<Application>(argc, argv);

#ifdef Q_OS_WIN
        // QCoreApplication::applicationDirPath() needs an Application object instantiated first
        // Let's hope that there won't be a crash before this line
        const char envName[] = "_NT_SYMBOL_PATH";
        const QString envValue = qEnvironmentVariable(envName);
        if (envValue.isEmpty())
            qputenv(envName, Application::applicationDirPath().toLocal8Bit());
        else
            qputenv(envName, u"%1;%2"_s.arg(envValue, Application::applicationDirPath()).toLocal8Bit());
#endif

        const QBtCommandLineParameters params = app->commandLineArgs();
        if (!params.unknownParameter.isEmpty())
        {
            throw CommandLineParameterError(QCoreApplication::translate("Main", "%1 is an unknown command line parameter.",
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
            throw CommandLineParameterError(QCoreApplication::translate("Main", "%1 must be the single command line parameter.")
                                     .arg(u"-v (or --version)"_s));
        }
#endif
        if (params.showHelp)
        {
            if (isOneArg)
            {
                displayUsage(QString::fromLocal8Bit(argv[0]));
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QCoreApplication::translate("Main", "%1 must be the single command line parameter.")
                                 .arg(u"-h (or --help)"_s));
        }

        // Check if qBittorrent is already running
        if (app->hasAnotherInstance())
        {
#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
            if (params.shouldDaemonize)
            {
                throw CommandLineParameterError(QCoreApplication::translate("Main", "You cannot use %1: qBittorrent is already running.")
                    .arg(u"-d (or --daemon)"_s));
            }

            // print friendly message if there are no other command line args
            if (argc == 1)
            {
                const QString message = QCoreApplication::translate("Main", "Another qBittorrent instance is already running.");
                printf("%s\n", qUtf8Printable(message));
            }
#endif

            QThread::msleep(300);
            app->callMainInstance();

            return EXIT_SUCCESS;
        }

        CachedSettingValue<bool> legalNoticeShown {u"LegalNotice/Accepted"_s, false};
        if (params.confirmLegalNotice)
            legalNoticeShown = true;

        if (!legalNoticeShown)
        {
#ifndef DISABLE_GUI
            const bool isInteractive = true;
#elif defined(Q_OS_WIN)
            const bool isInteractive = (_isatty(_fileno(stdin)) != 0) && (_isatty(_fileno(stdout)) != 0);
#else
            // when run in daemon mode user can only dismiss the notice with command line option
            const bool isInteractive = !params.shouldDaemonize
                && ((isatty(fileno(stdin)) != 0) && (isatty(fileno(stdout)) != 0));
#endif
            showLegalNotice(isInteractive);
            if (isInteractive)
                legalNoticeShown = true;
        }

#ifdef Q_OS_MACOS
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        const QByteArray path = "/usr/local/bin:" + qgetenv("PATH");
        qputenv("PATH", path.constData());

        // On OS X the standard is to not show icons in the menus
        app->setAttribute(Qt::AA_DontShowIconsInMenus);
#else
        if (!Preferences::instance()->iconsInMenusEnabled())
            app->setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
        if (params.shouldDaemonize)
        {
            app.reset(); // Destroy current application instance
            if (::daemon(1, 0) == 0)
            {
                app = std::make_unique<Application>(argc, argv);
                if (app->hasAnotherInstance())
                {
                    // It is undefined behavior to write to log file since there is another qbt instance
                    // in play. But we still do it since there is chance that the log message will survive.
                    const QString errorMessage = QCoreApplication::translate("Main", "Found unexpected qBittorrent instance. Exiting this instance. Current process ID: %1.")
                        .arg(QString::number(QCoreApplication::applicationPid()));
                    LogMsg(errorMessage, Log::CRITICAL);
                    // stdout, stderr is closed so we can't use them
                    return EXIT_FAILURE;
                }
            }
            else
            {
                const QString errorMessage = QCoreApplication::translate("Main", "Error when daemonizing. Reason: \"%1\". Error code: %2.")
                    .arg(QString::fromLocal8Bit(strerror(errno)), QString::number(errno));
                LogMsg(errorMessage, Log::CRITICAL);
                qCritical("%s", qUtf8Printable(errorMessage));
                return EXIT_FAILURE;
            }
        }
#elif !defined(DISABLE_GUI)
        if (!(params.noSplash || Preferences::instance()->isSplashScreenDisabled()))
            showSplashScreen();
#endif

        registerSignalHandlers();

        return app->exec();
    }
    catch (const CommandLineParameterError &er)
    {
        displayBadArgMessage(er.message());
        return EXIT_FAILURE;
    }
    catch (const RuntimeError &er)
    {
        displayErrorMessage(er.message());
        return EXIT_FAILURE;
    }
}
