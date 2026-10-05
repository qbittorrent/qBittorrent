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

#pragma once

#include <memory>
#include <optional>

#include <QHash>
#include <QObject>
#include <QQueue>

#include "base/3rdparty/expected.hpp"
#include "base/pathfwd.h"
#include "base/utils/version.h"
#include "pluginversion.h"

using LuaVersion = Utils::Version<3>;
using LuaBridgeVersion = Utils::Version<2>;

class Plugin;

namespace BitTorrent
{
    class Torrent;
}

struct PluginInfo
{
    QString id;
    QString name;
    PluginVersion version;
    bool invocable = false;
    bool enabled = false;
};

class PluginsEngine final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PluginsEngine)

public:
    static void initInstance();
    static void freeInstance();
    static PluginsEngine *instance();

    static LuaVersion luaVersion();
    static LuaBridgeVersion luaBridgeVersion();

    QHash<QString, PluginInfo> allPlugins() const;
    std::optional<PluginInfo> pluginInfo(const QString &pluginID) const;

    bool installPlugin(const Path &pluginPath);
    bool uninstallPlugin(const QString &pluginID);
    void setPluginEnabled(const QString &pluginID, bool enabled);

    void invokePlugin(const QString &pluginID);

signals:
    void pluginInstalled(const Path &pluginPath, const PluginInfo &pluginInfo);
    void pluginInstallationFailed(const Path &pluginPath, const QString &reason);
    void pluginUpdated(const Path &pluginPath, const PluginInfo &oldPluginInfo
            , const PluginInfo &newPluginInfo);
    void pluginUpdateFailed(const Path &pluginPath, const PluginInfo &currentPluginInfo
            , const PluginInfo &newPluginInfo, const QString &reason);
    void pluginUninstalled(const QString &pluginID);
    void pluginUninstallationFailed(const QString &pluginID, const QString &reason);
    void pluginEnabledChanged(const QString &pluginID, bool isEnabled);

private:
    struct PluginEntry
    {
        std::shared_ptr<Plugin> plugin;
        QString id;
        bool enabled = false;
    };

    static Path pluginsPath();

    explicit PluginsEngine(QObject *parent = nullptr);
    ~PluginsEngine() override;

    nonstd::expected<PluginEntry, QString> loadPlugin(const Path &path);
    void installPluginDeferred();
    void installPluginImpl();
    void uninstallPluginDeferred();
    void uninstallPluginImpl();

    void loadConfig();
    void loadPlugins();
    void connectEventHandlers();
    void storeConfig() const;
    PluginInfo getPluginInfo(const PluginEntry &pluginEntry) const;

    template <typename Signal>
    void connectEventHandler(Signal &&signal, const char *eventHandlerName);

    template <typename... Args>
    void callEventHandlers(const char *eventHandlerName, Args&&... args);

    QHash<QString, PluginEntry> m_plugins;
    QQueue<Path> m_pluginsToInstall;
    QQueue<QString> m_pluginsToUninstall;
    mutable bool m_configIsDirty = false;

    inline static PluginsEngine *m_instance = nullptr;
};
