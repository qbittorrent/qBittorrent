/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

/*
 * This code is based on QxtSmtp from libqxt (http://libqxt.org)
 */

#ifndef SMTP_H
#define SMTP_H

#include <QString>
#include <QObject>
#include <QByteArray>
#include <QHash>

QT_BEGIN_NAMESPACE
class QTextStream;
#ifndef QT_NO_OPENSSL
class QSslSocket;
#else
class QTcpSocket;
#endif
class QTextCodec;
QT_END_NAMESPACE

class Smtp : public QObject {
  Q_OBJECT

public:
  Smtp(QObject *parent = 0);
  ~Smtp();
  void sendMail(const QString &from, const QString &to, const QString &subject, const QString &body);

private slots:
  void readyRead();

private:
  QByteArray encode_mime_header(const QString& key, const QString& value, QTextCodec* latin1, const QByteArray& prefix=QByteArray());
  void ehlo();
  void helo();
  void parseEhloResponse(const QByteArray& code, bool continued, const QString& line);
  void authenticate();
  void startTLS();
  void authCramMD5(const QByteArray& challenge = QByteArray());
  void authPlain();
  void authLogin();
  void logError(const QString &msg);

private:
  enum states { Rcpt, EhloSent, HeloSent, EhloDone, EhloGreetReceived, AuthRequestSent, AuthSent,
                AuthUsernameSent, Authenticated, StartTLSSent, Data, Init, Body, Quit, Close };
  enum AuthType { AuthPlain, AuthLogin, AuthCramMD5 };

private:
  QByteArray message;
#ifndef QT_NO_OPENSSL
  QSslSocket *socket;
#else
  QTcpSocket *socket;
#endif
  QString from;
  QString rcpt;
  QString response;
  int state;
  QHash<QString, QString> extensions;
  QByteArray buffer;
  bool use_ssl;
  AuthType authType;
  QString username;
  QString password;
};
#endif
