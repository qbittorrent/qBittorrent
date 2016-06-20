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

#ifndef _JSON_SCANNER
#define _JSON_SCANNER

#include <QtCore/QIODevice>
#include <QtCore/QVariant>
#include <QtCore/QLocale>

#define YYSTYPE QVariant

// Only include FlexLexer.h if it hasn't been already included
#if ! defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

#include "parser_p.h"



namespace yy {
  class location;
  int yylex(YYSTYPE *yylval, yy::location *yylloc, QJson::ParserPrivate* driver);
}

class JSonScanner : public yyFlexLexer
{
    public:
        explicit JSonScanner(QIODevice* io);
        ~JSonScanner();

        void allowSpecialNumbers(bool allow);

        int yylex(YYSTYPE* yylval, yy::location *yylloc);
        int yylex();
        int LexerInput(char* buf, int max_size);
    protected:
        bool m_allowSpecialNumbers;
        QIODevice* m_io;

        YYSTYPE* m_yylval;
        yy::location* m_yylloc;
        bool m_criticalError;
        QString m_currentString;
        QLocale m_C_locale;
};

#endif
