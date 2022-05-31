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

#pragma once

#include <QObject>
#include <QString>

#include "base/path.h"

/// Utility class to defer file deletion
class FileGuard
{
public:
    explicit FileGuard(const Path &path = {});
    ~FileGuard();

    /// Cancels or re-enables deferred file deletion
    void setAutoRemove(bool remove) noexcept;

private:
    Path m_path;
    bool m_remove = false;
};

/// Reads settings for .torrent files from preferences
/// and sets the file guard up accordingly
class TorrentFileGuard : private FileGuard
{
    Q_GADGET

public:
    explicit TorrentFileGuard(const Path &path = {});
    ~TorrentFileGuard();

    /// marks the torrent file as loaded (added) into the BitTorrent::Session
    void markAsAddedToSession();
    using FileGuard::setAutoRemove;

    enum AutoDeleteMode : int     // do not change these names: they are stored in config file
    {
        Never,
        IfAdded,
        Always
    };

    // static interface to get/set preferences
    static AutoDeleteMode autoDeleteMode();
    static void setAutoDeleteMode(AutoDeleteMode mode);

private:
    TorrentFileGuard(const Path &path, AutoDeleteMode mode);

    Q_ENUM(AutoDeleteMode)
    AutoDeleteMode m_mode;
    bool m_wasAdded = false;
};
