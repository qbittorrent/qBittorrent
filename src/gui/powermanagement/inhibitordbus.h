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

#pragma once

#include <QDBusUnixFileDescriptor>
#include <QObject>

#include "inhibitor.h"

class QDBusInterface;
class QDBusPendingCallWatcher;

class InhibitorDBus final : public QObject, public Inhibitor
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(InhibitorDBus)

public:
    InhibitorDBus(QObject *parent = nullptr);

    bool requestBusy() override;
    bool requestIdle() override;

private slots:
    void onAsyncReply(QDBusPendingCallWatcher *call);

private:
    enum State
    {
        Error,
        Idle,
        RequestBusy,
        Busy,
        RequestIdle
    };

    enum class ManagerType
    {
        Freedesktop,  // https://www.freedesktop.org/wiki/Specifications/power-management-spec/
        Gnome,  // https://github.com/GNOME/gnome-settings-daemon/blob/master/gnome-settings-daemon/org.gnome.SessionManager.xml
        Systemd // https://www.freedesktop.org/software/systemd/man/org.freedesktop.login1.html
    };

    QDBusInterface *m_busInterface = nullptr;
    ManagerType m_manager = ManagerType::Gnome;

    enum State m_state = Error;
    enum State m_intendedState = Idle;
    uint m_cookie = 0;
    QDBusUnixFileDescriptor m_fd;
};
