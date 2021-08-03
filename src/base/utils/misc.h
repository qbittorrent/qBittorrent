/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <memory>
#include <Windows.h>
#endif

#include <QString>

enum class ShutdownDialogAction;

/*  Miscellaneous functions that can be useful */

namespace Utils::Misc
{
    // Allow the user to choose between bit or bytes and:
    // IEC 60027-2 prefix names: kibi, mebi, gibi...
    // SI prefix names: kilo, mega, giga...
    //
    // References:
    // https://en.wikipedia.org/wiki/Kilobyte
    // https://wiki.ubuntu.com/UnitsPolicy
    enum class unitType
    {
        decimalPrefixBytes = 0, // kilobytes, megabytes, gigabytes...
        decimalPrefixBits,      // kilobits, megabits, gigabits...
        binaryPrefixBytes,      // kibibytes, mebibytes, gibibytes...
        binaryPrefixBits        // kibibits, mebibits, gibibits...
    };

    enum class SizeUnit
    {
        B,    // 1024^0 or 1000^0 depending on unitType,
        Ki,   // 1024^1 or 1000^1 depending on unitType,
        Me,   // 1024^2 or 1000^2 depending on unitType,
        Gi,   // 1024^3 or 1000^3 depending on unitType,
        Te,   // 1024^4 or 1000^4 depending on unitType,
        Pe,   // 1024^5 or 1000^5 depending on unitType,
        Ex    // 1024^6 or 1000^6 depending on unitType
        // int64 is used for sizes and thus the next units can not be handled
        // Ze,   // 1024^7 or 1000^7 depending on unitType
        // Yo,   // 1024^8 or 1000^8 depending on unitType
    };

    QString parseHtmlLinks(const QString &rawText);

    void shutdownComputer(const ShutdownDialogAction &action);

    QString osName();
    QString boostVersionString();
    QString libtorrentVersionString();
    QString opensslVersionString();
    QString zlibVersionString();

    QString unitString(SizeUnit unit, const unitType prefix, const bool isSpeed = false);

    // return the best user friendly unit (B, KiB, MiB, GiB, kB, MB, GB, kb, Mb, Gb...)
    // value must be given in bytes but the output string can be bits or bytes
    // depending on the user preferences
    QString friendlyUnit(qint64 bytes, bool isSpeed = false);
    int friendlyUnitPrecision(SizeUnit unit);
    qint64 sizeInBytes(qreal size, SizeUnit unit, unitType prefix);
    bool prefixIsInBits(const unitType prefix);
    bool prefixIsDecimal(const unitType prefix);

    bool isPreviewable(const QString &filename);

    // Take a number of seconds and return a user-friendly
    // time duration like "1d 2h 10m".
    QString userFriendlyDuration(qlonglong seconds, qlonglong maxCap = -1);
    QString getUserIDString();

#ifdef Q_OS_WIN
    QString windowsSystemPath();

    template <typename T>
    T loadWinAPI(const QString &source, const char *funcName)
    {
        QString path = windowsSystemPath();
        if (!path.endsWith('\\'))
            path += '\\';

        path += source;

        auto pathWchar = std::make_unique<wchar_t[]>(path.length() + 1);
        path.toWCharArray(pathWchar.get());

        return reinterpret_cast<T>(
            ::GetProcAddress(::LoadLibraryW(pathWchar.get()), funcName));
    }
#endif // Q_OS_WIN
}
