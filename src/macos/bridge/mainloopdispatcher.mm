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

#include "mainloopdispatcher.h"

#import <AppKit/AppKit.h>

#include "qbtentry.h"

bool MainLoopDispatcher::processEvents(const QEventLoop::ProcessEventsFlags flags)
{
    const bool waits = flags.testFlag(QEventLoop::WaitForMoreEvents);
    if (!waits || m_isCocoaRunning)
    {
        // A loop nested in a Qt callback, or a plain poll: serve Qt's events
        // only. Cocoa's own events wait until control returns to [NSApp run].
        ++m_nestedLoopDepth;
        const bool result = QEventDispatcherCoreFoundation::processEvents(flags);
        --m_nestedLoopDepth;
        return result;
    }

    m_isCocoaRunning = true;
    qbt_macos_run_ui();
    m_isCocoaRunning = false;
    return true;
}

void MainLoopDispatcher::interrupt()
{
    QEventDispatcherCoreFoundation::interrupt();

    // Only the outermost loop is Cocoa's; a nested Qt loop exits on its own.
    if (!m_isCocoaRunning || (m_nestedLoopDepth > 0))
        return;

    [NSApp stop:nil];
    // -stop: takes effect once the current event has been handled, so post
    // one in case the run loop is idle.
    NSEvent *wakeUp = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
        modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:wakeUp atStart:YES];
}
