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

#include <cerrno>
#include <cstring>
#include <limits>

#include <sys/random.h>

#include <QtLogging>

namespace
{
    class RandomLayer
    {
    // need to satisfy UniformRandomBitGenerator requirements
    public:
        using result_type = uint32_t;

        RandomLayer()
        {
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
            const int RETRY_MAX = 3;

            for (int i = 0; i < RETRY_MAX; ++i)
            {
                result_type buf = 0;
                const ssize_t result = ::getrandom(&buf, sizeof(buf), 0);
                if (result == sizeof(buf))  // success
                    return buf;

                if (result < 0)
                    qFatal("getrandom() error. Reason: %s. Error code: %d.", std::strerror(errno), errno);
            }

            qFatal("getrandom() failed. Reason: too many retries.");
        }
    };
}
