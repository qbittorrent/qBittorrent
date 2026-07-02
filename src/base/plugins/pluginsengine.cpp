/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "pluginsengine.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QScopeGuard>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "plugin.h"

using namespace Qt::Literals::StringLiterals;

const QString CONF_FILE_NAME = u"plugins.json"_s;
const QString OPTION_ENABLED = u"enabled"_s;

namespace
{
    struct PluginEntryConfig
    {
        bool enabled = false;
    };

    PluginEntryConfig parsePluginEntryConfig(const QJsonObject &jsonObj)
    {
        return {.enabled = jsonObj.value(OPTION_ENABLED).toBool()};
    }

    QJsonObject serializePluginEntryConfig(const PluginEntryConfig &config)
    {
        return {{OPTION_ENABLED, config.enabled}};
    }
}

void PluginsEngine::initInstance()
{
    if (!PluginsEngine::m_instance)
        PluginsEngine::m_instance = new PluginsEngine;
}

void PluginsEngine::freeInstance()
{
    delete PluginsEngine::m_instance;
    PluginsEngine::m_instance = nullptr;
}

PluginsEngine *PluginsEngine::instance()
{
    return PluginsEngine::m_instance;
}

LuaVersion PluginsEngine::luaVersion()
{
    return LuaVersion(LUA_VERSION_MAJOR_N, LUA_VERSION_MINOR_N, LUA_VERSION_RELEASE_N);
}

LuaBridgeVersion PluginsEngine::luaBridgeVersion()
{
    // NOTE: Need to update it whenever LuaBridge built-in sources are updated to new version.
    return LuaBridgeVersion(3, 0);
}

PluginsEngine::PluginsEngine(QObject *parent)
    : QObject(parent)
{
    loadPlugins();
    connectEventHandlers();
}

PluginsEngine::~PluginsEngine()
{
    if (m_configIsDirty)
        storeConfig();
}

void PluginsEngine::loadConfig()
{
    const int fileMaxSize = 10 * 1024 * 1024;
    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);

    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        LogMsg(tr("Failed to load plugins configuration. %1").arg(readResult.error().message), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse plugins configuration from %1. Error: \"%2\"")
                .arg(path.toString(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load plugins configuration from %1. Error: \"Invalid data format.\"")
                .arg(path.toString()), Log::WARNING);
        return;
    }

    const QJsonObject jsonObj = jsonDoc.object();
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        const QString pluginID = it.key();
        const PluginEntryConfig pluginEntryConfig = parsePluginEntryConfig(it.value().toObject());
        if (const auto pluginsIter = m_plugins.find(pluginID); pluginsIter != m_plugins.end())
            pluginsIter->enabled = pluginEntryConfig.enabled;
    }
}

void PluginsEngine::storeConfig() const
{
    QJsonObject jsonObj;
    for (const auto &[pluginID, pluginEntry] : asConst(m_plugins).asKeyValueRange())
    {
        jsonObj[pluginID] = serializePluginEntryConfig({.enabled = pluginEntry.enabled});
    }

    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);
    const QByteArray data = QJsonDocument(jsonObj).toJson();
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, data);
    if (!result)
    {
        LogMsg(tr("Couldn't store plugins configuration to %1. Error: %2")
                .arg(path.toString(), result.error()), Log::WARNING);
    }

    m_configIsDirty = false;
}

PluginInfo PluginsEngine::getPluginInfo(const PluginEntry &pluginEntry) const
{
    return {
        .id = pluginEntry.id,
        .name = pluginEntry.plugin->name(),
        .version = pluginEntry.plugin->version(),
        .invocable = pluginEntry.plugin->isInvocable(),
        .enabled = pluginEntry.enabled
    };
}

Path PluginsEngine::pluginsPath()
{
    return specialFolderLocation(SpecialFolder::Data) / Path(u"plugins"_s);
}

QHash<QString, PluginInfo> PluginsEngine::allPlugins() const
{
    QHash<QString, PluginInfo> plugins;
    plugins.reserve(m_plugins.size());
    for (const auto &[pluginID, pluginEntry] : asConst(m_plugins).asKeyValueRange())
        plugins.emplace(pluginID, getPluginInfo(pluginEntry));

    return plugins;
}

std::optional<PluginInfo> PluginsEngine::pluginInfo(const QString &pluginID) const
{
    const auto pluginIter = m_plugins.constFind(pluginID);
    if (pluginIter == m_plugins.constEnd())
        return std::nullopt;

    return getPluginInfo(*pluginIter);
}

bool PluginsEngine::installPlugin(const Path &pluginPath)
{
    if (!pluginPath.isAbsolute())
    {
        const QString message = tr("'%1' is invalid plugin path. An absolute path is expected.").arg(pluginPath.toString());
        LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
        emit pluginInstallationFailed(pluginPath, message);
        return false;
    }

    if (m_pluginsToInstall.contains(pluginPath))
    {
        const QString message = tr("'%1' is already scheduled to install.").arg(pluginPath.toString());
        LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
        emit pluginInstallationFailed(pluginPath, message);
        return false;
    }

    m_pluginsToInstall.enqueue(pluginPath);
    if (m_pluginsToInstall.size() == 1)
        installPluginDeferred();

    return true;
}

void PluginsEngine::installPluginDeferred()
{
    if (!m_pluginsToInstall.isEmpty())
        QMetaObject::invokeMethod(this, &PluginsEngine::installPluginImpl, Qt::QueuedConnection);
}

void PluginsEngine::installPluginImpl()
{
    if (m_pluginsToInstall.isEmpty())
        return;

    const Path pluginPath = m_pluginsToInstall.dequeue();
    installPluginDeferred();

    auto result = loadPlugin(pluginPath);
    if (!result)
    {
        LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), result.error()), Log::WARNING);
        emit pluginInstallationFailed(pluginPath, result.error());
        return;
    }

    PluginEntry pluginEntry = std::move(result).value();

    const QString pluginID = pluginEntry.id;

    const auto [isExistingPlugin, existingPluginEntry] = std::invoke([this, pluginID]() -> std::pair<bool, PluginEntry>
    {
        const auto iter = m_plugins.constFind(pluginID);
        if (iter == m_plugins.constEnd())
            return std::make_pair(false, PluginEntry());

        return std::make_pair(true, *iter);
    });

    const Path targetPluginPath = pluginsPath() / Path(pluginPath.filename());

    if (isExistingPlugin)
    {
        if (pluginEntry.plugin->version() == existingPluginEntry.plugin->version())
        {
            const QString message = tr("The same version of plugin '%1' is already installed.").arg(pluginID);
            LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginUpdateFailed(pluginPath, getPluginInfo(existingPluginEntry), getPluginInfo(pluginEntry), message);
            return;
        }

        if (pluginEntry.plugin->version() < existingPluginEntry.plugin->version())
        {
            const QString message = tr("The newest version of plugin '%1' is already installed.").arg(pluginID);
            LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginUpdateFailed(pluginPath, getPluginInfo(existingPluginEntry), getPluginInfo(pluginEntry), message);
            return;
        }
    }

    if (!Utils::Fs::removeFile(targetPluginPath))
    {
        const QString message = tr("Couldn't remove file '%1'.").arg(targetPluginPath.toString());

        if (isExistingPlugin)
        {
            LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginUpdateFailed(pluginPath, getPluginInfo(existingPluginEntry), getPluginInfo(pluginEntry), message);
        }
        else
        {
            LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginInstallationFailed(pluginPath, message);
        }

        return;
    }

    if (!Utils::Fs::copyFile(pluginPath, targetPluginPath))
    {
        const QString message = tr("Couldn't copy file '%1' to plugins directory.").arg(pluginPath.toString());

        if (isExistingPlugin)
        {
            LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginUpdateFailed(pluginPath, getPluginInfo(existingPluginEntry), getPluginInfo(pluginEntry), message);
        }
        else
        {
            LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
            emit pluginInstallationFailed(pluginPath, message);
        }

        return;
    }

    if (isExistingPlugin)
        pluginEntry.enabled = existingPluginEntry.enabled;

    m_plugins.insert(pluginID, pluginEntry);

    if (isExistingPlugin)
    {
        LogMsg(tr("Updated plugin. ID: %1. Version: %2.").arg(pluginID, pluginEntry.plugin->version().toString()));
        emit pluginUpdated(pluginPath, getPluginInfo(existingPluginEntry), getPluginInfo(pluginEntry));
    }
    else
    {
        LogMsg(tr("Installed plugin. ID: %1. Version: %2.").arg(pluginID, pluginEntry.plugin->version().toString()));
        emit pluginInstalled(pluginPath, getPluginInfo(pluginEntry));
    }

    m_configIsDirty = true;
}

bool PluginsEngine::uninstallPlugin(const QString &pluginID)
{
    if (!m_plugins.contains(pluginID))
    {
        const QString message = tr("Plugin '%1' is not installed.").arg(pluginID);
        LogMsg(tr("Couldn't uninstall plugin. ID: %1. Reason: %2").arg(pluginID, message), Log::WARNING);
        emit pluginUninstallationFailed(pluginID, message);
        return false;
    }

    if (m_pluginsToUninstall.contains(pluginID))
    {
        const QString message = tr("Plugin '%1' is already scheduled to uninstall.").arg(pluginID);
        LogMsg(tr("Couldn't uninstall plugin. ID: %1. Reason: %2").arg(pluginID, message), Log::WARNING);
        emit pluginUninstallationFailed(pluginID, message);
        return false;
    }

    m_pluginsToUninstall.enqueue(pluginID);
    if (m_pluginsToUninstall.size() == 1)
        uninstallPluginDeferred();

    return true;
}

void PluginsEngine::uninstallPluginDeferred()
{
    if (!m_pluginsToUninstall.isEmpty())
        QMetaObject::invokeMethod(this, &PluginsEngine::uninstallPluginImpl, Qt::QueuedConnection);
    else
        storeConfig();
}

void PluginsEngine::uninstallPluginImpl()
{
    if (m_pluginsToUninstall.isEmpty())
        return;

    const QString pluginID = m_pluginsToUninstall.dequeue();
    if (auto result = Utils::Fs::removeFile(pluginsPath() / Path(pluginID + u".lua")))
    {
        const PluginEntry pluginEntry = m_plugins.take(pluginID);
        LogMsg(tr("Uninstalled plugin. ID: %1.").arg(pluginID));
        emit pluginUninstalled(pluginID);
        m_configIsDirty = true;
    }
    else
    {
        const QString message = result.error();
        LogMsg(tr("Couldn't uninstall plugin. ID: %1. Reason: %2").arg(pluginID, message), Log::WARNING);
        emit pluginUninstallationFailed(pluginID, message);
    }

    uninstallPluginDeferred();
}

void PluginsEngine::loadPlugins()
{
    const QDir pluginsDir {pluginsPath().data()};
    for (const QString &file : pluginsDir.entryList({u"*.lua"_s}))
    {
        if (auto result = loadPlugin(Path(pluginsDir.absoluteFilePath(file))))
        {
            PluginEntry &pluginEntry = result.value();
            const QString pluginID = pluginEntry.id;
            m_plugins.insert(pluginID, std::move(pluginEntry));
            LogMsg(tr("Loaded plugin. ID: %1.").arg(pluginID));
        }
        else
        {
            LogMsg(tr("Couldn't load plugin. File: %1. Reason: %2").arg(file, result.error()), Log::WARNING);
        }
    }

    loadConfig();
}

void PluginsEngine::connectEventHandlers()
{
    namespace BT = BitTorrent;

    connectEventHandler(&BT::Session::addTorrentFailed, "onAddTorrentFailed");
    connectEventHandler(&BT::Session::duplicateTorrentDetected, "onDuplicateTorrentDetected");
    connectEventHandler(&BT::Session::torrentsUpdated, "onTorrentsUpdated");

    connect(BT::Session::instance(), &BT::Session::torrentAdded, this, [this](BT::Torrent *torrent)
    {
        callEventHandlers("onTorrentAdded", torrent);

        if (torrent->hasMetadata())
            callEventHandlers("onTorrentMetadataReady", torrent);
    });

    connectEventHandler(&BT::Session::torrentMetadataReceived, "onTorrentMetadataReady");
    connectEventHandler(&BT::Session::torrentFinished, "onTorrentFinished");
    connectEventHandler(&BT::Session::torrentStarted, "onTorrentStarted");
    connectEventHandler(&BT::Session::torrentStopped, "onTorrentStopped");
    connectEventHandler(&BT::Session::torrentSavePathChanged, "onTorrentSavePathChanged");
    connectEventHandler(&BT::Session::torrentSavingModeChanged, "onTorrentSavingModeChanged");
    connectEventHandler(&BT::Session::torrentCategoryChanged, "onTorrentCategoryChanged");
    connectEventHandler(&BT::Session::torrentTagAdded, "onTorrentTagAdded");
    connectEventHandler(&BT::Session::torrentTagRemoved, "onTorrentTagRemoved");
    connectEventHandler(&BT::Session::torrentContentFileRenamed, "onTorrentContentFileRenamed");
    connectEventHandler(&BT::Session::torrentContentFolderRenamed, "onTorrentContentFolderRenamed");
    connectEventHandler(&BT::Session::torrentContentFolderRenamingFailed, "onTorrentContentFolderRenamingFailed");
    connectEventHandler(&BT::Session::torrentIOError, "onTorrentIOError");
    connectEventHandler(&BT::Session::torrentAboutToBeRemoved, "onTorrentAboutToBeRemoved");
    connectEventHandler(&BT::Session::trackerSuccess, "onTorrentAnnounceSuccess");
    connectEventHandler(&BT::Session::trackerWarning, "onTorrentAnnounceWarning");
    connectEventHandler(&BT::Session::trackerError, "onTorrentAnnounceError");
    connectEventHandler(&BT::Session::trackersAdded, "onTorrentTrackersAdded");
    connectEventHandler(&BT::Session::trackersRemoved, "onTorrentTrackersRemoved");
    connectEventHandler(&BT::Session::trackerEntryStatusesUpdated, "onTorrentTrackerStatusesUpdated");
}

template <typename Signal>
void PluginsEngine::connectEventHandler(Signal &&signal, const char *eventHandlerName)
{
    connect(BitTorrent::Session::instance(), std::forward<Signal>(signal), this, [this, eventHandlerName](auto&&... args)
    {
        this->callEventHandlers(eventHandlerName, std::forward<decltype(args)>(args)...);
    });
}

template <typename... Args>
void PluginsEngine::callEventHandlers(const char *eventHandlerName, Args&&... args)
{
    for (const PluginEntry &pluginEntry : asConst(m_plugins))
    {
        if (!pluginEntry.enabled)
            continue;

        try
        {
            pluginEntry.plugin->call(eventHandlerName, std::forward<Args>(args)...);
        }
        catch (const std::exception &ex)
        {
            LogMsg(tr("Failed to call the plugin. Plugin: %1. Reason: %2")
                    .arg(pluginEntry.id, QString::fromStdString(ex.what())), Log::WARNING);
        }
    }
}

void PluginsEngine::setPluginEnabled(const QString &pluginID, const bool enabled)
{
    Q_ASSERT(m_plugins.contains(pluginID));
    if (!m_plugins.contains(pluginID)) [[unlikely]]
        return;

    PluginEntry &pluginEntry = m_plugins[pluginID];
    if (pluginEntry.enabled != enabled)
    {
        pluginEntry.enabled = enabled;
        emit pluginEnabledChanged(pluginID, pluginEntry.enabled);
        storeConfig();
    }
}

void PluginsEngine::invokePlugin(const QString &pluginID)
{
    const auto pluginIter = m_plugins.constFind(pluginID);
    Q_ASSERT(pluginIter != m_plugins.constEnd());
    if (pluginIter == m_plugins.constEnd()) [[unlikely]]
        return;

    const PluginEntry &pluginEntry = *pluginIter;
    if (!pluginEntry.enabled)
        return;

    try
    {
        pluginEntry.plugin->invoke();
    }
    catch (const std::exception &ex)
    {
        LogMsg(tr("Failed to call the plugin. Plugin: %1. Reason: %2")
                .arg(pluginEntry.id, QString::fromStdString(ex.what())), Log::WARNING);
    }
}

nonstd::expected<PluginsEngine::PluginEntry, QString> PluginsEngine::loadPlugin(const Path &path)
{
    auto loadResult = Plugin::load(path);
    if (!loadResult)
        return loadResult.get_unexpected();

    PluginEntry pluginEntry {
        .plugin = loadResult.value(),
        .id = path.removedExtension().filename(),
    };

    return pluginEntry;
}
