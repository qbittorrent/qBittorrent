/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Vladimir Golovnev <glassez@yandex.ru>
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QtGlobal>

#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
#include "powermanagement_x11.h"
#endif
#include "powermanagement.h"

#ifdef Q_WS_MAC
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

#ifdef Q_WS_WIN
#include <Windows.h>
#endif

PowerManagement::PowerManagement(QObject *parent) : QObject(parent), m_busy(false)
{
#if defined(Q_WS_X11) && defined(QT_DBUS_LIB)
    m_inhibitor = new PowerManagementInhibitor(this);
#endif
}

PowerManagement::~PowerManagement()
{
}

void PowerManagement::setActivityState(bool busy)
{
    if (busy) setBusy();
    else setIdle();
}

void PowerManagement::setBusy()
{
    if (m_busy) return;
    m_busy = true;

#ifdef Q_WS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#elif defined(Q_WS_X11) && defined(QT_DBUS_LIB)
    m_inhibitor->RequestBusy();
#elif defined(Q_WS_MAC)
    IOReturn success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &m_assertionID);
    if (success != kIOReturnSuccess) m_busy = false;
#endif
}

void PowerManagement::setIdle()
{
    if (!m_busy) return;
    m_busy = false;

#ifdef Q_WS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(Q_WS_X11) && defined(QT_DBUS_LIB)
    m_inhibitor->RequestIdle();
#elif defined(Q_WS_MAC)
    IOPMAssertionRelease(m_assertionID);
#endif
}
