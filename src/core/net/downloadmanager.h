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

#ifndef NET_DOWNLOADMANAGER_H
#define NET_DOWNLOADMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
class QNetworkReply;
class QNetworkCookie;
class QSslError;
class QUrl;
QT_END_NAMESPACE

namespace Net
{
    class DownloadHandler;

    class DownloadManager : public QObject
    {
        Q_OBJECT

    public:
        static void initInstance();
        static void freeInstance();
        static DownloadManager *instance();

        DownloadHandler *downloadUrl(const QString &url, qint64 limit = 0);
        QList<QNetworkCookie> cookiesForUrl(const QString &url) const;
        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);

    private slots:
    #ifndef QT_NO_OPENSSL
        void ignoreSslErrors(QNetworkReply *,const QList<QSslError> &);
    #endif

    private:
        DownloadManager(QObject *parent = 0);
        ~DownloadManager();

        void applyProxySettings();

        static DownloadManager *m_instance;
        QNetworkAccessManager m_networkManager;
    };
}

#endif // NET_DOWNLOADMANAGER_H
