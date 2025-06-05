/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
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
#include <type_traits>

#include <QList>
#include <QString>
#include <QStringView>

namespace Utils
{
    // This class provides a default implementation of `isValid()` that should work for most cases
    // It is ultimately up to the user to decide whether the version numbers are useful/meaningful
    template <int N, int Mandatory = N>
    class Version final
    {
        static_assert((N > 0), "The number of version components may not be smaller than 1");
        static_assert((Mandatory > 0), "The number of mandatory components may not be smaller than 1");
        static_assert((N >= Mandatory),
                      "The number of mandatory components may not be larger than the total number of components");

    public:
        using ThisType = Version<N, Mandatory>;

        constexpr Version() = default;

        Version(const QStringView string)
        {
            *this = fromString(string);
        }

        template <typename ... Ts>
        constexpr Version(Ts ... params)
            requires std::conjunction_v<std::is_convertible<Ts, int>...>
            : m_components {{params ...}}
        {
            static_assert((sizeof...(Ts) <= N), "Too many parameters provided");
            static_assert((sizeof...(Ts) >= Mandatory), "Not enough parameters provided");
        }

        constexpr bool isValid() const
        {
            bool hasValid = false;
            for (const int i : m_components)
            {
                if (i < 0)
                    return false;
                if (i > 0)
                    hasValid = true;
            }
            return hasValid;
        }

        constexpr int majorNumber() const
        {
            return m_components[0];
        }

        constexpr int minorNumber() const
        {
            static_assert((N >= 2), "The number of version components is too small");

            return m_components[1];
        }

        constexpr int revisionNumber() const
        {
            static_assert((N >= 3), "The number of version components is too small");

            return m_components[2];
        }

        constexpr int patchNumber() const
        {
            static_assert((N >= 4), "The number of version components is too small");

            return m_components[3];
        }

        constexpr int operator[](const int i) const
        {
            return m_components.at(i);
        }

        QString toString() const
        {
            // find the last one non-zero component
            int lastSignificantIndex = N - 1;
            while ((lastSignificantIndex > 0) && (m_components[lastSignificantIndex] == 0))
                --lastSignificantIndex;

            if ((lastSignificantIndex + 1) < Mandatory)   // lastSignificantIndex >= 0
                lastSignificantIndex = Mandatory - 1;     // and Mandatory >= 1

            QString res = QString::number(m_components[0]);
            for (int i = 1; i <= lastSignificantIndex; ++i)
                res += (u'.' + QString::number(m_components[i]));
            return res;
        }

        friend constexpr bool operator==(const ThisType &left, const ThisType &right)
        {
            return (left.m_components == right.m_components);
        }

        friend constexpr bool operator<(const ThisType &left, const ThisType &right)
        {
            return (left.m_components < right.m_components);
        }

        static Version fromString(const QStringView string, const Version &defaultVersion = {})
        {
            const QList<QStringView> stringParts = string.split(u'.');
            const int count = stringParts.size();

            if ((count > N) || (count < Mandatory))
                return defaultVersion;

            Version version;
            for (int i = 0; i < count; ++i)
            {
                bool ok = false;
                version.m_components[i] = stringParts[i].toInt(&ok);
                if (!ok)
                    return defaultVersion;
            }

            return version;
        }

    private:
        std::array<int, N> m_components {{}};
    };

    template <int N, int Mandatory>
    constexpr bool operator>(const Version<N, Mandatory> &left, const Version<N, Mandatory> &right)
    {
        return (right < left);
    }

    template <int N, int Mandatory>
    constexpr bool operator<=(const Version<N, Mandatory> &left, const Version<N, Mandatory> &right)
    {
        return !(left > right);
    }

    template <int N, int Mandatory>
    constexpr bool operator>=(const Version<N, Mandatory> &left, const Version<N, Mandatory> &right)
    {
        return !(left < right);
    }
}
