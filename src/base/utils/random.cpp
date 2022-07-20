/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou
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

#include "random.h"

#include <random>

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Ntsecapi.h>
#else  // Q_OS_WIN
#include <cerrno>
#include <cstdio>
#include <cstring>
#endif

#include <QString>

#include "base/global.h"
#include "base/utils/misc.h"

namespace
{
#ifdef Q_OS_WIN
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
            : m_rtlGenRandom {Utils::Misc::loadWinAPI<PRTLGENRANDOM>(u"Advapi32.dll"_qs, "SystemFunction036")}
        {
            if (!m_rtlGenRandom)
                qFatal("Failed to load RtlGenRandom()");
        }

        static constexpr result_type min()
        {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max()
        {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()()
        {
            result_type buf = 0;
            const bool result = m_rtlGenRandom(&buf, sizeof(buf));
            if (!result)
                qFatal("RtlGenRandom() failed");

            return buf;
        }

    private:
        using PRTLGENRANDOM = BOOLEAN (WINAPI *)(PVOID, ULONG);
        const PRTLGENRANDOM m_rtlGenRandom;
    };
#else  // Q_OS_WIN
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
            : m_randDev {fopen("/dev/urandom", "rb")}
        {
            if (!m_randDev)
                qFatal("Failed to open /dev/urandom. Reason: %s. Error code: %d.\n", std::strerror(errno), errno);
        }

        ~RandomLayer()
        {
            fclose(m_randDev);
        }

        static constexpr result_type min()
        {
            return std::numeric_limits<result_type>::min();
        }

        static constexpr result_type max()
        {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()() const
        {
            result_type buf = 0;
            if (fread(&buf, sizeof(buf), 1, m_randDev) != 1)
                qFatal("Read /dev/urandom error. Reason: %s. Error code: %d.\n", std::strerror(errno), errno);

            return buf;
        }

    private:
        FILE *m_randDev = nullptr;
    };
#endif
}

uint32_t Utils::Random::rand(const uint32_t min, const uint32_t max)
{
    static RandomLayer layer;

    // new distribution is cheap: https://stackoverflow.com/a/19036349
    std::uniform_int_distribution<uint32_t> uniform(min, max);

    return uniform(layer);
}
