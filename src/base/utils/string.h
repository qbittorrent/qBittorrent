/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <optional>

#include <QChar>
#include <QMetaEnum>
#include <QString>
#include <Qt>
#include <QtContainerFwd>

namespace Utils::String
{
    QString wildcardToRegexPattern(const QString &pattern);

    template <typename T>
    T unquote(const T &str, const QString &quotes = QChar('"'))
    {
        if (str.length() < 2) return str;

        for (const QChar quote : quotes)
        {
            if (str.startsWith(quote) && str.endsWith(quote))
                return str.mid(1, (str.length() - 2));
        }

        return str;
    }

    std::optional<bool> parseBool(const QString &string);
    std::optional<int> parseInt(const QString &string);
    std::optional<double> parseDouble(const QString &string);

    QString join(const QVector<QStringView> &strings, QStringView separator);

    QString fromDouble(double n, int precision);

    template <typename T, typename std::enable_if_t<std::is_enum_v<T>, int> = 0>
    QString fromEnum(const T &value)
    {
        static_assert(std::is_same_v<int, typename std::underlying_type_t<T>>,
                      "Enumeration underlying type has to be int.");

        const auto metaEnum = QMetaEnum::fromType<T>();
        return QString::fromLatin1(metaEnum.valueToKey(static_cast<int>(value)));
    }

    template <typename T, typename std::enable_if_t<std::is_enum_v<T>, int> = 0>
    T toEnum(const QString &serializedValue, const T &defaultValue)
    {
        static_assert(std::is_same_v<int, typename std::underlying_type_t<T>>,
                      "Enumeration underlying type has to be int.");

        const auto metaEnum = QMetaEnum::fromType<T>();
        bool ok = false;
        const T value = static_cast<T>(metaEnum.keyToValue(serializedValue.toLatin1().constData(), &ok));
        return (ok ? value : defaultValue);
    }
}
