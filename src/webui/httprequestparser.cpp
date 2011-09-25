/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */


#include "httprequestparser.h"
#include <QUrl>
#include <QDebug>

HttpRequestParser::HttpRequestParser(): m_error(false)
{
}

HttpRequestParser::~HttpRequestParser()
{
}

bool HttpRequestParser::isError() const {
  return m_error;
}

QString HttpRequestParser::url() const {
  return m_path;
}

QByteArray HttpRequestParser::message() const {
  return m_data;
}

QString HttpRequestParser::get(const QString& key) const {
  return m_getMap.value(key);
}

QString HttpRequestParser::post(const QString& key) const {
  return m_postMap.value(key);
}

QByteArray HttpRequestParser::torrent() const {
  return m_torrentContent;
}

void HttpRequestParser::write(const QByteArray& ba) {
  int end_of_header = ba.indexOf("\r\n\r\n");
  if (end_of_header < 0) {
    qWarning() << "Could not find HTTP header: " << ba.constData();
    m_error = true;
    return;
  }

  // Parse header
  m_header = QHttpRequestHeader(ba.left(end_of_header));
  QUrl url = QUrl::fromEncoded(m_header.path().toAscii());
  m_path = url.path();

  // Parse GET parameters
  QListIterator<QPair<QString, QString> > i(url.queryItems());
  while (i.hasNext()) {
    QPair<QString, QString> pair = i.next();
    m_getMap[pair.first] = pair.second;
  }

  // Parse message content
  if (m_header.hasContentLength()) {
    m_data = ba.mid(end_of_header + 4, m_header.contentLength()); // +4 to skip "\r\n\r\n"

    // Parse POST data
    if(m_header.contentType() == "application/x-www-form-urlencoded") {
      QUrl url;
      url.setEncodedQuery(m_data);
      QListIterator<QPair<QString, QString> > i(url.queryItems());
      while (i.hasNext())	{
        QPair<QString, QString> pair = i.next();
        m_postMap[pair.first] = pair.second;
      }
    } else {
      // Parse form data (torrent file)
      if(m_header.contentType() == "multipart/form-data") {
        m_torrentContent = m_data.right(m_data.size()-m_data.indexOf("\r\n\r\n")-QByteArray("\r\n\r\n").size());
      }
    }
  }
}
