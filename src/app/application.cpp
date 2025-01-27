/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#include "application.h"

#include <algorithm>

#ifdef DISABLE_GUI
#include <cstdio>
#endif

#ifdef Q_OS_WIN
#include <memory>
#include <windows.h>
#include <shellapi.h>
#elif defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif

#include <QByteArray>
#include <QDebug>
#include <QLibraryInfo>
#include <QMetaObject>
#include <QProcess>

#ifndef DISABLE_GUI
#include <QAbstractButton>
#include <QMenu>
#include <QMessageBox>
#include <QPixmapCache>
#include <QProgressDialog>
#ifdef Q_OS_WIN
#include <QSessionManager>
#endif // Q_OS_WIN
#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#endif // Q_OS_MACOS
#endif

#include "base/addtorrentmanager.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/geoipmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/net/smtp.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_session.h"
#include "base/search/searchpluginmanager.h"
#include "base/settingsstorage.h"
#include "base/torrentfileswatcher.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/os.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "applicationinstancemanager.h"
#include "filelogger.h"
#include "upgrade.h"

#ifndef DISABLE_GUI
#include "gui/desktopintegration.h"
#include "gui/mainwindow.h"
#include "gui/shutdownconfirmdialog.h"
#include "gui/uithememanager.h"
#include "gui/windowstate.h"
#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
#include "webui/webui.h"
#ifdef DISABLE_GUI
#include "base/utils/password.h"
#endif
#endif

namespace
{
#define SETTINGS_KEY(name) u"Application/" name
#define FILELOGGER_SETTINGS_KEY(name) (SETTINGS_KEY(u"FileLogger/") name)
#define NOTIFICATIONS_SETTINGS_KEY(name) (SETTINGS_KEY(u"GUI/Notifications/"_s) name)

    const QString LOG_FOLDER = u"logs"_s;
    const QChar PARAMS_SEPARATOR = u'|';

    const Path DEFAULT_PORTABLE_MODE_PROFILE_DIR {u"profile"_s};

    const int MIN_FILELOG_SIZE = 1024; // 1KiB
    const int MAX_FILELOG_SIZE = 1000 * 1024 * 1024; // 1000MiB
    const int DEFAULT_FILELOG_SIZE = 65 * 1024; // 65KiB

#ifndef DISABLE_GUI
    const int PIXMAP_CACHE_SIZE = 64 * 1024 * 1024;  // 64MiB
#endif

    QString serializeParams(const QBtCommandLineParameters &params)
    {
        QStringList result;
        // Because we're passing a string list to the currently running
        // qBittorrent process, we need some way of passing along the options
        // the user has specified. Here we place special strings that are
        // almost certainly not going to collide with a file path or URL
        // specified by the user, and placing them at the beginning of the
        // string list so that they will be processed before the list of
        // torrent paths or URLs.

        const BitTorrent::AddTorrentParams &addTorrentParams = params.addTorrentParams;

        if (!addTorrentParams.savePath.isEmpty())
            result.append(u"@savePath=" + addTorrentParams.savePath.data());

        if (addTorrentParams.addStopped.has_value())
            result.append(*addTorrentParams.addStopped ? u"@addStopped=1"_s : u"@addStopped=0"_s);

        if (addTorrentParams.skipChecking)
            result.append(u"@skipChecking"_s);

        if (!addTorrentParams.category.isEmpty())
            result.append(u"@category=" + addTorrentParams.category);

        if (addTorrentParams.sequential)
            result.append(u"@sequential"_s);

        if (addTorrentParams.firstLastPiecePriority)
            result.append(u"@firstLastPiecePriority"_s);

        if (params.skipDialog.has_value())
            result.append(*params.skipDialog ? u"@skipDialog=1"_s : u"@skipDialog=0"_s);

        result += params.torrentSources;

        return result.join(PARAMS_SEPARATOR);
    }

    QBtCommandLineParameters parseParams(const QString &str)
    {
        QBtCommandLineParameters parsedParams;
        BitTorrent::AddTorrentParams &addTorrentParams = parsedParams.addTorrentParams;

        for (QString param : asConst(str.split(PARAMS_SEPARATOR, Qt::SkipEmptyParts)))
        {
            param = param.trimmed();

            // Process strings indicating options specified by the user.

            if (param.startsWith(u"@savePath="))
            {
                addTorrentParams.savePath = Path(param.mid(10));
                continue;
            }

            if (param.startsWith(u"@addStopped="))
            {
                addTorrentParams.addStopped = (QStringView(param).mid(11).toInt() != 0);
                continue;
            }

            if (param == u"@skipChecking")
            {
                addTorrentParams.skipChecking = true;
                continue;
            }

            if (param.startsWith(u"@category="))
            {
                addTorrentParams.category = param.mid(10);
                continue;
            }

            if (param == u"@sequential")
            {
                addTorrentParams.sequential = true;
                continue;
            }

            if (param == u"@firstLastPiecePriority")
            {
                addTorrentParams.firstLastPiecePriority = true;
                continue;
            }

            if (param.startsWith(u"@skipDialog="))
            {
                parsedParams.skipDialog = (QStringView(param).mid(12).toInt() != 0);
                continue;
            }

            parsedParams.torrentSources.append(param);
        }

        return parsedParams;
    }
}

Application::Application(int &argc, char **argv)
    : BaseApplication(argc, argv)
    , m_commandLineArgs(parseCommandLine(Application::arguments()))
    , m_storeInstanceName(SETTINGS_KEY(u"InstanceName"_s))
    , m_storeFileLoggerEnabled(FILELOGGER_SETTINGS_KEY(u"Enabled"_s))
    , m_storeFileLoggerBackup(FILELOGGER_SETTINGS_KEY(u"Backup"_s))
    , m_storeFileLoggerDeleteOld(FILELOGGER_SETTINGS_KEY(u"DeleteOld"_s))
    , m_storeFileLoggerMaxSize(FILELOGGER_SETTINGS_KEY(u"MaxSizeBytes"_s))
    , m_storeFileLoggerAge(FILELOGGER_SETTINGS_KEY(u"Age"_s))
    , m_storeFileLoggerAgeType(FILELOGGER_SETTINGS_KEY(u"AgeType"_s))
    , m_storeFileLoggerPath(FILELOGGER_SETTINGS_KEY(u"Path"_s))
    , m_storeMemoryWorkingSetLimit(SETTINGS_KEY(u"MemoryWorkingSetLimit"_s))
#ifdef Q_OS_WIN
    , m_processMemoryPriority(SETTINGS_KEY(u"ProcessMemoryPriority"_s))
#endif
#ifndef DISABLE_GUI
    , m_startUpWindowState(u"GUI/StartUpWindowState"_s)
    , m_storeNotificationTorrentAdded(NOTIFICATIONS_SETTINGS_KEY(u"TorrentAdded"_s))
#endif
{
    qRegisterMetaType<Log::Msg>("Log::Msg");
    qRegisterMetaType<Log::Peer>("Log::Peer");

    setApplicationName(u"qBittorrent"_s);
    setOrganizationDomain(u"qbittorrent.org"_s);
#if !defined(DISABLE_GUI)
    setDesktopFileName(u"org.qbittorrent.qBittorrent"_s);
    setQuitOnLastWindowClosed(false);
    setQuitLockEnabled(false);
    QPixmapCache::setCacheLimit(PIXMAP_CACHE_SIZE);
#endif

    Logger::initInstance();

    const auto portableProfilePath = Path(QCoreApplication::applicationDirPath()) / DEFAULT_PORTABLE_MODE_PROFILE_DIR;
    const bool portableModeEnabled = m_commandLineArgs.profileDir.isEmpty() && Utils::Fs::isDir(portableProfilePath);
    const Path profileDir = portableModeEnabled ? portableProfilePath : m_commandLineArgs.profileDir;
    Profile::initInstance(profileDir, m_commandLineArgs.configurationName,
                        (m_commandLineArgs.relativeFastresumePaths || portableModeEnabled));

    m_instanceManager = new ApplicationInstanceManager(Profile::instance()->location(SpecialFolder::Config), this);

    SettingsStorage::initInstance();
    Preferences::initInstance();

    const bool firstTimeUser = SettingsStorage::instance()->isEmpty();
    if (firstTimeUser)
    {
        setCurrentMigrationVersion();
        handleChangedDefaults(DefaultPreferencesMode::Current);
    }
    else
    {
        if (!upgrade())
            throw RuntimeError(u"Failed migration of old settings"_s); // Not translatable. Translation isn't configured yet.
        handleChangedDefaults(DefaultPreferencesMode::Legacy);
    }

    initializeTranslation();

    connect(this, &QCoreApplication::aboutToQuit, this, &Application::cleanup);
    connect(m_instanceManager, &ApplicationInstanceManager::messageReceived, this, &Application::processMessage);
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    connect(this, &QGuiApplication::commitDataRequest, this, &Application::shutdownCleanup, Qt::DirectConnection);
#endif

    LogMsg(tr("qBittorrent %1 started. Process ID: %2", "qBittorrent v3.2.0alpha started")
        .arg(QStringLiteral(QBT_VERSION), QString::number(QCoreApplication::applicationPid())));
    if (portableModeEnabled)
    {
        LogMsg(tr("Running in portable mode. Auto detected profile folder at: %1").arg(profileDir.toString()));
        if (m_commandLineArgs.relativeFastresumePaths)
            LogMsg(tr("Redundant command line flag detected: \"%1\". Portable mode implies relative fastresume.").arg(u"--relative-fastresume"_s), Log::WARNING); // to avoid translating the `--relative-fastresume` string
    }
    else
    {
        LogMsg(tr("Using config directory: %1").arg(Profile::instance()->location(SpecialFolder::Config).toString()));
    }

    if (isFileLoggerEnabled())
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));

    if (m_commandLineArgs.webUIPort > 0) // it will be -1 when user did not set any value
        Preferences::instance()->setWebUIPort(m_commandLineArgs.webUIPort);

    if (m_commandLineArgs.torrentingPort > 0) // it will be -1 when user did not set any value
    {
        SettingValue<int> port {u"BitTorrent/Session/Port"_s};
        port = m_commandLineArgs.torrentingPort;
    }
}

Application::~Application()
{
    // we still need to call cleanup()
    // in case the App failed to start
    cleanup();
}

#ifndef DISABLE_GUI
DesktopIntegration *Application::desktopIntegration()
{
    return m_desktopIntegration;
}

MainWindow *Application::mainWindow()
{
    return m_window;
}

WindowState Application::startUpWindowState() const
{
    return m_startUpWindowState;
}

void Application::setStartUpWindowState(const WindowState windowState)
{
    m_startUpWindowState = windowState;
}

bool Application::isTorrentAddedNotificationsEnabled() const
{
    return m_storeNotificationTorrentAdded;
}

void Application::setTorrentAddedNotificationsEnabled(const bool value)
{
    m_storeNotificationTorrentAdded = value;
}
#endif

const QBtCommandLineParameters &Application::commandLineArgs() const
{
    return m_commandLineArgs;
}

QString Application::instanceName() const
{
    return m_storeInstanceName;
}

void Application::setInstanceName(const QString &name)
{
    if (name == instanceName())
        return;

    m_storeInstanceName = name;
#ifndef DISABLE_GUI
    if (MainWindow *mw = mainWindow())
        mw->setTitleSuffix(name);
#endif
}

int Application::memoryWorkingSetLimit() const
{
    return m_storeMemoryWorkingSetLimit.get(512);
}

void Application::setMemoryWorkingSetLimit(const int size)
{
    if (size == memoryWorkingSetLimit())
        return;

    m_storeMemoryWorkingSetLimit = size;
#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
    applyMemoryWorkingSetLimit();
#endif
}

bool Application::isFileLoggerEnabled() const
{
    return m_storeFileLoggerEnabled.get(true);
}

void Application::setFileLoggerEnabled(const bool value)
{
    if (value && !m_fileLogger)
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    else if (!value)
        delete m_fileLogger;
    m_storeFileLoggerEnabled = value;
}

Path Application::fileLoggerPath() const
{
    return m_storeFileLoggerPath.get(specialFolderLocation(SpecialFolder::Data) / Path(LOG_FOLDER));
}

void Application::setFileLoggerPath(const Path &path)
{
    if (m_fileLogger)
        m_fileLogger->changePath(path);
    m_storeFileLoggerPath = path;
}

bool Application::isFileLoggerBackup() const
{
    return m_storeFileLoggerBackup.get(true);
}

void Application::setFileLoggerBackup(const bool value)
{
    if (m_fileLogger)
        m_fileLogger->setBackup(value);
    m_storeFileLoggerBackup = value;
}

bool Application::isFileLoggerDeleteOld() const
{
    return m_storeFileLoggerDeleteOld.get(true);
}

void Application::setFileLoggerDeleteOld(const bool value)
{
    if (value && m_fileLogger)
        m_fileLogger->deleteOld(fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    m_storeFileLoggerDeleteOld = value;
}

int Application::fileLoggerMaxSize() const
{
    const int val = m_storeFileLoggerMaxSize.get(DEFAULT_FILELOG_SIZE);
    return std::clamp(val, MIN_FILELOG_SIZE, MAX_FILELOG_SIZE);
}

void Application::setFileLoggerMaxSize(const int bytes)
{
    const int clampedValue = std::clamp(bytes, MIN_FILELOG_SIZE, MAX_FILELOG_SIZE);
    if (m_fileLogger)
        m_fileLogger->setMaxSize(clampedValue);
    m_storeFileLoggerMaxSize = clampedValue;
}

int Application::fileLoggerAge() const
{
    const int val = m_storeFileLoggerAge.get(1);
    return std::clamp(val, 1, 365);
}

void Application::setFileLoggerAge(const int value)
{
    m_storeFileLoggerAge = std::clamp(value, 1, 365);
}

int Application::fileLoggerAgeType() const
{
    const int val = m_storeFileLoggerAgeType.get(1);
    return ((val < 0) || (val > 2)) ? 1 : val;
}

void Application::setFileLoggerAgeType(const int value)
{
    m_storeFileLoggerAgeType = ((value < 0) || (value > 2)) ? 1 : value;
}

void Application::processMessage(const QString &message)
{
#ifndef DISABLE_GUI
    if (message.isEmpty())
    {
        if (BitTorrent::Session::instance()->isRestored()) [[likely]]
        {
            m_window->activate(); // show UI
        }
        else if (m_startupProgressDialog)
        {
            m_startupProgressDialog->show();
            m_startupProgressDialog->activateWindow();
            m_startupProgressDialog->raise();
        }
        else
        {
            createStartupProgressDialog();
        }

        return;
    }
#endif

    const QBtCommandLineParameters params = parseParams(message);
    // If Application is not allowed to process params immediately
    // (i.e., other components are not ready) store params
    if (m_isProcessingParamsAllowed)
        processParams(params);
    else
        m_paramsQueue.append(params);
}

void Application::runExternalProgram(const QString &programTemplate, const BitTorrent::Torrent *torrent) const
{
    // Cannot give users shell environment by default, as doing so could
    // enable command injection via torrent name and other arguments
    // (especially when some automated download mechanism has been setup).
    // See: https://github.com/qbittorrent/qBittorrent/issues/10925

    const auto replaceVariables = [torrent](QString str) -> QString
    {
        for (int i = (str.length() - 2); i >= 0; --i)
        {
            if (str[i] != u'%')
                continue;

            const ushort specifier = str[i + 1].unicode();
            switch (specifier)
            {
            case u'C':
                str.replace(i, 2, QString::number(torrent->filesCount()));
                break;
            case u'D':
                str.replace(i, 2, torrent->savePath().toString());
                break;
            case u'F':
                str.replace(i, 2, torrent->contentPath().toString());
                break;
            case u'G':
                str.replace(i, 2, Utils::String::joinIntoString(torrent->tags(), u","_s));
                break;
            case u'I':
                str.replace(i, 2, (torrent->infoHash().v1().isValid() ? torrent->infoHash().v1().toString() : u"-"_s));
                break;
            case u'J':
                str.replace(i, 2, (torrent->infoHash().v2().isValid() ? torrent->infoHash().v2().toString() : u"-"_s));
                break;
            case u'K':
                str.replace(i, 2, torrent->id().toString());
                break;
            case u'L':
                str.replace(i, 2, torrent->category());
                break;
            case u'N':
                str.replace(i, 2, torrent->name());
                break;
            case u'R':
                str.replace(i, 2, torrent->rootPath().toString());
                break;
            case u'T':
                str.replace(i, 2, torrent->currentTracker());
                break;
            case u'Z':
                str.replace(i, 2, QString::number(torrent->totalSize()));
                break;
            default:
                // do nothing
                break;
            }

            // decrement `i` to avoid unwanted replacement, example pattern: "%%N"
            --i;
        }

        return str;
    };

    const QString logMsg = tr("Running external program. Torrent: \"%1\". Command: `%2`");
    const QString logMsgError = tr("Failed to run external program. Torrent: \"%1\". Command: `%2`");

    // The processing sequence is different for Windows and other OS, this is intentional
#if defined(Q_OS_WIN)
    const QString program = replaceVariables(programTemplate);
    const std::wstring programWStr = program.toStdWString();

    // Need to split arguments manually because QProcess::startDetached(QString)
    // will strip off empty parameters.
    // E.g. `python.exe "1" "" "3"` will become `python.exe "1" "3"`
    int argCount = 0;
    std::unique_ptr<LPWSTR[], decltype(&::LocalFree)> args {::CommandLineToArgvW(programWStr.c_str(), &argCount), ::LocalFree};

    if (argCount <= 0)
        return;

    QStringList argList;
    for (int i = 1; i < argCount; ++i)
        argList += QString::fromWCharArray(args[i]);

    QProcess proc;
    proc.setProgram(QString::fromWCharArray(args[0]));
    proc.setArguments(argList);
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args)
    {
        if (Preferences::instance()->isAutoRunConsoleEnabled())
        {
            args->flags |= CREATE_NEW_CONSOLE;
            args->flags &= ~(CREATE_NO_WINDOW | DETACHED_PROCESS);
        }
        else
        {
            args->flags |= CREATE_NO_WINDOW;
            args->flags &= ~(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
        }
        args->inheritHandles = false;
        args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
        ::CloseHandle(args->startupInfo->hStdInput);
        ::CloseHandle(args->startupInfo->hStdOutput);
        ::CloseHandle(args->startupInfo->hStdError);
        args->startupInfo->hStdInput = nullptr;
        args->startupInfo->hStdOutput = nullptr;
        args->startupInfo->hStdError = nullptr;
    });

    if (proc.startDetached())
        LogMsg(logMsg.arg(torrent->name(), program));
    else
        LogMsg(logMsgError.arg(torrent->name(), program));
#else // Q_OS_WIN
    QStringList args = Utils::String::splitCommand(programTemplate);

    if (args.isEmpty())
        return;

    for (QString &arg : args)
    {
        // strip redundant quotes
        if (arg.startsWith(u'"') && arg.endsWith(u'"'))
            arg = arg.mid(1, (arg.size() - 2));

        arg = replaceVariables(arg);
    }

    const QString command = args.takeFirst();
    QProcess proc;
    proc.setProgram(command);
    proc.setArguments(args);

    if (proc.startDetached())
    {
        // show intended command in log
        LogMsg(logMsg.arg(torrent->name(), replaceVariables(programTemplate)));
    }
    else
    {
        // show intended command in log
        LogMsg(logMsgError.arg(torrent->name(), replaceVariables(programTemplate)));
    }
#endif
}

void Application::sendNotificationEmail(const BitTorrent::Torrent *torrent)
{
    // Prepare mail content
    const QString content = tr("Torrent name: %1").arg(torrent->name()) + u'\n'
        + tr("Torrent size: %1").arg(Utils::Misc::friendlyUnit(torrent->wantedSize())) + u'\n'
        + tr("Save path: %1").arg(torrent->savePath().toString()) + u"\n\n"
        + tr("The torrent was downloaded in %1.", "The torrent was downloaded in 1 hour and 20 seconds")
            .arg(Utils::Misc::userFriendlyDuration(torrent->activeTime())) + u"\n\n\n"
        + tr("Thank you for using qBittorrent.") + u'\n';

    // Send the notification email
    const Preferences *pref = Preferences::instance();
    auto *smtp = new Net::Smtp(this);
    smtp->sendMail(pref->getMailNotificationSender(),
                     pref->getMailNotificationEmail(),
                     tr("Torrent \"%1\" has finished downloading").arg(torrent->name()),
                     content);
}

void Application::sendTestEmail() const
{
    const Preferences *pref = Preferences::instance();
    if (pref->isMailNotificationEnabled())
    {
        // Prepare mail content
        const QString content = tr("This is a test email.") + u'\n'
            + tr("Thank you for using qBittorrent.") + u'\n';

        // Send the notification email
        auto *smtp = new Net::Smtp();
        smtp->sendMail(pref->getMailNotificationSender(),
                        pref->getMailNotificationEmail(),
                        tr("Test email"),
                        content);
    }
}

void Application::torrentAdded(const BitTorrent::Torrent *torrent) const
{
    const Preferences *pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunOnTorrentAddedEnabled())
        runExternalProgram(pref->getAutoRunOnTorrentAddedProgram().trimmed(), torrent);
}

void Application::torrentFinished(const BitTorrent::Torrent *torrent)
{
    const Preferences *pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunOnTorrentFinishedEnabled())
        runExternalProgram(pref->getAutoRunOnTorrentFinishedProgram().trimmed(), torrent);

    // Mail notification
    if (pref->isMailNotificationEnabled())
    {
        LogMsg(tr("Torrent: %1, sending mail notification").arg(torrent->name()));
        sendNotificationEmail(torrent);
    }

#ifndef DISABLE_GUI
    if (Preferences::instance()->isRecursiveDownloadEnabled())
    {
        // Check whether it contains .torrent files
        for (const Path &torrentRelpath : asConst(torrent->filePaths()))
        {
            if (torrentRelpath.hasExtension(u".torrent"_s))
            {
                askRecursiveTorrentDownloadConfirmation(torrent);
                break;
            }
        }
    }
#endif
}

void Application::allTorrentsFinished()
{
    Preferences *const pref = Preferences::instance();
    bool isExit = pref->shutdownqBTWhenDownloadsComplete();
    bool isShutdown = pref->shutdownWhenDownloadsComplete();
    bool isSuspend = pref->suspendWhenDownloadsComplete();
    bool isHibernate = pref->hibernateWhenDownloadsComplete();

    bool haveAction = isExit || isShutdown || isSuspend || isHibernate;
    if (!haveAction) return;

    ShutdownDialogAction action = ShutdownDialogAction::Exit;
    if (isSuspend)
        action = ShutdownDialogAction::Suspend;
    else if (isHibernate)
        action = ShutdownDialogAction::Hibernate;
    else if (isShutdown)
        action = ShutdownDialogAction::Shutdown;

#ifndef DISABLE_GUI
    // ask confirm
    if ((action == ShutdownDialogAction::Exit) && (pref->dontConfirmAutoExit()))
    {
        // do nothing & skip confirm
    }
    else
    {
        if (!ShutdownConfirmDialog::askForConfirmation(m_window, action)) return;
    }
#endif // DISABLE_GUI

    // Actually shut down
    if (action != ShutdownDialogAction::Exit)
    {
        qDebug("Preparing for auto-shutdown because all downloads are complete!");
        // Disabling it for next time
        pref->setShutdownWhenDownloadsComplete(false);
        pref->setSuspendWhenDownloadsComplete(false);
        pref->setHibernateWhenDownloadsComplete(false);
        // Make sure preferences are synced before exiting
        m_shutdownAct = action;
    }

    qDebug("Exiting the application");
    exit();
}

bool Application::callMainInstance()
{
    return m_instanceManager->sendMessage(serializeParams(commandLineArgs()));
}

void Application::processParams(const QBtCommandLineParameters &params)
{
#ifndef DISABLE_GUI
    // There are two circumstances in which we want to show the torrent
    // dialog. One is when the application settings specify that it should
    // be shown and skipTorrentDialog is undefined. The other is when
    // skipTorrentDialog is false, meaning that the application setting
    // should be overridden.
    AddTorrentOption addTorrentOption = AddTorrentOption::Default;
    if (params.skipDialog.has_value())
        addTorrentOption = params.skipDialog.value() ? AddTorrentOption::SkipDialog : AddTorrentOption::ShowDialog;
    for (const QString &torrentSource : params.torrentSources)
        m_addTorrentManager->addTorrent(torrentSource, params.addTorrentParams, addTorrentOption);
#else
    for (const QString &torrentSource : params.torrentSources)
        m_addTorrentManager->addTorrent(torrentSource, params.addTorrentParams);
#endif
}

int Application::exec()
{
#if !defined(DISABLE_WEBUI) && defined(DISABLE_GUI)
    const QString loadingStr = tr("WebUI will be started shortly after internal preparations. Please wait...");
    printf("%s\n", qUtf8Printable(loadingStr));
#endif

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
    applyMemoryWorkingSetLimit();
#endif

#ifdef Q_OS_WIN
    applyMemoryPriority();
    adjustThreadPriority();
#endif

    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();

    BitTorrent::Session::initInstance();
#ifndef DISABLE_GUI
    UIThemeManager::initInstance();

    m_desktopIntegration = new DesktopIntegration;
    m_desktopIntegration->setToolTip(tr("Loading torrents..."));
#ifndef Q_OS_MACOS
    auto *desktopIntegrationMenu = m_desktopIntegration->menu();
    auto *actionExit = new QAction(tr("E&xit"), desktopIntegrationMenu);
    actionExit->setIcon(UIThemeManager::instance()->getIcon(u"application-exit"_s));
    actionExit->setMenuRole(QAction::QuitRole);
    actionExit->setShortcut(Qt::CTRL | Qt::Key_Q);
    connect(actionExit, &QAction::triggered, this, []
    {
        QApplication::exit();
    });
    desktopIntegrationMenu->addAction(actionExit);

    const bool isHidden = m_desktopIntegration->isActive() && (startUpWindowState() == WindowState::Hidden);
#else
    const bool isHidden = false;
#endif

    if (!isHidden)
    {
        createStartupProgressDialog();
        // Add a small delay to avoid "flashing" the progress dialog in case there are not many torrents to restore.
        m_startupProgressDialog->setMinimumDuration(1000);
        if (startUpWindowState() != WindowState::Normal)
            m_startupProgressDialog->setWindowState(Qt::WindowMinimized);
    }
    else
    {
        connect(m_desktopIntegration, &DesktopIntegration::activationRequested, this, &Application::createStartupProgressDialog);
    }
#endif
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::restored, this, [this]()
    {
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAdded, this, &Application::torrentAdded);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &Application::torrentFinished);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::allTorrentsFinished, this, &Application::allTorrentsFinished, Qt::QueuedConnection);

        m_addTorrentManager = new AddTorrentManagerImpl(this, BitTorrent::Session::instance(), this);

        Net::GeoIPManager::initInstance();
        TorrentFilesWatcher::initInstance();

        new RSS::Session; // create RSS::Session singleton
        new RSS::AutoDownloader(this); // create RSS::AutoDownloader singleton

#ifndef DISABLE_GUI
        const auto *btSession = BitTorrent::Session::instance();
        connect(btSession, &BitTorrent::Session::fullDiskError, this
                , [this](const BitTorrent::Torrent *torrent, const QString &msg)
        {
            m_desktopIntegration->showNotification(tr("I/O Error", "i.e: Input/Output Error")
                    , tr("An I/O error occurred for torrent '%1'.\n Reason: %2"
                            , "e.g: An error occurred for torrent 'xxx.avi'.\n Reason: disk is full.").arg(torrent->name(), msg));
        });
        connect(btSession, &BitTorrent::Session::torrentFinished, this
                , [this](const BitTorrent::Torrent *torrent)
        {
            m_desktopIntegration->showNotification(tr("Download completed"), tr("'%1' has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(torrent->name()));
        });
        connect(m_addTorrentManager, &AddTorrentManager::torrentAdded, this
                , [this]([[maybe_unused]] const QString &source, const BitTorrent::Torrent *torrent)
        {
            if (isTorrentAddedNotificationsEnabled())
                m_desktopIntegration->showNotification(tr("Torrent added"), tr("'%1' was added.", "e.g: xxx.avi was added.").arg(torrent->name()));
        });
        connect(m_addTorrentManager, &AddTorrentManager::addTorrentFailed, this
                , [this](const QString &source, const QString &reason)
        {
            m_desktopIntegration->showNotification(tr("Add torrent failed")
                    , tr("Couldn't add torrent '%1', reason: %2.").arg(source, reason));
        });

        disconnect(m_desktopIntegration, &DesktopIntegration::activationRequested, this, &Application::createStartupProgressDialog);
#ifndef Q_OS_MACOS
        const WindowState windowState = !m_startupProgressDialog ? WindowState::Hidden
                : (m_startupProgressDialog->windowState() & Qt::WindowMinimized) ? WindowState::Minimized
                        : WindowState::Normal;
#else
        const WindowState windowState = (m_startupProgressDialog->windowState() & Qt::WindowMinimized)
                ? WindowState::Minimized : WindowState::Normal;
#endif
        m_window = new MainWindow(this, windowState, instanceName());

        delete m_startupProgressDialog;
#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
#ifndef DISABLE_GUI
        m_webui = new WebUI(this);
#else
        const auto *pref = Preferences::instance();

        const QString tempPassword = pref->getWebUIPassword().isEmpty()
                ? Utils::Password::generate() : QString();
        m_webui = new WebUI(this, (!tempPassword.isEmpty() ? Utils::Password::PBKDF2::generate(tempPassword) : QByteArray()));
        connect(m_webui, &WebUI::error, this, [](const QString &message)
        {
            fprintf(stderr, "WebUI configuration failed. Reason: %s\n", qUtf8Printable(message));
        });

        printf("%s", qUtf8Printable(u"\n******** %1 ********\n"_s.arg(tr("Information"))));

        if (m_webui->isErrored())
        {
            const QString error = m_webui->errorMessage() + u'\n'
                    + tr("To fix the error, you may need to edit the config file manually.");
            fprintf(stderr, "%s\n", qUtf8Printable(error));
        }
        else if (m_webui->isEnabled())
        {
            const QHostAddress address = m_webui->hostAddress();
            const QString url = u"%1://%2:%3"_s.arg((m_webui->isHttps() ? u"https"_s : u"http"_s)
                    , (address.isEqual(QHostAddress::Any, QHostAddress::ConvertUnspecifiedAddress) ? u"localhost"_s : address.toString())
                    , QString::number(m_webui->port()));
            printf("%s\n", qUtf8Printable(tr("To control qBittorrent, access the WebUI at: %1").arg(url)));

            if (!tempPassword.isEmpty())
            {
                const QString warning = tr("The WebUI administrator username is: %1").arg(pref->getWebUIUsername()) + u'\n'
                        + tr("The WebUI administrator password was not set. A temporary password is provided for this session: %1").arg(tempPassword) + u'\n'
                        + tr("You should set your own password in program preferences.") + u'\n';
                printf("%s", qUtf8Printable(warning));
            }
        }
        else
        {
            printf("%s\n", qUtf8Printable(tr("The WebUI is disabled! To enable the WebUI, edit the config file manually.")));
        }
#endif // DISABLE_GUI
#endif // DISABLE_WEBUI

        m_isProcessingParamsAllowed = true;
        for (const QBtCommandLineParameters &params : m_paramsQueue)
            processParams(params);
        m_paramsQueue.clear();
    });

    const QBtCommandLineParameters params = commandLineArgs();
    if (!params.torrentSources.isEmpty())
        m_paramsQueue.append(params);

    return BaseApplication::exec();
}

bool Application::hasAnotherInstance() const
{
    return !m_instanceManager->isFirstInstance();
}

#ifndef DISABLE_GUI
void Application::createStartupProgressDialog()
{
    Q_ASSERT(!m_startupProgressDialog);
    Q_ASSERT(m_desktopIntegration);

    disconnect(m_desktopIntegration, &DesktopIntegration::activationRequested, this, &Application::createStartupProgressDialog);

    m_startupProgressDialog = new QProgressDialog(tr("Loading torrents..."), tr("Exit"), 0, 100);
    m_startupProgressDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_startupProgressDialog->setWindowFlag(Qt::WindowMinimizeButtonHint);
    m_startupProgressDialog->setMinimumDuration(0); // Show dialog immediately by default
    m_startupProgressDialog->setAutoReset(false);
    m_startupProgressDialog->setAutoClose(false);

    connect(m_startupProgressDialog, &QProgressDialog::canceled, this, []()
    {
        QApplication::exit();
    });

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::startupProgressUpdated, m_startupProgressDialog, &QProgressDialog::setValue);

    connect(m_desktopIntegration, &DesktopIntegration::activationRequested, m_startupProgressDialog, [this]()
    {
#ifdef Q_OS_MACOS
        if (!m_startupProgressDialog->isVisible())
        {
            m_startupProgressDialog->show();
            m_startupProgressDialog->activateWindow();
            m_startupProgressDialog->raise();
        }
#else
        if (m_startupProgressDialog->isHidden())
        {
            // Make sure the window is not minimized
            m_startupProgressDialog->setWindowState((m_startupProgressDialog->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

            // Then show it
            m_startupProgressDialog->show();
            m_startupProgressDialog->raise();
            m_startupProgressDialog->activateWindow();
        }
        else
        {
            m_startupProgressDialog->hide();
        }
#endif
    });
}

void Application::askRecursiveTorrentDownloadConfirmation(const BitTorrent::Torrent *torrent)
{
    const auto torrentID = torrent->id();

    QMessageBox *confirmBox = new QMessageBox(QMessageBox::Question, tr("Recursive download confirmation")
            , tr("The torrent '%1' contains .torrent files, do you want to proceed with their downloads?").arg(torrent->name())
            , (QMessageBox::Yes | QMessageBox::No | QMessageBox::NoToAll), mainWindow());
    confirmBox->setAttribute(Qt::WA_DeleteOnClose);

    const QAbstractButton *yesButton = confirmBox->button(QMessageBox::Yes);
    QAbstractButton *neverButton = confirmBox->button(QMessageBox::NoToAll);
    neverButton->setText(tr("Never"));

    connect(confirmBox, &QMessageBox::buttonClicked, this
            , [this, torrentID, yesButton, neverButton](const QAbstractButton *button)
    {
        if (button == yesButton)
        {
            recursiveTorrentDownload(torrentID);
        }
        else if (button == neverButton)
        {
            Preferences::instance()->setRecursiveDownloadEnabled(false);
        }
    });
    confirmBox->open();
}

void Application::recursiveTorrentDownload(const BitTorrent::TorrentID &torrentID)
{
    const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->getTorrent(torrentID);
    if (!torrent)
        return;

    for (const Path &torrentRelpath : asConst(torrent->filePaths()))
    {
        if (torrentRelpath.hasExtension(u".torrent"_s))
        {
            const Path torrentFullpath = torrent->savePath() / torrentRelpath;

            LogMsg(tr("Recursive download .torrent file within torrent. Source torrent: \"%1\". File: \"%2\"")
                    .arg(torrent->name(), torrentFullpath.toString()));

            BitTorrent::AddTorrentParams params;
            // Passing the save path along to the sub torrent file
            params.savePath = torrent->savePath();
            addTorrentManager()->addTorrent(torrentFullpath.data(), params, AddTorrentOption::SkipDialog);
        }
    }
}

#ifdef Q_OS_MACOS
bool Application::event(QEvent *ev)
{
    if (ev->type() == QEvent::FileOpen)
    {
        QString path = static_cast<QFileOpenEvent *>(ev)->file();
        if (path.isEmpty())
            // Get the url instead
            path = static_cast<QFileOpenEvent *>(ev)->url().toString();
        qDebug("Received a mac file open event: %s", qUtf8Printable(path));

        QBtCommandLineParameters params;
        params.torrentSources.append(path);
        // If Application is not allowed to process params immediately
        // (i.e., other components are not ready) store params
        if (m_isProcessingParamsAllowed)
            processParams(params);
        else
            m_paramsQueue.append(params);

        return true;
    }

    return BaseApplication::event(ev);
}
#endif // Q_OS_MACOS
#endif // DISABLE_GUI

void Application::initializeTranslation()
{
    Preferences *const pref = Preferences::instance();
    // Load translation
    const QString localeStr = pref->getLocale();

    if (m_qtTranslator.load((u"qtbase_" + localeStr), QLibraryInfo::path(QLibraryInfo::TranslationsPath))
        || m_qtTranslator.load((u"qt_" + localeStr), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        qDebug("Qt %s locale recognized, using translation.", qUtf8Printable(localeStr));
    }
    else
    {
        qDebug("Qt %s locale unrecognized, using default (en).", qUtf8Printable(localeStr));
    }

    installTranslator(&m_qtTranslator);

    if (m_translator.load(u":/lang/qbittorrent_" + localeStr))
        qDebug("%s locale recognized, using translation.", qUtf8Printable(localeStr));
    else
        qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(localeStr));
    installTranslator(&m_translator);

#ifndef DISABLE_GUI
    if (localeStr.startsWith(u"ar") || localeStr.startsWith(u"he"))
    {
        qDebug("Right to Left mode");
        setLayoutDirection(Qt::RightToLeft);
    }
    else
    {
        setLayoutDirection(Qt::LeftToRight);
    }
#endif
}

#if (!defined(DISABLE_GUI) && defined(Q_OS_WIN))
void Application::shutdownCleanup([[maybe_unused]] QSessionManager &manager)
{
    // This is only needed for a special case on Windows XP.
    // (but is called for every Windows version)
    // If a process takes too much time to exit during OS
    // shutdown, the OS presents a dialog to the user.
    // That dialog tells the user that qbt is blocking the
    // shutdown, it shows a progress bar and it offers
    // a "Terminate Now" button for the user. However,
    // after the progress bar has reached 100% another button
    // is offered to the user reading "Cancel". With this the
    // user can cancel the **OS** shutdown. If we don't do
    // the cleanup by handling the commitDataRequest() signal
    // and the user clicks "Cancel", it will result in qbt being
    // killed and the shutdown proceeding instead. Apparently
    // aboutToQuit() is emitted too late in the shutdown process.
    cleanup();

    // According to the qt docs we shouldn't call quit() inside a slot.
    // aboutToQuit() is never emitted if the user hits "Cancel" in
    // the above dialog.
    QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}
#endif

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
void Application::applyMemoryWorkingSetLimit() const
{
    const size_t MiB = 1024 * 1024;
    const QString logMessage = tr("Failed to set physical memory (RAM) usage limit. Error code: %1. Error message: \"%2\"");

#ifdef Q_OS_WIN
    const SIZE_T maxSize = memoryWorkingSetLimit() * MiB;
    const auto minSize = std::min<SIZE_T>((64 * MiB), (maxSize / 2));
    if (!::SetProcessWorkingSetSizeEx(::GetCurrentProcess(), minSize, maxSize, QUOTA_LIMITS_HARDWS_MAX_ENABLE))
    {
        const DWORD errorCode = ::GetLastError();
        QString message;
        LPVOID lpMsgBuf = nullptr;
        const DWORD msgLength = ::FormatMessageW((FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS)
            , nullptr, errorCode, LANG_USER_DEFAULT, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, nullptr);
        if (msgLength > 0)
        {
            message = QString::fromWCharArray(reinterpret_cast<LPWSTR>(lpMsgBuf)).trimmed();
            ::LocalFree(lpMsgBuf);
        }
        LogMsg(logMessage.arg(QString::number(errorCode), message), Log::WARNING);
    }
#elif defined(Q_OS_UNIX)
    // has no effect on linux but it might be meaningful for other OS
    rlimit limit {};

    if (::getrlimit(RLIMIT_RSS, &limit) != 0)
        return;

    const size_t newSize = memoryWorkingSetLimit() * MiB;
    if (newSize > limit.rlim_max)
    {
        // try to raise the hard limit
        rlimit newLimit = limit;
        newLimit.rlim_max = newSize;
        if (::setrlimit(RLIMIT_RSS, &newLimit) != 0)
        {
            const auto message = QString::fromLocal8Bit(strerror(errno));
            LogMsg(tr("Failed to set physical memory (RAM) usage hard limit. Requested size: %1. System hard limit: %2. Error code: %3. Error message: \"%4\"")
                .arg(QString::number(newSize), QString::number(limit.rlim_max), QString::number(errno), message), Log::WARNING);
            return;
        }
    }

    limit.rlim_cur = newSize;
    if (::setrlimit(RLIMIT_RSS, &limit) != 0)
    {
        const auto message = QString::fromLocal8Bit(strerror(errno));
        LogMsg(logMessage.arg(QString::number(errno), message), Log::WARNING);
    }
#endif
}
#endif

#ifdef Q_OS_WIN
MemoryPriority Application::processMemoryPriority() const
{
    return m_processMemoryPriority.get(MemoryPriority::BelowNormal);
}

void Application::setProcessMemoryPriority(const MemoryPriority priority)
{
    if (processMemoryPriority() == priority)
        return;

    m_processMemoryPriority = priority;
    applyMemoryPriority();
}

void Application::applyMemoryPriority() const
{
    using SETPROCESSINFORMATION = BOOL (WINAPI *)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setProcessInformation = Utils::OS::loadWinAPI<SETPROCESSINFORMATION>(u"Kernel32.dll"_s, "SetProcessInformation");
    if (!setProcessInformation)  // only available on Windows >= 8
        return;

    using SETTHREADINFORMATION = BOOL (WINAPI *)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setThreadInformation = Utils::OS::loadWinAPI<SETTHREADINFORMATION>(u"Kernel32.dll"_s, "SetThreadInformation");
    if (!setThreadInformation)  // only available on Windows >= 8
        return;

#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    // this dummy struct is required to compile successfully when targeting older Windows version
    struct MEMORY_PRIORITY_INFORMATION
    {
        ULONG MemoryPriority;
    };

#define MEMORY_PRIORITY_LOWEST 0
#define MEMORY_PRIORITY_VERY_LOW 1
#define MEMORY_PRIORITY_LOW 2
#define MEMORY_PRIORITY_MEDIUM 3
#define MEMORY_PRIORITY_BELOW_NORMAL 4
#define MEMORY_PRIORITY_NORMAL 5
#endif

    MEMORY_PRIORITY_INFORMATION prioInfo {};
    switch (processMemoryPriority())
    {
    case MemoryPriority::Normal:
    default:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_NORMAL;
        break;
    case MemoryPriority::BelowNormal:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_BELOW_NORMAL;
        break;
    case MemoryPriority::Medium:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_MEDIUM;
        break;
    case MemoryPriority::Low:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_LOW;
        break;
    case MemoryPriority::VeryLow:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_VERY_LOW;
        break;
    }
    setProcessInformation(::GetCurrentProcess(), ProcessMemoryPriority, &prioInfo, sizeof(prioInfo));

    // To avoid thrashing/sluggishness of the app, set "main event loop" thread to normal memory priority
    // which is higher/equal than other threads
    prioInfo.MemoryPriority = MEMORY_PRIORITY_NORMAL;
    setThreadInformation(::GetCurrentThread(), ThreadMemoryPriority, &prioInfo, sizeof(prioInfo));
}

void Application::adjustThreadPriority() const
{
    // Workaround for improving responsiveness of qbt when CPU resources are scarce.
    // Raise main event loop thread to be just one level higher than libtorrent threads.
    // Also note that on *nix platforms there is no easy way to achieve it,
    // so implementation is omitted.

    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}
#endif

void Application::cleanup()
{
    // cleanup() can be called multiple times during shutdown. We only need it once.
    if (m_isCleanupRun.exchange(true, std::memory_order_acquire))
        return;

    LogMsg(tr("qBittorrent termination initiated"));

#ifndef DISABLE_GUI
    if (m_desktopIntegration)
    {
        m_desktopIntegration->disconnect();
        m_desktopIntegration->setToolTip(tr("qBittorrent is shutting down..."));
        if (m_desktopIntegration->menu())
            m_desktopIntegration->menu()->setEnabled(false);
    }

    if (m_window)
    {
        // Hide the window and don't leave it on screen as
        // unresponsive. Also for Windows take the WinId
        // after it's hidden, because hide() may cause a
        // WinId change.
        m_window->hide();

#ifdef Q_OS_WIN
        const std::wstring msg = tr("Saving torrent progress...").toStdWString();
        ::ShutdownBlockReasonCreate(reinterpret_cast<HWND>(m_window->effectiveWinId())
            , msg.c_str());
#endif // Q_OS_WIN

        // Do manual cleanup in MainWindow to force widgets
        // to save their Preferences, stop all timers and
        // delete as many widgets as possible to leave only
        // a 'shell' MainWindow.
        // We need a valid window handle for Windows Vista+
        // otherwise the system shutdown will continue even
        // though we created a ShutdownBlockReason
        m_window->cleanup();
    }
#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
    delete m_webui;
#endif

    delete RSS::AutoDownloader::instance();
    delete RSS::Session::instance();

    TorrentFilesWatcher::freeInstance();
    delete m_addTorrentManager;
    BitTorrent::Session::freeInstance();
    Net::GeoIPManager::freeInstance();
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    SearchPluginManager::freeInstance();
    Utils::Fs::removeDirRecursively(Utils::Fs::tempPath());

    LogMsg(tr("qBittorrent is now ready to exit"));
    Logger::freeInstance();
    delete m_fileLogger;

#ifndef DISABLE_GUI
    if (m_window)
    {
#ifdef Q_OS_WIN
        ::ShutdownBlockReasonDestroy(reinterpret_cast<HWND>(m_window->effectiveWinId()));
#endif // Q_OS_WIN
        delete m_window;
        delete m_desktopIntegration;
        UIThemeManager::freeInstance();
    }
#endif // DISABLE_GUI

    Profile::freeInstance();

    if (m_shutdownAct != ShutdownDialogAction::Exit)
    {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        Utils::OS::shutdownComputer(m_shutdownAct);
    }
}

AddTorrentManagerImpl *Application::addTorrentManager() const
{
    return m_addTorrentManager;
}

#ifndef DISABLE_WEBUI
WebUI *Application::webUI() const
{
    return m_webui;
}
#endif
