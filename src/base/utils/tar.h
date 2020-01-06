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

#pragma once

#include <QByteArray>
#include <QString>

namespace Utils
{
    namespace Tar
    {
        QHash<QString, QByteArray> untar(const QByteArray &data, QString &errorString);

        enum class EntryType : unsigned char
        {
            FileNull = '\0',
            FileAscii = '0',
            // We don't care about the other file types

            Unknown = std::numeric_limits<unsigned char>::max()
        };

        class StreamReader
        {
        public:
            explicit StreamReader(const QByteArray &data);
            explicit StreamReader(const QByteArray &&data) = delete;

            bool readNext();
            QString fileName() const;
            QByteArray fileData() const;
            EntryType entryType() const;

            bool atEnd() const;
            bool hasError() const;
            QString errorString() const;

        private:
            const QByteArray m_data;
            QString m_errorString;
            QString m_fileName;
            EntryType m_EntryType = EntryType::Unknown;
            int m_fileSize = 0;
            int m_pos = 0;
            int m_fileDataStart = 0;
        };
    }
}
