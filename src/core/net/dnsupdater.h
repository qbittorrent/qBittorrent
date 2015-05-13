/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
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

#ifndef DNSUPDATER_H
#define DNSUPDATER_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkReply>
#include <QDateTime>
#include <QTimer>
#include "core/preferences.h"

namespace Net
{ 
    // Based on http://www.dyndns.com/developers/specs/
    class DNSUpdater : public QObject
    {
        Q_OBJECT

    public:
        explicit DNSUpdater(QObject *parent = 0);
        ~DNSUpdater();

        static QUrl getRegistrationUrl(int service);

    public slots:
        void updateCredentials();

    private slots:
        void checkPublicIP();
        void ipRequestFinished(QNetworkReply *reply);
        void updateDNSService();
        void ipUpdateFinished(QNetworkReply *reply);

    private:
        QUrl getUpdateUrl() const;
        void processIPUpdateReply(const QString &reply);

    private:
        QHostAddress m_lastIP;
        QDateTime m_lastIPCheckTime;
        QTimer m_ipCheckTimer;
        int m_state;
        // Service creds
        DNS::Service m_service;
        QString m_domain;
        QString m_username;
        QString m_password;

    private:
        static const int IP_CHECK_INTERVAL_MS = 1800000; // 30 min

        enum State
        {
            OK,
            INVALID_CREDS,
            FATAL
        };
    };
}

#endif // DNSUPDATER_H
