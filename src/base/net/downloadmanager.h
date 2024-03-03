/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Jonathan Ketchker
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <chrono>

#include <QtTypes>
#include <QHash>
#include <QNetworkProxy>
#include <QObject>
#include <QQueue>
#include <QSet>

#include "base/path.h"

class QNetworkAccessManager;
class QNetworkCookie;
class QNetworkReply;
class QSslError;
class QUrl;

namespace Net
{
    struct ServiceID
    {
        QString hostName;
        int port;

        static ServiceID fromURL(const QUrl &url);
    };

    std::size_t qHash(const ServiceID &serviceID, std::size_t seed = 0);
    bool operator==(const ServiceID &lhs, const ServiceID &rhs);

    enum class DownloadStatus
    {
        Success,
        RedirectedToMagnet,
        Failed
    };

    class DownloadRequest
    {
    public:
        DownloadRequest(const QString &url);
        DownloadRequest(const DownloadRequest &other) = default;

        QString url() const;
        DownloadRequest &url(const QString &value);

        QString userAgent() const;
        DownloadRequest &userAgent(const QString &value);

        qint64 limit() const;
        DownloadRequest &limit(qint64 value);

        bool saveToFile() const;
        DownloadRequest &saveToFile(bool value);

        // if saveToFile is set, the file is saved in destFileName
        // (deprecated) if destFileName is not provided, the file will be saved
        // in a temporary file, the name of file is set in DownloadResult::filePath
        Path destFileName() const;
        DownloadRequest &destFileName(const Path &value);

    private:
        QString m_url;
        QString m_userAgent;
        qint64 m_limit = 0;
        bool m_saveToFile = false;
        Path m_destFileName;
    };

    struct DownloadResult
    {
        QString url;
        DownloadStatus status = DownloadStatus::Failed;
        QString errorString;
        QByteArray data;
        Path filePath;
        QString magnetURI;
    };

    class DownloadHandler : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(DownloadHandler)

    public:
        using QObject::QObject;

        virtual void cancel() = 0;

    signals:
        void finished(const DownloadResult &result);
    };

    class DownloadHandlerImpl;

    class DownloadManager final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(DownloadManager)

    public:
        static void initInstance();
        static void freeInstance();
        static DownloadManager *instance();

        DownloadHandler *download(const DownloadRequest &downloadRequest, bool useProxy);

        template <typename Context, typename Func>
        void download(const DownloadRequest &downloadRequest, bool useProxy, Context context, Func &&slot);

        void registerSequentialService(const ServiceID &serviceID, std::chrono::seconds delay = std::chrono::seconds(0));

        QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const;
        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);
        QList<QNetworkCookie> allCookies() const;
        void setAllCookies(const QList<QNetworkCookie> &cookieList);
        bool deleteCookie(const QNetworkCookie &cookie);

        static bool hasSupportedScheme(const QString &url);

    private:
        class NetworkCookieJar;

        explicit DownloadManager(QObject *parent = nullptr);

        void applyProxySettings();
        void processWaitingJobs(const ServiceID &serviceID);
        void processRequest(DownloadHandlerImpl *downloadHandler);

        static DownloadManager *m_instance;
        NetworkCookieJar *m_networkCookieJar = nullptr;
        QNetworkAccessManager *m_networkManager = nullptr;
        QNetworkProxy m_proxy;

        // m_sequentialServices value is delay for same host requests
        QHash<ServiceID, std::chrono::seconds> m_sequentialServices;
        QSet<ServiceID> m_busyServices;
        QHash<ServiceID, QQueue<DownloadHandlerImpl *>> m_waitingJobs;
    };

    template <typename Context, typename Func>
    void DownloadManager::download(const DownloadRequest &downloadRequest, bool useProxy, Context context, Func &&slot)
    {
        const DownloadHandler *handler = download(downloadRequest, useProxy);
        connect(handler, &DownloadHandler::finished, context, slot);
    }
}
