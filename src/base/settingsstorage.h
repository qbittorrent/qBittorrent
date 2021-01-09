/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
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

#include <QObject>
#include <QReadWriteLock>
#include <QTimer>
#include <QVariantHash>

#include "utils/string.h"

class SettingsStorage : public QObject
{
    Q_OBJECT
    SettingsStorage();
    ~SettingsStorage();

public:
    static void initInstance();
    static void freeInstance();
    static SettingsStorage *instance();

    template <typename T>
    T loadValue(const QString &key, const T &defaultValue = {}) const
    {
        if constexpr (std::is_enum_v<T>)
        {
            const auto value = loadValueImpl(key).toString();
            return Utils::String::toEnum(value, defaultValue);
        }
        else if constexpr (std::is_same_v<T, QVariant>)
        {
            return loadValueImpl(key, defaultValue);
        }
        else
        {
            const QVariant value = loadValueImpl(key);
            // check if retrieved value is convertible to T
            return value.template canConvert<T>() ? value.template value<T>() : defaultValue;
        }
    }

    template <typename T>
    void storeValue(const QString &key, const T &value)
    {
        if constexpr (std::is_enum_v<T>)
            storeValueImpl(key, Utils::String::fromEnum(value));
        else
            storeValueImpl(key, value);
    }

    void removeValue(const QString &key);

public slots:
    bool save();

private:
    QVariant loadValueImpl(const QString &key, const QVariant &defaultValue = {}) const;
    void storeValueImpl(const QString &key, const QVariant &value);

    static SettingsStorage *m_instance;

    bool m_dirty = false;
    QVariantHash m_data;
    QTimer m_timer;
    mutable QReadWriteLock m_lock;
};
