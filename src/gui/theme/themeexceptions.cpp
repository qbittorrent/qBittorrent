/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "themeexceptions.h"

Theme::ThemeNotFound::ThemeNotFound(const QString& themeName)
    : ThemeError("Could not find theme named '" + themeName.toStdString() + "'")
{
}

Theme::Serialization::ThemeElementMissing::ThemeElementMissing(const QString& elementName)
    : DeserializationError("Theme misses element '" + elementName.toStdString() + '\'')
    , m_elementName(elementName)
{
}

const QString &Theme::Serialization::ThemeElementMissing::elementName() const
{
    return m_elementName;
}

Theme::Serialization::ElementDeserializationError::ElementDeserializationError(
    const std::string& message, const QString& serializedValue)
    : DeserializationError("Could not deserialize theme object '" + serializedValue.toStdString() + "': " + message)
    , m_serializedValue(serializedValue)
{
}

const QString &Theme::Serialization::ElementDeserializationError::serializedValue() const
{
    return m_serializedValue;
}

Theme::Serialization::ParsingError::ParsingError(const QString& serializedValue)
    : ElementDeserializationError("Could not find scheme part", serializedValue)
{
}

// we know that serializedValue follows scheme:value pattern, otherwise this exception is not applicable
Theme::Serialization::UnknownProvider::UnknownProvider(const QString& serializedValue)
    : ElementDeserializationError("Could not find provider for scheme '"
        + serializedValue.section(QLatin1Char(':'), 0, 1).toStdString() + '\'', serializedValue)
{
}

QString Theme::Serialization::UnknownProvider::profiderName() const
{
    // we know that serializedValue follows scheme:value pattern, otherwise this exception is not applicable
    return this->serializedValue().section(QLatin1Char(':'), 0, 1);
}

Theme::Serialization::ValueParsingError::ValueParsingError(const std::string& message, const QString& serializedValue)
    : ElementDeserializationError("Could not parse element value: " + message, serializedValue)
{
}

QString Theme::Serialization::ValueParsingError::valueString() const
{
    // we know that serializedValue follows scheme:value pattern, otherwise this exception is not applicable
    return this->serializedValue().section(QLatin1Char(':'), 1);
}

