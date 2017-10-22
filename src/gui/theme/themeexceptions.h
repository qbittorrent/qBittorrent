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

#ifndef QBT_THEME_THEMEEXCEPTIONS_H
#define QBT_THEME_THEMEEXCEPTIONS_H

#include <stdexcept>

#include <QString>

namespace Theme
{

    class ThemeError: public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class ThemeNotFound: public ThemeError
    {
    public:
        ThemeNotFound(const QString &themeName);
    };

    namespace Serialization
    {
        class DeserializationError: public ThemeError
        {
        public:
            using ThemeError::ThemeError;
        };

        class ThemeElementMissing: public DeserializationError
        {
        public:
            ThemeElementMissing(const QString &elementName);

            const QString &elementName() const;
        private:
            QString m_elementName;
        };

        class ElementDeserializationError: public DeserializationError
        {
        public:
            ElementDeserializationError(const std::string &message, const QString &serializedValue);
            const QString &serializedValue() const;

        private:
            QString m_serializedValue;
        };

        class ParsingError: public ElementDeserializationError
        {
        public:
            ParsingError(const QString &serializedValue);
        };

        class UnknownProvider: public ElementDeserializationError
        {
        public:
            UnknownProvider(const QString &serializedValue);
            QString profiderName() const;
        };

        class ValueParsingError: public ElementDeserializationError
        {
        public:
            ValueParsingError(const std::string &message, const QString &serializedValue);
            QString valueString() const;
        };
    }
}

#endif // QBT_THEME_THEMEEXCEPTIONS_H
