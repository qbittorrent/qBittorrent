/****************************************************************************
** $Id: qt/smtp.h   3.3.6   edited Aug 31 2005 $
**
** Copyright (C) 1992-2005 Trolltech AS.  All rights reserved.
**
** This file is part of an example program for Qt.  This example
** program may be used, distributed and modified without limitation.
**
*****************************************************************************/
#include "smtp.h"
#include "preferences.h"

#include <QTextStream>
#include <QTcpSocket>
#include <QTextCodec>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>

Smtp::Smtp(const QString &from, const QString &to, const QString &subject, const QString &body) {
  socket = new QTcpSocket(this);

  connect( socket, SIGNAL( readyRead() ), this, SLOT( readyRead() ) );
  QTextCodec* latin1 = QTextCodec::codecForName("latin1");
  message = "";
  message += encode_mime_header("Date", QDateTime::currentDateTimeUtc().toString("ddd, d MMM yyyy hh:mm:ss UT"), latin1);
  message += encode_mime_header("From", from, latin1);
  message += encode_mime_header("Subject", subject, latin1);
  message += encode_mime_header("To", to, latin1);
  message += "MIME-Version: 1.0\r\n";
  message += "Content-Type: text/plain; charset=UTF-8\r\n";
  message += "Content-Transfer-Encoding: base64\r\n";
  message += "\r\n";
  // Encode the body in base64
  QString crlf_body = body;
  QByteArray b = crlf_body.replace("\n","\r\n").toUtf8().toBase64();
  int ct = b.length();
  for (int i = 0; i < ct; i += 78)
  {
    message += b.mid(i, 78);
  }
  this->from = from;
  rcpt = to;
  state = Init;
  socket->connectToHost(Preferences().getMailNotificationSMTP(), 25);
  if(socket->waitForConnected ( 30000 )) {
    qDebug("connected");
  } else {
    t = 0;
    deleteLater();
  }
  t = new QTextStream(socket);
}

QByteArray Smtp::encode_mime_header(const QString& key, const QString& value, QTextCodec* latin1, const QByteArray& prefix)
{
  QByteArray rv = "";
  QByteArray line = key.toAscii() + ": ";
  if (!prefix.isEmpty()) line += prefix;
  if (!value.contains("=?") && latin1->canEncode(value)) {
    bool firstWord = true;
    foreach(const QByteArray& word, value.toAscii().split(' ')) {
      if (line.size() > 78) {
        rv = rv + line + "\r\n";
        line.clear();
      }
      if (firstWord)
        line += word;
      else
        line += " " + word;
      firstWord = false;
    }
  } else {
    // The text cannot be losslessly encoded as Latin-1. Therefore, we
    // must use base64 encoding.
    QByteArray utf8 = value.toUtf8();
    int ct = utf8.length();
    // Use base64 encoding
    QByteArray base64 = utf8.toBase64();
    ct = base64.length();
    line += "=?utf-8?b?";
    for (int i = 0; i < ct; i += 4) {
      /*if (line.length() > 72) {
        rv += line + "?\n\r";
        line = " =?utf-8?b?";
      }*/
      line = line + base64.mid(i, 4);
    }
    line += "?="; // end encoded-word atom
  }
  return rv + line + "\r\n";
}

Smtp::~Smtp()
{
  if(t)
    delete t;
  delete socket;
}

void Smtp::readyRead()
{

  qDebug() << "readyRead";
  // SMTP is line-oriented

  QString responseLine;
  do
  {
    responseLine = socket->readLine();
    response += responseLine;
  }
  while ( socket->canReadLine() && responseLine[3] != ' ' );

  qDebug("Response line: %s", qPrintable(response));

  responseLine.truncate( 3 );


  if ( state == Init && responseLine[0] == '2' )
  {
    // banner was okay, let's go on
    QByteArray address = "127.0.0.1";
    foreach(const QHostAddress& addr, QNetworkInterface::allAddresses())
    {
      if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6)
        continue;
      address = addr.toString().toAscii();
      break;
    }
    *t << "ehlo "+ address + "\r\n";
    t->flush();

    state = Mail;
  }
  else if ( state == Mail || state == Mail2 )
  {
    if(responseLine[0] == '2') {
      // EHLO response was okay (well, it has to be)
      *t << "mail from:<" << from << ">\r\n";
      t->flush();
      state = Rcpt;
    } else {
      if(state == Mail) {
        // ehlo did not work, try helo instead
        *t << "helo\r\n";
        t->flush();
        state = Mail2;
      }
    }
  }
  else if ( state == Rcpt && responseLine[0] == '2' )
  {

    *t << "rcpt to:<" << rcpt << ">\r\n"; //r
    t->flush();
    state = Data;
  }
  else if ( state == Data && responseLine[0] == '2' )
  {

    *t << "data\r\n";
    t->flush();
    state = Body;
  }
  else if ( state == Body && responseLine[0] == '3' )
  {

    *t << message << "\r\n.\r\n";
    t->flush();
    state = Quit;
  }
  else if(state == Quit && responseLine[0] == '2')
  {

    *t << "QUIT\r\n";
    t->flush();
    // here, we just close.
    state = Close;
  }
  else if ( state == Close )
  {
    deleteLater();
    return;
  }
  else
  {
    // something broke.
    state = Close;
  }
  response = "";
}
