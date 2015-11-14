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

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>

#include "powermanagement_x11.h"

PowerManagementInhibitor::PowerManagementInhibitor(QObject *parent) : QObject(parent)
{
    if (!QDBusConnection::sessionBus().isConnected())
    {
        qDebug("D-Bus: Could not connect to session bus");
        m_state = error;
    }
    else
    {
        m_state = idle;
    }

    m_intended_state = idle;
    m_cookie = 0;
    m_use_gsm = false;
}

PowerManagementInhibitor::~PowerManagementInhibitor()
{
}

void PowerManagementInhibitor::RequestIdle()
{
    m_intended_state = idle;
    if (m_state == error || m_state == idle || m_state == request_idle || m_state == request_busy)
        return;

    qDebug("D-Bus: PowerManagementInhibitor: Requesting idle");

    QDBusMessage call;
    if (!m_use_gsm)
        call = QDBusMessage::createMethodCall(
                "org.freedesktop.PowerManagement",
                "/org/freedesktop/PowerManagement/Inhibit",
                "org.freedesktop.PowerManagement.Inhibit",
                "UnInhibit");
    else
        call = QDBusMessage::createMethodCall(
                "org.gnome.SessionManager",
                "/org/gnome/SessionManager",
                "org.gnome.SessionManager",
                "Uninhibit");

    m_state = request_idle;

    QList<QVariant> args;
    args << m_cookie;
    call.setArguments(args);

    QDBusPendingCall pcall = QDBusConnection::sessionBus().asyncCall(call, 1000);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(OnAsyncReply(QDBusPendingCallWatcher*)));
}


void PowerManagementInhibitor::RequestBusy()
{
    m_intended_state = busy;
    if (m_state == error || m_state == busy || m_state == request_busy || m_state == request_idle)
        return;

    qDebug("D-Bus: PowerManagementInhibitor: Requesting busy");

    QDBusMessage call;
    if (!m_use_gsm)
        call = QDBusMessage::createMethodCall(
                "org.freedesktop.PowerManagement",
                "/org/freedesktop/PowerManagement/Inhibit",
                "org.freedesktop.PowerManagement.Inhibit",
                "Inhibit");
    else
        call = QDBusMessage::createMethodCall(
                "org.gnome.SessionManager",
                "/org/gnome/SessionManager",
                "org.gnome.SessionManager",
                "Inhibit");

    m_state = request_busy;

    QList<QVariant> args;
    args << "qBittorrent";
    if (m_use_gsm) args << (uint)0;
    args << "Active torrents are presented";
    if (m_use_gsm) args << (uint)8;
    call.setArguments(args);

    QDBusPendingCall pcall = QDBusConnection::sessionBus().asyncCall(call, 1000);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(OnAsyncReply(QDBusPendingCallWatcher*)));
}

void PowerManagementInhibitor::OnAsyncReply(QDBusPendingCallWatcher *call)
{
    if (m_state == request_idle)
    {
        QDBusPendingReply<> reply = *call;

        if (reply.isError())
        {
            qDebug("D-Bus: Reply: Error: %s", qPrintable(reply.error().message()));
            m_state = error;
        }
        else
        {
            m_state = idle;
            qDebug("D-Bus: PowerManagementInhibitor: Request successful");
            if (m_intended_state == busy) RequestBusy();
        }
    }
    else if (m_state == request_busy)
    {
        QDBusPendingReply<uint> reply = *call;

        if (reply.isError())
        {
            qDebug("D-Bus: Reply: Error: %s", qPrintable(reply.error().message()));

            if (!m_use_gsm)
            {
                qDebug("D-Bus: Falling back to org.gnome.SessionManager");
                m_use_gsm = true;
                m_state = idle;
                if (m_intended_state == busy)
                    RequestBusy();
            }
            else
            {
                m_state = error;
            }
        }
        else
        {
            m_state = busy;
            m_cookie = reply.value();
            qDebug("D-Bus: PowerManagementInhibitor: Request successful, cookie is %d", m_cookie);
            if (m_intended_state == idle) RequestIdle();
        }
    }
    else
    {
        qDebug("D-Bus: Unexpected reply in state %d", m_state);
        m_state = error;
    }

    call->deleteLater();
}
