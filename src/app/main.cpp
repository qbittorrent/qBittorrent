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

#else // DISABLE_GUI
#include <cstdio>
#endif // DISABLE_GUI

#include "base/global.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/version.h"
#include "application.h"
#include "cmdoptions.h"
#include "signalhandler.h"
#include "upgrade.h"

#ifndef DISABLE_GUI
#include "gui/utils.h"
#endif

using namespace std::chrono_literals;

void displayVersion();
bool userAgreesWithLegalNotice();
void displayBadArgMessage(const QString &message);

#ifndef DISABLE_GUI
void showSplashScreen();
#endif  // DISABLE_GUI

#ifdef Q_OS_UNIX
void adjustFileDescriptorLimit();
#endif

// Main
int main(int argc, char *argv[])
{
#ifdef Q_OS_UNIX
    adjustFileDescriptorLimit();
#endif

    // We must save it here because QApplication constructor may change it
    bool isOneArg = (argc == 2);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) && !defined(DISABLE_GUI)
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
                                     .arg(u"-v (or --version)"_qs));
        }
#endif
        if (params.showHelp)
        {
            if (isOneArg)
            {
                displayUsage(QString::fromLocal8Bit(argv[0]));
                return EXIT_SUCCESS;
            }
            throw CommandLineParameterError(QObject::tr("%1 must be the single command line parameter.")
                                 .arg(u"-h (or --help)"_qs));
        }

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

            setCurrentMigrationVersion();
        }

        // Check if qBittorrent is already running for this user
        if (app->isRunning())
        {
#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
            if (params.shouldDaemonize)
            {
                throw CommandLineParameterError(QObject::tr("You cannot use %1: qBittorrent is already running for this user.")
                                     .arg(u"-d (or --daemon)"_qs));
            }
#endif

            QThread::msleep(300);
            app->sendParams(params.paramList());

            return EXIT_SUCCESS;
        }

#ifdef Q_OS_WIN
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
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) && !defined(DISABLE_GUI)
        // this is the default in Qt6
        app->setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#endif // Q_OS_WIN

#ifdef Q_OS_MACOS
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        QByteArray path = "/usr/local/bin:";
        path += qgetenv("PATH");
        qputenv("PATH", path.constData());

        // On OS X the standard is to not show icons in the menus
        app->setAttribute(Qt::AA_DontShowIconsInMenus);
#else
        if (!Preferences::instance()->iconsInMenusEnabled())
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

        registerSignalHandlers();

        return app->exec(params.paramList());
    }
    catch (const CommandLineParameterError &er)
    {
        displayBadArgMessage(er.message());
        return EXIT_FAILURE;
    }
}

#if !defined(DISABLE_GUI)
void showSplashScreen()
{
    QPixmap splashImg(u":/icons/splash.png"_qs);
    QPainter painter(&splashImg);
    const auto version = QStringLiteral(QBT_VERSION);
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont(u"Arial"_qs, 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().horizontalAdvance(version), 270, version);
    QSplashScreen *splash = new QSplashScreen(splashImg);
    splash->show();
    QTimer::singleShot(1500ms, splash, &QObject::deleteLater);
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
                       (message + u'\n' + help), QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
#else
    const QString errMsg = QObject::tr("Bad command line: ") + u'\n'
        + message + u'\n'
        + help + u'\n';
    fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
}

bool userAgreesWithLegalNotice()
{
    Preferences *const pref = Preferences::instance();
    Q_ASSERT(!pref->getAcceptedLegal());

#ifdef DISABLE_GUI
    const QString eula = u"\n*** %1 ***\n"_qs.arg(QObject::tr("Legal Notice"))
        + QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.") + u"\n\n"
        + QObject::tr("No further notices will be issued.") + u"\n\n"
        + QObject::tr("Press %1 key to accept and continue...").arg(u"'y'"_qs) + u'\n';
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

#ifdef Q_OS_UNIX
void adjustFileDescriptorLimit()
{
    rlimit limit {};

    if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
        return;

    limit.rlim_cur = limit.rlim_max;
    setrlimit(RLIMIT_NOFILE, &limit);
}
#endif
