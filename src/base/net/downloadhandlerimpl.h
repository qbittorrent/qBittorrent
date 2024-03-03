/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <QNetworkReply>

#include "base/net/downloadmanager.h"

class QObject;
class QUrl;

namespace Net
{
    class DownloadHandlerImpl final : public DownloadHandler
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(DownloadHandlerImpl)

    public:
        DownloadHandlerImpl(DownloadManager *manager, const DownloadRequest &downloadRequest, bool useProxy);

        void cancel() override;

        QString url() const;
        DownloadRequest downloadRequest() const;
        bool useProxy() const;

        void assignNetworkReply(QNetworkReply *reply);
        QNetworkReply *assignedNetworkReply() const;

    private:
        void processFinishedDownload();
        void checkDownloadSize(qint64 bytesReceived, qint64 bytesTotal);
        void handleRedirection(const QUrl &newUrl);
        void setError(const QString &error);
        void finish();

        static QString errorCodeToString(QNetworkReply::NetworkError status);

        DownloadManager *m_manager = nullptr;
        QNetworkReply *m_reply = nullptr;
        DownloadHandlerImpl *m_redirectionHandler = nullptr;
        const DownloadRequest m_downloadRequest;
        const bool m_useProxy = false;
        short m_redirectionCount = 0;
        DownloadResult m_result;
        bool m_isFinished = false;
    };
}
