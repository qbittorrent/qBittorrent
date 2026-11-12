/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
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

#include <QtCore/private/qeventdispatcher_cf_p.h>

// Lets Qt's main event loop and Cocoa's share the main thread.
//
// Qt's events are served from the main thread's CFRunLoop, which Cocoa also
// runs. The first time QCoreApplication::exec() waits for events, this
// dispatcher hands the thread to [NSApp run] instead, which serves both. When
// QCoreApplication::exit() is called, from anywhere, [NSApp run] is stopped,
// so that exec() returns and qBittorrent shuts down as usual.
class MainLoopDispatcher final : public QEventDispatcherCoreFoundation
{
    Q_DISABLE_COPY_MOVE(MainLoopDispatcher)

public:
    using QEventDispatcherCoreFoundation::QEventDispatcherCoreFoundation;

    bool processEvents(QEventLoop::ProcessEventsFlags flags) override;
    void interrupt() override;

private:
    bool m_isCocoaRunning = false;
    int m_nestedLoopDepth = 0;
};
