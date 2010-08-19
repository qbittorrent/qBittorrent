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
#include <QDebug>

Smtp::Smtp(const QString &from, const QString &to, const QString &subject, const QString &body) {
  socket = new QTcpSocket(this);

  connect( socket, SIGNAL( readyRead() ), this, SLOT( readyRead() ) );
  message = "To: " + to + "\n";
  message.append("From: " + from + "\n");
  message.append("Subject: " + subject + "\n");
  message.append(body);
  message.replace( QString::fromLatin1( "\n" ), QString::fromLatin1( "\r\n" ) );
  message.replace( QString::fromLatin1( "\r\n.\r\n" ),
                   QString::fromLatin1( "\r\n..\r\n" ) );
  this->from = from;
  rcpt = to;
  state = Init;
  socket->connectToHost(Preferences::getMailNotificationSMTP(), 25);
  if(socket->waitForConnected ( 30000 )) {
    qDebug("connected");
  } else {
    t = 0;
    deleteLater();
  }
  t = new QTextStream(socket);
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

    *t << "HELO there\r\n";
    t->flush();

    state = Mail;
  }
  else if ( state == Mail && responseLine[0] == '2' )
  {
    // HELO response was okay (well, it has to be)

    *t << "MAIL FROM: " << from << "\r\n";
    t->flush();
    state = Rcpt;
  }
  else if ( state == Rcpt && responseLine[0] == '2' )
  {

    *t << "RCPT TO: " << rcpt << "\r\n"; //r
    t->flush();
    state = Data;
  }
  else if ( state == Data && responseLine[0] == '2' )
  {

    *t << "DATA\r\n";
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
