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

#include "smtp.h"
#include "preferences.h"
#include "qbtsession.h"

#include <QTextStream>
#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#else
#include <QTcpSocket>
#endif
#include <QTextCodec>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QCryptographicHash>

const short DEFAULT_PORT = 25;
const short DEFAULT_PORT_SSL = 465;

QByteArray hmacMD5(QByteArray key, const QByteArray &msg)
{
  const int blockSize = 64; // HMAC-MD5 block size
  if (key.length() > blockSize) { // if key is longer than block size (64), reduce key length with MD5 compression
    key = QCryptographicHash::hash(key, QCryptographicHash::Md5);
  }

  QByteArray innerPadding(blockSize, char(0x36)); // initialize inner padding with char "6"
  QByteArray outerPadding(blockSize, char(0x5c)); // initialize outer padding with char "\"
  // ascii characters 0x36 ("6") and 0x5c ("\") are selected because they have large
  // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)

  for (int i = 0; i < key.length(); i++) {
    innerPadding[i] = innerPadding[i] ^ key.at(i); // XOR operation between every byte in key and innerpadding, of key length
    outerPadding[i] = outerPadding[i] ^ key.at(i); // XOR operation between every byte in key and outerpadding, of key length
  }

  // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseString ) ).toBase64
  QByteArray total = outerPadding;
  QByteArray part = innerPadding;
  part.append(msg);
  total.append(QCryptographicHash::hash(part, QCryptographicHash::Md5));
  return QCryptographicHash::hash(total, QCryptographicHash::Md5);
}

Smtp::Smtp(QObject *parent): QObject(parent),
  state(Init), use_ssl(false) {
#ifndef QT_NO_OPENSSL
  socket = new QSslSocket(this);
#else
  socket = new QTcpSocket(this);
#endif

  connect(socket, SIGNAL(readyRead()), SLOT(readyRead()));
  connect(socket, SIGNAL(disconnected()), SLOT(deleteLater()));

  // Test hmacMD5 function (http://www.faqs.org/rfcs/rfc2202.html)
  Q_ASSERT(hmacMD5("Jefe", "what do ya want for nothing?").toHex()
           == "750c783e6ab0b503eaa86e310a5db738");
  Q_ASSERT(hmacMD5(QByteArray::fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                   "Hi There").toHex()
           == "9294727a3638bb1c13f48ef8158bfc9d");
}

Smtp::~Smtp() {
  qDebug() << Q_FUNC_INFO;
}

void Smtp::sendMail(const QString &from, const QString &to, const QString &subject, const QString &body) {
  Preferences pref;
  QTextCodec* latin1 = QTextCodec::codecForName("latin1");
  message = "";
  message += encode_mime_header("Date", QDateTime::currentDateTime().toUTC().toString("ddd, d MMM yyyy hh:mm:ss UT"), latin1);
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
  // Authentication
  if (pref.getMailNotificationSMTPAuth()) {
    username = pref.getMailNotificationSMTPUsername();
    password = pref.getMailNotificationSMTPPassword();
  }

  // Connect to SMTP server
#ifndef QT_NO_OPENSSL
  if (pref.getMailNotificationSMTPSSL()) {
    socket->connectToHostEncrypted(pref.getMailNotificationSMTP(), DEFAULT_PORT_SSL);
    use_ssl = true;
  } else {
#endif
    socket->connectToHost(pref.getMailNotificationSMTP(), DEFAULT_PORT);
    use_ssl = false;
#ifndef QT_NO_OPENSSL
  }
#endif
}

void Smtp::readyRead()
{
  qDebug() << Q_FUNC_INFO;
  // SMTP is line-oriented
  buffer += socket->readAll();
  while (true)
  {
    int pos = buffer.indexOf("\r\n");
    if (pos < 0) return; // Loop exit condition
    QByteArray line = buffer.left(pos);
    buffer = buffer.mid(pos + 2);
    qDebug() << "Response line:" << line;
    // Extract reponse code
    QByteArray code = line.left(3);

    switch(state) {
    case Init: {
      if (code[0] == '2') {
        // Connection was successful
        ehlo();
      } else {
        logError("Connection failed, unrecognized reply: "+line);
        state = Close;
      }
      break;
    }
    case EhloSent:
    case HeloSent:
    case EhloGreetReceived:
      parseEhloResponse(code, line[3] != ' ', line.mid(4));
      break;
#ifndef QT_NO_OPENSSL
    case StartTLSSent:
      if (code == "220") {
        socket->startClientEncryption();
        ehlo();
      } else {
        authenticate();
      }
      break;
#endif
    case AuthRequestSent:
    case AuthUsernameSent:
      if (authType == AuthPlain) authPlain();
      else if (authType == AuthLogin) authLogin();
      else authCramMD5(line.mid(4));
      break;
    case AuthSent:
    case Authenticated:
      if (code[0] == '2') {
        qDebug() << "Sending <mail from>...";
        socket->write("mail from:<" + from.toAscii() + ">\r\n");
        socket->flush();
        state = Rcpt;
      } else {
        // Authentication failed!
        logError("Authentication failed, msg: "+line);
        state = Close;
      }
      break;
    case Rcpt:
      if (code[0] == '2') {
        socket->write("rcpt to:<" + rcpt.toAscii() + ">\r\n");
        socket->flush();
        state = Data;
      } else {
        logError("<mail from> was rejected by server, msg: "+line);
        state = Close;
      }
      break;
    case Data:
      if (code[0] == '2') {
        socket->write("data\r\n");
        socket->flush();
        state = Body;
      } else {
        logError("<Rcpt to> was rejected by server, msg: "+line);
        state = Close;
      }
      break;
    case Body:
      if (code[0] == '3') {
        socket->write(message + "\r\n.\r\n");
        socket->flush();
        state = Quit;
      } else {
        logError("<data> was rejected by server, msg: "+line);
        state = Close;
      }
      break;
    case Quit:
      if (code[0] == '2') {
        socket->write("QUIT\r\n");
        socket->flush();
        // here, we just close.
        state = Close;
      } else {
        logError("Message was rejected by the server, error: "+line);
        state = Close;
      }
      break;
    default:
      qDebug() << "Disconnecting from host";
      socket->disconnectFromHost();
      return;
    }
  }
}

QByteArray Smtp::encode_mime_header(const QString& key, const QString& value, QTextCodec* latin1, const QByteArray& prefix)
{
  QByteArray rv = "";
  QByteArray line = key.toAscii() + ": ";
  if (!prefix.isEmpty()) line += prefix;
  if (!value.contains("=?") && latin1->canEncode(value)) {
    bool firstWord = true;
    foreach (const QByteArray& word, value.toAscii().split(' ')) {
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
    // Use base64 encoding
    QByteArray base64 = utf8.toBase64();
    int ct = base64.length();
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

void Smtp::ehlo()
{
  QByteArray address = "127.0.0.1";
  foreach (const QHostAddress& addr, QNetworkInterface::allAddresses())
  {
    if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6)
      continue;
    address = addr.toString().toAscii();
    break;
  }
  // Send EHLO
  socket->write("ehlo "+ address + "\r\n");
  socket->flush();
  state = EhloSent;
}

void Smtp::parseEhloResponse(const QByteArray& code, bool continued, const QString& line)
{
  if (code != "250") {
    // Error
    if (state == EhloSent) {
      // try to send HELO instead of EHLO
      qDebug() << "EHLO failed, trying HELO instead...";
      socket->write("helo\r\n");
      socket->flush();
      state = HeloSent;
    } else {
      // Both EHLO and HELO failed, chances are this is NOT
      // a SMTP server
      logError("Both EHLO and HELO failed, msg: "+line);
      state = Close;
    }
    return;
  }
  if (state != EhloGreetReceived) {
    if (!continued) {
      // greeting only, no extensions
      qDebug() << "No extension";
      state = EhloDone;
    } else {
      // greeting followed by extensions
      state = EhloGreetReceived;
      qDebug () << "EHLO greet received";
      return;
    }
  } else {
    qDebug() << Q_FUNC_INFO << "Supported extension: " << line.section(' ', 0, 0).toUpper()
             << line.section(' ', 1);
    extensions[line.section(' ', 0, 0).toUpper()] = line.section(' ', 1);
    if (!continued)
      state = EhloDone;
  }
  if (state != EhloDone) return;
  if (extensions.contains("STARTTLS") && use_ssl) {
    qDebug() << "STARTTLS";
    startTLS();
  } else {
    authenticate();
  }
}

void Smtp::authenticate()
{
  qDebug() << Q_FUNC_INFO;
  if (!extensions.contains("AUTH") ||
      username.isEmpty() || password.isEmpty()) {
    // Skip authentication
    qDebug() << "Skipping authentication...";
    state = Authenticated;
    return;
  }
  // AUTH extension is supported, check which
  // authentication modes are supported by
  // the server
  QStringList auth = extensions["AUTH"].toUpper().split(' ', QString::SkipEmptyParts);
  if (auth.contains("CRAM-MD5")) {
    qDebug() << "Using CRAM-MD5 authentication...";
    authCramMD5();
  }
  else if (auth.contains("PLAIN")) {
    qDebug() << "Using PLAIN authentication...";
    authPlain();
  }
  else if (auth.contains("LOGIN")) {
    qDebug() << "Using LOGIN authentication...";
    authLogin();
  } else {
    // Skip authentication
    logError("The SMTP server does not seem to support any of the authentications modes "
             "we support [CRAM-MD5|PLAIN|LOGIN], skipping authentication, "
             "knowing it is likely to fail... Server Auth Modes: "+auth.join("|"));
    state = Authenticated;
  }
}

void Smtp::startTLS()
{
  qDebug() << Q_FUNC_INFO;
#ifndef QT_NO_OPENSSL
  socket->write("starttls\r\n");
  socket->flush();
  state = StartTLSSent;
#else
  authenticate();
#endif
}

void Smtp::authCramMD5(const QByteArray& challenge)
{
  if (state != AuthRequestSent) {
    socket->write("auth cram-md5\r\n");
    socket->flush();
    authType = AuthCramMD5;
    state = AuthRequestSent;
  } else {
    QByteArray response = username.toAscii() + ' '
        + hmacMD5(password.toAscii(), QByteArray::fromBase64(challenge)).toHex();
    socket->write(response.toBase64() + "\r\n");
    socket->flush();
    state = AuthSent;
  }
}

void Smtp::authPlain()
{
  if (state != AuthRequestSent) {
    authType = AuthPlain;
    // Prepare Auth string
    QByteArray auth;
    auth += '\0';
    auth += username.toAscii();
    qDebug() << "username: " << username.toAscii();
    auth += '\0';
    auth += password.toAscii();
    qDebug() << "password: " << password.toAscii();
    // Send it
    socket->write("auth plain "+ auth.toBase64() + "\r\n");
    socket->flush();
    state = AuthSent;
  }
}

void Smtp::authLogin()
{
  if (state != AuthRequestSent && state != AuthUsernameSent) {
    socket->write("auth login\r\n");
    socket->flush();
    authType = AuthLogin;
    state = AuthRequestSent;
  }
  else if (state == AuthRequestSent) {
    socket->write(username.toAscii().toBase64() + "\r\n");
    socket->flush();
    state = AuthUsernameSent;
  }
  else {
    socket->write(password.toAscii().toBase64() + "\r\n");
    socket->flush();
    state = AuthSent;
  }
}

void Smtp::logError(const QString &msg)
{
  qDebug() << "Email Notification Error:" << msg;
  QBtSession::instance()->addConsoleMessage("Email Notification Error: "+msg, "red");
}
