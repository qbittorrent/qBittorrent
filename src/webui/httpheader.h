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

#ifndef HTTPHEADER_H
#define HTTPHEADER_H

#include <QString>
#include <QStringList>
#include <QMultiHash>
#include <QPair>

class HttpHeader {

public:
  HttpHeader();
  HttpHeader(const QString &str);
  virtual ~HttpHeader();
  void addValue(const QString &key, const QString &value);
  QStringList allValues(const QString &key) const;
  uint contentLength() const;
  QString contentType() const;
  bool hasContentLength() const;
  bool hasContentType() const;
  bool hasKey(const QString &key) const;
  bool isValid() const;
  QStringList keys() const;
  virtual int majorVersion() const =0;
  virtual int minorVersion() const =0;
  void removeAllValues(const QString &key);
  void removeValue(const QString &key);
  void setContentLength(int len);
  void setContentType(const QString &type);
  void setValue(const QString &key, const QString &value);
  void setValues(const QList<QPair<QString, QString> > &values);
  virtual QString toString() const;
  QString value(const QString &key) const;
  QList<QPair<QString, QString> > values() const;

protected:
  void parse(const QString &str);
  void setValid(bool valid = true);

private:
  QMultiHash<QString, QString> m_headers;
  bool m_valid;
};

#endif // HTTPHEADER_H
