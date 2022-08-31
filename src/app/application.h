/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
 * Copyright (C) 2015, 2019  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QtGlobal>
#include <QAtomicInt>
#include <QCoreApplication>
#include <QPointer>
#include <QStringList>
#include <QTranslator>

#ifndef DISABLE_GUI
#include <QApplication>
#endif

#include "base/bittorrent/addtorrentparams.h"
#include "base/interfaces/iapplication.h"
#include "base/path.h"
#include "base/settingvalue.h"
#include "base/types.h"
#include "cmdoptions.h"

#ifndef DISABLE_GUI
#include "gui/interfaces/iguiapplication.h"
#endif

class ApplicationInstanceManager;
class FileLogger;

namespace BitTorrent
{
    class Torrent;
}

namespace RSS
{
    class Session;
    class AutoDownloader;
}

#ifndef DISABLE_GUI
class QProgressDialog;

class DesktopIntegration;
class MainWindow;

using BaseApplication = QApplication;
using BaseIApplication = IGUIApplication;

#ifdef Q_OS_WIN
class QSessionManager;
#endif
#else // DISABLE_GUI
using BaseApplication = QCoreApplication;
using BaseIApplication = IApplication;
#endif // DISABLE_GUI

#ifndef DISABLE_WEBUI
class WebUI;
#endif

class Application final : public BaseApplication, public BaseIApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)

public:
    Application(int &argc, char **argv);
    ~Application() override;

    int exec(const QStringList &params);

    bool isRunning();
    bool sendParams(const QStringList &params);
    const QBtCommandLineParameters &commandLineArgs() const;

    // FileLogger properties
    bool isFileLoggerEnabled() const override;
    void setFileLoggerEnabled(bool value) override;
    Path fileLoggerPath() const override;
    void setFileLoggerPath(const Path &path) override;
    bool isFileLoggerBackup() const override;
    void setFileLoggerBackup(bool value) override;
    bool isFileLoggerDeleteOld() const override;
    void setFileLoggerDeleteOld(bool value) override;
    int fileLoggerMaxSize() const override;
    void setFileLoggerMaxSize(int bytes) override;
    int fileLoggerAge() const override;
    void setFileLoggerAge(int value) override;
    int fileLoggerAgeType() const override;
    void setFileLoggerAgeType(int value) override;

    int memoryWorkingSetLimit() const override;
    void setMemoryWorkingSetLimit(int size) override;

#ifdef Q_OS_WIN
    MemoryPriority processMemoryPriority() const override;
    void setProcessMemoryPriority(MemoryPriority priority) override;
#endif

#ifndef DISABLE_GUI
    DesktopIntegration *desktopIntegration() override;
    MainWindow *mainWindow() override;

    bool isTorrentAddedNotificationsEnabled() const override;
    void setTorrentAddedNotificationsEnabled(bool value) override;
#endif

private slots:
    void processMessage(const QString &message);
    void torrentAdded(const BitTorrent::Torrent *torrent) const;
    void torrentFinished(const BitTorrent::Torrent *torrent);
    void allTorrentsFinished();
    void cleanup();

#if (!defined(DISABLE_GUI) && defined(Q_OS_WIN))
    void shutdownCleanup(QSessionManager &manager);
#endif

private:
    struct AddTorrentParams
    {
        QString torrentSource;
        BitTorrent::AddTorrentParams torrentParams;
        std::optional<bool> skipTorrentDialog;
    };

    void initializeTranslation();
    AddTorrentParams parseParams(const QStringList &params) const;
    void processParams(const AddTorrentParams &params);
    void runExternalProgram(const QString &programTemplate, const BitTorrent::Torrent *torrent) const;
    void sendNotificationEmail(const BitTorrent::Torrent *torrent);

#ifdef QBT_USES_LIBTORRENT2
    void applyMemoryWorkingSetLimit() const;
#endif

#ifdef Q_OS_WIN
    void applyMemoryPriority() const;
    void adjustThreadPriority() const;
#endif

#ifndef DISABLE_GUI
    void createStartupProgressDialog();
#ifdef Q_OS_MACOS
    bool event(QEvent *) override;
#endif
#endif

    ApplicationInstanceManager *m_instanceManager = nullptr;
    QAtomicInt m_isCleanupRun;
    bool m_isProcessingParamsAllowed = false;
    ShutdownDialogAction m_shutdownAct;
    QBtCommandLineParameters m_commandLineArgs;

    // FileLog
    QPointer<FileLogger> m_fileLogger;

    QTranslator m_qtTranslator;
    QTranslator m_translator;

    QList<AddTorrentParams> m_paramsQueue;

    SettingValue<bool> m_storeFileLoggerEnabled;
    SettingValue<bool> m_storeFileLoggerBackup;
    SettingValue<bool> m_storeFileLoggerDeleteOld;
    SettingValue<int> m_storeFileLoggerMaxSize;
    SettingValue<int> m_storeFileLoggerAge;
    SettingValue<int> m_storeFileLoggerAgeType;
    SettingValue<Path> m_storeFileLoggerPath;
    SettingValue<int> m_storeMemoryWorkingSetLimit;

#ifdef Q_OS_WIN
    SettingValue<MemoryPriority> m_processMemoryPriority;
#endif

#ifndef DISABLE_GUI
    SettingValue<bool> m_storeNotificationTorrentAdded;

    DesktopIntegration *m_desktopIntegration = nullptr;
    MainWindow *m_window = nullptr;
    QProgressDialog *m_startupProgressDialog = nullptr;
#endif

#ifndef DISABLE_WEBUI
    WebUI *m_webui = nullptr;
#endif
};
