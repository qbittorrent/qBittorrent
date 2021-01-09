/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin
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

#include <array>
#include <stdexcept>

#include <QDebug>
#include <QString>

namespace Utils
{
    template <typename T, std::size_t N, std::size_t Mandatory = N>
    class Version
    {
        static_assert(N > 0, "The number of version components may not be smaller than 1");
        static_assert(N >= Mandatory,
                      "The number of mandatory components may not be larger than the total number of components");

    public:
        typedef T ComponentType;
        typedef Version<T, N, Mandatory> ThisType;

        constexpr Version()
            : m_components {{}}
        {
        }

        constexpr Version(const ThisType &other) = default;

        template <typename ... Other>
        constexpr Version(Other ... components)
            : m_components {{static_cast<T>(components) ...}}
        {
        }

        /**
         * @brief Creates version from string in format "x.y.z"
         *
         * @param version Version string in format "x.y.z"
         * @throws std::runtime_error if parsing fails
         */
        Version(const QString &version)
            : Version {version.split(QLatin1Char('.'))}
        {
        }

        /**
         * @brief Creates version from byte array in format "x.y.z"
         *
         * @param version Version string in format "x.y.z"
         * @throws std::runtime_error if parsing fails
         */
        Version(const QByteArray &version)
            : Version {version.split('.')}
        {
        }

        constexpr ComponentType majorNumber() const
        {
            static_assert(N >= 1, "The number of version components is too small");
            return m_components[0];
        }

        constexpr ComponentType minorNumber() const
        {
            static_assert(N >= 2, "The number of version components is too small");
            return m_components[1];
        }

        constexpr ComponentType revisionNumber() const
        {
            static_assert(N >= 3, "The number of version components is too small");
            return m_components[2];
        }

        constexpr ComponentType patchNumber() const
        {
            static_assert(N >= 4, "The number of version components is too small");
            return m_components[3];
        }

        constexpr ComponentType operator[](const std::size_t i) const
        {
            return m_components.at(i);
        }

        operator QString() const
        {
            // find the last one non-zero component
            std::size_t lastSignificantIndex = N - 1;
            while ((lastSignificantIndex > 0) && ((*this)[lastSignificantIndex] == 0))
                --lastSignificantIndex;

            if (lastSignificantIndex + 1 < Mandatory)     // lastSignificantIndex >= 0
                lastSignificantIndex = Mandatory - 1;     // and Mandatory >= 1

            QString res = QString::number((*this)[0]);
            for (std::size_t i = 1; i <= lastSignificantIndex; ++i)
                res += QLatin1Char('.') + QString::number((*this)[i]);
            return res;
        }

        constexpr bool isValid() const
        {
            return (*this != ThisType {});
        }

        // TODO: remove manually defined operators and use compiler generated `operator<=>()` in C++20
        friend bool operator==(const ThisType &left, const ThisType &right)
        {
            return (left.m_components == right.m_components);
        }

        friend bool operator<(const ThisType &left, const ThisType &right)
        {
            return (left.m_components < right.m_components);
        }

        template <typename StringClassWithSplitMethod>
        static Version tryParse(const StringClassWithSplitMethod &s, const Version &defaultVersion)
        {
            try
            {
                return Version(s);
            }
            catch (const std::runtime_error &er)
            {
                qDebug() << "Error parsing version:" << er.what();
                return defaultVersion;
            }
        }

    private:
        using ComponentsArray = std::array<T, N>;

        template <typename StringsList>
        static ComponentsArray parseList(const StringsList &versionParts)
        {
            if ((static_cast<std::size_t>(versionParts.size()) > N)
                || (static_cast<std::size_t>(versionParts.size()) < Mandatory))
                throw std::runtime_error("Incorrect number of version components");

            bool ok = false;
            ComponentsArray res {{}};
            for (std::size_t i = 0; i < static_cast<std::size_t>(versionParts.size()); ++i)
            {
                res[i] = static_cast<T>(versionParts[static_cast<typename StringsList::size_type>(i)].toInt(&ok));
                if (!ok)
                    throw std::runtime_error("Can not parse version component");
            }
            return res;
        }

        template <typename StringsList>
        Version(const StringsList &versionParts)
            : m_components (parseList(versionParts)) // GCC 4.8.4 has problems with the uniform initializer here
        {
        }

        ComponentsArray m_components;
    };

    template <typename T, std::size_t N, std::size_t Mandatory>
    constexpr bool operator!=(const Version<T, N, Mandatory> &left, const Version<T, N, Mandatory> &right)
    {
        return !(left == right);
    }

    template <typename T, std::size_t N, std::size_t Mandatory>
    constexpr bool operator>(const Version<T, N, Mandatory> &left, const Version<T, N, Mandatory> &right)
    {
        return (right < left);
    }

    template <typename T, std::size_t N, std::size_t Mandatory>
    constexpr bool operator<=(const Version<T, N, Mandatory> &left, const Version<T, N, Mandatory> &right)
    {
        return !(left > right);
    }

    template <typename T, std::size_t N, std::size_t Mandatory>
    constexpr bool operator>=(const Version<T, N, Mandatory> &left, const Version<T, N, Mandatory> &right)
    {
        return !(left < right);
    }
}
