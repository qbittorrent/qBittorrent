/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Mike Tzou (Chocobo1)
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

#include <iterator>
#include <memory>

#include <libtorrent/fwd.hpp>

#include <QIODevice>

#include "base/3rdparty/expected.hpp"
#include "base/pathfwd.h"

class QByteArray;
class QFileDevice;
class QString;

namespace Utils::IO
{
    // A wrapper class that satisfy LegacyOutputIterator requirement
    class FileDeviceOutputIterator
    {
    public:
        // std::iterator_traits
        using iterator_category = std::output_iterator_tag;
        using difference_type = void;
        using value_type = void;
        using pointer = void;
        using reference = void;

        explicit FileDeviceOutputIterator(QFileDevice &device, int bufferSize = (4 * 1024));
        FileDeviceOutputIterator(const FileDeviceOutputIterator &other) = default;
        ~FileDeviceOutputIterator();

        // mimic std::ostream_iterator behavior
        FileDeviceOutputIterator &operator=(char c);

        constexpr FileDeviceOutputIterator &operator*()
        {
            return *this;
        }

        constexpr FileDeviceOutputIterator &operator++()
        {
            return *this;
        }

        constexpr FileDeviceOutputIterator &operator++(int)
        {
            return *this;
        }

    private:
        QFileDevice *m_device = nullptr;
        std::shared_ptr<QByteArray> m_buffer;
        int m_bufferSize = 0;
    };

    struct ReadError
    {
        enum Code
        {
            NotExist,
            ExceedSize,
            Failed,  // `read()` operation failed
            SizeMismatch
        };

        Code status = {};
        QString message;
    };

    // TODO: define a specific type for `additionalMode`
    // providing `size` is explicit and is strongly recommended
    nonstd::expected<QByteArray, ReadError> readFile(const Path &path, qint64 size, QIODevice::OpenMode additionalMode = {});

    nonstd::expected<void, QString> saveToFile(const Path &path, const QByteArray &data);
    nonstd::expected<void, QString> saveToFile(const Path &path, const lt::entry &data);
    nonstd::expected<Path, QString> saveToTempFile(const QByteArray &data);
    nonstd::expected<Path, QString> saveToTempFile(const lt::entry &data);
}
