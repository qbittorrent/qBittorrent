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

#include "httpresponseheader.h"

HttpResponseHeader::HttpResponseHeader(): HttpHeader(), m_code(200), m_majorVersion(1),  m_minorVersion(1) {}

HttpResponseHeader::HttpResponseHeader(int code, const QString &text, int majorVer, int minorVer):
  HttpHeader(), m_code(code), m_text(text), m_majorVersion(majorVer), m_minorVersion(minorVer) {}

HttpResponseHeader::HttpResponseHeader(const QString &str): HttpHeader() {
  int line = str.indexOf("\r\n", 0, Qt::CaseInsensitive);
  QString res = str.left(line);
  QString headers = str.right(str.size() - line - 2); //"\r\n" == 2
  QStringList status = res.split(" ", QString::SkipEmptyParts, Qt::CaseInsensitive);
  if (status.size() != 3 || status.size() != 2) //reason-phrase could be empty
    setValid(false);
  else {
    if (parseVersions(status[0])) {
      bool ok;
      m_code = status[1].toInt(&ok);
      if (ok) {
        m_text = status[2];
        parse(headers);
      }
      else
        setValid(false);
    }
    else
      setValid(false);
  }
}

int HttpResponseHeader::majorVersion() const {
  return m_majorVersion;
}

int HttpResponseHeader::minorVersion() const {
  return m_minorVersion;
}

QString HttpResponseHeader::reasonPhrase() const {
  return m_text;
}

void HttpResponseHeader::setStatusLine(int code, const QString &text, int majorVer, int minorVer) {
  m_code = code;
  m_text = text;
  m_majorVersion = majorVer;
  m_minorVersion = minorVer;
}

int HttpResponseHeader::statusCode() const {
  return m_code;
}

QString HttpResponseHeader::toString() const {
  QString str = "HTTP/" + QString::number(m_majorVersion) + "." + QString::number(m_minorVersion) + " ";

  QString code = QString::number(m_code);
  if (code.size() > 3) {
    str+= code.left(3) + " ";
  }
  else if (code.size() < 3) {
    int padding = 3 - code.size();
    for (int i = 0; i < padding; ++i)
      code.push_back("0");
    str += code + " ";
  }
  else {
    str += code + " ";
  }

  str += m_text + "\r\n";
  str += HttpHeader::toString();
  return str;
}

bool HttpResponseHeader::parseVersions(const QString &str) {
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
