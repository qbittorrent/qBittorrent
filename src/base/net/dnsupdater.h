/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QTimer>

#include "base/preferences.h"

namespace Net
{
    struct DownloadResult;

    // Based on http://www.dyndns.com/developers/specs/
    class DNSUpdater : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(DNSUpdater)

    public:
        explicit DNSUpdater(QObject *parent = nullptr);
        ~DNSUpdater();

        static QUrl getRegistrationUrl(DNS::Service service);

    public slots:
        void updateCredentials();

    private slots:
        void checkPublicIP();
        void ipRequestFinished(const DownloadResult &result);
        void updateDNSService();
        void ipUpdateFinished(const DownloadResult &result);

    private:
        enum State
        {
            OK,
            INVALID_CREDS,
            FATAL
        };

        QString getUpdateUrl() const;
        void processIPUpdateReply(const QString &reply);

        QHostAddress m_lastIP;
        QDateTime m_lastIPCheckTime;
        QTimer m_ipCheckTimer;
        int m_state = OK;
        // Service creds
        DNS::Service m_service = DNS::Service::None;
        QString m_domain;
        QString m_username;
        QString m_password;
    };
}
