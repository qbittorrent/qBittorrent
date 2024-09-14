/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Mike Tzou (Chocobo1)
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

#include <limits>

#include <windows.h>

#include <QtLogging>

#include "base/global.h"
#include "base/utils/os.h"

namespace
{
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
            : m_processPrng {Utils::OS::loadWinAPI<PPROCESSPRNG>(u"BCryptPrimitives.dll"_s, "ProcessPrng")}
        {
            if (!m_processPrng)
                qFatal("Failed to load ProcessPrng().");
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
            const bool result = m_processPrng(reinterpret_cast<PBYTE>(&buf), sizeof(buf));
            if (!result)
                qFatal("ProcessPrng() failed.");

            return buf;
        }

    private:
        using PPROCESSPRNG = BOOL (WINAPI *)(PBYTE, SIZE_T);
        const PPROCESSPRNG m_processPrng = nullptr;
    };
}
