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

#include "httpheader.h"

HttpHeader::HttpHeader() : m_valid(true) {}

HttpHeader::HttpHeader(const QString &str) : m_valid(true) {
  parse(str);
}

HttpHeader::~HttpHeader() {}

void HttpHeader::addValue(const QString &key, const QString &value) {
  m_headers.insert(key.toLower(), value.trimmed());
}

QStringList HttpHeader::allValues(const QString &key) const {
  QStringList list = m_headers.values(key);
  return list;
}

uint HttpHeader::contentLength() const {
  QString lenVal = m_headers.value("content-length");
  return lenVal.toUInt();
}

QString HttpHeader::contentType() const {
  QString str = m_headers.value("content-type");
  // content-type might have a parameter so we need to strip it.
  // eg. application/x-www-form-urlencoded; charset=utf-8
  int index = str.indexOf(';');
  if (index == -1)
    return str;
  else
    return str.left(index);
}

bool HttpHeader::hasContentLength() const {
  return m_headers.contains("content-length");
}

bool HttpHeader::hasContentType() const {
  return m_headers.contains("content-type");
}

bool HttpHeader::hasKey(const QString &key) const {
  return m_headers.contains(key.toLower());
}

bool HttpHeader::isValid() const {
  return m_valid;
}

QStringList HttpHeader::keys() const {
  QStringList list = m_headers.keys();
  return list;
}

void HttpHeader::removeAllValues(const QString &key) {
  m_headers.remove(key);
}

void HttpHeader::removeValue(const QString &key) {
  m_headers.remove(key);
}

void HttpHeader::setContentLength(int len) {
  m_headers.replace("content-length", QString::number(len));
}

void HttpHeader::setContentType(const QString &type) {
  m_headers.replace("content-type", type.trimmed());
}

void HttpHeader::setValue(const QString &key, const QString &value) {
  m_headers.replace(key.toLower(), value.trimmed());
}

void HttpHeader::setValues(const QList<QPair<QString, QString> > &values) {
  for (int i=0; i < values.size(); ++i) {
    setValue(values[i].first, values[i].second);
  }
}

QString HttpHeader::toString() const {
  QString str;
  typedef QMultiHash<QString, QString>::const_iterator h_it;

  for (h_it it = m_headers.begin(), itend = m_headers.end();
       it != itend; ++it) {
    str = str + it.key() + ": " + it.value() + "\r\n";
  }

  str += "\r\n";
  return str;
}

QString HttpHeader::value(const QString &key) const {
  return m_headers.value(key.toLower());
}

QList<QPair<QString, QString> > HttpHeader::values() const {
  QList<QPair<QString, QString> > list;
  typedef QMultiHash<QString, QString>::const_iterator h_it;

  for (h_it it = m_headers.begin(), itend = m_headers.end();
       it != itend; ++it) {
    list.append(qMakePair(it.key(), it.value()));
  }

  return list;
}

void HttpHeader::parse(const QString &str) {
  QStringList headers = str.split("\r\n", QString::SkipEmptyParts, Qt::CaseInsensitive);
  for (int i=0; i < headers.size(); ++i) {
    int index = headers[i].indexOf(':', Qt::CaseInsensitive);
    if (index == -1) {
      setValid(false);
      break;
    }

    QString key = headers[i].left(index);
    QString value = headers[i].right(headers[i].size() - index - 1);
    m_headers.insert(key.toLower(), value.trimmed());
  }
}

void HttpHeader::setValid(bool valid) {
  m_valid = valid;
  if (!m_valid)
    m_headers.clear();
}
