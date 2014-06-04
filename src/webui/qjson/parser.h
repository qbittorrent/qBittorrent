/* This file is part of QJson
 *
 * Copyright (C) 2008 Flavio Castelli <flavio.castelli@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software Foundation.
 * 
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

#ifndef QJSON_PARSER_H
#define QJSON_PARSER_H

#include "qjson_export.h"

QT_BEGIN_NAMESPACE
class QIODevice;
class QVariant;
QT_END_NAMESPACE

/**
 * Namespace used by QJson
 */
namespace QJson {

  class ParserPrivate;

  /**
   * @brief Main class used to convert JSON data to QVariant objects
   */
  class QJSON_EXPORT Parser
  {
    public:
      Parser();
      ~Parser();

      /**
      * Read JSON string from the I/O Device and converts it to a QVariant object
      * @param io Input output device
      * @param ok if a conversion error occurs, *ok is set to false; otherwise *ok is set to true.
      * @returns a QVariant object generated from the JSON string
      */
      QVariant parse(QIODevice* io, bool* ok = 0);

      /**
      * This is a method provided for convenience.
      * @param jsonData data containing the JSON object representation
      * @param ok if a conversion error occurs, *ok is set to false; otherwise *ok is set to true.
      * @returns a QVariant object generated from the JSON string
      * @sa errorString
      * @sa errorLine
      */
      QVariant parse(const QByteArray& jsonData, bool* ok = 0);

      /**
      * This method returns the error message
      * @returns a QString object containing the error message of the last parse operation
      * @sa errorLine
      */
      QString errorString() const;

      /**
      * This method returns line number where the error occurred
      * @returns the line number where the error occurred
      * @sa errorString
      */
      int errorLine() const;

      /**
       * Sets whether special numbers (Infinity, -Infinity, NaN) are allowed as an extension to
       * the standard
       * @param  allowSpecialNumbers new value of whether special numbers are allowed
       * @sa specialNumbersAllowed
       */
      void allowSpecialNumbers(bool allowSpecialNumbers);

      /**
       * @returns whether special numbers (Infinity, -Infinity, NaN) are allowed
       * @sa allowSpecialNumbers
       */
      bool specialNumbersAllowed() const;

    private:
      Q_DISABLE_COPY(Parser)
      ParserPrivate* const d;
  };
}

#endif // QJSON_PARSER_H
