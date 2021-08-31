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

#include "proxyconfigurationmanager.h"

#include "base/settingsstorage.h"

#define SETTINGS_KEY(name) QStringLiteral("Network/Proxy/" name)
const QString KEY_ONLY_FOR_TORRENTS = SETTINGS_KEY("OnlyForTorrents");
const QString KEY_TYPE = SETTINGS_KEY("Type");
const QString KEY_IP = SETTINGS_KEY("IP");
const QString KEY_PORT = SETTINGS_KEY("Port");
const QString KEY_USERNAME = SETTINGS_KEY("Username");
const QString KEY_PASSWORD = SETTINGS_KEY("Password");

namespace
{
    inline SettingsStorage *settings() { return SettingsStorage::instance(); }
}

bool Net::operator==(const ProxyConfiguration &left, const ProxyConfiguration &right)
{
    return (left.type == right.type)
            && (left.ip == right.ip)
            && (left.port == right.port)
            && (left.username == right.username)
            && (left.password == right.password);
}

bool Net::operator!=(const ProxyConfiguration &left, const ProxyConfiguration &right)
{
    return !(left == right);
}

using namespace Net;

ProxyConfigurationManager *ProxyConfigurationManager::m_instance = nullptr;

ProxyConfigurationManager::ProxyConfigurationManager(QObject *parent)
    : QObject(parent)
{
    m_isProxyOnlyForTorrents = settings()->loadValue(KEY_ONLY_FOR_TORRENTS, false);
    m_config.type = static_cast<ProxyType>(
                settings()->loadValue(KEY_TYPE, static_cast<int>(ProxyType::None)));
    if ((m_config.type < ProxyType::None) || (m_config.type > ProxyType::SOCKS4))
        m_config.type = ProxyType::None;
    m_config.ip = settings()->loadValue<QString>(KEY_IP, "0.0.0.0");
    m_config.port = settings()->loadValue<ushort>(KEY_PORT, 8080);
    m_config.username = settings()->loadValue<QString>(KEY_USERNAME);
    m_config.password = settings()->loadValue<QString>(KEY_PASSWORD);
    configureProxy();
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
    if (config != m_config)
    {
        m_config = config;
        settings()->storeValue(KEY_TYPE, static_cast<int>(config.type));
        settings()->storeValue(KEY_IP, config.ip);
        settings()->storeValue(KEY_PORT, config.port);
        settings()->storeValue(KEY_USERNAME, config.username);
        settings()->storeValue(KEY_PASSWORD, config.password);
        configureProxy();

        emit proxyConfigurationChanged();
    }
}

bool ProxyConfigurationManager::isProxyOnlyForTorrents() const
{
    return m_isProxyOnlyForTorrents || (m_config.type == ProxyType::SOCKS4);
}

void ProxyConfigurationManager::setProxyOnlyForTorrents(bool onlyForTorrents)
{
    if (m_isProxyOnlyForTorrents != onlyForTorrents)
    {
        settings()->storeValue(KEY_ONLY_FOR_TORRENTS, onlyForTorrents);
        m_isProxyOnlyForTorrents = onlyForTorrents;
    }
}

bool ProxyConfigurationManager::isAuthenticationRequired() const
{
    return m_config.type == ProxyType::SOCKS5_PW
            || m_config.type == ProxyType::HTTP_PW;
}

void ProxyConfigurationManager::configureProxy()
{
    // Define environment variables for urllib in search engine plugins
    QString proxyStrHTTP, proxyStrSOCK;
    if (!m_isProxyOnlyForTorrents)
    {
        switch (m_config.type)
        {
        case ProxyType::HTTP_PW:
            proxyStrHTTP = QString::fromLatin1("http://%1:%2@%3:%4").arg(m_config.username
                , m_config.password, m_config.ip, QString::number(m_config.port));
            break;
        case ProxyType::HTTP:
            proxyStrHTTP = QString::fromLatin1("http://%1:%2").arg(m_config.ip, QString::number(m_config.port));
            break;
        case ProxyType::SOCKS5:
            proxyStrSOCK = QString::fromLatin1("%1:%2").arg(m_config.ip, QString::number(m_config.port));
            break;
        case ProxyType::SOCKS5_PW:
            proxyStrSOCK = QString::fromLatin1("%1:%2@%3:%4").arg(m_config.username
                , m_config.password, m_config.ip, QString::number(m_config.port));
            break;
        default:
            qDebug("Disabling HTTP communications proxy");
        }

        qDebug("HTTP communications proxy string: %s"
               , qUtf8Printable((m_config.type == ProxyType::SOCKS5) || (m_config.type == ProxyType::SOCKS5_PW)
                            ? proxyStrSOCK : proxyStrHTTP));
    }

    qputenv("http_proxy", proxyStrHTTP.toLocal8Bit());
    qputenv("https_proxy", proxyStrHTTP.toLocal8Bit());
    qputenv("sock_proxy", proxyStrSOCK.toLocal8Bit());
}
