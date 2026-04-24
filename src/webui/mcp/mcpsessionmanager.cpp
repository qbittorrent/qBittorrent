/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Ortes <malo.allee@gmail.com>
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

#include "mcpsessionmanager.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QUuid>

#include "base/global.h"
#include "mcpprotocol.h"

QString MCP::SessionManager::create(const QHostAddress &remote, const QString &protocolVersion)
{
    const QMutexLocker locker {&m_mutex};

    const QString ipKey = remote.toString();
    if (m_perIpCount.value(ipKey, 0) >= Protocol::SESSION_CAP_PER_IP)
        return {};
    if (m_sessions.size() >= Protocol::SESSION_CAP_GLOBAL)
        return {};

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sessions.insert(id, Session{
        .id = id,
        .remoteAddress = remote,
        .lastActivity = QDateTime::currentDateTimeUtc(),
        .negotiatedProtocolVersion = protocolVersion,
    });
    ++m_perIpCount[ipKey];
    return id;
}

std::optional<MCP::Session> MCP::SessionManager::find(const QString &id)
{
    const QMutexLocker locker {&m_mutex};
    const auto it = m_sessions.constFind(id);
    if (it == m_sessions.constEnd())
        return std::nullopt;

    const qint64 ageSeconds = it->lastActivity.secsTo(QDateTime::currentDateTimeUtc());
    if (ageSeconds > Protocol::SESSION_IDLE_TIMEOUT_SECONDS)
        return std::nullopt;

    return *it;
}

void MCP::SessionManager::remove(const QString &id)
{
    const QMutexLocker locker {&m_mutex};
    const auto it = m_sessions.constFind(id);
    if (it == m_sessions.constEnd())
        return;
    const QString ipKey = it->remoteAddress.toString();
    m_sessions.erase(it);
    if (auto countIt = m_perIpCount.find(ipKey); countIt != m_perIpCount.end())
    {
        if (--countIt.value() <= 0)
            m_perIpCount.erase(countIt);
    }
}

void MCP::SessionManager::touch(const QString &id)
{
    const QMutexLocker locker {&m_mutex};
    if (auto it = m_sessions.find(id); it != m_sessions.end())
        it->lastActivity = QDateTime::currentDateTimeUtc();
}

void MCP::SessionManager::sweepExpired()
{
    const QMutexLocker locker {&m_mutex};
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_sessions.begin(); it != m_sessions.end();)
    {
        if (it->lastActivity.secsTo(now) > Protocol::SESSION_IDLE_TIMEOUT_SECONDS)
        {
            const QString ipKey = it->remoteAddress.toString();
            if (auto countIt = m_perIpCount.find(ipKey); countIt != m_perIpCount.end())
            {
                if (--countIt.value() <= 0)
                    m_perIpCount.erase(countIt);
            }
            it = m_sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
