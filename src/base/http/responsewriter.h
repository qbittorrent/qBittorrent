/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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
#include <QPointer>

#include "base/pathfwd.h"
#include "headermap.h"
#include "request.h"
#include "response.h"

class QAbstractSocket;
class QThread;

namespace Http
{
    class ResponseWriter final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(ResponseWriter)

    public:
        ResponseWriter(QAbstractSocket *socket, Request request, QObject *parent = nullptr);
        ~ResponseWriter() override;

        // Send entire response at once.
        // Allow response content to be gzip encoded.
        void setResponse(const Response &response);

        // Allow to stream file using separate IO thread for reading.
        // Support Range requests.
        void streamFile(const Path &filePath, const HeaderMap &headers);

        bool isFinished() const;

    signals:
        void finished(QPrivateSignal);

    private:
        void writeCurrentData();
        void finish();

        QPointer<QAbstractSocket> m_socket;
        Request m_request;

        class Worker;
        Worker *m_asyncWorker = nullptr;
        QThread *m_workerThread = nullptr;
        bool m_isAsyncWorkerFinished = false;

        QByteArray m_dataToWrite;

        bool m_isWritingContent = false;
        bool m_isFinished = false;
    };
}
