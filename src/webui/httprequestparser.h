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


#ifndef HTTPREQUESTPARSER_H
#define HTTPREQUESTPARSER_H

#include <QString>
#include <QHash>

class HttpRequestParser {

public:
  enum ErrorCode { NoError = 0, IncompleteRequest, BadRequest };

  HttpRequestParser(uint maxContentLength = 10000000 /* ~10MB */);

  void parse(const QByteArray& data);

  // when error() != NoError all request properties are undefined
  inline ErrorCode error() const { return m_error; }
  inline int length() const { return m_length; }
  inline const QString& method() const { return m_m; }
  inline const QString& url() const { return m_path; }
  inline const QHash<QString, QString>& gets() const { return m_getMap; }
  inline QString get(const QString& key) const { return m_getMap.value(key); }
  inline QString post(const QString& key) const { return m_postMap.value(key); }
  inline const QList<QByteArray>& torrents() const { return m_torrents; }

  QString value(const QString &key) const;
  QString contentType() const;
  bool hasContentLength() const;
  uint contentLength() const;

  bool acceptsEncoding();

private:
  bool parseHeaderLine(const QString &line, int number);
  bool parseHeader(const QByteArray &data);
  bool parseContent(const QByteArray& data);

  bool hasKey(const QString &key) const;
  void addValue(const QString &key, const QString &value);

  ErrorCode m_error;
  int m_length;
  uint m_maxContentLength;
  QString m_m;
  int m_majVer;
  int m_minVer;
  QList<QPair<QString, QString> > m_values;
  QString m_path;
  QHash<QString, QString> m_postMap;
  QHash<QString, QString> m_getMap;
  QList<QByteArray> m_torrents;
};

#endif
