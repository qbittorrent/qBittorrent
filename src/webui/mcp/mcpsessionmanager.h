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

#pragma once

#include <optional>

#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QString>

#include "mcpsession.h"

namespace MCP
{
    class SessionManager
    {
    public:
        SessionManager() = default;

        /**
         * Create a new session for the given client address.
         * @return new session id, or empty string if cap exceeded.
         */
        QString create(const QHostAddress &remote, const QString &protocolVersion);

        /** @return session if found and not expired, nullopt otherwise. */
        std::optional<Session> find(const QString &id);

        /** Remove session (idempotent). */
        void remove(const QString &id);

        /** Touch last-activity for an existing session. */
        void touch(const QString &id);

        /** Sweep sessions idle longer than the configured timeout. */
        void sweepExpired();

    private:
        Q_DISABLE_COPY_MOVE(SessionManager)

        // Precondition: m_mutex is already held by the caller.
        // Erases the entry pointed to by @it, decrements the per-IP counter,
        // and returns the next valid iterator (suitable for use in erase loops).
        QHash<QString, Session>::iterator removeInternal(QHash<QString, Session>::iterator it);

        mutable QMutex m_mutex;
        QHash<QString, Session> m_sessions;
        QHash<QString, int> m_perIpCount;
    };
}
