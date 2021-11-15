/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QPointer>
#include <QStringList>
#include <QTranslator>

#ifndef DISABLE_GUI
#include <QApplication>
using BaseApplication = QApplication;
class MainWindow;

#ifdef Q_OS_WIN
class QSessionManager;
#endif // Q_OS_WIN

#else
#include <QCoreApplication>
using BaseApplication = QCoreApplication;
#endif // DISABLE_GUI

#include "base/path.h"
#include "base/settingvalue.h"
#include "base/types.h"
#include "cmdoptions.h"

#ifndef DISABLE_WEBUI
class WebUI;
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

class Application final : public BaseApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)

public:
    Application(int &argc, char **argv);
    ~Application() override;

    bool isRunning();
    int exec(const QStringList &params);
    bool sendParams(const QStringList &params);

#ifndef DISABLE_GUI
    QPointer<MainWindow> mainWindow();
#endif

    const QBtCommandLineParameters &commandLineArgs() const;

    // FileLogger properties
    bool isFileLoggerEnabled() const;
    void setFileLoggerEnabled(bool value);
    Path fileLoggerPath() const;
    void setFileLoggerPath(const Path &path);
    bool isFileLoggerBackup() const;
    void setFileLoggerBackup(bool value);
    bool isFileLoggerDeleteOld() const;
    void setFileLoggerDeleteOld(bool value);
    int fileLoggerMaxSize() const;
    void setFileLoggerMaxSize(int bytes);
    int fileLoggerAge() const;
    void setFileLoggerAge(int value);
    int fileLoggerAgeType() const;
    void setFileLoggerAgeType(int value);

protected:
#ifndef DISABLE_GUI
#ifdef Q_OS_MACOS
    bool event(QEvent *) override;
#endif
#endif

private slots:
    void processMessage(const QString &message);
    void torrentFinished(BitTorrent::Torrent *const torrent);
    void allTorrentsFinished();
    void cleanup();
#if (!defined(DISABLE_GUI) && defined(Q_OS_WIN))
    void shutdownCleanup(QSessionManager &manager);
#endif

private:
    void initializeTranslation();
    void processParams(const QStringList &params);
    void runExternalProgram(const BitTorrent::Torrent *torrent) const;
    void sendNotificationEmail(const BitTorrent::Torrent *torrent);

    ApplicationInstanceManager *m_instanceManager = nullptr;
    bool m_running;
    ShutdownDialogAction m_shutdownAct;
    QBtCommandLineParameters m_commandLineArgs;

#ifndef DISABLE_GUI
    QPointer<MainWindow> m_window;
#endif

#ifndef DISABLE_WEBUI
    WebUI *m_webui = nullptr;
#endif

    // FileLog
    QPointer<FileLogger> m_fileLogger;

    QTranslator m_qtTranslator;
    QTranslator m_translator;
    QStringList m_paramsQueue;

    SettingValue<bool> m_storeFileLoggerEnabled;
    SettingValue<bool> m_storeFileLoggerBackup;
    SettingValue<bool> m_storeFileLoggerDeleteOld;
    SettingValue<int> m_storeFileLoggerMaxSize;
    SettingValue<int> m_storeFileLoggerAge;
    SettingValue<int> m_storeFileLoggerAgeType;
    SettingValue<Path> m_storeFileLoggerPath;
};
