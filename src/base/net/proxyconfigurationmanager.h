/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QObject>

namespace Net
{
    enum class ProxyType
    {
        None = 0,
        HTTP = 1,
        SOCKS5 = 2,
        HTTP_PW = 3,
        SOCKS5_PW = 4,
        SOCKS4 = 5
    };

    struct ProxyConfiguration
    {
        ProxyType type = ProxyType::None;
        QString ip = "0.0.0.0";
        ushort port = 8080;
        QString username;
        QString password;
    };
    bool operator==(const ProxyConfiguration &left, const ProxyConfiguration &right);
    bool operator!=(const ProxyConfiguration &left, const ProxyConfiguration &right);

    class ProxyConfigurationManager : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(ProxyConfigurationManager)

        explicit ProxyConfigurationManager(QObject *parent = nullptr);
        ~ProxyConfigurationManager() = default;

    public:
        static void initInstance();
        static void freeInstance();
        static ProxyConfigurationManager *instance();

        ProxyConfiguration proxyConfiguration() const;
        void setProxyConfiguration(const ProxyConfiguration &config);
        bool isProxyOnlyForTorrents() const;
        void setProxyOnlyForTorrents(bool onlyForTorrents);

        bool isAuthenticationRequired() const;

    signals:
        void proxyConfigurationChanged();

    private:
        void configureProxy();

        static ProxyConfigurationManager *m_instance;
        ProxyConfiguration m_config;
        bool m_isProxyOnlyForTorrents;
    };
}
