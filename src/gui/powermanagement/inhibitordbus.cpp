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

#include "inhibitordbus.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingReply>

#include "base/global.h"
#include "base/logger.h"

InhibitorDBus::InhibitorDBus(QObject *parent)
    : QObject(parent)
    , m_busInterface {new QDBusInterface(u"org.gnome.SessionManager"_s, u"/org/gnome/SessionManager"_s
        , u"org.gnome.SessionManager"_s, QDBusConnection::sessionBus(), this)}
{
    if (!m_busInterface->isValid())
    {
        delete m_busInterface;

        m_busInterface = new QDBusInterface(u"org.freedesktop.login1"_s, u"/org/freedesktop/login1"_s
            , u"org.freedesktop.login1.Manager"_s, QDBusConnection::systemBus(), this);
        m_manager = ManagerType::Systemd;
        if (!m_busInterface->isValid())
        {
            delete m_busInterface;

            m_busInterface = new QDBusInterface(u"org.freedesktop.PowerManagement"_s, u"/org/freedesktop/PowerManagement/Inhibit"_s
                , u"org.freedesktop.PowerManagement.Inhibit"_s, QDBusConnection::sessionBus(), this);
            m_manager = ManagerType::Freedesktop;
            if (!m_busInterface->isValid())
            {
                delete m_busInterface;
                m_busInterface = nullptr;
            }
        }
    }

    if (m_busInterface)
    {
        m_state = Idle;
        LogMsg(tr("Power management found suitable D-Bus interface. Interface: %1").arg(m_busInterface->interface()));
    }
    else
    {
        m_state = Error;
        LogMsg(tr("Power management error. Did not found suitable D-Bus interface."), Log::WARNING);
    }
}

bool InhibitorDBus::requestBusy()
{
    m_intendedState = Busy;

    switch (m_state)
    {
    case Busy:
    case RequestBusy:
        return true;
    case Error:
    case RequestIdle:
        return false;
    case Idle:
        break;
    };

    m_state = RequestBusy;

    const QString message = u"Active torrents are currently present"_s;

    QList<QVariant> args;
    switch (m_manager)
    {
    case ManagerType::Freedesktop:
        args = {u"qBittorrent"_s, message};
        break;
    case ManagerType::Gnome:
        args = {u"qBittorrent"_s, 0u, message, 4u};
        break;
    case ManagerType::Systemd:
        args = {u"sleep"_s, u"qBittorrent"_s, message, u"block"_s};
        break;
    }

    const QDBusPendingCall pcall = m_busInterface->asyncCallWithArgumentList(u"Inhibit"_s, args);
    const auto *watcher = new QDBusPendingCallWatcher(pcall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDBus::onAsyncReply);
    return true;
}

bool InhibitorDBus::requestIdle()
{
    m_intendedState = Idle;

    switch (m_state)
    {
    case Idle:
    case RequestIdle:
        return true;
    case Error:
    case RequestBusy:
        return false;
    case Busy:
        break;
    };

    if (m_manager == ManagerType::Systemd)
    {
        m_fd = {};
        m_state = Idle;
        return true;
    }

    m_state = RequestIdle;

    const QString method = (m_manager == ManagerType::Gnome)
        ? u"Uninhibit"_s
        : u"UnInhibit"_s;
    const QDBusPendingCall pcall = m_busInterface->asyncCall(method, m_cookie);
    const auto *watcher = new QDBusPendingCallWatcher(pcall, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDBus::onAsyncReply);
    return true;
}

void InhibitorDBus::onAsyncReply(QDBusPendingCallWatcher *call)
{
    call->deleteLater();

    if (m_state == RequestIdle)
    {
        const QDBusPendingReply reply = *call;

        if (reply.isError())
        {
            LogMsg(tr("Power management error. Action: %1. Error: %2").arg(u"RequestIdle"_s
                , reply.error().message()), Log::WARNING);
            m_state = Error;
        }
        else
        {
            m_state = Idle;
            if (m_intendedState == Busy)
                requestBusy();
        }
    }
    else if (m_state == RequestBusy)
    {
        if (m_manager == ManagerType::Systemd)
        {
            const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *call;

            if (reply.isError())
            {
                LogMsg(tr("Power management error. Action: %1. Error: %2").arg(u"RequestBusy"_s
                    , reply.error().message()), Log::WARNING);
                m_state = Error;
            }
            else
            {
                m_state = Busy;
                m_fd = reply.value();
                if (m_intendedState == Idle)
                    requestIdle();
            }
        }
        else
        {
            const QDBusPendingReply<uint> reply = *call;

            if (reply.isError())
            {
                LogMsg(tr("Power management error. Action: %1. Error: %2").arg(u"RequestBusy"_s
                    , reply.error().message()), Log::WARNING);
                m_state = Error;
            }
            else
            {
                m_state = Busy;
                m_cookie = reply.value();
                if (m_intendedState == Idle)
                    requestIdle();
            }
        }
    }
    else
    {
        const QDBusPendingReply reply = *call;
        const QDBusError error = reply.error();

        if (error.isValid())
        {
            LogMsg(tr("Power management unexpected error. State: %1. Error: %2").arg(QString::number(m_state)
                , error.message()), Log::WARNING);
        }
        m_state = Error;
    }
}
