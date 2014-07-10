/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2014  sledgehammer999
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
 * Contact : hammered999@gmail.com
 */

#include "httprequestheader.h"


HttpRequestHeader::HttpRequestHeader() : HttpHeader(), m_majorVersion(1), m_minorVersion(1) {}

HttpRequestHeader::HttpRequestHeader(const QString &method, const QString &path, int majorVer, int minorVer) :
  HttpHeader(), m_method(method), m_path(path), m_majorVersion(majorVer), m_minorVersion(minorVer) {}

HttpRequestHeader::HttpRequestHeader(const QString &str): HttpHeader() {
  int line = str.indexOf("\r\n", 0, Qt::CaseInsensitive);
  QString req = str.left(line);
  QString headers = str.right(str.size() - line - 2); //"\r\n" == 2
  QStringList method = req.split(" ", QString::SkipEmptyParts, Qt::CaseInsensitive);
  if (method.size() != 3)
    setValid(false);
  else {
    m_method = method[0];
    m_path = method[1];
    if (parseVersions(method[2]))
      parse(headers);
    else
      setValid(false);
  }
}

int HttpRequestHeader::majorVersion() const {
  return m_majorVersion;
}

int HttpRequestHeader::minorVersion() const {
  return m_minorVersion;
}

QString HttpRequestHeader::method() const {
  return m_method;
}

QString HttpRequestHeader::path() const {
  return m_path;
}

void HttpRequestHeader::setRequest(const QString &method, const QString &path, int majorVer, int minorVer) {
  m_method = method;
  m_path = path;
  m_majorVersion = majorVer;
  m_minorVersion = minorVer;
}

QString HttpRequestHeader::toString() const {
  QString str = m_method + " ";
  str+= m_path + " ";
  str+= "HTTP/" + QString::number(m_majorVersion) + "." + QString::number(m_minorVersion) + "\r\n";
  str += HttpHeader::toString();
  return str;
}

bool HttpRequestHeader::parseVersions(const QString &str) {
  if (str.size() <= 5) // HTTP/ which means version missing
    return false;

  if (str.left(4) != "HTTP")
    return false;

  QString versions = str.right(str.size() - 5); // Strip "HTTP/"

  int decPoint = versions.indexOf('.');
  if (decPoint == -1)
    return false;

  bool ok;

  m_majorVersion = versions.left(decPoint).toInt(&ok);
  if (!ok)
    return false;

  m_minorVersion = versions.right(versions.size() - decPoint - 1).toInt(&ok);
  if (!ok)
    return false;

  return true;

}
