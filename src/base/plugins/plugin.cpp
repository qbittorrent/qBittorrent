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

#include "plugin.h"

#include <chrono>

#include <QDeadlineTimer>
#include <QHash>
#include <QScopeGuard>
#include <QString>

#include "base/path.h"
#include "base/utils/io.h"
#include "luaclasses.h"
#include "luafunctions.h"
#include "luanamespace.h"
#include "luastack.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

// This timeout is used to interrupt the execution
// of the faulty script (e.g., if it enters an endless loop, etc.)
const std::chrono::milliseconds LUA_TIMEOUT = 1s;

namespace
{
    QHash<lua_State *, QDeadlineTimer> LUA_DEADLINES_REGISTRY;

    void checkLuaDeadline(lua_State *luaState, lua_Debug *)
    {
        const QDeadlineTimer &deadlineTimer = LUA_DEADLINES_REGISTRY.value(luaState);
        if (deadlineTimer.hasExpired())
            luaL_error(luaState, Plugin::tr("Timed out.").toUtf8().constData());
    }

    void resetLuaDeadline(lua_State *luaState)
    {
        QDeadlineTimer &deadlineTimer = LUA_DEADLINES_REGISTRY[luaState];
        deadlineTimer.setRemainingTime(LUA_TIMEOUT);
    }
}

nonstd::expected<std::shared_ptr<Plugin>, QString> Plugin::load(const Path &pluginPath)
{
    lua_State *luaState = luaL_newstate();
    if (!luaState)
        return nonstd::make_unexpected(tr("Failed to allocate Lua state."));

    LUA_DEADLINES_REGISTRY.emplace(luaState);

    [[maybe_unused]] auto scopeGuard = qScopeGuard([luaState]
    {
        LUA_DEADLINES_REGISTRY.remove(luaState);
        lua_close(luaState);
    });

    lua_sethook(luaState, checkLuaDeadline, LUA_MASKCOUNT, 100);

    const auto result = Utils::IO::readFile(pluginPath, 1024 * 1024);
    if (!result)
        return nonstd::make_unexpected(result.error().message);

    using namespace luabridge;

    enableExceptions(luaState);

    if (luaL_loadstring(luaState, result.value().constData()) != LUA_OK)
    {
        const auto message = LuaRef::fromStack(luaState, -1).cast<QString>().valueOr(u""_s);
        return nonstd::make_unexpected(message);
    }

    const int libLoadMask = LUA_GLIBK | LUA_STRLIBK | LUA_TABLIBK | LUA_MATHLIBK | LUA_UTF8LIBK | LUA_COLIBK;
    luaL_openselectedlibs(luaState, libLoadMask, 0);

    {
        LuaRef baseLib = getGlobal(luaState, LUA_GNAME);
        baseLib["collectgarbage"] = nullptr;
        baseLib["dofile"] = nullptr;
        baseLib["getmetatable"] = nullptr;
        baseLib["setmetatable"] = nullptr;
        baseLib["load"] = nullptr;
        baseLib["loadfile"] = nullptr;
        baseLib["rawequal"] = nullptr;
        baseLib["rawlen"] = nullptr;
        baseLib["rawget"] = nullptr;
        baseLib["rawset"] = nullptr;
        baseLib["warn"] = nullptr;
    }

    {
        LuaRef stringLib = getGlobal(luaState, LUA_STRLIBNAME);
        stringLib["dump"] = nullptr;
    }

    ::resetLuaDeadline(luaState);
    if (lua_pcall(luaState, 0, 0, 0) != LUA_OK)
    {
        const auto message = LuaRef::fromStack(luaState, -1).cast<QString>().valueOr(u""_s);
        return nonstd::make_unexpected(message);
    }

    const auto pluginName = getGlobal(luaState, "NAME").cast<QString>().valueOr(u""_s);
    if (pluginName.isEmpty())
        return nonstd::make_unexpected(tr("Plugin name is missing or invalid."));

    const PluginVersion pluginVersion = std::invoke([luaState]
    {
        const auto pluginVersionStr = getGlobal(luaState, "VERSION").cast<QString>().valueOr(u""_s);
        return PluginVersion::fromString(pluginVersionStr);
    });
    if (!pluginVersion.isValid())
        return nonstd::make_unexpected(tr("Plugin version is missing or invalid."));

    scopeGuard.dismiss();
    return std::shared_ptr<Plugin>(new Plugin(luaState, pluginName, pluginVersion));
}

Plugin::Plugin(lua_State *luaState, const QString &name, const PluginVersion &version)
    : m_luaState {luaState}
    , m_name {name}
    , m_version {version}
{
    m_isInvocable = luabridge::getGlobal(luaState, "invoke").isFunction();

    luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE)
        .addFunction("debug", LuaFunctions::debug)
        .addFunction("log", LuaFunctions::log)
        .addFunction("exec", LuaFunctions::exec)
        .addFunction("sendMail", luabridge::bind_back(LuaFunctions::sendMail, this))
        .addFunction("friendlySizeUnit", LuaFunctions::friendlySizeUnit1, LuaFunctions::friendlySizeUnit2)
        .addFunction("friendlySpeedUnit", LuaFunctions::friendlySpeedUnit1, LuaFunctions::friendlySpeedUnit2)
        .addFunction("friendlyDuration", LuaFunctions::friendlyDuration)
        .addFunction("formatDateTime", LuaFunctions::formatDateTime1, LuaFunctions::formatDateTime2);

    registerLuaClasses(luaState);
}

Plugin::~Plugin()
{
    LUA_DEADLINES_REGISTRY.remove(m_luaState);
    lua_close(m_luaState);
}

QString Plugin::name() const
{
    return m_name;
}

PluginVersion Plugin::version() const
{
    return m_version;
}

bool Plugin::isInvocable() const
{
    return m_isInvocable;
}

void Plugin::invoke() const
{
    resetLuaDeadline();
    luabridge::getGlobal(m_luaState, "invoke").call();
}

void Plugin::resetLuaDeadline() const
{
    ::resetLuaDeadline(m_luaState);
}
