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

#include <lua/lua.hpp>
#include <LuaBridge/LuaBridge.h>

#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>

#include "base/bittorrent/addtorrenterror.h"
#include "base/bittorrent/sharelimits.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/path.h"

namespace luabridge
{
    // LuaBridge Stack specialization for `QString`.
    template <>
    struct Stack<QString>
    {
        [[nodiscard]] static Result push(lua_State *luaState, const QString &str)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (!lua_checkstack(luaState, 1))
                return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

            const QByteArray utf8data = str.toUtf8();
            lua_pushlstring(luaState, utf8data.constData(), utf8data.size());
            return {};
        }

        [[nodiscard]] static TypeResult<QString> get(lua_State *luaState, const int index)
        {
            size_t len = 0;
            const char *str = nullptr;

            if (lua_type(luaState, index) == LUA_TSTRING)
            {
                str = lua_tolstring(luaState, index, &len);
            }
#if !LUABRIDGE_STRICT_STACK_CONVERSIONS
            else
            {
#if LUABRIDGE_SAFE_STACK_CHECKS
                if (!lua_checkstack(luaState, 1))
                    return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

                // Lua reference manual:
                // If the value is a number, then lua_tolstring also changes the actual value in the stack
                // to a string. (This change confuses lua_next when lua_tolstring is applied to keys during
                // a table traversal.)
                lua_pushvalue(luaState, index);
                str = lua_tolstring(luaState, -1, &len);
                const auto string = QString::fromUtf8(str, len);
                lua_pop(luaState, 1); // Pop the temporary string
                return string;
            }
#endif

            if (str == nullptr)
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            return QString::fromUtf8(str, len);
        }

        [[nodiscard]] static bool isInstance(lua_State *luaState, const int index)
        {
            return lua_type(luaState, index) == LUA_TSTRING;
        }
    };

    // LuaBridge Stack specialization for `QUrl`.
    template <>
    struct Stack<QUrl>
    {
        [[nodiscard]] static Result push(lua_State *luaState, const QUrl &url)
        {
            return Stack<QString>::push(luaState, url.toString());
        }

        [[nodiscard]] static TypeResult<QUrl> get(lua_State *luaState, const int index)
        {
            return QUrl(Stack<QString>::get(luaState, index).valueOr(QString()));
        }

        [[nodiscard]] static bool isInstance(lua_State *luaState, const int index)
        {
            return lua_type(luaState, index) == LUA_TSTRING;
        }
    };

    // LuaBridge Stack specialization for `Path`.
    template <>
    struct Stack<Path>
    {
        [[nodiscard]] static Result push(lua_State *luaState, const Path &path)
        {
            return Stack<QString>::push(luaState, path.toString());
        }

        [[nodiscard]] static TypeResult<Path> get(lua_State *luaState, const int index)
        {
            return Path(Stack<QString>::get(luaState, index).valueOr(QString()));
        }

        [[nodiscard]] static bool isInstance(lua_State *luaState, const int index)
        {
            return lua_type(luaState, index) == LUA_TSTRING;
        }
    };

    // LuaBridge Stack specialization for `QList`.
    template <typename T>
    struct Stack<QList<T>>
    {
        using Type = QList<T>;

        [[nodiscard]] static Result push(lua_State *luaState, const Type &list)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (!lua_checkstack(luaState, 3))
                return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

            StackRestore stackRestore {luaState};

            const qsizetype listSize = list.size();
            lua_createtable(luaState, static_cast<int>(listSize), 0);

            for (qsizetype i = 0; i < listSize; ++i)
            {
                lua_pushinteger(luaState, static_cast<lua_Integer>(i + 1));

                auto result = Stack<T>::push(luaState, list[i]);
                if (!result)
                    return result;

                lua_settable(luaState, -3);
            }

            stackRestore.reset();
            return {};
        }

        [[nodiscard]] static TypeResult<Type> get(lua_State *luaState, const int index)
        {
            if (!lua_istable(luaState, index))
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            const StackRestore stackRestore {luaState};

            Type list;
            list.reserve(static_cast<qsizetype>(get_length(luaState, index)));

            const int absIndex = lua_absindex(luaState, index);
            lua_pushnil(luaState);

            while (lua_next(luaState, absIndex) != 0)
            {
                auto item = Stack<T>::get(luaState, -1);
                if (!item)
                    return makeErrorCode(ErrorCode::InvalidTypeCast);

                list.emplace_back(*item);
                lua_pop(luaState, 1);
            }

            return list;
        }

        [[nodiscard]] static bool isInstance(lua_State *luaState, const int index)
        {
            return lua_istable(luaState, index);
        }
    };

    // LuaBridge Stack specialization for `QHash`.
    template <class K, class V>
    struct Stack<QHash<K, V>>
    {
        using Type = QHash<K, V>;

        [[nodiscard]] static Result push(lua_State *luaState, const Type &hashTable)
        {
#if LUABRIDGE_SAFE_STACK_CHECKS
            if (!lua_checkstack(luaState, 3))
                return makeErrorCode(ErrorCode::LuaStackOverflow);
#endif

            StackRestore stackRestore {luaState};
            lua_createtable(luaState, 0, static_cast<int>(hashTable.size()));

            for (auto it = hashTable.cbegin(); it != hashTable.cend(); ++it)
            {
                auto result = Stack<K>::push(luaState, it.key());
                if (!result)
                    return result;

                result = Stack<V>::push(luaState, it.value());
                if (!result)
                    return result;

                lua_settable(luaState, -3);
            }

            stackRestore.reset();
            return {};
        }

        [[nodiscard]] static TypeResult<Type> get(lua_State *luaState, const int index)
        {
            if (!lua_istable(luaState, index))
                return makeErrorCode(ErrorCode::InvalidTypeCast);

            const StackRestore stackRestore {luaState};

            Type hashTable;

            int absIndex = lua_absindex(luaState, index);
            lua_pushnil(luaState);

            while (lua_next(luaState, absIndex) != 0)
            {
                const auto value = Stack<V>::get(luaState, -1);
                if (!value)
                    return makeErrorCode(ErrorCode::InvalidTypeCast);

                const auto key = Stack<K>::get(luaState, -2);
                if (!key)
                    return makeErrorCode(ErrorCode::InvalidTypeCast);

                hashTable.emplace(*key, *value);
                lua_pop(luaState, 1);
            }

            return hashTable;
        }

        [[nodiscard]] static bool isInstance(lua_State *luaState, const int index)
        {
            return lua_istable(luaState, index);
        }
    };

    template <>
    struct Stack<BitTorrent::TrackerEndpointState>
        : Enum<BitTorrent::TrackerEndpointState
            , BitTorrent::TrackerEndpointState::NotContacted
            , BitTorrent::TrackerEndpointState::Working
            , BitTorrent::TrackerEndpointState::NotWorking
            , BitTorrent::TrackerEndpointState::TrackerError
            , BitTorrent::TrackerEndpointState::Unreachable>
    {
    };

    template <>
    struct Stack<BitTorrent::ShareLimitAction>
        : Enum<BitTorrent::ShareLimitAction
            , BitTorrent::ShareLimitAction::Default
            , BitTorrent::ShareLimitAction::Stop
            , BitTorrent::ShareLimitAction::Remove
            , BitTorrent::ShareLimitAction::RemoveWithContent
            , BitTorrent::ShareLimitAction::EnableSuperSeeding>
    {
    };

    template <>
    struct Stack<BitTorrent::ShareLimitsMode>
        : Enum<BitTorrent::ShareLimitsMode
               , BitTorrent::ShareLimitsMode::Default
               , BitTorrent::ShareLimitsMode::MatchAny
               , BitTorrent::ShareLimitsMode::MatchAll>
    {
    };

    template <>
    struct Stack<BitTorrent::AddTorrentError::Kind>
        : Enum<BitTorrent::AddTorrentError::Kind
            , BitTorrent::AddTorrentError::Kind::DuplicateTorrent
            , BitTorrent::AddTorrentError::Kind::Other>
    {
    };
}
