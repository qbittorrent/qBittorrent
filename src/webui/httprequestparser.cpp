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
#include <QStringList>
#include <QUrl>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QUrlQuery>
#endif
#include <QDebug>

HttpRequestParser::HttpRequestParser(uint maxContentLength)
  : m_error(BadRequest), m_length(0), m_maxContentLength(maxContentLength)
{
}

void HttpRequestParser::parse(const QByteArray &data)
{
  m_error = NoError;

  // Parse HTTP request header
  const int header_end = data.indexOf("\r\n\r\n");
  if (header_end < 0)
  {
    qDebug() << "Partial request: \n" << data;
    m_error = IncompleteRequest;
    return;
  }

  if (!parseHeader(data.left(header_end)))
  {
    qWarning() << Q_FUNC_INFO << "header parsing error";
    m_error = BadRequest;
    return;
  }

  int content_length = 0;

  // Parse HTTP request message
  if (hasContentLength())
  {
    content_length = contentLength();
    QByteArray content = data.mid(header_end + 4, content_length);

    if (content_length > m_maxContentLength)
    {
      qWarning() << "Bad request: message too long";
      m_error = BadRequest;
      return;
    }

    if (content.length() < content_length)
    {
      qDebug() << "Partial message:\n" << content;
      m_error = IncompleteRequest;
      return;
    }

    if (!parseContent(content))
    {
      qWarning() << Q_FUNC_INFO << "message parsing error";
      m_error = BadRequest;
    }
  }

  // total request length
  m_length = header_end + 4 + content_length;
}

bool HttpRequestParser::hasKey(const QString &key) const
{
    QString lowercaseKey = key.toLower();
    QList<QPair<QString, QString> >::ConstIterator it = m_values.constBegin();
    while (it != m_values.constEnd()) {
        if ((*it).first.toLower() == lowercaseKey)
            return true;
        ++it;
    }
    return false;
}

QString HttpRequestParser::value(const QString &key) const
{
    QString lowercaseKey = key.toLower();
    QList<QPair<QString, QString> >::ConstIterator it = m_values.constBegin();
    while (it != m_values.constEnd()) {
        if ((*it).first.toLower() == lowercaseKey)
            return (*it).second;
        ++it;
    }
    return QString();
}

QString HttpRequestParser::contentType() const
{
    QString type = value(QLatin1String("content-type"));
    if (type.isEmpty())
        return QString();

    int pos = type.indexOf(QLatin1Char(';'));
    if (pos == -1)
        return type;

    return type.left(pos).trimmed();
}

bool HttpRequestParser::hasContentLength() const
{
    return hasKey(QLatin1String("content-length"));
}

uint HttpRequestParser::contentLength() const
{
    return value(QLatin1String("content-length")).toUInt();
}

void HttpRequestParser::addValue(const QString &key, const QString &value)
{
    m_values.append(qMakePair(key, value));
}

bool HttpRequestParser::parseHeaderLine(const QString &line, int number)
{
    if (number != 0)
    {
      int i = line.indexOf(QLatin1Char(':'));
      if (i == -1)
        return false;

      addValue(line.left(i).trimmed(), line.mid(i + 1).trimmed());
      return true;
    }

    QStringList lst = line.simplified().split(QLatin1String(" "));
    if (lst.count() > 2)
    {
      m_m = lst[0]; // Method

      QUrl url = QUrl::fromEncoded(lst[1].toLatin1());
      m_path = url.path(); // Path

      QString v = lst[2];
      if (v.length() >= 8 && v.left(5) == QLatin1String("HTTP/") &&
          v[5].isDigit() && v[6] == QLatin1Char('.') && v[7].isDigit())
      {
        m_majVer = v[5].toLatin1() - '0';
        m_minVer = v[7].toLatin1() - '0';

        // Parse GET parameters
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        QListIterator<QPair<QString, QString> > i(url.queryItems());
#else
        QListIterator<QPair<QString, QString> > i(QUrlQuery(url).queryItems());
#endif
        while (i.hasNext())
        {
          QPair<QString, QString> pair = i.next();
          m_getMap[pair.first] = pair.second;
        }

        return true;
      }
    }

    return false;
}

bool HttpRequestParser::parseHeader(const QByteArray &data)
{
  QString str = QString::fromUtf8(data);
  QStringList lst;
  int pos = str.indexOf(QLatin1Char('\n'));
  if (pos > 0 && str.at(pos - 1) == QLatin1Char('\r'))
    lst = str.trimmed().split(QLatin1String("\r\n"));
  else
    lst = str.trimmed().split(QLatin1String("\n"));
  lst.removeAll(QString()); // No empties

  if (lst.isEmpty())
    return true;

  QStringList lines;
  QStringList::Iterator it = lst.begin();
  for (; it != lst.end(); ++it)
  {
    if (!(*it).isEmpty())
    {
      if ((*it)[0].isSpace())
      {
        if (!lines.isEmpty())
        {
          lines.last() += QLatin1Char(' ');
          lines.last() += (*it).trimmed();
        }
      }
      else
      {
        lines.append((*it));
      }
    }
  }

  int number = 0;
  it = lines.begin();
  for (; it != lines.end(); ++it)
  {
    if (!parseHeaderLine(*it, number++))
      return false;
  }

  return true;
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

bool HttpRequestParser::parseContent(const QByteArray& data)
{
  // Parse message content
  qDebug() << Q_FUNC_INFO << "Content-Length: " << contentLength();
  qDebug() << Q_FUNC_INFO << "data.size(): " << data.size();

  // Parse POST data
  if (contentType() == "application/x-www-form-urlencoded") {
    QUrl url;
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    url.setEncodedQuery(data);
    QListIterator<QPair<QString, QString> > i(url.queryItems());
#else
    url.setQuery(data/*, QUrl::DecodedMode*/);
    QListIterator<QPair<QString, QString> > i(QUrlQuery(url).queryItems());
#endif
    while (i.hasNext())	{
      QPair<QString, QString> pair = i.next();
      m_postMap[pair.first] = pair.second;
    }
    return true;
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
  const QString content_type = value(QLatin1String("content-type"));
  if (content_type.startsWith("multipart/form-data")) {
    static QRegExp boundaryRegexQuoted("boundary=\"([ \\w'()+,-\\./:=\\?]+)\"");
    static QRegExp boundaryRegexNotQuoted("boundary=([\\w'()+,-\\./:=\\?]+)");
    QByteArray boundary;
    if (boundaryRegexQuoted.indexIn(content_type) < 0) {
      if (boundaryRegexNotQuoted.indexIn(content_type) < 0) {
        qWarning() << "Could not find boundary in multipart/form-data header!";
        return false;
      } else {
        boundary = "--" + boundaryRegexNotQuoted.cap(1).toAscii();
      }
    } else {
      boundary = "--" + boundaryRegexQuoted.cap(1).toAscii();
    }
    qDebug() << "Boundary is " << boundary;
    QList<QByteArray> parts = splitRawData(data, boundary);
    qDebug() << parts.size() << "parts in data";
    foreach (const QByteArray& part, parts) {
      const int filenameIndex = part.indexOf("filename=");
      if (filenameIndex < 0)
        continue;
      qDebug() << "Found a torrent";
      m_torrents << part.mid(part.indexOf("\r\n\r\n", filenameIndex + 9) + 4);
    }
  }

  return true;
}

bool HttpRequestParser::acceptsEncoding()
{
  QString encoding = value("Accept-Encoding");

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
