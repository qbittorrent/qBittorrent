/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "torrentfileguard.h"

#include "settingvalue.h"
#include "utils/fs.h"

namespace
{
    SettingValue<TorrentFileGuard::AutoDeleteMode> autoDeleteModeSetting()
    {
        return SettingValue<TorrentFileGuard::AutoDeleteMode> {u"Core/AutoDeleteAddedTorrentFile"_s};
    }
}

FileGuard::FileGuard(const Path &path)
    : m_path {path}
    , m_remove {true}
{
}

void FileGuard::setAutoRemove(const bool remove) noexcept
{
    m_remove = remove;
}

FileGuard::~FileGuard()
{
    if (m_remove && !m_path.isEmpty())
        Utils::Fs::removeFile(m_path); // removeFile() checks for file existence
}

TorrentFileGuard::TorrentFileGuard(const Path &path, const TorrentFileGuard::AutoDeleteMode mode)
    : FileGuard {mode != Never ? path : Path()}
    , m_mode {mode}
{
}

TorrentFileGuard::TorrentFileGuard(const Path &path)
    : TorrentFileGuard {path, autoDeleteMode()}
{
}

TorrentFileGuard::~TorrentFileGuard()
{
    if (!m_wasAdded && (m_mode != Always))
        setAutoRemove(false);
}

void TorrentFileGuard::markAsAddedToSession()
{
    m_wasAdded = true;
}

TorrentFileGuard::AutoDeleteMode TorrentFileGuard::autoDeleteMode()
{
    return autoDeleteModeSetting().get(AutoDeleteMode::Never);
}

void TorrentFileGuard::setAutoDeleteMode(const TorrentFileGuard::AutoDeleteMode mode)
{
    autoDeleteModeSetting() = mode;
}
