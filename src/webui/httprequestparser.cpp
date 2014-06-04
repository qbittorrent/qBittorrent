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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QUrlQuery>
#endif
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

const QString& HttpRequestParser::url() const {
  return m_path;
}

const QByteArray& HttpRequestParser::message() const {
  return m_data;
}

QString HttpRequestParser::get(const QString& key) const {
  return m_getMap.value(key);
}

QString HttpRequestParser::post(const QString& key) const {
  return m_postMap.value(key);
}

const QList<QByteArray>& HttpRequestParser::torrents() const {
  return m_torrents;
}

void HttpRequestParser::writeHeader(const QByteArray& ba) {
  m_error = false;
  // Parse header
  m_header = HttpRequestHeader(ba);
  QUrl url = QUrl::fromEncoded(m_header.path().toLatin1());
  m_path = url.path();

  // Parse GET parameters
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
  QUrlQuery query(url);
  QListIterator<QPair<QString, QString> > i(query.queryItems());
#else
  QListIterator<QPair<QString, QString> > i(url.queryItems());
#endif
  while (i.hasNext()) {
    QPair<QString, QString> pair = i.next();
    m_getMap[pair.first] = pair.second;
  }
}

static QList<QByteArray> splitRawData(QByteArray rawData, const QByteArray& sep)
{
  QList<QByteArray> ret;
  const int sepLength = sep.size();
  int index = 0;
  while ((index = rawData.indexOf(sep)) >= 0) {
    ret << rawData.left(index);
    rawData = rawData.mid(index + sepLength);
  }
  return ret;
}

void HttpRequestParser::writeMessage(const QByteArray& ba) {
  // Parse message content
  Q_ASSERT (m_header.hasContentLength());
  m_error = false;
  m_data = ba;
  qDebug() << Q_FUNC_INFO << "m_data.size(): " << m_data.size();

  // Parse POST data
  if (m_header.contentType() == "application/x-www-form-urlencoded") {
    QUrl url;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    QString tmp(m_data);
    QUrlQuery query(tmp);
    QListIterator<QPair<QString, QString> > i(query.queryItems(QUrl::FullyDecoded));
#else
    url.setEncodedQuery(m_data);
    QListIterator<QPair<QString, QString> > i(url.queryItems());
#endif
    while (i.hasNext())	{
      QPair<QString, QString> pair = i.next();
      m_postMap[pair.first] = pair.second;
    }
    return;
  }

  // Parse multipart/form data (torrent file)
  /**
        m_data has the following format (if boundary is "cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5")

--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"Filename\"

PB020344.torrent
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"torrentfile"; filename=\"PB020344.torrent\"
Content-Type: application/x-bittorrent

BINARY DATA IS HERE
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5
Content-Disposition: form-data; name=\"Upload\"

Submit Query
--cH2ae0GI3KM7GI3Ij5ae0ei4Ij5Ij5--
**/
  if (m_header.contentType().startsWith("multipart/form-data")) {
    qDebug() << Q_FUNC_INFO << "header is: " << m_header.toString();
    static QRegExp boundaryRegexQuoted("boundary=\"([ \\w'()+,-\\./:=\\?]+)\"");
    static QRegExp boundaryRegexNotQuoted("boundary=([\\w'()+,-\\./:=\\?]+)");
    QByteArray boundary;
    if (boundaryRegexQuoted.indexIn(m_header.toString()) < 0) {
      if (boundaryRegexNotQuoted.indexIn(m_header.toString()) < 0) {
        qWarning() << "Could not find boundary in multipart/form-data header!";
        m_error = true;
        return;
      } else {
        boundary = "--" + boundaryRegexNotQuoted.cap(1).toLatin1();
      }
    } else {
      boundary = "--" + boundaryRegexQuoted.cap(1).toLatin1();
    }
    qDebug() << "Boundary is " << boundary;
    QList<QByteArray> parts = splitRawData(m_data, boundary);
    qDebug() << parts.size() << "parts in data";
    foreach (const QByteArray& part, parts) {
      const int filenameIndex = part.indexOf("filename=");
      if (filenameIndex < 0)
        continue;
      qDebug() << "Found a torrent";
      m_torrents << part.mid(part.indexOf("\r\n\r\n", filenameIndex + 9) + 4);
    }
  }
}

bool HttpRequestParser::acceptsEncoding() {
  QString encoding = m_header.value("Accept-Encoding");

  int pos = encoding.indexOf("gzip", 0, Qt::CaseInsensitive);
  if (pos == -1)
    return false;

  // Let's see if there's a qvalue of 0.0 following
  if (encoding[pos+4] != ';') //there isn't, so it accepts gzip anyway
    return true;

  //So let's find = and the next comma
  pos = encoding.indexOf("=", pos+4, Qt::CaseInsensitive);
  int comma_pos = encoding.indexOf(",", pos, Qt::CaseInsensitive);

  QString value;
  if (comma_pos == -1)
    value = encoding.mid(pos+1, comma_pos);
  else
    value = encoding.mid(pos+1, comma_pos-(pos+1));

  if (value.toDouble() == 0.0)
    return false;
  else
    return true;
}
