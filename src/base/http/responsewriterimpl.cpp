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

#include "responsewriterimpl.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QFile>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStringView>

#include "base/path.h"
#include "asyncfilesender.h"
#include "responseserialization.h"

Http::ResponseWriterImpl::ResponseWriterImpl(QAbstractSocket *socket, QObject *parent)
    : ResponseWriter(parent)
    , m_socket {socket}
{
}

Http::ResponseWriterImpl::~ResponseWriterImpl()
{
    delete m_asyncFileSender;
}

void Http::ResponseWriterImpl::prepare(const Request &request)
{
    Q_ASSERT(!m_asyncFileSender);

    m_isFinished = false;
    m_request = request;
}

void Http::ResponseWriterImpl::setResponse(const Response &response)
{
    Q_ASSERT(!m_isFinished && !m_asyncFileSender);
    if (m_isFinished || m_asyncFileSender) [[unlikely]]
        return;

    const QByteArray data = serializeResponse(response, m_request);
    if (m_socket->write(data) < 0)
    {
        m_socket->abort();
        return;
    }

    finish();
}

void Http::ResponseWriterImpl::streamFile(const Path &filePath, const HeaderMap &headers)
{
    Q_ASSERT(!m_isFinished && !m_asyncFileSender);
    if (m_isFinished || m_asyncFileSender) [[unlikely]]
        return;

    m_asyncFileSender = new AsyncFileSender(m_request, filePath, headers, m_socket);

    connect(m_asyncFileSender, &AsyncFileSender::finished, this, [this]
    {
        m_asyncFileSender->deleteLater();
        m_asyncFileSender = nullptr;
        finish();
    });

    m_asyncFileSender->run();
}

void Http::ResponseWriterImpl::finish()
{
    if (m_isFinished)
        return;

    Q_ASSERT(!m_asyncFileSender);

    m_isFinished = true;
    emit finished();
}

bool Http::ResponseWriterImpl::isFinished() const
{
    return m_isFinished;
}
