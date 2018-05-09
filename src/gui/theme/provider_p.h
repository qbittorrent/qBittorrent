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

#ifndef QBT_THEME_PROVIDER_P_H
#define QBT_THEME_PROVIDER_P_H

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>

#include <QString>

#include "themecommon.h"
#include "themeexceptions.h"

namespace Theme
{
    namespace Serialization
    {
        /*
         * Serialization: each serializable object shall be able to serialize itself into a QString value.
         * To freely mix objects from different sources within a theme, they are serialized as ID:value pairs.
         * For example:
         *
         * Background=RGB:128,128,125
         * Stalled=QPalette:Inactive,Midlight
         *
         * The value format is determined by the object, while <ID>: part is written and read
         * by the managing code.
         */

        template <class Value>
        class Entity
        {
        public:
            using ValueType = Value;

            virtual ~Entity() = default;
            virtual Value value() const = 0;
            virtual QString serializedValue() const = 0;
            virtual QString explicitSerializedValue() const = 0;
            virtual QString serializationKey() const = 0;
        };

        template <class Entity>
        class Provider
        {
        public:
            using EntityType = Entity;
            using EntityUPtr = std::unique_ptr<Entity>;
            virtual ~Provider() = default;
            QString name() const;
            virtual EntityUPtr load(const QString &serialized) const = 0;

        protected:
            Provider(const QString &name);

        private:
            QString m_name;
        };

        template <class Provider>
        class ProviderRegistry
        {
        public:
            using ProviderType = Provider;
            using ProviderUPtr = std::unique_ptr<ProviderType>;
            using EntityUPtr = typename ProviderType::EntityUPtr;

            void registerProvider(ProviderUPtr creator);
            const ProviderType *provider(const QString &name) const;
            EntityUPtr load(const QString &serializedWithKey) const;

            void applicationPaletteChanged() const;

        protected:
            ~ProviderRegistry() = default;

            using ProvidersMap = std::map<QString, ProviderUPtr>;
            const ProvidersMap &providers() const;

        private:
            ProvidersMap m_providers;
        };

        // ------------------------- implementation ------------------------------
        template <class Entity>
        inline Provider<Entity>::Provider(const QString &name)
            : m_name(name)
        {
        }

        template <class Entity>
        inline QString Provider<Entity>::name() const
        {
            return m_name;
        }

        template <class Provider>
        inline void ProviderRegistry<Provider>::registerProvider(ProviderUPtr creator)
        {
            const QString id = creator->name();
            Q_ASSERT_X(m_providers.count(id) == 0, Q_FUNC_INFO,
                       "Attempted to register the same theme color type twice");
            m_providers[id] = std::move(creator);
        }

        template <class Provider>
        inline const typename ProviderRegistry<Provider>::ProviderType *
        ProviderRegistry<Provider>::provider(const QString &name) const
        {
            const auto i = m_providers.find(name);
            if (i != m_providers.end())
                return i->second.get();
            return nullptr;
        }

        template <class Provider>
        inline typename ProviderRegistry<Provider>::EntityUPtr
        ProviderRegistry<Provider>::load(const QString &serializedWithKey) const
        {
            const int keyValueSeparatorPos = serializedWithKey.indexOf(QLatin1Char(':'), 0);
            if (keyValueSeparatorPos != -1) {
                const QString id = serializedWithKey.left(keyValueSeparatorPos);
                const auto *provider = this->provider(id);
                if (provider)
                    return provider->load(serializedWithKey.right(serializedWithKey.size() - keyValueSeparatorPos - 1));
                throw UnknownProvider(serializedWithKey);
            }
            qCInfo(theme, "Failed to parse serialized value '%s'", qPrintable(serializedWithKey));
            throw ParsingError(serializedWithKey);
        }

        template<class Provider>
        const typename ProviderRegistry<Provider>::ProvidersMap &ProviderRegistry<Provider>::providers() const
        {
            return m_providers;
        }
    }
}

#endif // QBT_THEME_PROVIDER_P_H
