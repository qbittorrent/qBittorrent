/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QHTTPAUTHENTICATOR_P_H
#define QHTTPAUTHENTICATOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qhash.h>
#include <qbytearray.h>
#include <qstring.h>
#include <qvariant.h>
#include <QAuthenticator>

class QHttpResponseHeader;

class QHttpAuthenticatorPrivate;
class QUrl;

class QHttpAuthenticator
{
public:
    QHttpAuthenticator();
    ~QHttpAuthenticator();

    QHttpAuthenticator(const QHttpAuthenticator &other);
    QHttpAuthenticator &operator=(const QHttpAuthenticator &other);

    bool operator==(const QHttpAuthenticator &other) const;
    inline bool operator!=(const QHttpAuthenticator &other) const { return !operator==(other); }

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

    QString realm() const;

    QVariant option(const QString &opt) const;
    QVariantHash options() const;
    void setOption(const QString &opt, const QVariant &value);

    bool isNull() const;
    void detach();

    QHttpAuthenticator &operator=(const QAuthenticator& auth);
    QAuthenticator toQAuthenticator();
private:
    friend class QHttpAuthenticatorPrivate;
    QHttpAuthenticatorPrivate *d;
};

class QHttpAuthenticatorPrivate
{
public:
    enum Method { None, Basic, Plain, Login, Ntlm, CramMd5, DigestMd5 };
    QHttpAuthenticatorPrivate();

    QAtomicInt ref;
    QString user;
    QString extractedUser;
    QString password;
    QVariantHash options;
    Method method;
    QString realm;
    QByteArray challenge;
    bool hasFailed; //credentials have been tried but rejected by server.

    enum Phase {
        Start,
        Phase2,
        Done,
        Invalid
    };
    Phase phase;

    // digest specific
    QByteArray cnonce;
    int nonceCount;

    // ntlm specific
    QString workstation;
    QString userDomain;

    QByteArray calculateResponse(const QByteArray &method, const QByteArray &path);

    inline static QHttpAuthenticatorPrivate *getPrivate(QHttpAuthenticator &auth) { return auth.d; }
    inline static const QHttpAuthenticatorPrivate *getPrivate(const QHttpAuthenticator &auth) { return auth.d; }

    QByteArray digestMd5Response(const QByteArray &challenge, const QByteArray &method, const QByteArray &path);
    static QHash<QByteArray, QByteArray> parseDigestAuthenticationChallenge(const QByteArray &challenge);

#ifndef QT_NO_HTTP
    void parseHttpResponse(const QHttpResponseHeader &, bool isProxy);
#endif
    void parseHttpResponse(const QList<QPair<QByteArray, QByteArray> >&, bool isProxy);

};

#endif
