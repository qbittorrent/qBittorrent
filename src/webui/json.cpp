/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2012  Ishan Arora and Christophe Dumez
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

#include "json.h"

#include <QStringList>
#include <QVariantMap>

QString json::toJson(const QVariant& v) {
  if (v.isNull())
    return "null";
  switch(v.type())
  {
  case QVariant::Bool:
  case QVariant::Double:
  case QVariant::Int:
  case QVariant::LongLong:
  case QVariant::UInt:
  case QVariant::ULongLong:
    return v.value<QString>();
  case QVariant::StringList:
  case QVariant::List: {
    QStringList strList;
    foreach (const QVariant &var, v.toList()) {
      strList << toJson(var);
    }
    return "["+strList.join(",")+"]";
  }
  case QVariant::String: {
    QString s = v.value<QString>();
    QString result = "\"";
    for (int i=0; i<s.size(); ++i) {
      const QChar ch = s[i];
      switch(ch.toAscii())
      {
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '\"':
      case '\\':
        result += '\\';
      case '\0':
      default:
        result += ch;
      }
    }
    result += "\"";
    return result;
  }
  default:
    qDebug("Unknown QVariantType: %d", (int)v.type());
    return "undefined";
  }
}

QVariantMap json::fromJson(const QString& json) {
  qDebug("JSON is %s", qPrintable(json));
  QVariantMap m;
  if (json.startsWith("{") && json.endsWith("}")) {
    QStringList couples;
    QString tmp = "";
    bool in_list = false;
    foreach (const QChar &c, json.mid(1, json.length()-2)) {
      if (c == ',' && !in_list) {
        couples << tmp;
        tmp = "";
      } else {
        if (c == '[')
          in_list = true;
        else if (c == ']')
          in_list = false;
        tmp += c;
      }
    }
    if (!tmp.isEmpty()) couples << tmp;

    foreach (const QString &couple, couples) {
      QStringList parts;
      int jsonSep = couple.indexOf(":");
      parts << couple.left(jsonSep);
      parts << couple.mid(jsonSep + 1);
      Q_ASSERT(parts.size() == 2);
      QString key = parts.first();
      if (key.startsWith("\"") && key.endsWith("\"")) {
        key = key.mid(1, key.length()-2);
      }
      QString value_str = parts.last();
      QVariant value;
      if (value_str.startsWith("[") && value_str.endsWith("]")) {
        value_str = value_str.mid(1, value_str.length()-2);
        QStringList list_elems = value_str.split(",", QString::SkipEmptyParts);
        QVariantList varlist;
        foreach (const QString &list_val, list_elems) {
          if (list_val.startsWith("\"") && list_val.endsWith("\"")) {
            varlist << list_val.mid(1, list_val.length()-2).replace("\\n", "\n");
          } else {
            if (list_val.compare("false", Qt::CaseInsensitive) == 0)
              varlist << false;
            else if (list_val.compare("true", Qt::CaseInsensitive) == 0)
              varlist << true;
            else
              varlist << list_val.toInt();
          }
        }
        value = varlist;
      } else {
        if (value_str.startsWith("\"") && value_str.endsWith("\"")) {
          value_str = value_str.mid(1, value_str.length()-2).replace("\\n", "\n");
          value = value_str;
        } else {
          if (value_str.compare("false", Qt::CaseInsensitive) == 0)
            value = false;
          else if (value_str.compare("true", Qt::CaseInsensitive) == 0)
            value = true;
          else
            value = value_str.toInt();
        }
      }
      m.insert(key, value);
      qDebug("%s:%s", qPrintable(key), qPrintable(value_str));
    }
  }
  return m;
}
