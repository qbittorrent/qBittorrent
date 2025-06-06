/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Mike Tzou (Chocobo1)
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
 */

#include "powermanagement.h"

#include <QtSystemDetection>

#if defined(Q_OS_MACOS)
#include "inhibitormacos.h"
using InhibitorImpl = InhibitorMacOS;
#elif defined(Q_OS_WIN)
#include "inhibitorwindows.h"
using InhibitorImpl = InhibitorWindows;
#elif defined(QBT_USES_DBUS)
#include "inhibitordbus.h"
using InhibitorImpl = InhibitorDBus;
#else
#include "inhibitor.h"
using InhibitorImpl = Inhibitor;
#endif

PowerManagement::PowerManagement()
    : m_inhibitor {new InhibitorImpl}
{
}

PowerManagement::~PowerManagement()
{
    setIdle();
    delete m_inhibitor;
}

void PowerManagement::setActivityState(const ActivityState state)
{
    switch (state)
    {
    case ActivityState::Busy:
        setBusy();
        break;

    case ActivityState::Idle:
        setIdle();
        break;
    };
}

void PowerManagement::setBusy()
{
    if (m_state == ActivityState::Busy)
        return;

    if (m_inhibitor->requestBusy())
        m_state = ActivityState::Busy;
}

void PowerManagement::setIdle()
{
    if (m_state == ActivityState::Idle)
        return;

    if (m_inhibitor->requestIdle())
        m_state = ActivityState::Idle;
}
