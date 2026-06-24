/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2018-2025  Mike Tzou (Chocobo1)
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

#include "reg.h"

#include <QScopeGuard>
#include <QStringList>

QStringList Utils::Reg::getSubkeys(const HKEY handle)
{
    QStringList keys;

    DWORD cSubKeys = 0;
    DWORD cMaxSubKeyLen = 0;
    const LSTATUS result = ::RegQueryInfoKeyW(handle, NULL, NULL, NULL, &cSubKeys, &cMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);

    if (result == ERROR_SUCCESS)
    {
        ++cMaxSubKeyLen; // For null character
        LPWSTR lpName = new WCHAR[cMaxSubKeyLen] {0};
        [[maybe_unused]] const auto lpNameGuard = qScopeGuard([&lpName] { delete[] lpName; });

        keys.reserve(cSubKeys);

        for (DWORD i = 0; i < cSubKeys; ++i)
        {
            DWORD cName = cMaxSubKeyLen;
            const LSTATUS res = ::RegEnumKeyExW(handle, i, lpName, &cName, NULL, NULL, NULL, NULL);
            if (res == ERROR_SUCCESS)
                keys.append(QString::fromWCharArray(lpName));
        }
    }

    return keys;
}

QString Utils::Reg::getStringValue(const HKEY handle, const QString &name)
{
    const std::wstring nameWStr = name.toStdWString();
    DWORD type = 0;
    DWORD cbData = 0;
    // Discover the size of the value
    ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, NULL, &cbData);

    const DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
    LPWSTR lpData = new WCHAR[cBuffer] {};
    [[maybe_unused]] const auto lpDataGuard = qScopeGuard([&lpData] { delete[] lpData; });

    if (!((type == REG_SZ) || (type == REG_EXPAND_SZ)))
        return {};

    const LSTATUS res = ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, reinterpret_cast<LPBYTE>(lpData), &cbData);
    if (res == ERROR_SUCCESS)
        return QString::fromWCharArray(lpData);

    return {};
}

ulong Utils::Reg::getULongValue(const HKEY handle, const QString &name)
{
    const std::wstring nameWStr = name.toStdWString();
    DWORD type = 0;
    DWORD cbData = 0;
    // Discover the size of the value
    ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, NULL, &cbData);

    if (type != REG_DWORD)
        return 0;

    DWORD lpData = 0;
    const LSTATUS res = ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, reinterpret_cast<LPBYTE>(&lpData), &cbData);
    if (res == ERROR_SUCCESS)
        return lpData;

    return {};
}
