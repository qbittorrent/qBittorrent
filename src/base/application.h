/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
 * Copyright (C) 2015, 2019, 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtSystemDetection>
#include <QMetaObject>

#include "base/addtorrentmanager.h"

#define qBt (Application::instance())

class QString;

class Path;
struct QBtCommandLineParameters;

namespace BitTorrent
{
    class Session;
}

namespace Net
{
    class DownloadManager;
    class GeoIPManager;
    class PortForwarder;
    class ProxyConfigurationManager;
}

namespace RSS
{
    class AutoDownloader;
    class Session;
}

class IconProvider;
class Logger;
class Preferences;
class Profile;
class SearchPluginManager;
class SettingsStorage;
class TorrentFilesWatcher;

#ifdef Q_OS_WIN
inline namespace ApplicationSettingsEnums
{
    Q_NAMESPACE

    enum class MemoryPriority : int
    {
        Normal = 0,
        BelowNormal = 1,
        Medium = 2,
        Low = 3,
        VeryLow = 4
    };
    Q_ENUM_NS(MemoryPriority)
}
#endif

class Application
{
public:
    virtual ~Application() = default;

    // FileLogger properties
    virtual bool isFileLoggerEnabled() const = 0;
    virtual void setFileLoggerEnabled(bool value) = 0;
    virtual Path fileLoggerPath() const = 0;
    virtual void setFileLoggerPath(const Path &path) = 0;
    virtual bool isFileLoggerBackup() const = 0;
    virtual void setFileLoggerBackup(bool value) = 0;
    virtual bool isFileLoggerDeleteOld() const = 0;
    virtual void setFileLoggerDeleteOld(bool value) = 0;
    virtual int fileLoggerMaxSize() const = 0;
    virtual void setFileLoggerMaxSize(int bytes) = 0;
    virtual int fileLoggerAge() const = 0;
    virtual void setFileLoggerAge(int value) = 0;
    virtual int fileLoggerAgeType() const = 0;
    virtual void setFileLoggerAgeType(int value) = 0;

    virtual int memoryWorkingSetLimit() const = 0;
    virtual void setMemoryWorkingSetLimit(int size) = 0;

#ifdef Q_OS_WIN
    virtual MemoryPriority processMemoryPriority() const = 0;
    virtual void setProcessMemoryPriority(MemoryPriority priority) = 0;
#endif

    virtual AddTorrentManager *addTorrentManager() const = 0;
    virtual BitTorrent::Session *btSession() const = 0;
    virtual Net::DownloadManager *downloadManager() const = 0;
    virtual Net::GeoIPManager *geoIPManager() const = 0;
    virtual IconProvider *iconProvider() const = 0;
    virtual Logger *logger() const = 0;
    virtual Net::PortForwarder *portForwarder() const = 0;
    virtual Preferences *preferences() const = 0;
    virtual Profile *profile() const = 0;
    virtual Net::ProxyConfigurationManager *proxyConfigurationManager() const = 0;
    virtual RSS::AutoDownloader *rssAutoDownloader() const = 0;
    virtual RSS::Session *rssSession() const = 0;
    virtual SearchPluginManager *searchPluginManager() const = 0;
    virtual SettingsStorage *settings() const = 0;
    virtual TorrentFilesWatcher *torrentFilesWatcher() const = 0;

    static Application *instance();
};
