/* This file is part of QJson
 *
 * Copyright (C) 2008 Flavio Castelli <flavio.castelli@gmail.com>
 * Copyright (C) 2013 Silvio Moioli <silvio@moioli.net>
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
#include "json_scanner.cc"

#include "qjson_debug.h"
#include "json_scanner.h"
#include "json_parser.hh"

#include <ctype.h>

#include <QtCore/QDebug>
#include <QtCore/QRegExp>

#include <cassert>


JSonScanner::JSonScanner(QIODevice* io)
  : m_allowSpecialNumbers(false),
    m_io (io),
    m_criticalError(false),
    m_C_locale(QLocale::C)
{

}

JSonScanner::~JSonScanner()
{
}

void JSonScanner::allowSpecialNumbers(bool allow) {
  m_allowSpecialNumbers = allow;
}

int JSonScanner::yylex(YYSTYPE* yylval, yy::location *yylloc) {
  m_yylval = yylval;
  m_yylloc = yylloc;
  m_yylloc->step();
  int result = yylex();
  
  if (m_criticalError) {
    return -1;
  }
  
  return result;
}

int JSonScanner::LexerInput(char* buf, int max_size) {
  if (!m_io->isOpen()) {
    qCritical() << "JSonScanner::yylex - io device is not open";
    m_criticalError = true;
    return 0;
  }
  
  int readBytes = m_io->read(buf, max_size);
  if(readBytes < 0) {
    qCritical() << "JSonScanner::yylex - error while reading from io device";
    m_criticalError = true;
    return 0;
  }
  
  return readBytes;
}


