/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#ifndef Q_MOC_RUN
#include <boost/circular_buffer.hpp>
#endif

#include <QtTypes>

template<typename T>
struct Sample
{
    constexpr Sample() = default;

    constexpr Sample(const T dl, const T ul)
        : download {dl}
        , upload {ul}
    {
    }

    constexpr Sample<T> &operator+=(const Sample<T> &other)
    {
        download += other.download;
        upload += other.upload;
        return *this;
    }

    constexpr Sample<T> &operator-=(const Sample<T> &other)
    {
        download -= other.download;
        upload -= other.upload;
        return *this;
    }

    T download {};
    T upload {};
};

using SpeedSample = Sample<qlonglong>;
using SpeedSampleAvg = Sample<qreal>;

class SpeedMonitor
{
public:
    SpeedMonitor();

    void addSample(const SpeedSample &sample);
    SpeedSampleAvg average() const;
    void reset();

private:
    static const int MAX_SAMPLES = 30;
    boost::circular_buffer<SpeedSample> m_speedSamples;
    SpeedSample m_sum;
};
