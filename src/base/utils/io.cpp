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

#include "io.h"

#include <limits>
#include <utility>

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>

#include <QCoreApplication>
#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>
#include <QString>
#include <QTemporaryFile>

#include "base/path.h"
#include "base/utils/fs.h"

Utils::IO::FileDeviceOutputIterator::FileDeviceOutputIterator(QFileDevice &device, const int bufferSize)
    : m_device {&device}
    , m_buffer {std::make_shared<QByteArray>()}
    , m_bufferSize {bufferSize}
{
    m_buffer->reserve(bufferSize);
}

Utils::IO::FileDeviceOutputIterator::~FileDeviceOutputIterator()
{
    if (m_buffer.use_count() == 1)
    {
        if (m_device->error() == QFileDevice::NoError)
            m_device->write(*m_buffer);
        m_buffer->clear();
    }
}

Utils::IO::FileDeviceOutputIterator &Utils::IO::FileDeviceOutputIterator::operator=(const char c)
{
    m_buffer->append(c);
    if (m_buffer->size() >= m_bufferSize)
    {
        if (m_device->error() == QFileDevice::NoError)
            m_device->write(*m_buffer);
        m_buffer->clear();
    }
    return *this;
}

nonstd::expected<QByteArray, Utils::IO::ReadError> Utils::IO::readFile(const Path &path, const qint64 maxSize, const QIODevice::OpenMode additionalMode)
{
    QFile file {path.data()};
    if (!file.open(QIODevice::ReadOnly | additionalMode))
    {
        const QString message = QCoreApplication::translate("Utils::IO", "File open error. File: \"%1\". Error: \"%2\"")
            .arg(file.fileName(), file.errorString());
        return nonstd::make_unexpected(ReadError {ReadError::NotExist, message});
    }

    const qint64 fileSize = file.size();
    if ((maxSize >= 0) && (fileSize > maxSize))
    {
        const QString message = QCoreApplication::translate("Utils::IO", "File size exceeds limit. File: \"%1\". File size: %2. Size limit: %3")
            .arg(file.fileName(), QString::number(fileSize), QString::number(maxSize));
        return nonstd::make_unexpected(ReadError {ReadError::ExceedSize, message});
    }
    if (!std::in_range<qsizetype>(fileSize))
    {
        const QString message = QCoreApplication::translate("Utils::IO", "File size exceeds data size limit. File: \"%1\". File size: %2. Array limit: %3")
            .arg(file.fileName(), QString::number(fileSize), QString::number(std::numeric_limits<qsizetype>::max()));
        return nonstd::make_unexpected(ReadError {ReadError::ExceedSize, message});
    }

    QByteArray ret {static_cast<qsizetype>(fileSize), Qt::Uninitialized};
    const qint64 actualSize = file.read(ret.data(), fileSize);

    if (actualSize < 0)
    {
        const QString message = QCoreApplication::translate("Utils::IO", "File read error. File: \"%1\". Error: \"%2\"")
            .arg(file.fileName(), file.errorString());
        return nonstd::make_unexpected(ReadError {ReadError::Failed, message});
    }

    if (actualSize < fileSize)
    {
        // `QIODevice::Text` will convert CRLF to LF on-the-fly and affects return value
        // of `qint64 QIODevice::read(char *data, qint64 maxSize)`
        if (additionalMode.testFlag(QIODevice::Text))
        {
            ret.truncate(actualSize);
        }
        else
        {
            const QString message = QCoreApplication::translate("Utils::IO", "Read size mismatch. File: \"%1\". Expected: %2. Actual: %3")
                .arg(file.fileName(), QString::number(fileSize), QString::number(actualSize));
            return nonstd::make_unexpected(ReadError {ReadError::SizeMismatch, message});
        }
    }

    return ret;
}

nonstd::expected<void, QString> Utils::IO::saveToFile(const Path &path, const QByteArray &data)
{
    if (const Path parentPath = path.parentPath(); !parentPath.isEmpty())
        Utils::Fs::mkpath(parentPath);
    QSaveFile file {path.data()};
    if (!file.open(QIODevice::WriteOnly) || (file.write(data) != data.size()) || !file.flush() || !file.commit())
        return nonstd::make_unexpected(file.errorString());
    return {};
}

nonstd::expected<void, QString> Utils::IO::saveToFile(const Path &path, const lt::entry &data)
{
    QSaveFile file {path.data()};
    if (!file.open(QIODevice::WriteOnly))
        return nonstd::make_unexpected(file.errorString());

    const int bencodedDataSize = lt::bencode(Utils::IO::FileDeviceOutputIterator {file}, data);
    if ((file.size() != bencodedDataSize) || !file.flush() || !file.commit())
        return nonstd::make_unexpected(file.errorString());

    return {};
}

nonstd::expected<Path, QString> Utils::IO::saveToTempFile(const QByteArray &data)
{
    QTemporaryFile file {(Utils::Fs::tempPath() / Path(u"file_"_s)).data()};
    if (!file.open() || (file.write(data) != data.length()) || !file.flush())
        return nonstd::make_unexpected(file.errorString());

    file.setAutoRemove(false);
    return Path(file.fileName());
}

nonstd::expected<Path, QString> Utils::IO::saveToTempFile(const lt::entry &data)
{
    QTemporaryFile file {(Utils::Fs::tempPath() / Path(u"file_"_s)).data()};
    if (!file.open())
        return nonstd::make_unexpected(file.errorString());

    const int bencodedDataSize = lt::bencode(Utils::IO::FileDeviceOutputIterator {file}, data);
    if ((file.size() != bencodedDataSize) || !file.flush())
        return nonstd::make_unexpected(file.errorString());

    file.setAutoRemove(false);
    return Path(file.fileName());
}
