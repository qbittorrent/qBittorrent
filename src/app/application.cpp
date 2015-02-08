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

#ifndef DISABLE_GUI
#include "gui/guiiconprovider.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#include <QSharedMemory>
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
#include "../webui/webui.h"
#endif

#include "application.h"
#include "core/logger.h"
#include "core/preferences.h"
#include "core/misc.h"
#include "core/iconprovider.h"
#include "core/scanfoldersmodel.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrenthandle.h"

static const char PARAMS_SEPARATOR[] = "|";

Application::Application(const QString &id, int &argc, char **argv)
    : BaseApplication(id, argc, argv)
    , m_running(false)
#ifndef DISABLE_GUI
    , m_shutdownAct(NO_SHUTDOWN)
#endif
{
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
#endif

    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(processMessage(const QString &)));
    connect(this, SIGNAL(aboutToQuit()), SLOT(cleanup()));
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

void Application::torrentFinished(BitTorrent::TorrentHandle *const torrent)
{
    Preferences *const pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunEnabled())
        misc::autoRunExternalProgram(pref->getAutoRunProgram().trimmed(), torrent);

    // Mail notification
    if (pref->isMailNotificationEnabled())
        misc::sendNotificationEmail(pref->getMailNotificationEmail(), torrent);

#ifndef DISABLE_GUI
    bool will_shutdown = (pref->shutdownWhenDownloadsComplete()
                          || pref->shutdownqBTWhenDownloadsComplete()
                          || pref->suspendWhenDownloadsComplete()
                          || pref->hibernateWhenDownloadsComplete())
            && !BitTorrent::Session::instance()->hasDownloadingTorrents();

    // Auto-Shutdown
    if (will_shutdown) {
        bool suspend = pref->suspendWhenDownloadsComplete();
        bool hibernate = pref->hibernateWhenDownloadsComplete();
        bool shutdown = pref->shutdownWhenDownloadsComplete();

        // Confirm shutdown
        ShutDownAction action = NO_SHUTDOWN;
        if (suspend)
            action = SUSPEND_COMPUTER;
        else if (hibernate)
            action = HIBERNATE_COMPUTER;
        else if (shutdown)
            action = SHUTDOWN_COMPUTER;

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
#ifdef DISABLE_GUI
    IconProvider::initInstance(new IconProvider(this));
#else
    IconProvider::initInstance(new GuiIconProvider(this));
#endif
    BitTorrent::Session::initInstance(this);
    connect(BitTorrent::Session::instance(), SIGNAL(torrentFinished(BitTorrent::TorrentHandle *const)), SLOT(torrentFinished(BitTorrent::TorrentHandle *const)));

    ScanFoldersModel::initInstance(this);

#ifndef DISABLE_WEBUI
    m_webui = new WebUI;
#endif

#ifdef DISABLE_GUI
#ifndef DISABLE_WEBUI
    Preferences* const pref = Preferences::instance();
    if (pref->isWebUiEnabled()) {
        // Display some information to the user
        std::cout << std::endl << "******** " << qPrintable(tr("Information")) << " ********" << std::endl;
        std::cout << qPrintable(tr("To control qBittorrent, access the Web UI at http://localhost:%1").arg(QString::number(pref->getWebUiPort()))) << std::endl;
        std::cout << qPrintable(tr("The Web UI administrator user name is: %1").arg(pref->getWebUiUsername())) << std::endl;
        qDebug() << "Password:" << pref->getWebUiPassword();
        if (pref->getWebUiPassword() == "f6fdffe48c908deb0f4c3bd36c032e72") {
            std::cout << qPrintable(tr("The Web UI administrator password is still the default one: %1").arg("adminadmin")) << std::endl;
            std::cout << qPrintable(tr("This is a security risk, please consider changing your password from program preferences.")) << std::endl;
        }
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
        if (running_)
            processParams(QStringList(path));
        else
            paramsQueue_.append(path);
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

void Application::cleanup()
{
#ifndef DISABLE_GUI
    delete m_window;
#endif
#ifndef DISABLE_WEBUI
    delete m_webui;
#endif
    ScanFoldersModel::freeInstance();
    BitTorrent::Session::freeInstance();
    Preferences::freeInstance();
    Logger::freeInstance();
    IconProvider::freeInstance();
#ifndef DISABLE_GUI
    if (m_shutdownAct != NO_SHUTDOWN) {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        misc::shutdownComputer(m_shutdownAct);
    }
#endif
}
