/****************************************************************************
** $Id: qt/smtp.h   3.3.6   edited Aug 31 2005 $
**
** Copyright (C) 1992-2005 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/

#ifndef SMTP_H
#define SMTP_H


#include <QString>
#include <QObject>
#include <QByteArray>

struct QTextStream;
struct QTcpSocket;
class QTextCodec;

class Smtp : public QObject {
  Q_OBJECT

public:
  Smtp(const QString &from, const QString &to, const QString &subject, const QString &body);
  ~Smtp();

private slots:
  void readyRead();

private:
  QByteArray encode_mime_header(const QString& key, const QString& value, QTextCodec* latin1, const QByteArray& prefix=QByteArray());

private:
  QByteArray message;
  QTextStream *t;
  QTcpSocket *socket;
  QString from;
  QString rcpt;
  QString response;
  enum states{Rcpt,Mail,Mail2,Data,Init,Body,Quit,Close};
  int state;

};
#endif
