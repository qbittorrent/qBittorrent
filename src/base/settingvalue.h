/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016, 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QString>

#include "application.h"
#include "settingsstorage.h"

// This is a thin/handy wrapper over `SettingsStorage`. Use it when store/load value
// rarely occurs, otherwise use `CachedSettingValue`.
template <typename T>
class SettingValue
{
public:
    SettingValue(SettingsStorage *settings, const QString &keyName)
        : m_keyName {keyName}
        , m_settings {settings}
    {
        Q_ASSERT(m_settings);
    }

    explicit SettingValue(const QString &keyName)
        : SettingValue(qBt->settings(), keyName)
    {
    }

    T get(const T &defaultValue = {}) const
    {
        return m_settings->loadValue(m_keyName, defaultValue);
    }

    operator T() const
    {
        return get();
    }

    SettingValue<T> &operator=(const T &value)
    {
        m_settings->storeValue(m_keyName, value);
        return *this;
    }

private:
    const QString m_keyName;
    SettingsStorage *m_settings = nullptr;
};

template <typename T>
class CachedSettingValue
{
public:
    CachedSettingValue(SettingsStorage *settings, const QString &keyName, const T &defaultValue = {})
        : m_settingValue {settings, keyName}
        , m_cache {m_settingValue.get(defaultValue)}
    {
    }

    explicit CachedSettingValue(const QString &keyName, const T &defaultValue = {})
        : m_settingValue {keyName}
        , m_cache {m_settingValue.get(defaultValue)}
    {
    }

    // The signature of the ProxyFunc should be equivalent to the following:
    // T proxyFunc(const T &a);
    template <typename ProxyFunc>
    explicit CachedSettingValue(SettingsStorage *settings, const QString &keyName, const T &defaultValue, ProxyFunc &&proxyFunc)
        : m_settingValue {settings, keyName}
        , m_cache {proxyFunc(m_settingValue.get(defaultValue))}
    {
    }

    template <typename ProxyFunc>
    explicit CachedSettingValue(const QString &keyName, const T &defaultValue, ProxyFunc &&proxyFunc)
        : m_settingValue {keyName}
        , m_cache {proxyFunc(m_settingValue.get(defaultValue))}
    {
    }

    T get() const
    {
        return m_cache;
    }

    operator T() const
    {
        return get();
    }

    CachedSettingValue<T> &operator=(const T &value)
    {
        if (m_cache == value)
            return *this;

        m_settingValue = value;
        m_cache = value;
        return *this;
    }

private:
    SettingValue<T> m_settingValue;
    T m_cache;
};
