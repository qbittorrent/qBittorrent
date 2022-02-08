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

#include "base/pathfwd.h"

enum class ShutdownDialogAction;

/*  Miscellaneous functions that can be useful */

namespace Utils::Misc
{
    // use binary prefix standards from IEC 60027-2
    // see http://en.wikipedia.org/wiki/Kilobyte
    enum class SizeUnit
    {
        Byte,       // 1024^0,
        KibiByte,   // 1024^1,
        MebiByte,   // 1024^2,
        GibiByte,   // 1024^3,
        TebiByte,   // 1024^4,
        PebiByte,   // 1024^5,
        ExbiByte    // 1024^6,
        // int64 is used for sizes and thus the next units can not be handled
        // ZebiByte,   // 1024^7,
        // YobiByte,   // 1024^8
    };

    QString parseHtmlLinks(const QString &rawText);

    void shutdownComputer(const ShutdownDialogAction &action);

    QString osName();
    QString boostVersionString();
    QString libtorrentVersionString();
    QString opensslVersionString();
    QString zlibVersionString();

    QString unitString(SizeUnit unit, bool isSpeed = false);

    // return the best user friendly storage unit (B, KiB, MiB, GiB, TiB)
    // value must be given in bytes
    QString friendlyUnit(qint64 bytes, bool isSpeed = false);
    int friendlyUnitPrecision(SizeUnit unit);
    qint64 sizeInBytes(qreal size, SizeUnit unit);

    bool isPreviewable(const Path &filePath);

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
