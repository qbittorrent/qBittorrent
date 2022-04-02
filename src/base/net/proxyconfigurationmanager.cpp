/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "proxyconfigurationmanager.h"

#define SETTINGS_KEY(name) (u"Network/Proxy/" name)

bool Net::operator==(const ProxyConfiguration &left, const ProxyConfiguration &right)
{
    return (left.type == right.type)
            && (left.ip == right.ip)
            && (left.port == right.port)
            && (left.authEnabled == right.authEnabled)
            && (left.username == right.username)
            && (left.password == right.password)
            && (left.hostnameLookupEnabled == right.hostnameLookupEnabled);
}

using namespace Net;

ProxyConfigurationManager *ProxyConfigurationManager::m_instance = nullptr;

ProxyConfigurationManager::ProxyConfigurationManager(QObject *parent)
    : QObject(parent)
    , m_storeProxyType {SETTINGS_KEY(u"Type"_s)}
    , m_storeProxyIP {SETTINGS_KEY(u"IP"_s)}
    , m_storeProxyPort {SETTINGS_KEY(u"Port"_s)}
    , m_storeProxyAuthEnabled {SETTINGS_KEY(u"AuthEnabled"_s)}
    , m_storeProxyUsername {SETTINGS_KEY(u"Username"_s)}
    , m_storeProxyPassword {SETTINGS_KEY(u"Password"_s)}
    , m_storeProxyHostnameLookupEnabled {SETTINGS_KEY(u"HostnameLookupEnabled"_s)}
{
    m_config.type = m_storeProxyType.get(ProxyType::None);
    if ((m_config.type < ProxyType::None) || (m_config.type > ProxyType::SOCKS4))
        m_config.type = ProxyType::None;
    m_config.ip = m_storeProxyIP.get((m_config.type == ProxyType::None) ? u""_s : u"0.0.0.0"_s);
    m_config.port = m_storeProxyPort.get(8080);
    m_config.authEnabled = m_storeProxyAuthEnabled;
    m_config.username = m_storeProxyUsername;
    m_config.password = m_storeProxyPassword;
    m_config.hostnameLookupEnabled = m_storeProxyHostnameLookupEnabled.get(true);
}

void ProxyConfigurationManager::initInstance()
{
    if (!m_instance)
        m_instance = new ProxyConfigurationManager;
}

void ProxyConfigurationManager::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

ProxyConfigurationManager *ProxyConfigurationManager::instance()
{
    return m_instance;
}

ProxyConfiguration ProxyConfigurationManager::proxyConfiguration() const
{
    return m_config;
}

void ProxyConfigurationManager::setProxyConfiguration(const ProxyConfiguration &config)
{
    if (m_config != config)
    {
        m_config = config;
        m_storeProxyType = config.type;
        m_storeProxyIP = config.ip;
        m_storeProxyPort = config.port;
        m_storeProxyAuthEnabled = config.authEnabled;
        m_storeProxyUsername = config.username;
        m_storeProxyPassword = config.password;
        m_storeProxyHostnameLookupEnabled = config.hostnameLookupEnabled;

        emit proxyConfigurationChanged();
    }
}
