/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Mike Tzou (Chocobo1)
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
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

#include <QtSystemDetection>

#ifdef Q_OS_WIN
#include <string>

#include <windows.h>

#include <QString>
#endif

#include "base/path.h"

enum class ShutdownDialogAction;

namespace Utils::OS
{
    void shutdownComputer(const ShutdownDialogAction &action);

#ifdef Q_OS_MACOS
    bool isTorrentFileAssocSet();
    void setTorrentFileAssoc();
    bool isMagnetLinkAssocSet();
    void setMagnetLinkAssoc();
#endif // Q_OS_MACOS

#ifdef Q_OS_WIN
    Path windowsSystemPath();

    template <typename T>
    T loadWinAPI(const QString &source, const char *funcName)
    {
        const std::wstring path = (windowsSystemPath() / Path(source)).toString().toStdWString();
        return reinterpret_cast<T>(::GetProcAddress(::LoadLibraryW(path.c_str()), funcName));
    }
#endif // Q_OS_WIN

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    bool applyMarkOfTheWeb(const Path &file, const QString &url = {});
#endif // Q_OS_MACOS || Q_OS_WIN
}
