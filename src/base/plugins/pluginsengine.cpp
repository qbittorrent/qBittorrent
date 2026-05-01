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

#include <lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>

#include "base/3rdparty/expected.hpp"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "luaclasses.h"
#include "luafunctions.h"
#include "luastack.h"

using namespace Qt::Literals::StringLiterals;

const QString CONF_FILE_NAME = u"plugins.json"_s;
const QString OPTION_ENABLED = u"enabled"_s;

namespace
{
    PluginInfo parsePluginInfo(const QJsonObject &jsonObj)
    {
        PluginInfo pluginInfo;
        pluginInfo.enabled = jsonObj.value(OPTION_ENABLED).toBool();

        return pluginInfo;
    }

    QJsonObject serializePluginInfo(const PluginInfo &pluginInfo)
    {
        return {{OPTION_ENABLED, pluginInfo.enabled}};
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
    const QHash<QString, PluginInfo> pluginsConfig = loadConfig();
    const QDir pluginsDir {pluginsPath().data()};
    for (const QString &file : pluginsDir.entryList({u"*.lua"_s}))
    {
        if (auto result = loadPlugin(Path(pluginsDir.absoluteFilePath(file))))
        {
            PluginInfo &pluginInfo = result.value().pluginInfo;
            const QString pluginID = pluginInfo.id;
            pluginInfo.enabled = pluginsConfig.value(pluginID).enabled;

            m_plugins.insert(pluginID, std::move(result).value());
            LogMsg(tr("Loaded plugin. ID: %1.").arg(pluginID));
        }
        else
        {
            LogMsg(tr("Couldn't load plugin. File: %1. Reason: %2").arg(file, result.error()), Log::WARNING);
        }
    }

    connectEventHandlers();
}

PluginsEngine::~PluginsEngine()
{
    if (m_configIsDirty)
        storeConfig();
}

QHash<QString, PluginInfo> PluginsEngine::loadConfig() const
{
    const int fileMaxSize = 10 * 1024 * 1024;
    const Path path = specialFolderLocation(SpecialFolder::Config) / Path(CONF_FILE_NAME);

    const auto readResult = Utils::IO::readFile(path, fileMaxSize);
    if (!readResult)
    {
        LogMsg(tr("Failed to load plugins configuration. %1").arg(readResult.error().message), Log::WARNING);
        return {};
    }

    QJsonParseError jsonError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(readResult.value(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("Failed to parse plugins configuration from %1. Error: \"%2\"")
                .arg(path.toString(), jsonError.errorString()), Log::WARNING);
        return {};
    }

    if (!jsonDoc.isObject())
    {
        LogMsg(tr("Failed to load plugins configuration from %1. Error: \"Invalid data format.\"")
                .arg(path.toString()), Log::WARNING);
        return {};
    }

    QHash<QString, PluginInfo> config;
    const QJsonObject jsonObj = jsonDoc.object();
    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it)
    {
        const QString pluginID = it.key();
        const PluginInfo pluginInfo = parsePluginInfo(it.value().toObject());
        config.insert(pluginID, pluginInfo);
    }

    return config;
}

void PluginsEngine::storeConfig() const
{
    QJsonObject jsonObj;
    for (const auto &[pluginID, pluginEntry] : asConst(m_plugins).asKeyValueRange())
    {
        jsonObj[pluginID] = serializePluginInfo(pluginEntry.pluginInfo);
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

Path PluginsEngine::pluginsPath()
{
    return specialFolderLocation(SpecialFolder::Data) / Path(u"plugins"_s);
}

QHash<QString, PluginInfo> PluginsEngine::allPlugins() const
{
    QHash<QString, PluginInfo> plugins;
    plugins.reserve(m_plugins.size());
    for (const auto &[pluginID, pluginEntry] : asConst(m_plugins.asKeyValueRange()))
        plugins.insert(pluginID, pluginEntry.pluginInfo);

    return plugins;
}

std::optional<PluginInfo> PluginsEngine::pluginInfo(const QString &pluginID) const
{
    const auto pluginIter = m_plugins.constFind(pluginID);
    if (pluginIter == m_plugins.constEnd())
        return std::nullopt;

    return pluginIter->pluginInfo;
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

    [[maybe_unused]] const auto scopeGuard = qScopeGuard([this] { installPluginDeferred(); });

    const Path pluginPath = m_pluginsToInstall.dequeue();

    if (auto result = loadPlugin(pluginPath))
    {
        const PluginInfo pluginInfo = result.value().pluginInfo;
        const QString pluginID = pluginInfo.id;

        const auto [isExistingPlugin, existingPluginInfo] = std::invoke([this, pluginID]() -> std::pair<bool, PluginInfo>
        {
            const auto iter = m_plugins.constFind(pluginID);
            if (iter == m_plugins.constEnd())
                return std::make_pair(false, PluginInfo());

            return std::make_pair(true, iter->pluginInfo);
        });

        const Path targetPluginPath = pluginsPath() / Path(pluginPath.filename());

        if (isExistingPlugin)
        {
            if (pluginInfo.version == existingPluginInfo.version)
            {
                const QString message = tr("The same version of plugin '%1' is already installed.").arg(pluginID);
                LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
                emit pluginUpdateFailed(pluginPath, existingPluginInfo, pluginInfo, message);
                return;
            }

            if (pluginInfo.version < existingPluginInfo.version)
            {
                const QString message = tr("The newest version of plugin '%1' is already installed.").arg(pluginID);
                LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
                emit pluginUpdateFailed(pluginPath, existingPluginInfo, pluginInfo, message);
                return;
            }

            if (!Utils::Fs::removeFile(targetPluginPath))
            {
                const QString message = tr("Couldn't remove file '%1'.").arg(targetPluginPath.toString());
                LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
                emit pluginUpdateFailed(pluginPath, existingPluginInfo, pluginInfo, message);
                return;
            }
        }

        if (!Utils::Fs::copyFile(pluginPath, targetPluginPath))
        {
            const QString message = tr("Couldn't copy file '%1' to plugins directory.").arg(pluginPath.toString());

            if (isExistingPlugin)
            {
                LogMsg(tr("Couldn't update plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
                emit pluginUpdateFailed(pluginPath, existingPluginInfo, pluginInfo, message);
            }
            else
            {
                LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), message), Log::WARNING);
                emit pluginInstallationFailed(pluginPath, message);
            }

            return;
        }

        PluginEntry pluginEntry = std::move(result).value();
        if (isExistingPlugin)
            pluginEntry.pluginInfo.enabled = existingPluginInfo.enabled;
        m_plugins.insert(pluginID, pluginEntry);

        if (isExistingPlugin)
        {
            LogMsg(tr("Updated plugin. ID: %1. Version: %2.").arg(pluginID, pluginInfo.version.toString()));
            emit pluginUpdated(pluginPath, existingPluginInfo, pluginInfo);
        }
        else
        {
            LogMsg(tr("Installed plugin. ID: %1. Version: %2.").arg(pluginID, pluginInfo.version.toString()));
            emit pluginInstalled(pluginPath, pluginInfo);
        }

        m_configIsDirty = true;
    }
    else
    {
        LogMsg(tr("Couldn't install plugin. File: %1. Reason: %2").arg(pluginPath.toString(), result.error()), Log::WARNING);
        emit pluginInstallationFailed(pluginPath, result.error());
    }
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
        m_plugins.remove(pluginID);
        LogMsg(tr("Uninstalled plugin. ID: %1.").arg(pluginID));
        emit pluginUninstalled(pluginID);
        m_configIsDirty = true;
    }
    else
    {
        LogMsg(tr("Couldn't uninstall plugin. ID: %1. Reason: %2").arg(pluginID, result.error()), Log::WARNING);
    }

    uninstallPluginDeferred();
}

void PluginsEngine::connectEventHandlers()
{
    namespace BT = BitTorrent;

    connectEventHandler(&BT::Session::addTorrentFailed, "onAddTorrentFailed");
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
    for (const PluginEntry &plugin : asConst(m_plugins))
    {
        if (!plugin.pluginInfo.enabled)
            continue;

        using namespace luabridge;
        try
        {
            const LuaRef func = getGlobal(plugin.luaState.get(), eventHandlerName);
            if (func.isFunction())
                func(std::forward<Args>(args)...);
        }
        catch (const LuaException &ex)
        {
            LogMsg(tr("Failed to call the plugin. Plugin: %1. Reason: %2")
                    .arg(plugin.pluginInfo.id, QString::fromStdString(ex.what())), Log::WARNING);
        }
    }
}

void PluginsEngine::setPluginEnabled(const QString &pluginID, const bool enabled)
{
    Q_ASSERT(m_plugins.contains(pluginID));
    if (!m_plugins.contains(pluginID)) [[unlikely]]
        return;

    PluginInfo &pluginInfo = m_plugins[pluginID].pluginInfo;
    if (pluginInfo.enabled != enabled)
    {
        pluginInfo.enabled = enabled;
        emit pluginEnabledChanged(pluginID, pluginInfo.enabled);
        storeConfig();
    }
}

void PluginsEngine::invokePlugin(const QString &pluginID)
{
    const auto pluginIter = m_plugins.constFind(pluginID);
    Q_ASSERT(pluginIter != m_plugins.constEnd());
    if (pluginIter == m_plugins.constEnd()) [[unlikely]]
        return;

    const PluginEntry &plugin = *pluginIter;
    if (!plugin.pluginInfo.enabled)
        return;

    using namespace luabridge;
    try
    {
        const LuaRef func = getGlobal(plugin.luaState.get(), "invoke");
        if (func.isFunction())
            func();
    }
    catch (const LuaException &ex)
    {
        LogMsg(tr("Failed to call the plugin. Plugin: %1. Reason: %2")
                .arg(plugin.pluginInfo.id, QString::fromStdString(ex.what())), Log::WARNING);
    }
}

nonstd::expected<PluginsEngine::PluginEntry, QString> PluginsEngine::loadPlugin(const Path &path)
{
    LuaStatePtr luaState {luaL_newstate(), &lua_close};
    if (!luaState)
        return nonstd::make_unexpected(tr("Failed to allocate Lua state."));

    const auto result = Utils::IO::readFile(path, 1024 * 1024);
    if (!result)
        return nonstd::make_unexpected(result.error().message);

    using namespace luabridge;

    enableExceptions(luaState.get());

    if (luaL_loadstring(luaState.get(), result.value().constData()) != LUA_OK)
    {
        const auto message = LuaRef::fromStack(luaState.get(), -1).cast<QString>().valueOr(u""_s);
        return nonstd::make_unexpected(message);
    }

    luaL_openlibs(luaState.get());

    if (lua_pcall(luaState.get(), 0, 0, 0) != LUA_OK)
    {
        const auto message = LuaRef::fromStack(luaState.get(), -1).cast<QString>().valueOr(u""_s);
        return nonstd::make_unexpected(message);
    }

    const auto pluginName = getGlobal(luaState.get(), "name").cast<QString>().valueOr(u""_s);
    if (pluginName.isEmpty())
        return nonstd::make_unexpected(tr("Plugin name is missing or invalid."));

    const PluginVersion pluginVersion = std::invoke([luaState = luaState.get()]
    {
        const auto pluginVersionStr = getGlobal(luaState, "version").cast<QString>().valueOr(u""_s);
        return PluginVersion::fromString(pluginVersionStr);
    });
    if (!pluginVersion.isValid())
        return nonstd::make_unexpected(tr("Plugin version is missing or invalid."));

    const bool isInvocable = std::invoke([luaState = luaState.get()]
    {
        const LuaRef func = getGlobal(luaState, "invoke");
        return func.isFunction();
    });

    registerLuaFunctions(luaState.get());
    registerLuaClasses(luaState.get());

    PluginEntry pluginEntry {
        .luaState = std::move(luaState),
        .pluginInfo = {
            .id = path.removedExtension().filename(),
            .name = pluginName,
            .version = pluginVersion,
            .invocable = isInvocable
        }
    };

    return pluginEntry;
}
