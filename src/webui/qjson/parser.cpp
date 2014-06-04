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

#include "parser.h"
#include "parser_p.h"
#include "json_parser.hh"
#include "json_scanner.h"

#include <QtCore/QBuffer>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

using namespace QJson;

ParserPrivate::ParserPrivate() :
  m_scanner(0)
{
  m_specialNumbersAllowed = false;
  reset();
}

ParserPrivate::~ParserPrivate()
{
  if (m_scanner)
    delete m_scanner;
}

void ParserPrivate::setError(QString errorMsg, int errorLine) {
  m_error = true;
  m_errorMsg = errorMsg;
  m_errorLine = errorLine;
}

void ParserPrivate::reset()
{
  m_error = false;
  m_errorLine = 0;
  m_errorMsg.clear();
  if (m_scanner) {
    delete m_scanner;
    m_scanner = 0;
  }
}

Parser::Parser() :
    d(new ParserPrivate)
{
}

Parser::~Parser()
{
  delete d;
}

QVariant Parser::parse (QIODevice* io, bool* ok)
{
  d->reset();

  if (!io->isOpen()) {
    if (!io->open(QIODevice::ReadOnly)) {
      if (ok != 0)
        *ok = false;
      qCritical ("Error opening device");
      return QVariant();
    }
  }

  if (!io->isReadable()) {
    if (ok != 0)
      *ok = false;
    qCritical ("Device is not readable");
    io->close();
    return QVariant();
  }

  if (io->atEnd()) {
    if (ok != 0)
      *ok = false;
    d->setError(QLatin1String("No data"), 0);
    io->close();
    return QVariant();
  }

  d->m_scanner = new JSonScanner (io);
  d->m_scanner->allowSpecialNumbers(d->m_specialNumbersAllowed);
  yy::json_parser parser(d);
  parser.parse();

  delete d->m_scanner;
  d->m_scanner = 0;

  if (ok != 0)
    *ok = !d->m_error;

  io->close();
  return d->m_result;
}

QVariant Parser::parse(const QByteArray& jsonString, bool* ok) {
  QBuffer buffer;
  buffer.open(QBuffer::ReadWrite);
  buffer.write(jsonString);
  buffer.seek(0);
  return parse (&buffer, ok);
}

QString Parser::errorString() const
{
  return d->m_errorMsg;
}

int Parser::errorLine() const
{
  return d->m_errorLine;
}

void QJson::Parser::allowSpecialNumbers(bool allowSpecialNumbers) {
  d->m_specialNumbersAllowed = allowSpecialNumbers;
}

bool Parser::specialNumbersAllowed() const {
  return d->m_specialNumbersAllowed;
}
