/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Kieran Devlin
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

#include <QMetaEnum>

namespace BitTorrent
{
    // Using `Q_ENUM_NS()` without a wrapper namespace in our case is not advised
    // since `Q_NAMESPACE` cannot be used when the same namespace resides at different files.
    // https://www.kdab.com/new-qt-5-8-meta-object-support-namespaces/#comment-143779
    inline namespace FileSchedulingModeNS
    {
        Q_NAMESPACE

        // Controls how strictly the per-file download order ("rank") is enforced.
        // See `filescheduler.h` and `doc/file-download-order.md` for the full description.
        enum class FileSchedulingMode
        {
            // Ranked files are preferred in their order (libtorrent weighs piece priority against rarity),
            // other files can still be downloaded at the same time. The head of the queue is preferred most.
            Soft = 0,
            // Like `Soft`, but unranked files are not downloaded until all ranked files are complete.
            RankedFirst = 1,
            // Only the file(s) at the head of the queue are downloaded. Everything else waits.
            Strict = 2
        };

        Q_ENUM_NS(FileSchedulingMode)
    }
}
