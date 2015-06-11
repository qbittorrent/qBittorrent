/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef BITTORRENT_SESSIONSTATUS_H
#define BITTORRENT_SESSIONSTATUS_H

#include <libtorrent/session_status.hpp>
#include <QtGlobal>

namespace BitTorrent
{
    class SessionStatus
    {
    public:
        SessionStatus(const libtorrent::session_status &nativeStatus);

        bool hasIncomingConnections() const;

        // Return current download rate for the BT
        // session. Payload means that it only take into
        // account "useful" part of the rate
        int payloadDownloadRate() const;

        // Return current upload rate for the BT
        // session. Payload means that it only take into
        // account "useful" part of the rate
        int payloadUploadRate() const;

        qlonglong totalDownload() const;
        qlonglong totalUpload() const;
        qlonglong totalPayloadDownload() const;
        qlonglong totalPayloadUpload() const;
        qlonglong totalWasted() const;
        int diskReadQueue() const;
        int diskWriteQueue() const;
        int dhtNodes() const;
        int peersCount() const;

    private:
        libtorrent::session_status m_nativeStatus;
    };
}

#endif // BITTORRENT_SESSIONSTATUS_H
