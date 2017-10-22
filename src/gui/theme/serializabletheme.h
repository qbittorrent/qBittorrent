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

#ifndef QBT_THEME_SERIALIZABLE_THEME_H
#define QBT_THEME_SERIALIZABLE_THEME_H

#include "themecommon.h"
#include "themeinfo.h"

#include <QString>
#include <QVariant>

#include <functional>
#include <map>
#include <type_traits>

namespace Theme
{
    using ElementsMap = std::map<QString, QString>;

    class ThemeSerializer
    {
    public:
        using NamesMap = std::map<QByteArray, int>;
        using ElementDeserializer = std::function<void (int elementId, const QString &value)>;
        using ElementSerializer = std::function<QString (int elementId, bool explicitValue)>;

        ThemeSerializer(Kind kind, const QString &themeName,
                        const NamesMap& names, ElementDeserializer deserializer);

        void save(const QString &themeFileName, bool explicitValues, ElementSerializer serializer) const;

        const ThemeInfo &info() const;
    private:

        NamesMap m_names;
        ThemeInfo m_info;
        Kind m_kind;
    };

    template <typename ProviderRegistry, typename Element>
    class SerializableTheme
    {
        static_assert(std::is_same<int, typename std::underlying_type<Element>::type>::value,
                      "Element underlying type has to be int");

    public:
        using EntityProviderType = typename ProviderRegistry::ProviderType;
        using EntityType = typename EntityProviderType::EntityType;
        using ThemeObjectType = typename EntityType::ValueType;
        using ElementType = Element;
        using NamesMap = typename ThemeSerializer::NamesMap;

        using ThisType = SerializableTheme<EntityType, ElementType>;

        const ThemeObjectType &element(Element e) const;
        const ThemeInfo &info() const;

    protected:
        SerializableTheme(const QString &themeName, const NamesMap& names);
        void updateCache() const;

        void save(const QString &themeFileName, bool explicitValues) const;

    private:
        QString serialize(int elementId, bool explicitValue) const;
        void deserialize(int elementId, const QString &value);

        using ElementsMap = std::map<Element, typename EntityProviderType::EntityUPtr>;
        using ObjectsMap = std::map<Element, ThemeObjectType>;

        ElementsMap m_elements;
        ThemeSerializer m_serializer;
        mutable ObjectsMap m_cache;
    };
}

template<typename ProviderRegistry, typename Element>
const typename Theme::SerializableTheme<ProviderRegistry, Element>::ThemeObjectType
&Theme::SerializableTheme<ProviderRegistry, Element>::element(Element e) const
{
    return m_cache[e];
}

template<typename ProviderRegistry, typename Element>
const Theme::ThemeInfo & Theme::SerializableTheme<ProviderRegistry, Element>::info() const
{
    return m_serializer.info();
}

template<typename ProviderRegistry, typename Element>
Theme::SerializableTheme<ProviderRegistry, Element>::SerializableTheme(
            const QString& themeName, const NamesMap& names)
    : m_elements{}
    , m_serializer(ProviderRegistry::kind, themeName, names,
           std::bind(&SerializableTheme::deserialize, this, std::placeholders::_1, std::placeholders::_2))
{
    updateCache();
}

template<typename ProviderRegistry, typename Element>
void Theme::SerializableTheme<ProviderRegistry, Element>::save(const QString& themeFileName, bool explicitValues) const
{
    m_serializer.save(themeFileName, explicitValues,
                      std::bind(&SerializableTheme::serialize, this, std::placeholders::_1, std::placeholders::_2));
}

template<typename ProviderRegistry, typename Element>
QString Theme::SerializableTheme<ProviderRegistry, Element>::serialize(int elementId, bool explicitValue) const
{
    const auto &element = m_elements.at(static_cast<Element>(elementId));
    return element->serializationKey() + QLatin1Char(':')
        + (explicitValue ? element->explicitSerializedValue() : element->serializedValue());
}

template<typename ProviderRegistry, typename Element>
void Theme::SerializableTheme<ProviderRegistry, Element>::deserialize(int elementId, const QString &value)
{
    m_elements[static_cast<Element>(elementId)] = ProviderRegistry::instance().load(value);
}

template<typename ProviderRegistry, typename Element>
void Theme::SerializableTheme<ProviderRegistry, Element>::updateCache() const
{
    for (const auto &p : m_elements) {
        m_cache[p.first] = p.second->value();
    }
}


#endif // QBT_THEME_SERIALIZABLE_THEME_H
