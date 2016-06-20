/* This file is part of QJson
 *
 * Copyright (C) 2008 Flavio Castelli <flavio.castelli@gmail.com>
 * Copyright (C) 2009 Michael Leupold <lemma@confuego.org>
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

#ifndef QJSON_PARSER_P_H
#define QJSON_PARSER_P_H

#include "parser.h"

#include <QtCore/QString>
#include <QtCore/QVariant>

class JSonScanner;

namespace yy {
  class json_parser;
}

namespace QJson {

  class ParserPrivate
  {
    public:
      ParserPrivate();
      ~ParserPrivate();

      void reset();

      void setError(QString errorMsg, int line);

      JSonScanner* m_scanner;
      bool m_error;
      int m_errorLine;
      QString m_errorMsg;
      QVariant m_result;
      bool m_specialNumbersAllowed;
  };
}

#endif // QJSON_PARSER_H
