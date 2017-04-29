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

#include <QMetaEnum>
#include "settingsstorage.h"
#include "utils/fs.h"

namespace
{
    const QLatin1String KEY_AUTO_DELETE_ENABLED ("Core/AutoDeleteAddedTorrentFile");
}

FileGuard::FileGuard(const QString &path)
    : m_path {path}
    , m_remove {true}
{
}

void FileGuard::setAutoRemove(bool remove) noexcept
{
    m_remove = remove;
}

FileGuard::~FileGuard()
{
    if (m_remove && !m_path.isEmpty())
        Utils::Fs::forceRemove(m_path); // forceRemove() checks for file existence
}

TorrentFileGuard::TorrentFileGuard(const QString &path, TorrentFileGuard::AutoDeleteMode mode)
    : FileGuard {mode != Never ? path : QString()}
    , m_mode {mode}
    , m_wasAdded {false}
{
}

TorrentFileGuard::TorrentFileGuard(const QString &path)
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
    QMetaEnum meta {modeMetaEnum()};
    return static_cast<AutoDeleteMode>(meta.keyToValue(SettingsStorage::instance()->loadValue(
                                                           KEY_AUTO_DELETE_ENABLED, meta.valueToKey(Never)).toByteArray()));
}

void TorrentFileGuard::setAutoDeleteMode(TorrentFileGuard::AutoDeleteMode mode)
{
    QMetaEnum meta {modeMetaEnum()};
    SettingsStorage::instance()->storeValue(KEY_AUTO_DELETE_ENABLED, meta.valueToKey(mode));
}

QMetaEnum TorrentFileGuard::modeMetaEnum()
{
    return QMetaEnum::fromType<AutoDeleteMode>();
}
