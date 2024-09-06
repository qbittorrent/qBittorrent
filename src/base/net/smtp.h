/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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
 */

/*
 * This code is based on QxtSmtp from libqxt (http://libqxt.org)
 */

#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#ifndef QT_NO_OPENSSL
class QSslSocket;
#else
class QTcpSocket;
#endif

namespace Net
{
    class Smtp : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Smtp)

    public:
        Smtp(QObject *parent = nullptr);
        ~Smtp();

        void sendMail(const QString &from, const QString &to, const QString &subject, const QString &body);

    private slots:
        void readyRead();
        void error(QAbstractSocket::SocketError socketError);

    private:
        enum States
        {
            Rcpt,
            EhloSent,
            HeloSent,
            EhloDone,
            EhloGreetReceived,
            AuthRequestSent,
            AuthSent,
            AuthUsernameSent,
            Authenticated,
            StartTLSSent,
            Data,
            Init,
            Body,
            Quit,
            Close
        };

        enum AuthType
        {
            AuthPlain,
            AuthLogin,
            AuthCramMD5
        };

        QByteArray encodeMimeHeader(const QString &key, const QString &value, const QByteArray &prefix = {});
        void ehlo();
        void helo();
        void parseEhloResponse(const QByteArray &code, bool continued, const QString &line);
        void authenticate();
        void startTLS();
        void authCramMD5(const QByteArray &challenge = {});
        void authPlain();
        void authLogin();
        void logError(const QString &msg);
        QString getCurrentDateTime() const;

        QByteArray m_message;
#ifndef QT_NO_OPENSSL
        QSslSocket *m_socket = nullptr;
#else
        QTcpSocket *m_socket = nullptr;
#endif
        QString m_from;
        QString m_rcpt;
        QString m_response;
        int m_state = Init;
        QHash<QString, QString> m_extensions;
        QByteArray m_buffer;
        bool m_useSsl = false;
        AuthType m_authType = AuthPlain;
        QString m_username;
        QString m_password;
    };
}
