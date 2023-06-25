/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2023  Vladimir Golovnev <glassez@yandex.ru>
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
    void handleFastresumeRejectedAlert(const lt::fastresume_rejected_alert *alert)
    {
        alert->handle.unset_flags(lt::torrent_flags::auto_managed);
        alert->handle.pause();
    }
}

bool NativeSessionExtension::isSessionListening() const
{
    const QReadLocker locker {&m_lock};
    return m_isSessionListening;
}

void NativeSessionExtension::added(const lt::session_handle &nativeSession)
{
    m_nativeSession = nativeSession;
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
    case lt::session_stats_alert::alert_type:
        handleSessionStatsAlert(static_cast<const lt::session_stats_alert *>(alert));
        break;
    case lt::fastresume_rejected_alert::alert_type:
        handleFastresumeRejectedAlert(static_cast<const lt::fastresume_rejected_alert *>(alert));
        break;
    default:
        break;
    }
}

void NativeSessionExtension::handleSessionStatsAlert([[maybe_unused]] const lt::session_stats_alert *alert)
{
    const QWriteLocker locker {&m_lock};
    m_isSessionListening = m_nativeSession.is_listening();
}
