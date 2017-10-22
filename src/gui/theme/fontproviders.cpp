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

#include "fontproviders.h"

#include "themeexceptions.h"

namespace
{
    struct FontProvidersRegistrator
    {
        FontProvidersRegistrator()
        {
            using Registry = Theme::Serialization::FontProviderRegistry;
            using ProviderUPtr = Registry::ProviderUPtr;

            Registry::instance().registerProvider(ProviderUPtr(new Theme::Serialization::ExplicitFontProvider()));
        }
    };
}

void Theme::Serialization::registerFontProviders()
{
    static FontProvidersRegistrator fontProvidersRegistrator;
}

Theme::Serialization::ExplicitFont::ExplicitFont(const QFont &color)
    : m_value {color}
{
}

/* ExplicitFont is just what it means: a single QFont value.
 * It is serialized into a string formatted as QFont::toString()
 */
Theme::Serialization::ExplicitFont::ExplicitFont(const QString &serialized)
    : m_value {fromString(serialized)}
{
}

QFont Theme::Serialization::ExplicitFont::fromString(const QString &str)
{
    if (str.isEmpty()) {
        // they want default app font
        return QFont();
    }
    QFont res;
    if (!res.fromString(str)) {
        throw ValueParsingError("QFont could not recognize font string", str);
    }
    return res;
}

QFont Theme::Serialization::ExplicitFont::value() const
{
    return m_value;
}

QString Theme::Serialization::ExplicitFont::explicitSerializedValue() const
{
    return m_value.toString();
}

QString Theme::Serialization::ExplicitFont::serializedValue() const
{
    return explicitSerializedValue();
}

QString Theme::Serialization::ExplicitFont::serializationKey() const
{
    return QLatin1String("QFont");
}

Theme::Serialization::ExplicitFontProvider::ExplicitFontProvider()
    : FontProvider(ExplicitFont(QFont()).serializationKey())
{
}

Theme::Serialization::ExplicitFontProvider::EntityUPtr
Theme::Serialization::ExplicitFontProvider::load(const QString& serialized) const
{
    return EntityUPtr(new ExplicitFont(serialized));
}
