/*
 *   Copyright (C) 2007 by Ishan Arora
 *   ishanarora@gmail.com
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
            return v.value<QString>();
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
                return "undefined";
        }
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
