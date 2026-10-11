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

#include <memory>

#include <QObject>
#include <QPointer>

#include "base/path.h"
#include "headermap.h"
#include "request.h"

class QAbstractSocket;
class QThread;

namespace Http
{
    class AsyncFileSender final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(AsyncFileSender)

    public:
        enum class State
        {
            Ready,
            Running,
            Finished,
            Failed
        };

        AsyncFileSender(const Request &request, const Path &filePath, const HeaderMap &headers, QAbstractSocket *socket, QObject *parent = nullptr);
        ~AsyncFileSender() override;

        State state() const;

        void run();

    signals:
        void failed();
        void finished();

    private:
        void processNextData();
        void fail();
        void finish();

        State m_state = State::Ready;

        Request m_request;
        Path m_filePath;
        HeaderMap m_headers;
        QAbstractSocket *m_socket = nullptr;

        class DataPipe;
        std::shared_ptr<DataPipe> m_dataPipe;

        class Worker;
        QPointer<Worker> m_worker;
    };
}
