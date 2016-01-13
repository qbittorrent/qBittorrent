/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QDateTime>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSslError>
#include <QUrl>
#include <QDebug>

#include "base/preferences.h"
#include "downloadhandler.h"
#include "downloadmanager.h"

// Spoof Firefox 38 user agent to avoid web server banning
const char DEFAULT_USER_AGENT[] = "Mozilla/5.0 (X11; Linux i686; rv:38.0) Gecko/20100101 Firefox/38.0";

namespace
{
    class NetworkCookieJar: public QNetworkCookieJar
    {
    public:
        explicit NetworkCookieJar(QObject *parent = 0)
            : QNetworkCookieJar(parent)
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = Preferences::instance()->getNetworkCookies();
            foreach (const QNetworkCookie &cookie, Preferences::instance()->getNetworkCookies()) {
                if (cookie.isSessionCookie() || (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            setAllCookies(cookies);
        }

        ~NetworkCookieJar()
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = allCookies();
            foreach (const QNetworkCookie &cookie, allCookies()) {
                if (cookie.isSessionCookie() || (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            Preferences::instance()->setNetworkCookies(cookies);
        }

#ifndef QBT_USES_QT5
        virtual bool deleteCookie(const QNetworkCookie &cookie)
        {
            auto myCookies = allCookies();

            QList<QNetworkCookie>::Iterator it;
            for (it = myCookies.begin(); it != myCookies.end(); ++it) {
                if ((it->name() == cookie.name())
                        && (it->domain() == cookie.domain())
                        && (it->path() == cookie.path())) {
                    myCookies.erase(it);
                    setAllCookies(myCookies);
                    return true;
                }
            }

            return false;
        }
#endif

        QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = QNetworkCookieJar::cookiesForUrl(url);
            foreach (const QNetworkCookie &cookie, QNetworkCookieJar::cookiesForUrl(url)) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return cookies;
        }

        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override
        {
            QDateTime now = QDateTime::currentDateTime();
            QList<QNetworkCookie> cookies = cookieList;
            foreach (const QNetworkCookie &cookie, cookieList) {
                if (!cookie.isSessionCookie() && (cookie.expirationDate() <= now))
                    cookies.removeAll(cookie);
            }

            return QNetworkCookieJar::setCookiesFromUrl(cookies, url);
        }
    };
}

using namespace Net;

DownloadManager *DownloadManager::m_instance = 0;

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
#ifndef QT_NO_OPENSSL
    connect(&m_networkManager, SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)), this, SLOT(ignoreSslErrors(QNetworkReply *, QList<QSslError>)));
#endif
    m_networkManager.setCookieJar(new NetworkCookieJar(this));
}

void DownloadManager::initInstance()
{
    if (!m_instance)
        m_instance = new DownloadManager;
}

void DownloadManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

DownloadManager *DownloadManager::instance()
{
    return m_instance;
}

DownloadHandler *DownloadManager::downloadUrl(const QString &url, bool saveToFile, qint64 limit, bool handleRedirectToMagnet, const QString &userAgent)
{
    // Update proxy settings
    applyProxySettings();

    // Process download request
    qDebug("url is %s", qPrintable(url));
    const QUrl qurl = QUrl::fromEncoded(url.toUtf8());
    QNetworkRequest request(qurl);

    if (userAgent.isEmpty())
        request.setRawHeader("User-Agent", DEFAULT_USER_AGENT);
    else
        request.setRawHeader("User-Agent", userAgent.toUtf8());

    // Spoof HTTP Referer to allow adding torrent link from Torcache/KickAssTorrents
    request.setRawHeader("Referer", request.url().toEncoded().data());

    qDebug("Downloading %s...", request.url().toEncoded().data());
    qDebug() << "Cookies:" << m_networkManager.cookieJar()->cookiesForUrl(request.url());
    // accept gzip
    request.setRawHeader("Accept-Encoding", "gzip");
    return new DownloadHandler(m_networkManager.get(request), this, saveToFile, limit, handleRedirectToMagnet);
}

QList<QNetworkCookie> DownloadManager::cookiesForUrl(const QUrl &url) const
{
    return m_networkManager.cookieJar()->cookiesForUrl(url);
}

bool DownloadManager::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    return m_networkManager.cookieJar()->setCookiesFromUrl(cookieList, url);
}

bool DownloadManager::deleteCookie(const QNetworkCookie &cookie)
{
    return static_cast<NetworkCookieJar *>(m_networkManager.cookieJar())->deleteCookie(cookie);
}

void DownloadManager::applyProxySettings()
{
    QNetworkProxy proxy;
    const Preferences* const pref = Preferences::instance();

    if (pref->isProxyEnabled() && !pref->isProxyOnlyForTorrents()) {
        // Proxy enabled
        proxy.setHostName(pref->getProxyIp());
        proxy.setPort(pref->getProxyPort());
        // Default proxy type is HTTP, we must change if it is SOCKS5
        const int proxyType = pref->getProxyType();
        if ((proxyType == Proxy::SOCKS5) || (proxyType == Proxy::SOCKS5_PW)) {
            qDebug() << Q_FUNC_INFO << "using SOCKS proxy";
            proxy.setType(QNetworkProxy::Socks5Proxy);
        }
        else {
            qDebug() << Q_FUNC_INFO << "using HTTP proxy";
            proxy.setType(QNetworkProxy::HttpProxy);
        }
        // Authentication?
        if (pref->isProxyAuthEnabled()) {
            qDebug("Proxy requires authentication, authenticating");
            proxy.setUser(pref->getProxyUsername());
            proxy.setPassword(pref->getProxyPassword());
        }
    }
    else {
        proxy.setType(QNetworkProxy::NoProxy);
    }

    m_networkManager.setProxy(proxy);
}

#ifndef QT_NO_OPENSSL
void DownloadManager::ignoreSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
    // Ignore all SSL errors
    reply->ignoreSslErrors();
}
#endif
