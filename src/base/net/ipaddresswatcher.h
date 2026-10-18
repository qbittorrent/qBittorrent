/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  WordlessEcho <wordless@echo.moe>
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

class QSocketNotifier;
class QTimer;

namespace Net
{
    // Watches the IPv4 and IPv6 addresses of the system and notifies when they change.
    // Bursts of OS events are merged into one `addressesChanged()` signal.
    // Implemented on Linux (netlink), macOS (kernel events) and Windows. It does nothing on other platforms.
    class IPAddressWatcher final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(IPAddressWatcher)

    public:
        explicit IPAddressWatcher(QObject *parent = nullptr);
        ~IPAddressWatcher() override;

    signals:
        void addressesChanged();

    private:
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        void readEventSocket();
#endif

        QTimer *m_debounceTimer = nullptr;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        int m_eventSocket = -1;
        QSocketNotifier *m_socketNotifier = nullptr;
#elif defined(Q_OS_WIN)
        void *m_notificationHandle = nullptr;  // HANDLE
#endif
    };
}
