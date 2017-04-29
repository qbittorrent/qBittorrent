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
 *
 */

#include "random.h"

#include <cassert>
#include <chrono>
#include <random>

#include <QtGlobal>

#ifdef Q_OS_MAC
#include <QThreadStorage>
#endif

// on some platform `std::random_device` may generate the same number sequence
static bool hasTrueRandomDevice{ std::random_device{}() != std::random_device{}() };

uint32_t Utils::Random::rand(const uint32_t min, const uint32_t max)
{
#ifdef Q_OS_MAC  // workaround for Apple xcode: https://stackoverflow.com/a/29929949
    static QThreadStorage<std::mt19937> generator;
    if (!generator.hasLocalData())
        generator.localData().seed(
            hasTrueRandomDevice
            ? std::random_device{}()
            : (std::random_device::result_type) std::chrono::system_clock::now().time_since_epoch().count()
        );
#else
    static thread_local std::mt19937 generator{
        hasTrueRandomDevice
        ? std::random_device{}()
        : (std::random_device::result_type) std::chrono::system_clock::now().time_since_epoch().count()
    };
#endif

    // better replacement for `std::rand`, don't use this for real cryptography application
    // min <= returned_value <= max
    assert(min <= max);

    // new distribution is cheap: http://stackoverflow.com/a/19036349
    std::uniform_int_distribution<uint32_t> uniform(min, max);

#ifdef Q_OS_MAC
    return uniform(generator.localData());
#else
    return uniform(generator);
#endif
}
