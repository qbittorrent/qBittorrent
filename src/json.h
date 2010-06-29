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


#ifndef JSON_H
#define JSON_H

#include <QVariant>

namespace json {

  QString toJson(QVariant v) {
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
    case QMetaType::Float:
      return v.value<QString>();
    case QVariant::StringList:
    case QVariant::List: {
        QStringList strList;
        foreach(const QVariant &var, v.toList()) {
          strList << toJson(var);
        }
        return "["+strList.join(",")+"]";
      }
    case QVariant::String:
      {
        QString s = v.value<QString>();
        QString result = "\"";
        for(int i=0; i<s.size(); i++)
        {
          QChar ch = s[i];
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
          case '\'':
          case '\\':
          case '&':
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

  QString toJson(QVariantMap m) {
    QStringList vlist;
    foreach(QString key, m.keys()) {
      vlist << toJson(key)+":"+toJson(m[key]);
    }
    return "{"+vlist.join(",")+"}";
  }

  QVariantMap fromJson(QString json) {
    qDebug("JSON is %s", qPrintable(json));
    QVariantMap m;
    if(json.startsWith("{") && json.endsWith("}")) {
      json.chop(1);
      json = json.replace(0, 1, "");
      QStringList couples;
      QString tmp = "";
      bool in_list = false;
      foreach(QChar c, json) {
        if(c == ',' && !in_list) {
          couples << tmp;
          tmp = "";
        } else {
          if(c == '[') {
            in_list = true;
          } else {
            if(c == ']') {
              in_list = false;
            }
          }
          tmp += c;
        }
      }
      foreach(QString couple, couples) {
        QStringList parts = couple.split(":");
        if(parts.size() != 2) continue;
        QString key = parts.first();
        if(key.startsWith("\"") && key.endsWith("\"")) {
          key.chop(1);
          key = key.replace(0, 1, "");
        }
        QString value_str = parts.last();
        QVariant value;
        if(value_str.startsWith("[") && value_str.endsWith("]")) {
          value_str.chop(1);
          value_str.replace(0, 1, "");
          QStringList list_elems = value_str.split(",", QString::SkipEmptyParts);
          QVariantList varlist;
          foreach(QString list_val, list_elems) {
            if(list_val.startsWith("\"") && list_val.endsWith("\"")) {
              list_val.chop(1);
              list_val = list_val.replace(0, 1, "");
              varlist << list_val;
            } else {
              varlist << list_val.toInt();
            }
          }
          value = varlist;
        } else {
          if(value_str.startsWith("\"") && value_str.endsWith("\"")) {
            value_str.chop(1);
            value_str = value_str.replace(0, 1, "");
            value = value_str;
          } else {
            value = value_str.toInt();
          }
        }
        m.insert(key,value);
        qDebug("%s:%s", key.toLocal8Bit().data(), value_str.toLocal8Bit().data());
      }
    }
    return m;
  }

  QString toJson(QList<QVariantMap> v) {
    QStringList res;
    foreach(QVariantMap m, v) {
      QStringList vlist;
      foreach(QString key, m.keys()) {
        vlist << toJson(key)+":"+toJson(m[key]);
      }
      res << "{"+vlist.join(",")+"}";
    }
    return "["+res.join(",")+"]";
  }
}

#endif
