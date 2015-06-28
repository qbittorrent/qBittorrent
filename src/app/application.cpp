/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QLibraryInfo>
#include <QSysInfo>
#include <QProcess>

#ifndef DISABLE_GUI
#include "gui/guiiconprovider.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#include <QSharedMemory>
#include <QSessionManager>
#endif // Q_OS_WIN
#ifdef Q_OS_MAC
#include <QFileOpenEvent>
#include <QFont>
#include <QUrl>
#endif // Q_OS_MAC
#include "mainwindow.h"
#include "addnewtorrentdialog.h"
#include "shutdownconfirm.h"
#else // DISABLE_GUI
#include <iostream>
#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
#include "webui/webui.h"
#endif

#include "application.h"
#include "core/logger.h"
#include "core/preferences.h"
#include "core/utils/misc.h"
#include "core/iconprovider.h"
#include "core/scanfoldersmodel.h"
#include "core/net/smtp.h"
#include "core/net/downloadmanager.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"

static const char PARAMS_SEPARATOR[] = "|";

Application::Application(const QString &id, int &argc, char **argv)
    : BaseApplication(id, argc, argv)
    , m_running(false)
#ifndef DISABLE_GUI
    , m_shutdownAct(ShutdownAction::None)
#endif
{
    Logger::initInstance();
    Preferences::initInstance();

#if defined(Q_OS_MACX) && !defined(DISABLE_GUI)
    if (QSysInfo::MacintoshVersion > QSysInfo::MV_10_8) {
        // fix Mac OS X 10.9 (mavericks) font issue
        // https://bugreports.qt-project.org/browse/QTBUG-32789
        QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
    }
#endif
    setApplicationName("qBittorrent");
    initializeTranslation();
#ifndef DISABLE_GUI
    setStyleSheet("QStatusBar::item { border-width: 0; }");
    setQuitOnLastWindowClosed(false);
#ifdef Q_OS_WIN
    connect(this, SIGNAL(commitDataRequest(QSessionManager &)), this, SLOT(shutdownCleanup(QSessionManager &)), Qt::DirectConnection);
#endif // Q_OS_WIN
#endif // DISABLE_GUI

    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(processMessage(const QString &)));
    connect(this, SIGNAL(aboutToQuit()), SLOT(cleanup()));

    Logger::instance()->addMessage(tr("qBittorrent %1 started", "qBittorrent v3.2.0alpha started").arg(VERSION));
}

void Application::processMessage(const QString &message)
{
    QStringList params = message.split(QLatin1String(PARAMS_SEPARATOR), QString::SkipEmptyParts);
    // If Application is not running (i.e., other
    // components are not ready) store params
    if (m_running)
        processParams(params);
    else
        m_paramsQueue.append(params);
}

void Application::sendNotificationEmail(BitTorrent::TorrentHandle *const torrent)
{
    // Prepare mail content
    QString content = QObject::tr("Torrent name: %1").arg(torrent->name()) + "\n";
    content += QObject::tr("Torrent size: %1").arg(Utils::Misc::friendlyUnit(torrent->wantedSize())) + "\n";
    content += QObject::tr("Save path: %1").arg(torrent->savePath()) + "\n\n";
    content += QObject::tr("The torrent was downloaded in %1.",
                  "The torrent was downloaded in 1 hour and 20 seconds")
            .arg(Utils::Misc::userFriendlyDuration(torrent->activeTime())) + "\n\n\n";
    content += QObject::tr("Thank you for using qBittorrent.") + "\n";

    // Send the notification email
    Net::Smtp *sender = new Net::Smtp;
    sender->sendMail("notification@qbittorrent.org",
                     Preferences::instance()->getMailNotificationEmail(),
                     QObject::tr("[qBittorrent] %1 has finished downloading").arg(torrent->name()),
                     content);
}

void Application::torrentFinished(BitTorrent::TorrentHandle *const torrent)
{
    Preferences *const pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunEnabled()) {
        QString program = pref->getAutoRunProgram().trimmed();
        // Replace %f by torrent path
        program.replace("%f", torrent->savePathParsed());
        // Replace %n by torrent name
        program.replace("%n", torrent->name());
        QProcess::startDetached(program);
    }

    // Mail notification
    if (pref->isMailNotificationEnabled())
        sendNotificationEmail(torrent);
}

void Application::allTorrentsFinished()
{
    Preferences *const pref = Preferences::instance();

#ifndef DISABLE_GUI
    bool will_shutdown = (pref->shutdownWhenDownloadsComplete()
                          || pref->shutdownqBTWhenDownloadsComplete()
                          || pref->suspendWhenDownloadsComplete()
                          || pref->hibernateWhenDownloadsComplete());

    // Auto-Shutdown
    if (will_shutdown) {
        bool suspend = pref->suspendWhenDownloadsComplete();
        bool hibernate = pref->hibernateWhenDownloadsComplete();
        bool shutdown = pref->shutdownWhenDownloadsComplete();

        // Confirm shutdown
        ShutdownAction action = ShutdownAction::None;
        if (suspend)
            action = ShutdownAction::Suspend;
        else if (hibernate)
            action = ShutdownAction::Hibernate;
        else if (shutdown)
            action = ShutdownAction::Shutdown;

        if (!ShutdownConfirmDlg::askForConfirmation(action)) return;

        // Actually shut down
        if (suspend || hibernate || shutdown) {
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
#endif // DISABLE_GUI
}

bool Application::sendParams(const QStringList &params)
{
    return sendMessage(params.join(QLatin1String(PARAMS_SEPARATOR)));
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void Application::processParams(const QStringList &params)
{
#ifndef DISABLE_GUI
    if (params.isEmpty()) {
        m_window->activate(); // show UI
        return;
    }
#endif

    foreach (QString param, params) {
        param = param.trimmed();
#ifndef DISABLE_GUI
        if (Preferences::instance()->useAdditionDialog())
            AddNewTorrentDialog::show(param, m_window);
        else
#endif
            BitTorrent::Session::instance()->addTorrent(param);
    }
}

int Application::exec(const QStringList &params)
{
    Net::DownloadManager::initInstance();
#ifdef DISABLE_GUI
    IconProvider::initInstance();
#else
    GuiIconProvider::initInstance();
#endif
    BitTorrent::Session::initInstance();
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle *const)), SLOT(torrentFinished(BitTorrent::TorrentHandle *const)));
    connect(BitTorrent::Session::instance(), SIGNAL(allTorrentsFinished()), SLOT(allTorrentsFinished()));

    ScanFoldersModel::initInstance(this);

#ifndef DISABLE_WEBUI
    m_webui = new WebUI;
#endif

#ifdef DISABLE_GUI
#ifndef DISABLE_WEBUI
    Preferences* const pref = Preferences::instance();
    // Display some information to the user
    std::cout << std::endl << "******** " << qPrintable(tr("Information")) << " ********" << std::endl;
    std::cout << qPrintable(tr("To control qBittorrent, access the Web UI at http://localhost:%1").arg(QString::number(pref->getWebUiPort()))) << std::endl;
    std::cout << qPrintable(tr("The Web UI administrator user name is: %1").arg(pref->getWebUiUsername())) << std::endl;
    qDebug() << "Password:" << pref->getWebUiPassword();
    if (pref->getWebUiPassword() == "f6fdffe48c908deb0f4c3bd36c032e72") {
        std::cout << qPrintable(tr("The Web UI administrator password is still the default one: %1").arg("adminadmin")) << std::endl;
        std::cout << qPrintable(tr("This is a security risk, please consider changing your password from program preferences.")) << std::endl;
    }
#endif // DISABLE_WEBUI
#else
    m_window = new MainWindow;
#endif // DISABLE_GUI

    m_running = true;
    m_paramsQueue = params + m_paramsQueue;
    if (!m_paramsQueue.isEmpty()) {
        processParams(m_paramsQueue);
        m_paramsQueue.clear();
    }

    return BaseApplication::exec();
}

#ifndef DISABLE_GUI
#ifdef Q_OS_WIN
bool Application::isRunning()
{
    bool running = BaseApplication::isRunning();
    QSharedMemory *sharedMem = new QSharedMemory(id() + QLatin1String("-shared-memory-key"), this);
    if (!running) {
        // First instance creates shared memory and store PID
        if (sharedMem->create(sizeof(DWORD)) && sharedMem->lock()) {
            *(static_cast<DWORD*>(sharedMem->data())) = ::GetCurrentProcessId();
            sharedMem->unlock();
        }
    }
    else {
        // Later instances attach to shared memory and retrieve PID
        if (sharedMem->attach() && sharedMem->lock()) {
            ::AllowSetForegroundWindow(*(static_cast<DWORD*>(sharedMem->data())));
            sharedMem->unlock();
        }
    }

    if (!sharedMem->isAttached())
        qWarning() << "Failed to initialize shared memory: " << sharedMem->errorString();

    return running;
}
#endif // Q_OS_WIN

#ifdef Q_OS_MAC
bool Application::event(QEvent *ev)
{
    if (ev->type() == QEvent::FileOpen) {
        QString path = static_cast<QFileOpenEvent *>(ev)->file();
        if (path.isEmpty())
            // Get the url instead
            path = static_cast<QFileOpenEvent *>(ev)->url().toString();
        qDebug("Received a mac file open event: %s", qPrintable(path));
        if (m_running)
            processParams(QStringList(path));
        else
            m_paramsQueue.append(path);
        return true;
    }
    else {
        return BaseApplication::event(ev);
    }
}
#endif // Q_OS_MAC

bool Application::notify(QObject *receiver, QEvent *event)
{
    try {
        return QApplication::notify(receiver, event);
    }
    catch (const std::exception &e) {
        qCritical() << "Exception thrown:" << e.what() << ", receiver: " << receiver->objectName();
        receiver->dumpObjectInfo();
    }

    return false;
}
#endif // DISABLE_GUI

void Application::initializeTranslation()
{
    Preferences* const pref = Preferences::instance();
    // Load translation
    QString locale = pref->getLocale();

    if (locale.isEmpty()) {
        locale = QLocale::system().name();
        pref->setLocale(locale);
    }

    if (m_qtTranslator.load(
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
            QString::fromUtf8("qtbase_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)) ||
        m_qtTranslator.load(
#endif
            QString::fromUtf8("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
            qDebug("Qt %s locale recognized, using translation.", qPrintable(locale));
    }
    else {
        qDebug("Qt %s locale unrecognized, using default (en).", qPrintable(locale));
    }
    installTranslator(&m_qtTranslator);

    if (m_translator.load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
        qDebug("%s locale recognized, using translation.", qPrintable(locale));
    }
    else {
        qDebug("%s locale unrecognized, using default (en).", qPrintable(locale));
    }
    installTranslator(&m_translator);

#ifndef DISABLE_GUI
    if (locale.startsWith("ar") || locale.startsWith("he")) {
        qDebug("Right to Left mode");
        setLayoutDirection(Qt::RightToLeft);
    }
    else {
        setLayoutDirection(Qt::LeftToRight);
    }
#endif
}

#if (!defined(DISABLE_GUI) && defined(Q_OS_WIN))
void Application::shutdownCleanup(QSessionManager &manager)
{
    Q_UNUSED(manager);

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
    QTimer::singleShot(0, qApp, SLOT(quit()));
}
#endif

void Application::cleanup()
{
#ifndef DISABLE_GUI
#ifdef Q_OS_WIN
    // cleanup() can be called multiple times during shutdown. We only need it once.
    static bool alreadyDone = false;

    if (alreadyDone)
        return;
    alreadyDone = true;
#endif // Q_OS_WIN

    // Hide the window and not leave it on screen as
    // unresponsive. Also for Windows take the WinId
    // after it's hidden, because hide() may cause a
    // WinId change.
    m_window->hide();

#ifdef Q_OS_WIN
    typedef BOOL (WINAPI *PSHUTDOWNBRCREATE)(HWND, LPCWSTR);
    PSHUTDOWNBRCREATE shutdownBRCreate = (PSHUTDOWNBRCREATE)::GetProcAddress(::GetModuleHandleW(L"User32.dll"), "ShutdownBlockReasonCreate");
    // Only available on Vista+
    if (shutdownBRCreate)
        shutdownBRCreate((HWND)m_window->effectiveWinId(), tr("Saving torrent progress...").toStdWString().c_str());
#endif // Q_OS_WIN

    // Do manual cleanup in MainWindow to force widgets
    // to save their Preferences, stop all timers and
    // delete as many widgets as possible to leave only
    // a 'shell' MainWindow.
    // We need a valid window handle for Windows Vista+
    // otherwise the system shutdown will continue even
    // though we created a ShutdownBlockReason
    m_window->cleanup();

#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
    delete m_webui;
#endif

    ScanFoldersModel::freeInstance();
    BitTorrent::Session::freeInstance();
    Preferences::freeInstance();
    Logger::freeInstance();
    IconProvider::freeInstance();
    Net::DownloadManager::freeInstance();
#ifndef DISABLE_GUI
#ifdef Q_OS_WIN
    typedef BOOL (WINAPI *PSHUTDOWNBRDESTROY)(HWND);
    PSHUTDOWNBRDESTROY shutdownBRDestroy = (PSHUTDOWNBRDESTROY)::GetProcAddress(::GetModuleHandleW(L"User32.dll"), "ShutdownBlockReasonDestroy");
    // Only available on Vista+
    if (shutdownBRDestroy)
        shutdownBRDestroy((HWND)m_window->effectiveWinId());
#endif // Q_OS_WIN
    delete m_window;
    if (m_shutdownAct != ShutdownAction::None) {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        Utils::Misc::shutdownComputer(m_shutdownAct);
    }
#endif
}
