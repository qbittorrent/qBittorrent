/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include <type_traits>

#include <QMetaEnum>
#include <QString>

#include "settingsstorage.h"
#include "utils/string.h"

template <typename T>
class CachedSettingValue
{
public:
    explicit CachedSettingValue(const char *keyName, const T &defaultValue = T())
        : m_keyName(QLatin1String(keyName))
        , m_value(loadValue(defaultValue))
    {
    }

    // The signature of the ProxyFunc should be equivalent to the following:
    // T proxyFunc(const T &a);
    template <typename ProxyFunc>
    explicit CachedSettingValue(const char *keyName, const T &defaultValue
                                , ProxyFunc &&proxyFunc)
        : m_keyName(QLatin1String(keyName))
        , m_value(proxyFunc(loadValue(defaultValue)))
    {
    }

    T value() const
    {
        return m_value;
    }

    operator T() const
    {
        return value();
    }

    CachedSettingValue<T> &operator=(const T &newValue)
    {
        if (m_value == newValue)
            return *this;

        m_value = newValue;
        storeValue(m_value);
        return *this;
    }

private:
    // regular load/save pair
    template <typename U, typename std::enable_if_t<!std::is_enum<U>::value, int> = 0>
    U loadValue(const U &defaultValue)
    {
        return SettingsStorage::instance()->loadValue(m_keyName, defaultValue).template value<T>();
    }

    template <typename U, typename std::enable_if_t<!std::is_enum<U>::value, int> = 0>
    void storeValue(const U &value)
    {
        SettingsStorage::instance()->storeValue(m_keyName, value);
    }

    // load/save pair for an enum
    // saves literal value of the enum constant, obtained from QMetaEnum
    template <typename U, typename std::enable_if_t<std::is_enum<U>::value, int> = 0>
    U loadValue(const U &defaultValue)
    {
        return Utils::String::toEnum(SettingsStorage::instance()->loadValue(m_keyName).toString(), defaultValue);
    }

    template <typename U, typename std::enable_if_t<std::is_enum<U>::value, int> = 0>
    void storeValue(const U &value)
    {
        SettingsStorage::instance()->storeValue(m_keyName, Utils::String::fromEnum(value));
    }

    const QString m_keyName;
    T m_value;
};
