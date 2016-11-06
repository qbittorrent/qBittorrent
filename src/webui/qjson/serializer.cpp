/* This file is part of qjson
  *
  * Copyright (C) 2009 Till Adam <adam@kde.org>
  * Copyright (C) 2009 Flavio Castelli <flavio@castelli.name>
  * Copyright (C) 2016 Anton Kudryavtsev <a.kudryavtsev@netris.ru>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License version 2.1, as published by the Free Software Foundation.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */

#include "serializer.h"

#include <QtCore/QDataStream>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

// cmath does #undef for isnan and isinf macroses what can be defined in math.h
#if defined(Q_OS_SYMBIAN) || defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_SOLARIS)
# include <math.h>
#else
# include <cmath>
#endif

#ifdef Q_OS_SOLARIS
# ifndef isinf
#  include <ieeefp.h>
#  define isinf(x) (!finite((x)) && (x)==(x))
# endif
#endif

#ifdef _MSC_VER  // using MSVC compiler
#include <float.h>
#endif

using namespace QJson;

class Serializer::SerializerPrivate {
  public:
    SerializerPrivate() :
      specialNumbersAllowed(false),
      indentMode(QJson::IndentNone),
      doublePrecision(6) {
        errorMessage.clear();
    }
    QString errorMessage;
    bool specialNumbersAllowed;
    IndentMode indentMode;
    int doublePrecision;

    QByteArray serialize( const QVariant &v, bool *ok, int indentLevel = 0);

    static QByteArray buildIndent(int spaces);
    static QByteArray escapeString( const QString& str );
    static QByteArray join( const QList<QByteArray>& list, const QByteArray& sep );
    static QByteArray join( const QList<QByteArray>& list, char sep );
};

QByteArray Serializer::SerializerPrivate::join( const QList<QByteArray>& list, const QByteArray& sep ) {
  QByteArray res;
  Q_FOREACH( const QByteArray& i, list ) {
    if ( !res.isEmpty() )
      res += sep;
    res += i;
  }
  return res;
}

QByteArray Serializer::SerializerPrivate::join( const QList<QByteArray>& list, char sep ) {
  QByteArray res;
  Q_FOREACH( const QByteArray& i, list ) {
    if ( !res.isEmpty() )
      res += sep;
    res += i;
  }
  return res;
}

QByteArray Serializer::SerializerPrivate::serialize( const QVariant &v, bool *ok, int indentLevel)
{
  QByteArray str;
  const QVariant::Type type = v.type();

  if ( ! v.isValid() ) { // invalid or null?
    str = "null";
  } else if (( type == QVariant::List ) || ( type == QVariant::StringList )) { // an array or a stringlist?
    const QVariantList list = v.toList();
    QList<QByteArray> values;
    Q_FOREACH( const QVariant& var, list )
    {
      QByteArray serializedValue;

      serializedValue = serialize( var, ok, indentLevel+1);

      if ( !*ok ) {
        break;
      }
      switch(indentMode) {
        case QJson::IndentFull :
        case QJson::IndentMedium :
        case QJson::IndentMinimum :
          values << serializedValue;
          break;
        case QJson::IndentCompact :
        case QJson::IndentNone :
        default:
          values << serializedValue.trimmed();
          break;
      }
    }

    if (indentMode == QJson::IndentMedium || indentMode == QJson::IndentFull ) {
      QByteArray indent = buildIndent(indentLevel);
      str = indent + "[\n" + join( values, ",\n" ) + '\n' + indent + ']';
    }
    else if (indentMode == QJson::IndentMinimum) {
      QByteArray indent = buildIndent(indentLevel);
      str = indent + "[\n" + join( values, ",\n" ) + '\n' + indent + ']';
    }
    else if (indentMode == QJson::IndentCompact) {
      str = '[' + join( values, "," ) + ']';
    }
    else {
      str = "[ " + join( values, ", " ) + " ]";
    }

  } else if ( type == QVariant::Map ) { // variant is a map?
    const QVariantMap vmap = v.toMap();

    if (indentMode == QJson::IndentMinimum) {
      QByteArray indent = buildIndent(indentLevel);
      str = indent + "{ ";
    }
    else if (indentMode == QJson::IndentMedium || indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel);
      QByteArray nextindent = buildIndent(indentLevel + 1);
      str = indent + "{\n" + nextindent;
    }
    else if (indentMode == QJson::IndentCompact) {
      str = "{";
    }
    else {
      str = "{ ";
    }

    QList<QByteArray> pairs;
    for (QVariantMap::const_iterator it = vmap.begin(), end = vmap.end(); it != end; ++it) {
      indentLevel++;
      QByteArray serializedValue = serialize( it.value(), ok, indentLevel);
      indentLevel--;
      if ( !*ok ) {
        break;
      }
      QByteArray key   = escapeString( it.key() );
      QByteArray value = serializedValue.trimmed();
      if (indentMode == QJson::IndentCompact) {
        pairs << key + ':' + value;
      } else {
        pairs << key + " : " + value;
      }
    }

    if (indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel + 1);
      str += join( pairs, ",\n" + indent);
    }
    else if (indentMode == QJson::IndentCompact) {
      str += join( pairs, ',' );
    }
    else {
      str += join( pairs, ", " );
    }

    if (indentMode == QJson::IndentMedium || indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel);
      str += '\n' + indent + '}';
    }
    else if (indentMode == QJson::IndentCompact) {
      str += '}';
    }
    else {
      str += " }";
    }

  } else if ( type == QVariant::Hash ) { // variant is a hash?
    const QVariantHash vhash = v.toHash();

    if (indentMode == QJson::IndentMinimum) {
      QByteArray indent = buildIndent(indentLevel);
      str = indent + "{ ";
    }
    else if (indentMode == QJson::IndentMedium || indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel);
      QByteArray nextindent = buildIndent(indentLevel + 1);
      str = indent + "{\n" + nextindent;
    }
    else if (indentMode == QJson::IndentCompact) {
      str = "{";
    }
    else {
      str = "{ ";
    }

    QList<QByteArray> pairs;
    for (QVariantHash::const_iterator it = vhash.begin(), end = vhash.end(); it != end; ++it) {
      QByteArray serializedValue = serialize( it.value(), ok, indentLevel + 1);

      if ( !*ok ) {
        break;
      }
      QByteArray key   = escapeString( it.key() );
      QByteArray value = serializedValue.trimmed();
      if (indentMode == QJson::IndentCompact) {
        pairs << key + ':' + value;
      } else {
        pairs << key + " : " + value;
      }
    }

    if (indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel + 1);
      str += join( pairs, ",\n" + indent);
    }
    else if (indentMode == QJson::IndentCompact) {
      str += join( pairs, ',' );
    }
    else {
      str += join( pairs, ", " );
    }

    if (indentMode == QJson::IndentMedium || indentMode == QJson::IndentFull) {
      QByteArray indent = buildIndent(indentLevel);
      str += '\n' + indent + '}';
    }
    else if (indentMode == QJson::IndentCompact) {
      str += '}';
    }
    else {
      str += " }";
    }

  } else {
    // Add indent, we may need to remove it later for some layouts
    switch(indentMode) {
      case QJson::IndentFull :
      case QJson::IndentMedium :
      case QJson::IndentMinimum :
        str += buildIndent(indentLevel);
        break;
      case QJson::IndentCompact :
      case QJson::IndentNone :
      default:
        break;
    }

    if (( type == QVariant::String ) ||  ( type == QVariant::ByteArray )) { // a string or a byte array?
      str += escapeString( v.toString() );
    } else if (( type == QVariant::Double) || ((QMetaType::Type)type == QMetaType::Float)) { // a double or a float?
      const double value = v.toDouble();
  #if defined _WIN32 && !defined(Q_OS_SYMBIAN)
      const bool special = _isnan(value) || !_finite(value);
  #elif defined(Q_OS_SYMBIAN) || defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_SOLARIS)
      const bool special = isnan(value) || isinf(value);
  #else
      const bool special = std::isnan(value) || std::isinf(value);
  #endif
      if (special) {
        if (specialNumbersAllowed) {
  #if defined _WIN32 && !defined(Q_OS_SYMBIAN)
          if (_isnan(value)) {
  #elif defined(Q_OS_SYMBIAN) || defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_SOLARIS)
          if (isnan(value)) {
  #else
          if (std::isnan(value)) {
  #endif
            str += "NaN";
          } else {
            if (value<0) {
              str += '-';
            }
            str += "Infinity";
          }
        } else {
          errorMessage += QLatin1String("Attempt to write NaN or infinity, which is not supported by json\n");
          *ok = false;
      }
      } else {
        str = QByteArray::number( value , 'g', doublePrecision);
        if( !str.contains( '.' ) && !str.contains( 'e' ) ) {
          str += ".0";
        }
      }
    } else if ( type == QVariant::Bool ) { // boolean value?
      str += ( v.toBool() ? "true" : "false" );
    } else if ( type == QVariant::ULongLong ) { // large unsigned number?
      str += QByteArray::number( v.value<qulonglong>() );
    } else if ( type == QVariant::UInt ) { // unsigned int number?
      str += QByteArray::number( v.value<quint32>() );
    } else if ( v.canConvert<qlonglong>() ) { // any signed number?
      str += QByteArray::number( v.value<qlonglong>() );
    } else if ( v.canConvert<int>() ) { // unsigned short number?
      str += QByteArray::number( v.value<int>() );
    } else if ( v.canConvert<QString>() ){ // can value be converted to string?
      // this will catch QDate, QDateTime, QUrl, ...
      str += escapeString( v.toString() );
      //TODO: catch other values like QImage, QRect, ...
    } else {
      *ok = false;
      errorMessage += QLatin1String("Cannot serialize ");
      errorMessage += v.toString();
      errorMessage += QLatin1String(" because type ");
      errorMessage += QLatin1String(v.typeName());
      errorMessage += QLatin1String(" is not supported by QJson\n");
    }
  }
  if ( *ok )
  {
    return str;
  }
  else
    return QByteArray();
}

QByteArray Serializer::SerializerPrivate::buildIndent(int spaces)
{
   QByteArray indent;
   if (spaces < 0) {
     spaces = 0;
   }
   for (int i = 0; i < spaces; i++ ) {
     indent += ' ';
   }
   return indent;
}

QByteArray Serializer::SerializerPrivate::escapeString( const QString& str )
{
  QByteArray result;
  result.reserve(str.size() + 2);
  result.append('\"');
  for (QString::const_iterator it = str.begin(), end = str.end(); it != end; ++it) {
    ushort unicode = it->unicode();
    switch ( unicode ) {
      case '\"':
        result.append("\\\"");
        break;
      case '\\':
        result.append("\\\\");
        break;
      case '\b':
        result.append("\\b");
        break;
      case '\f':
        result.append("\\f");
        break;
      case '\n':
        result.append("\\n");
        break;
      case '\r':
        result.append("\\r");
        break;
      case '\t':
        result.append("\\t");
        break;
      default:
        if ( unicode > 0x1F && unicode < 128 ) {
          result.append(static_cast<char>(unicode));
        } else {
          char escaped[7];
          qsnprintf(escaped, sizeof(escaped)/sizeof(char), "\\u%04x", unicode);
          result.append(escaped);
        }
    }
  }
  result.append('\"');
  return result;
}

Serializer::Serializer()
  : d( new SerializerPrivate )
{
}

Serializer::~Serializer() {
  delete d;
}

void Serializer::serialize( const QVariant& v, QIODevice* io, bool* ok)
{
  Q_ASSERT( io );
  *ok = true;

  if (!io->isOpen()) {
    if (!io->open(QIODevice::WriteOnly)) {
      d->errorMessage = QLatin1String("Error opening device");
      *ok = false;
      return;
    }
  }

  if (!io->isWritable()) {
    d->errorMessage = QLatin1String("Device is not readable");
    io->close();
    *ok = false;
    return;
  }

  const QByteArray str = serialize( v, ok);
  if (*ok && (io->write(str) != str.count())) {
    *ok = false;
    d->errorMessage = QLatin1String("Something went wrong while writing to IO device");
  }
}

QByteArray Serializer::serialize( const QVariant &v)
{
  bool ok;

  return serialize(v, &ok);
}

QByteArray Serializer::serialize( const QVariant &v, bool *ok)
{
  bool _ok = true;
  d->errorMessage.clear();

  if (ok) {
    *ok = true;
  } else {
    ok = &_ok;
  }

  return d->serialize(v, ok);
}

void QJson::Serializer::allowSpecialNumbers(bool allow) {
  d->specialNumbersAllowed = allow;
}

bool QJson::Serializer::specialNumbersAllowed() const {
  return d->specialNumbersAllowed;
}

void QJson::Serializer::setIndentMode(IndentMode mode) {
  d->indentMode = mode;
}

void QJson::Serializer::setDoublePrecision(int precision) {
  d->doublePrecision = precision;
}

IndentMode QJson::Serializer::indentMode() const {
  return d->indentMode;
}

QString QJson::Serializer::errorMessage() const {
  return d->errorMessage;
}

