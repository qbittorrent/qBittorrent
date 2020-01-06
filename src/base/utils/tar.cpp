/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  sledgehammer999 <hammered999@gmail.com>
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

#include "tar.h"

#include <QCoreApplication>
#include <QHash>

#include "base/utils/bytearray.h"
#include "base/utils/fs.h"

namespace
{
    const int TAR_HEADER_SIZE = 512;

    const int TAR_FILENAME_OFFSET = 0;
    const int TAR_FILESIZE_OFFSET = 124;
    const int TAR_CHECKSUM_OFFSET = 148;
    const int TAR_FILETYPE_OFFSET = 156;
    const int TAR_FILENAME_PREFIX_OFFSET = 345;

    const int TAR_FILENAME_SIZE = 100;
    const int TAR_FILESIZE_SIZE = 12;
    const int TAR_CHECKSUM_SIZE = 8;
    const int TAR_FILENAME_PREFIX_SIZE = 155;

    enum class State
    {
        Empty,
        Invalid,
        Valid
    };

    int octalToDec(const QByteArray &data)
    {
        // only the ASCII characters from '0' to '7' are valid
        int number = 0;
        for (const char byte : data) {
            if (byte < '0' || byte > '7') // skip trailing invalid bytes
                break;

            number = (8 * number) + (byte - '0');
        }

        return number;
    }

    State verifyAndCheckNull(const QByteArray &data)
    {
        int calculatedChecksum = 0;
        for (int i = 0; i < TAR_CHECKSUM_OFFSET; ++i)
            calculatedChecksum += static_cast<uchar>(data.at(i));

        calculatedChecksum += TAR_CHECKSUM_SIZE * ' '; // 8 ASCII spaces for the checksum field

        for (int i = TAR_FILETYPE_OFFSET; i < data.size(); ++i)
            calculatedChecksum += static_cast<uchar>(data.at(i));

        const int checkSum = octalToDec(Utils::ByteArray::midView(data, TAR_CHECKSUM_OFFSET, TAR_CHECKSUM_SIZE));
        if ((calculatedChecksum == TAR_CHECKSUM_SIZE * ' ') && (checkSum == 0))
            return State::Empty;
        else if (calculatedChecksum == checkSum)
            return State::Valid;

        return State::Invalid;
    }

    State parseHeader(const QByteArray &header, QString &fileName, int &fileSize, Utils::Tar::EntryType &entryType)
    {
        if (header.size() != TAR_HEADER_SIZE)
            return State::Invalid;

        State state = verifyAndCheckNull(header);

        // Invalid header: The first header field represents the filename.
        // If the 1st byte is null then we can't extract a name. So the whole header becomes invalid to us.
        if ((state == State::Valid) && header.startsWith('\0'))
            return State::Invalid;
        else if (state != State::Valid)
            return state;

        fileSize = octalToDec(Utils::ByteArray::midView(header, TAR_FILESIZE_OFFSET, TAR_FILESIZE_SIZE));
        entryType = static_cast<Utils::Tar::EntryType>(header.at(TAR_FILETYPE_OFFSET));

        const QByteArray name = Utils::ByteArray::midView(header, TAR_FILENAME_OFFSET, TAR_FILENAME_SIZE);
        const int nameLen = strlen(name);
        if (nameLen == 0)
            return State::Invalid;

        const QByteArray namePrefix = Utils::ByteArray::midView(header, TAR_FILENAME_PREFIX_OFFSET, TAR_FILENAME_PREFIX_SIZE);
        const int namePrefixLen = strlen(namePrefix);

        const QString fullName = (namePrefixLen > 0)
                                  ? QString::fromUtf8(namePrefix.constData(), namePrefixLen)
                                    + '/' + QString::fromUtf8(name.constData(), nameLen)
                                  : QString::fromUtf8(name.constData(), nameLen);

        fileName = Utils::Fs::toValidFileSystemName(fullName, true);

        return State::Valid;
    }
}

QHash<QString, QByteArray> Utils::Tar::untar(const QByteArray &data, QString &errorString)
{
    QHash<QString, QByteArray> files;
    StreamReader reader(data);
    while (reader.readNext()) {
        if (!(reader.entryType() == EntryType::FileAscii
              || reader.entryType() == EntryType::FileNull)) {
            continue;
        }

        files.insert(reader.fileName(), reader.fileData());
    }

    errorString = reader.errorString();
    return files;
}

Utils::Tar::StreamReader::StreamReader(const QByteArray &data)
    : m_data(data)
{
    if (m_data.size() < TAR_HEADER_SIZE) {
        m_errorString = QCoreApplication::translate(
                            "Utils::Tar",
                            "The tar file is only %1 bytes long.",
                            "'tar' should be left untranslated. It is a filetype.")
                        .arg(m_data.size());
    }
}

bool Utils::Tar::StreamReader::readNext()
{
    if (atEnd() || hasError())
        return false;

    int dataSize = m_data.size();
    if (dataSize - m_pos < TAR_HEADER_SIZE) {
        m_errorString = QCoreApplication::translate(
                            "Utils::Tar",
                            "The tar file has ended prematurely",
                            "'tar' should be left untranslated. It is a filetype.");
        return false;
    }

    State state = parseHeader(Utils::ByteArray::midView(m_data, m_pos, TAR_HEADER_SIZE), m_fileName, m_fileSize, m_EntryType);

    if (state == State::Empty) {
        m_pos = dataSize;
        return false;
    }

    if (state == State::Invalid) {
        m_errorString = QCoreApplication::translate(
                            "Utils::Tar",
                            "The tar file has an invalid header at position %1",
                            "'tar' should be left untranslated. It is a filetype.")
                        .arg(m_pos);
        return false;
    }

    const int fileSize = m_fileSize;
    const int availableSize = m_data.size() - m_pos - TAR_HEADER_SIZE;
    if (availableSize < fileSize) {
        m_errorString = QCoreApplication::translate(
                            "Utils::Tar",
                            "The tar file has ended prematurely",
                            "'tar' should be left untranslated. It is a filetype.");
        return false;
    }

    m_fileDataStart = m_pos + TAR_HEADER_SIZE;
    const int padding = (m_fileSize > 0) ? (TAR_HEADER_SIZE - (m_fileSize % TAR_HEADER_SIZE))
                                       : 0;


    m_pos += TAR_HEADER_SIZE + m_fileSize + padding;
    return true;
}

QString Utils::Tar::StreamReader::fileName() const
{
    if (atEnd() || hasError())
        return {};

    return m_fileName;
}

QByteArray Utils::Tar::StreamReader::fileData() const
{
    if (atEnd() || hasError())
        return {};

    return {m_data.constData() + m_fileDataStart, m_fileSize};
}

Utils::Tar::EntryType Utils::Tar::StreamReader::entryType() const
{
    if (atEnd() || hasError())
        return EntryType::Unknown;

    return m_EntryType;
}

bool Utils::Tar::StreamReader::atEnd() const
{
    return (m_pos >= m_data.size());
}

bool Utils::Tar::StreamReader::hasError() const
{
    return !errorString().isEmpty();
}

QString Utils::Tar::StreamReader::errorString() const
{
    return m_errorString;
}
