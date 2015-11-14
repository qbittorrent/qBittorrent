/*
 * Bittorrent Client using Qt and libtorrent.
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
#include "core/preferences.h"
#include "core/logger.h"

#include <QTextStream>
#ifndef QT_NO_OPENSSL
#include <QSslSocket>
#else
#include <QTcpSocket>
#endif
#include <QTextCodec>
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QStringList>

namespace
{
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

    QByteArray determineFQDN()
    {
        QString hostname = QHostInfo::localHostName();
        if (hostname.isEmpty())
            hostname = "localhost";

        return hostname.toLocal8Bit();
    }
} // namespace

using namespace Net;

Smtp::Smtp(QObject *parent)
    : QObject(parent)
    , m_state(Init)
    , m_useSsl(false)
{
#ifndef QT_NO_OPENSSL
    m_socket = new QSslSocket(this);
#else
    m_socket = new QTcpSocket(this);
#endif

    connect(m_socket, SIGNAL(readyRead()), SLOT(readyRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(deleteLater()));

    // Test hmacMD5 function (http://www.faqs.org/rfcs/rfc2202.html)
    Q_ASSERT(hmacMD5("Jefe", "what do ya want for nothing?").toHex()
             == "750c783e6ab0b503eaa86e310a5db738");
    Q_ASSERT(hmacMD5(QByteArray::fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), "Hi There").toHex()
             == "9294727a3638bb1c13f48ef8158bfc9d");
}

Smtp::~Smtp()
{
    qDebug() << Q_FUNC_INFO;
}

void Smtp::sendMail(const QString &from, const QString &to, const QString &subject, const QString &body)
{
    const Preferences* const pref = Preferences::instance();
    QTextCodec* latin1 = QTextCodec::codecForName("latin1");
    m_message = "";
    m_message += encodeMimeHeader("Date", QDateTime::currentDateTime().toUTC().toString("ddd, d MMM yyyy hh:mm:ss UT"), latin1);
    m_message += encodeMimeHeader("From", from, latin1);
    m_message += encodeMimeHeader("Subject", subject, latin1);
    m_message += encodeMimeHeader("To", to, latin1);
    m_message += "MIME-Version: 1.0\r\n";
    m_message += "Content-Type: text/plain; charset=UTF-8\r\n";
    m_message += "Content-Transfer-Encoding: base64\r\n";
    m_message += "\r\n";
    // Encode the body in base64
    QString crlf_body = body;
    QByteArray b = crlf_body.replace("\n","\r\n").toUtf8().toBase64();
    int ct = b.length();
    for (int i = 0; i < ct; i += 78)
        m_message += b.mid(i, 78);
    m_from = from;
    m_rcpt = to;
    // Authentication
    if (pref->getMailNotificationSMTPAuth()) {
        m_username = pref->getMailNotificationSMTPUsername();
        m_password = pref->getMailNotificationSMTPPassword();
    }

    // Connect to SMTP server
#ifndef QT_NO_OPENSSL
    if (pref->getMailNotificationSMTPSSL()) {
        m_socket->connectToHostEncrypted(pref->getMailNotificationSMTP(), DEFAULT_PORT_SSL);
        m_useSsl = true;
    }
    else {
#endif
        m_socket->connectToHost(pref->getMailNotificationSMTP(), DEFAULT_PORT);
        m_useSsl = false;
#ifndef QT_NO_OPENSSL
    }
#endif
}

void Smtp::readyRead()
{
    qDebug() << Q_FUNC_INFO;
    // SMTP is line-oriented
    m_buffer += m_socket->readAll();
    while (true) {
        int pos = m_buffer.indexOf("\r\n");
        if (pos < 0) return; // Loop exit condition
        QByteArray line = m_buffer.left(pos);
        m_buffer = m_buffer.mid(pos + 2);
        qDebug() << "Response line:" << line;
        // Extract response code
        QByteArray code = line.left(3);

        switch (m_state) {
        case Init: {
            if (code[0] == '2') {
                // The server may send a multiline greeting/INIT/220 response.
                // We wait until it finishes.
                if (line[3] != ' ')
                    break;
                // Connection was successful
                ehlo();
            }
            else {
                logError("Connection failed, unrecognized reply: "+line);
                m_state = Close;
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
                m_socket->startClientEncryption();
                ehlo();
            }
            else {
                authenticate();
            }
            break;
#endif
        case AuthRequestSent:
        case AuthUsernameSent:
            if (m_authType == AuthPlain) authPlain();
            else if (m_authType == AuthLogin) authLogin();
            else authCramMD5(line.mid(4));
            break;
        case AuthSent:
        case Authenticated:
            if (code[0] == '2') {
                qDebug() << "Sending <mail from>...";
                m_socket->write("mail from:<" + m_from.toLatin1() + ">\r\n");
                m_socket->flush();
                m_state = Rcpt;
            }
            else {
                // Authentication failed!
                logError("Authentication failed, msg: "+line);
                m_state = Close;
            }
            break;
        case Rcpt:
            if (code[0] == '2') {
                m_socket->write("rcpt to:<" + m_rcpt.toLatin1() + ">\r\n");
                m_socket->flush();
                m_state = Data;
            }
            else {
                logError("<mail from> was rejected by server, msg: "+line);
                m_state = Close;
            }
            break;
        case Data:
            if (code[0] == '2') {
                m_socket->write("data\r\n");
                m_socket->flush();
                m_state = Body;
            }
            else {
                logError("<Rcpt to> was rejected by server, msg: "+line);
                m_state = Close;
            }
            break;
        case Body:
            if (code[0] == '3') {
                m_socket->write(m_message + "\r\n.\r\n");
                m_socket->flush();
                m_state = Quit;
            }
            else {
                logError("<data> was rejected by server, msg: "+line);
                m_state = Close;
            }
            break;
        case Quit:
            if (code[0] == '2') {
                m_socket->write("QUIT\r\n");
                m_socket->flush();
                // here, we just close.
                m_state = Close;
            }
            else {
                logError("Message was rejected by the server, error: "+line);
                m_state = Close;
            }
            break;
        default:
            qDebug() << "Disconnecting from host";
            m_socket->disconnectFromHost();
            return;
        }
    }
}

QByteArray Smtp::encodeMimeHeader(const QString &key, const QString &value, QTextCodec *latin1, const QByteArray &prefix)
{
    QByteArray rv = "";
    QByteArray line = key.toLatin1() + ": ";
    if (!prefix.isEmpty()) line += prefix;
    if (!value.contains("=?") && latin1->canEncode(value)) {
        bool firstWord = true;
        foreach (const QByteArray& word, value.toLatin1().split(' ')) {
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
    }
    else {
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
    QByteArray address = determineFQDN();
    m_socket->write("ehlo " + address + "\r\n");
    m_socket->flush();
    m_state = EhloSent;
}

void Smtp::helo()
{
    QByteArray address = determineFQDN();
    m_socket->write("helo " + address + "\r\n");
    m_socket->flush();
    m_state = HeloSent;
}

void Smtp::parseEhloResponse(const QByteArray &code, bool continued, const QString &line)
{
    if (code != "250") {
        // Error
        if (m_state == EhloSent) {
            // try to send HELO instead of EHLO
            qDebug() << "EHLO failed, trying HELO instead...";
            helo();
        }
        else {
            // Both EHLO and HELO failed, chances are this is NOT
            // a SMTP server
            logError("Both EHLO and HELO failed, msg: "+line);
            m_state = Close;
        }
        return;
    }

    if (m_state != EhloGreetReceived) {
        if (!continued) {
            // greeting only, no extensions
            qDebug() << "No extension";
            m_state = EhloDone;
        }
        else {
            // greeting followed by extensions
            m_state = EhloGreetReceived;
            qDebug () << "EHLO greet received";
            return;
        }
    }
    else {
        qDebug() << Q_FUNC_INFO << "Supported extension: " << line.section(' ', 0, 0).toUpper()
                 << line.section(' ', 1);
        m_extensions[line.section(' ', 0, 0).toUpper()] = line.section(' ', 1);
        if (!continued)
            m_state = EhloDone;
    }

    if (m_state != EhloDone) return;

    if (m_extensions.contains("STARTTLS") && m_useSsl) {
        qDebug() << "STARTTLS";
        startTLS();
    }
    else {
        authenticate();
    }
}

void Smtp::authenticate()
{
    qDebug() << Q_FUNC_INFO;
    if (!m_extensions.contains("AUTH") ||
        m_username.isEmpty() || m_password.isEmpty()) {
        // Skip authentication
        qDebug() << "Skipping authentication...";
        m_state = Authenticated;
        // At this point the server will not send any response
        // So fill the buffer with a fake one to pass the tests
        // in readyRead()
        m_buffer.push_front("250 QBT FAKE RESPONSE\r\n");
        return;
    }
    // AUTH extension is supported, check which
    // authentication modes are supported by
    // the server
    QStringList auth = m_extensions["AUTH"].toUpper().split(' ', QString::SkipEmptyParts);
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
    }
    else {
        // Skip authentication
        logError("The SMTP server does not seem to support any of the authentications modes "
                 "we support [CRAM-MD5|PLAIN|LOGIN], skipping authentication, "
                 "knowing it is likely to fail... Server Auth Modes: "+auth.join("|"));
        m_state = Authenticated;
        // At this point the server will not send any response
        // So fill the buffer with a fake one to pass the tests
        // in readyRead()
        m_buffer.push_front("250 QBT FAKE RESPONSE\r\n");
    }
}

void Smtp::startTLS()
{
    qDebug() << Q_FUNC_INFO;
#ifndef QT_NO_OPENSSL
    m_socket->write("starttls\r\n");
    m_socket->flush();
    m_state = StartTLSSent;
#else
    authenticate();
#endif
}

void Smtp::authCramMD5(const QByteArray& challenge)
{
    if (m_state != AuthRequestSent) {
        m_socket->write("auth cram-md5\r\n");
        m_socket->flush();
        m_authType = AuthCramMD5;
        m_state = AuthRequestSent;
    }
    else {
        QByteArray response = m_username.toLatin1() + ' '
                + hmacMD5(m_password.toLatin1(), QByteArray::fromBase64(challenge)).toHex();
        m_socket->write(response.toBase64() + "\r\n");
        m_socket->flush();
        m_state = AuthSent;
    }
}

void Smtp::authPlain()
{
    if (m_state != AuthRequestSent) {
        m_authType = AuthPlain;
        // Prepare Auth string
        QByteArray auth;
        auth += '\0';
        auth += m_username.toLatin1();
        qDebug() << "username: " << m_username.toLatin1();
        auth += '\0';
        auth += m_password.toLatin1();
        qDebug() << "password: " << m_password.toLatin1();
        // Send it
        m_socket->write("auth plain "+ auth.toBase64() + "\r\n");
        m_socket->flush();
        m_state = AuthSent;
    }
}

void Smtp::authLogin()
{
    if ((m_state != AuthRequestSent) && (m_state != AuthUsernameSent)) {
        m_socket->write("auth login\r\n");
        m_socket->flush();
        m_authType = AuthLogin;
        m_state = AuthRequestSent;
    }
    else if (m_state == AuthRequestSent) {
        m_socket->write(m_username.toLatin1().toBase64() + "\r\n");
        m_socket->flush();
        m_state = AuthUsernameSent;
    }
    else {
        m_socket->write(m_password.toLatin1().toBase64() + "\r\n");
        m_socket->flush();
        m_state = AuthSent;
    }
}

void Smtp::logError(const QString &msg)
{
    qDebug() << "Email Notification Error:" << msg;
    Logger::instance()->addMessage(tr("Email Notification Error:") + " " + msg, Log::CRITICAL);
}
