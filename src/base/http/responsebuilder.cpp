/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include "responsebuilder.h"

using namespace Http;

void ResponseBuilder::status(const uint code, const QString &text)
{
    m_response.status = {code, text};
}

void ResponseBuilder::setHeader(const Header &header)
{
    m_response.headers[header.name] = header.value;
}

void ResponseBuilder::print(const QString &text, const QString &type)
{
    print_impl(text.toUtf8(), type);
}

void ResponseBuilder::print(const QByteArray &data, const QString &type)
{
    print_impl(data, type);
}

void ResponseBuilder::clear()
{
    m_response = Response();
}

Response ResponseBuilder::response() const
{
    return m_response;
}

void ResponseBuilder::print_impl(const QByteArray &data, const QString &type)
{
    if (!m_response.headers.contains(HEADER_CONTENT_TYPE))
        m_response.headers[HEADER_CONTENT_TYPE] = type;

    m_response.content += data;
}
