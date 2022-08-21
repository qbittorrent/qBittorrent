/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "nativesessionextension.h"

#include <libtorrent/alert_types.hpp>

#include "extensiondata.h"
#include "nativetorrentextension.h"

namespace
{
    void handleAddTorrentAlert([[maybe_unused]] const lt::add_torrent_alert *alert)
    {
#ifndef QBT_USES_LIBTORRENT2
        if (alert->error)
            return;

        // libtorrent < 2.0.7 has a bug that add_torrent_alert is posted too early
        // (before torrent is fully initialized and torrent extensions are created)
        // so we have to fill "extension data" in add_torrent_alert handler

        // NOTE: `data` may not exist if a torrent is added behind the scenes to download metadata
        auto *data = static_cast<ExtensionData *>(alert->params.userdata);
        if (data)
        {
            data->status = alert->handle.status({});
            data->trackers = alert->handle.trackers();
        }
#endif
    }

    void handleFastresumeRejectedAlert(const lt::fastresume_rejected_alert *alert)
    {
        alert->handle.unset_flags(lt::torrent_flags::auto_managed);
        alert->handle.pause();
    }
}

lt::feature_flags_t NativeSessionExtension::implemented_features()
{
    return alert_feature;
}

std::shared_ptr<lt::torrent_plugin> NativeSessionExtension::new_torrent(const lt::torrent_handle &torrentHandle, LTClientData clientData)
{
    return std::make_shared<NativeTorrentExtension>(torrentHandle, static_cast<ExtensionData *>(clientData));
}

void NativeSessionExtension::on_alert(const lt::alert *alert)
{
    switch (alert->type())
    {
    case lt::add_torrent_alert::alert_type:
        handleAddTorrentAlert(static_cast<const lt::add_torrent_alert *>(alert));
        break;
    case lt::fastresume_rejected_alert::alert_type:
        handleFastresumeRejectedAlert(static_cast<const lt::fastresume_rejected_alert *>(alert));
        break;
    default:
        break;
    }
}
