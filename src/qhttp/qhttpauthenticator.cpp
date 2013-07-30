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

#include "qhttpauthenticator_p.h"
#include <qdebug.h>
#include <qhash.h>
#include <qbytearray.h>
#include <qcryptographichash.h>
#include "qhttp.h"
#include <qiodevice.h>
#include <qdatastream.h>
#include <qendian.h>
#include <qstring.h>
#include <qdatetime.h>

//#define NTLMV1_CLIENT

QT_BEGIN_NAMESPACE

#ifdef NTLMV1_CLIENT
#include "../../3rdparty/des/des.cpp"
#endif

static QByteArray qNtlmPhase1();
static QByteArray qNtlmPhase3(QHttpAuthenticatorPrivate *ctx, const QByteArray& phase2data);

/*!
  \class QHttpAuthenticator
  \brief The QHttpAuthenticator class provides an authentication object.
  \since 4.3

  \reentrant
  \ingroup network
  \inmodule QtNetwork

  The QHttpAuthenticator class is usually used in the
  \l{QNetworkAccessManager::}{authenticationRequired()} and
  \l{QNetworkAccessManager::}{proxyAuthenticationRequired()} signals of QNetworkAccessManager and
  QAbstractSocket. The class provides a way to pass back the required
  authentication information to the socket when accessing services that
  require authentication.

  QHttpAuthenticator supports the following authentication methods:
  \list
    \o Basic
    \o NTLM version 1
    \o Digest-MD5
  \endlist

  Note that, in particular, NTLM version 2 is not supported.

  \section1 Options

  In addition to the username and password required for authentication, a
  QHttpAuthenticator object can also contain additional options. The
  options() function can be used to query incoming options sent by
  the server; the setOption() function can
  be used to set outgoing options, to be processed by the authenticator
  calculation. The options accepted and provided depend on the authentication
  type (see method()).

  The following tables list known incoming options as well as accepted
  outgoing options. The list of incoming options is not exhaustive, since
  servers may include additional information at any time. The list of
  outgoing options is exhaustive, however, and no unknown options will be
  treated or sent back to the server.

  \section2 Basic

  \table
    \header \o Option \o Direction \o Description
    \row \o \tt{realm} \o Incoming \o Contains the realm of the authentication, the same as realm()
  \endtable

  The Basic authentication mechanism supports no outgoing options.

  \section2 NTLM version 1

  The NTLM authentication mechanism currently supports no incoming or outgoing options.

  \section2 Digest-MD5

  \table
    \header \o Option \o Direction \o Description
    \row \o \tt{realm} \o Incoming \o Contains the realm of the authentication, the same as realm()
  \endtable

  The Digest-MD5 authentication mechanism supports no outgoing options.

  \sa QSslSocket
*/


/*!
  Constructs an empty authentication object
*/
QHttpAuthenticator::QHttpAuthenticator()
    : d(0)
{
}

/*!
  Destructs the object
*/
QHttpAuthenticator::~QHttpAuthenticator()
{
    if (d && !d->ref.deref())
        delete d;
}

/*!
    Constructs a copy of \a other.
*/
QHttpAuthenticator::QHttpAuthenticator(const QHttpAuthenticator &other)
    : d(other.d)
{
    if (d)
        d->ref.ref();
}

/*!
    Assigns the contents of \a other to this authenticator.
*/
QHttpAuthenticator &QHttpAuthenticator::operator=(const QHttpAuthenticator &other)
{
    if (d == other.d)
        return *this;

    if (d && !d->ref.deref())
        delete d;

    d = other.d;
    if (d)
        d->ref.ref();
    return *this;
}

/*!
    Returns true if this authenticator is identical to \a other; otherwise
    returns false.
*/
bool QHttpAuthenticator::operator==(const QHttpAuthenticator &other) const
{
    if (d == other.d)
        return true;
    return d->user == other.d->user
        && d->password == other.d->password
        && d->realm == other.d->realm
        && d->method == other.d->method
        && d->options == other.d->options;
}

QHttpAuthenticator &QHttpAuthenticator::operator=(const QAuthenticator& auth)
{
    detach();
    d->user = auth.user();
    d->password = auth.password();
    d->options = auth.options();
    return *this;
}

QAuthenticator QHttpAuthenticator::toQAuthenticator()
{
    QAuthenticator rv;
    rv.setUser(d->user);
    rv.setPassword(d->password);
    foreach (const QString& option, d->options.keys())
        rv.setOption(option, d->options.value(option));
    return rv;
}


/*!
    \fn bool QHttpAuthenticator::operator!=(const QHttpAuthenticator &other) const

    Returns true if this authenticator is different from \a other; otherwise
    returns false.
*/

/*!
  returns the user used for authentication.
*/
QString QHttpAuthenticator::user() const
{
    return d ? d->user : QString();
}

/*!
  Sets the \a user used for authentication.

  \sa QNetworkAccessManager::authenticationRequired()
*/
void QHttpAuthenticator::setUser(const QString &user)
{
    detach();
    int separatorPosn = 0;

    switch(d->method) {
    case QHttpAuthenticatorPrivate::Ntlm:
        if((separatorPosn = user.indexOf(QLatin1String("\\"))) != -1) {
            //domain name is present
            d->realm.clear();
            d->userDomain = user.left(separatorPosn);
            d->extractedUser = user.mid(separatorPosn + 1);
            d->user = user;
        } else {
            d->extractedUser = user;
            d->user = user;
	    d->realm.clear();
            d->userDomain.clear();
        }
        break;
    default:
        d->user = user;
        d->userDomain.clear();
        break;
    }
}

/*!
  returns the password used for authentication.
*/
QString QHttpAuthenticator::password() const
{
    return d ? d->password : QString();
}

/*!
  Sets the \a password used for authentication.

  \sa QNetworkAccessManager::authenticationRequired()
*/
void QHttpAuthenticator::setPassword(const QString &password)
{
    detach();
    d->password = password;
}

/*!
  \internal
*/
void QHttpAuthenticator::detach()
{
    if (!d) {
        d = new QHttpAuthenticatorPrivate;
        d->ref.store(1);
        return;
    }

    qAtomicDetach(d);
    d->phase = QHttpAuthenticatorPrivate::Start;
}

/*!
  returns the realm requiring authentication.
*/
QString QHttpAuthenticator::realm() const
{
    return d ? d->realm : QString();
}

/*!
    \since 4.7
    Returns the value related to option \a opt if it was set by the server.
    See \l{QHttpAuthenticator#Options} for more information on incoming options.
    If option \a opt isn't found, an invalid QVariant will be returned.

    \sa options(), QHttpAuthenticator#Options
*/
QVariant QHttpAuthenticator::option(const QString &opt) const
{
    return d ? d->options.value(opt) : QVariant();
}

/*!
    \since 4.7
    Returns all incoming options set in this QHttpAuthenticator object by parsing
    the server reply. See \l{QHttpAuthenticator#Options} for more information
    on incoming options.

    \sa option(), QHttpAuthenticator#Options
*/
QVariantHash QHttpAuthenticator::options() const
{
    return d ? d->options : QVariantHash();
}

/*!
    \since 4.7

    Sets the outgoing option \a opt to value \a value.
    See \l{QHttpAuthenticator#Options} for more information on outgoing options.

    \sa options(), option(), QHttpAuthenticator#Options
*/
void QHttpAuthenticator::setOption(const QString &opt, const QVariant &value)
{
    detach();
    d->options.insert(opt, value);
}


/*!
    Returns true if the authenticator is null.
*/
bool QHttpAuthenticator::isNull() const
{
    return !d;
}

QHttpAuthenticatorPrivate::QHttpAuthenticatorPrivate()
    : ref(0)
    , method(None)
    , hasFailed(false)
    , phase(Start)
    , nonceCount(0)
{
    cnonce = QCryptographicHash::hash(QByteArray::number(qrand(), 16) + QByteArray::number(qrand(), 16),
                                      QCryptographicHash::Md5).toHex();
    nonceCount = 0;
}

#ifndef QT_NO_HTTP
void QHttpAuthenticatorPrivate::parseHttpResponse(const QHttpResponseHeader &header, bool isProxy)
{
    const QList<QPair<QString, QString> > values = header.values();
    QList<QPair<QByteArray, QByteArray> > rawValues;

    QList<QPair<QString, QString> >::const_iterator it, end;
    for (it = values.constBegin(), end = values.constEnd(); it != end; ++it)
        rawValues.append(qMakePair(it->first.toLatin1(), it->second.toUtf8()));

    // continue in byte array form
    parseHttpResponse(rawValues, isProxy);
}
#endif

void QHttpAuthenticatorPrivate::parseHttpResponse(const QList<QPair<QByteArray, QByteArray> > &values, bool isProxy)
{
    const char *search = isProxy ? "proxy-authenticate" : "www-authenticate";

    method = None;
    /*
      Fun from the HTTP 1.1 specs, that we currently ignore:

      User agents are advised to take special care in parsing the WWW-
      Authenticate field value as it might contain more than one challenge,
      or if more than one WWW-Authenticate header field is provided, the
      contents of a challenge itself can contain a comma-separated list of
      authentication parameters.
    */

    QByteArray headerVal;
    for (int i = 0; i < values.size(); ++i) {
        const QPair<QByteArray, QByteArray> &current = values.at(i);
        if (current.first.toLower() != search)
            continue;
        QByteArray str = current.second.toLower();
        if (method < Basic && str.startsWith("basic")) {
            method = Basic;
            headerVal = current.second.mid(6);
        } else if (method < Ntlm && str.startsWith("ntlm")) {
            method = Ntlm;
            headerVal = current.second.mid(5);
        } else if (method < DigestMd5 && str.startsWith("digest")) {
            method = DigestMd5;
            headerVal = current.second.mid(7);
        }
    }

    challenge = headerVal.trimmed();
    QHash<QByteArray, QByteArray> options = parseDigestAuthenticationChallenge(challenge);

    switch(method) {
    case Basic:
        this->options[QLatin1String("realm")] = realm = QString::fromLatin1(options.value("realm"));
        if (user.isEmpty() && password.isEmpty())
            phase = Done;
        break;
    case Ntlm:
        // #### extract from header
        break;
    case DigestMd5: {
        this->options[QLatin1String("realm")] = realm = QString::fromLatin1(options.value("realm"));
        if (options.value("stale").toLower() == "true")
            phase = Start;
        if (user.isEmpty() && password.isEmpty())
            phase = Done;
        break;
    }
    default:
        realm.clear();
        challenge = QByteArray();
        phase = Invalid;
    }
}

QByteArray QHttpAuthenticatorPrivate::calculateResponse(const QByteArray &requestMethod, const QByteArray &path)
{
    QByteArray response;
    const char *methodString = 0;
    switch(method) {
    case QHttpAuthenticatorPrivate::None:
        methodString = "";
        phase = Done;
        break;
    case QHttpAuthenticatorPrivate::Plain:
        response = '\0' + user.toUtf8() + '\0' + password.toUtf8();
        phase = Done;
        break;
    case QHttpAuthenticatorPrivate::Basic:
        methodString = "Basic ";
        response = user.toLatin1() + ':' + password.toLatin1();
        response = response.toBase64();
        phase = Done;
        break;
    case QHttpAuthenticatorPrivate::Login:
        if (challenge.contains("VXNlciBOYW1lAA==")) {
            response = user.toUtf8().toBase64();
            phase = Phase2;
        } else if (challenge.contains("UGFzc3dvcmQA")) {
            response = password.toUtf8().toBase64();
            phase = Done;
        }
        break;
    case QHttpAuthenticatorPrivate::CramMd5:
        break;
    case QHttpAuthenticatorPrivate::DigestMd5:
        methodString = "Digest ";
        response = digestMd5Response(challenge, requestMethod, path);
        phase = Done;
        break;
    case QHttpAuthenticatorPrivate::Ntlm:
        methodString = "NTLM ";
        if (challenge.isEmpty()) {
            response = qNtlmPhase1().toBase64();
            if (user.isEmpty())
                phase = Done;
            else
                phase = Phase2;
        } else {
            response = qNtlmPhase3(this, QByteArray::fromBase64(challenge)).toBase64();
            phase = Done;
        }

        break;
    }
    return QByteArray(methodString) + response;
}


// ---------------------------- Digest Md5 code ----------------------------------------

QHash<QByteArray, QByteArray> QHttpAuthenticatorPrivate::parseDigestAuthenticationChallenge(const QByteArray &challenge)
{
    QHash<QByteArray, QByteArray> options;
    // parse the challenge
    const char *d = challenge.constData();
    const char *end = d + challenge.length();
    while (d < end) {
        while (d < end && (*d == ' ' || *d == '\n' || *d == '\r'))
            ++d;
        const char *start = d;
        while (d < end && *d != '=')
            ++d;
        QByteArray key = QByteArray(start, d - start);
        ++d;
        if (d >= end)
            break;
        bool quote = (*d == '"');
        if (quote)
            ++d;
        if (d >= end)
            break;
        start = d;
        QByteArray value;
        while (d < end) {
            bool backslash = false;
            if (*d == '\\' && d < end - 1) {
                ++d;
                backslash = true;
            }
            if (!backslash) {
                if (quote) {
                    if (*d == '"')
                        break;
                } else {
                    if (*d == ',')
                        break;
                }
            }
            value += *d;
            ++d;
        }
        while (d < end && *d != ',')
            ++d;
        ++d;
        options[key] = value;
    }

    QByteArray qop = options.value("qop");
    if (!qop.isEmpty()) {
        QList<QByteArray> qopoptions = qop.split(',');
        if (!qopoptions.contains("auth"))
            return QHash<QByteArray, QByteArray>();
        // #### can't do auth-int currently
//         if (qop.contains("auth-int"))
//             qop = "auth-int";
//         else if (qop.contains("auth"))
//             qop = "auth";
//         else
//             qop = QByteArray();
        options["qop"] = "auth";
    }

    return options;
}

/*
  Digest MD5 implementation

  Code taken from RFC 2617

  Currently we don't support the full SASL authentication mechanism (which includes cyphers)
*/


/* calculate request-digest/response-digest as per HTTP Digest spec */
static QByteArray digestMd5ResponseHelper(
    const QByteArray &alg,
    const QByteArray &userName,
    const QByteArray &realm,
    const QByteArray &password,
    const QByteArray &nonce,       /* nonce from server */
    const QByteArray &nonceCount,  /* 8 hex digits */
    const QByteArray &cNonce,      /* client nonce */
    const QByteArray &qop,         /* qop-value: "", "auth", "auth-int" */
    const QByteArray &method,      /* method from the request */
    const QByteArray &digestUri,   /* requested URL */
    const QByteArray &hEntity       /* H(entity body) if qop="auth-int" */
    )
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(userName);
    hash.addData(":", 1);
    hash.addData(realm);
    hash.addData(":", 1);
    hash.addData(password);
    QByteArray ha1 = hash.result();
    if (alg.toLower() == "md5-sess") {
        hash.reset();
        // RFC 2617 contains an error, it was:
        // hash.addData(ha1);
        // but according to the errata page at http://www.rfc-editor.org/errata_list.php, ID 1649, it
        // must be the following line:
        hash.addData(ha1.toHex());
        hash.addData(":", 1);
        hash.addData(nonce);
        hash.addData(":", 1);
        hash.addData(cNonce);
        ha1 = hash.result();
    };
    ha1 = ha1.toHex();

    // calculate H(A2)
    hash.reset();
    hash.addData(method);
    hash.addData(":", 1);
    hash.addData(digestUri);
    if (qop.toLower() == "auth-int") {
        hash.addData(":", 1);
        hash.addData(hEntity);
    }
    QByteArray ha2hex = hash.result().toHex();

    // calculate response
    hash.reset();
    hash.addData(ha1);
    hash.addData(":", 1);
    hash.addData(nonce);
    hash.addData(":", 1);
    if (!qop.isNull()) {
        hash.addData(nonceCount);
        hash.addData(":", 1);
        hash.addData(cNonce);
        hash.addData(":", 1);
        hash.addData(qop);
        hash.addData(":", 1);
    }
    hash.addData(ha2hex);
    return hash.result().toHex();
}

QByteArray QHttpAuthenticatorPrivate::digestMd5Response(const QByteArray &challenge, const QByteArray &method, const QByteArray &path)
{
    QHash<QByteArray,QByteArray> options = parseDigestAuthenticationChallenge(challenge);

    ++nonceCount;
    QByteArray nonceCountString = QByteArray::number(nonceCount, 16);
    while (nonceCountString.length() < 8)
        nonceCountString.prepend('0');

    QByteArray nonce = options.value("nonce");
    QByteArray opaque = options.value("opaque");
    QByteArray qop = options.value("qop");

//    qDebug() << "calculating digest: method=" << method << "path=" << path;
    QByteArray response = digestMd5ResponseHelper(options.value("algorithm"), user.toLatin1(),
                                              realm.toLatin1(), password.toLatin1(),
                                              nonce, nonceCountString,
                                              cnonce, qop, method,
                                              path, QByteArray());


    QByteArray credentials;
    credentials += "username=\"" + user.toLatin1() + "\", ";
    credentials += "realm=\"" + realm.toLatin1() + "\", ";
    credentials += "nonce=\"" + nonce + "\", ";
    credentials += "uri=\"" + path + "\", ";
    if (!opaque.isEmpty())
        credentials += "opaque=\"" + opaque + "\", ";
    credentials += "response=\"" + response + '\"';
    if (!options.value("algorithm").isEmpty())
        credentials += ", algorithm=" + options.value("algorithm");
    if (!options.value("qop").isEmpty()) {
        credentials += ", qop=" + qop + ", ";
        credentials += "nc=" + nonceCountString + ", ";
        credentials += "cnonce=\"" + cnonce + '\"';
    }

    return credentials;
}

// ---------------------------- Digest Md5 code ----------------------------------------



/*
 * NTLM message flags.
 *
 * Copyright (c) 2004 Andrey Panin <pazke@donpac.ru>
 *
 * This software is released under the MIT license.
 */

/*
 * Indicates that Unicode strings are supported for use in security
 * buffer data.
 */
#define NTLMSSP_NEGOTIATE_UNICODE 0x00000001

/*
 * Indicates that OEM strings are supported for use in security buffer data.
 */
#define NTLMSSP_NEGOTIATE_OEM 0x00000002

/*
 * Requests that the server's authentication realm be included in the
 * Type 2 message.
 */
#define NTLMSSP_REQUEST_TARGET 0x00000004

/*
 * Specifies that authenticated communication between the client and server
 * should carry a digital signature (message integrity).
 */
#define NTLMSSP_NEGOTIATE_SIGN 0x00000010

/*
 * Specifies that authenticated communication between the client and server
 * should be encrypted (message confidentiality).
 */
#define NTLMSSP_NEGOTIATE_SEAL 0x00000020

/*
 * Indicates that datagram authentication is being used.
 */
#define NTLMSSP_NEGOTIATE_DATAGRAM 0x00000040

/*
 * Indicates that the LAN Manager session key should be
 * used for signing and sealing authenticated communications.
 */
#define NTLMSSP_NEGOTIATE_LM_KEY 0x00000080

/*
 * Indicates that NTLM authentication is being used.
 */
#define NTLMSSP_NEGOTIATE_NTLM 0x00000200

/*
 * Sent by the client in the Type 1 message to indicate that the name of the
 * domain in which the client workstation has membership is included in the
 * message. This is used by the server to determine whether the client is
 * eligible for local authentication.
 */
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED 0x00001000

/*
 * Sent by the client in the Type 1 message to indicate that the client
 * workstation's name is included in the message. This is used by the server
 * to determine whether the client is eligible for local authentication.
 */
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED 0x00002000

/*
 * Sent by the server to indicate that the server and client are on the same
 * machine. Implies that the client may use the established local credentials
 * for authentication instead of calculating a response to the challenge.
 */
#define NTLMSSP_NEGOTIATE_LOCAL_CALL 0x00004000

/*
 * Indicates that authenticated communication between the client and server
 * should be signed with a "dummy" signature.
 */
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN 0x00008000

/*
 * Sent by the server in the Type 2 message to indicate that the target
 * authentication realm is a domain.
 */
#define NTLMSSP_TARGET_TYPE_DOMAIN 0x00010000

/*
 * Sent by the server in the Type 2 message to indicate that the target
 * authentication realm is a server.
 */
#define NTLMSSP_TARGET_TYPE_SERVER 0x00020000

/*
 * Sent by the server in the Type 2 message to indicate that the target
 * authentication realm is a share. Presumably, this is for share-level
 * authentication. Usage is unclear.
 */
#define NTLMSSP_TARGET_TYPE_SHARE 0x00040000

/*
 * Indicates that the NTLM2 signing and sealing scheme should be used for
 * protecting authenticated communications. Note that this refers to a
 * particular session security scheme, and is not related to the use of
 * NTLMv2 authentication.
 */
#define NTLMSSP_NEGOTIATE_NTLM2 0x00080000

/*
 * Sent by the server in the Type 2 message to indicate that it is including
 * a Target Information block in the message. The Target Information block
 * is used in the calculation of the NTLMv2 response.
 */
#define NTLMSSP_NEGOTIATE_TARGET_INFO 0x00800000

/*
 * Indicates that 128-bit encryption is supported.
 */
#define NTLMSSP_NEGOTIATE_128 0x20000000

/*
 * Indicates that the client will provide an encrypted master session key in
 * the "Session Key" field of the Type 3 message. This is used in signing and
 * sealing, and is RC4-encrypted using the previous session key as the
 * encryption key.
 */
#define NTLMSSP_NEGOTIATE_KEY_EXCHANGE 0x40000000

/*
 * Indicates that 56-bit encryption is supported.
 */
#define NTLMSSP_NEGOTIATE_56 0x80000000

/*
 * AvId values
 */
#define AVTIMESTAMP 7

//#define NTLMV1_CLIENT


//************************Global variables***************************

const int blockSize = 64; //As per RFC2104 Block-size is 512 bits
const int nDigestLen = 16; //Trunctaion Length of the Hmac-Md5 digest
const quint8 respversion = 1;
const quint8 hirespversion = 1;

/* usage:
   // fill up ctx with what we know.
   QByteArray response = qNtlmPhase1(ctx);
   // send response (b64 encoded??)
   // get response from server (b64 decode?)
   Phase2Block pb;
   qNtlmDecodePhase2(response, pb);
   response = qNtlmPhase3(ctx, pb);
   // send response (b64 encoded??)
*/

/*
   TODO:
    - Fix unicode handling
    - add v2 handling
*/

class QNtlmBuffer {
public:
    QNtlmBuffer() : len(0), maxLen(0), offset(0) {}
    quint16 len;
    quint16 maxLen;
    quint32 offset;
    enum { Size = 8 };
};

class QNtlmPhase1BlockBase
{
public:
    char magic[8];
    quint32 type;
    quint32 flags;
    QNtlmBuffer domain;
    QNtlmBuffer workstation;
    enum { Size = 32 };
};

// ################# check paddings
class QNtlmPhase2BlockBase
{
public:
    char magic[8];
    quint32 type;
    QNtlmBuffer targetName;
    quint32 flags;
    unsigned char challenge[8];
    quint32 context[2];
    QNtlmBuffer targetInfo;
    enum { Size = 48 };
};

class QNtlmPhase3BlockBase {
public:
    char magic[8];
    quint32 type;
    QNtlmBuffer lmResponse;
    QNtlmBuffer ntlmResponse;
    QNtlmBuffer domain;
    QNtlmBuffer user;
    QNtlmBuffer workstation;
    QNtlmBuffer sessionKey;
    quint32 flags;
    enum { Size = 64 };
};

static void qStreamNtlmBuffer(QDataStream& ds, const QByteArray& s)
{
    ds.writeRawData(s.constData(), s.size());
}


static void qStreamNtlmString(QDataStream& ds, const QString& s, bool unicode)
{
    if (!unicode) {
        qStreamNtlmBuffer(ds, s.toLatin1());
        return;
    }
    const ushort *d = s.utf16();
    for (int i = 0; i < s.length(); ++i)
        ds << d[i];
}



static int qEncodeNtlmBuffer(QNtlmBuffer& buf, int offset, const QByteArray& s)
{
    buf.len = s.size();
    buf.maxLen = buf.len;
    buf.offset = (offset + 1) & ~1;
    return buf.offset + buf.len;
}


static int qEncodeNtlmString(QNtlmBuffer& buf, int offset, const QString& s, bool unicode)
{
    if (!unicode)
        return qEncodeNtlmBuffer(buf, offset, s.toLatin1());
    buf.len = 2 * s.length();
    buf.maxLen = buf.len;
    buf.offset = (offset + 1) & ~1;
    return buf.offset + buf.len;
}


static QDataStream& operator<<(QDataStream& s, const QNtlmBuffer& b)
{
    s << b.len << b.maxLen << b.offset;
    return s;
}

static QDataStream& operator>>(QDataStream& s, QNtlmBuffer& b)
{
    s >> b.len >> b.maxLen >> b.offset;
    return s;
}


class QNtlmPhase1Block : public QNtlmPhase1BlockBase
{  // request
public:
    QNtlmPhase1Block() {
        qstrncpy(magic, "NTLMSSP", 8);
        type = 1;
        flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_REQUEST_TARGET;
    }

    // extracted
    QString domainStr, workstationStr;
};


class QNtlmPhase2Block : public QNtlmPhase2BlockBase
{  // challenge
public:
    QNtlmPhase2Block() {
        magic[0] = 0;
        type = 0xffffffff;
    }

    // extracted
    QString targetNameStr, targetInfoStr;
    QByteArray targetInfoBuff;
};



class QNtlmPhase3Block : public QNtlmPhase3BlockBase {  // response
public:
    QNtlmPhase3Block() {
        qstrncpy(magic, "NTLMSSP", 8);
        type = 3;
        flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_TARGET_INFO;
    }

    // extracted
    QByteArray lmResponseBuf, ntlmResponseBuf;
    QString domainStr, userStr, workstationStr, sessionKeyStr;
    QByteArray v2Hash;
};


static QDataStream& operator<<(QDataStream& s, const QNtlmPhase1Block& b) {
    bool unicode = (b.flags & NTLMSSP_NEGOTIATE_UNICODE);

    s.writeRawData(b.magic, sizeof(b.magic));
    s << b.type;
    s << b.flags;
    s << b.domain;
    s << b.workstation;
    if (!b.domainStr.isEmpty())
        qStreamNtlmString(s, b.domainStr, unicode);
    if (!b.workstationStr.isEmpty())
        qStreamNtlmString(s, b.workstationStr, unicode);
    return s;
}


static QDataStream& operator<<(QDataStream& s, const QNtlmPhase3Block& b) {
    bool unicode = (b.flags & NTLMSSP_NEGOTIATE_UNICODE);
    s.writeRawData(b.magic, sizeof(b.magic));
    s << b.type;
    s << b.lmResponse;
    s << b.ntlmResponse;
    s << b.domain;
    s << b.user;
    s << b.workstation;
    s << b.sessionKey;
    s << b.flags;

    if (!b.domainStr.isEmpty())
        qStreamNtlmString(s, b.domainStr, unicode);

    qStreamNtlmString(s, b.userStr, unicode);

    if (!b.workstationStr.isEmpty())
        qStreamNtlmString(s, b.workstationStr, unicode);

    // Send auth info
    qStreamNtlmBuffer(s, b.lmResponseBuf);
    qStreamNtlmBuffer(s, b.ntlmResponseBuf);


    return s;
}


static QByteArray qNtlmPhase1()
{
    QByteArray rc;
    QDataStream ds(&rc, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    QNtlmPhase1Block pb;
    ds << pb;
    return rc;
}


static QByteArray qStringAsUcs2Le(const QString& src)
{
    QByteArray rc(2*src.length(), 0);
    const unsigned short *s = src.utf16();
    unsigned short *d = (unsigned short*)rc.data();
    for (int i = 0; i < src.length(); ++i) {
        d[i] = qToLittleEndian(s[i]);
    }
    return rc;
}


static QString qStringFromUcs2Le(const QByteArray& src)
{
    Q_ASSERT(src.size() % 2 == 0);
    unsigned short *d = (unsigned short*)src.data();
    for (int i = 0; i < src.length() / 2; ++i) {
        d[i] = qFromLittleEndian(d[i]);
    }
    return QString((const QChar *)src.data(), src.size()/2);
}

#ifdef NTLMV1_CLIENT
static QByteArray qEncodeNtlmResponse(const QHttpAuthenticatorPrivate *ctx, const QNtlmPhase2Block& ch)
{
    QCryptographicHash md4(QCryptographicHash::Md4);
    QByteArray asUcs2Le = qStringAsUcs2Le(ctx->password);
    md4.addData(asUcs2Le.data(), asUcs2Le.size());

    unsigned char md4hash[22];
    memset(md4hash, 0, sizeof(md4hash));
    QByteArray hash = md4.result();
    Q_ASSERT(hash.size() == 16);
    memcpy(md4hash, hash.constData(), 16);

    QByteArray rc(24, 0);
    deshash((unsigned char *)rc.data(), md4hash, (unsigned char *)ch.challenge);
    deshash((unsigned char *)rc.data() + 8, md4hash + 7, (unsigned char *)ch.challenge);
    deshash((unsigned char *)rc.data() + 16, md4hash + 14, (unsigned char *)ch.challenge);

    hash.fill(0);
    return rc;
}


static QByteArray qEncodeLmResponse(const QHttpAuthenticatorPrivate *ctx, const QNtlmPhase2Block& ch)
{
    QByteArray hash(21, 0);
    QByteArray key(14, 0);
    qstrncpy(key.data(), ctx->password.toUpper().toLatin1(), 14);
    const char *block = "KGS!@#$%";

    deshash((unsigned char *)hash.data(), (unsigned char *)key.data(), (unsigned char *)block);
    deshash((unsigned char *)hash.data() + 8, (unsigned char *)key.data() + 7, (unsigned char *)block);
    key.fill(0);

    QByteArray rc(24, 0);
    deshash((unsigned char *)rc.data(), (unsigned char *)hash.data(), ch.challenge);
    deshash((unsigned char *)rc.data() + 8, (unsigned char *)hash.data() + 7, ch.challenge);
    deshash((unsigned char *)rc.data() + 16, (unsigned char *)hash.data() + 14, ch.challenge);

    hash.fill(0);
    return rc;
}
#endif

/*********************************************************************
* Function Name: qEncodeHmacMd5
* Params:
*    key:   Type - QByteArray
*         - It is the Authentication key
*    message:   Type - QByteArray
*         - This is the actual message which will be encoded
*           using HMacMd5 hash algorithm
*
* Return Value:
*    hmacDigest:   Type - QByteArray
*
* Description:
*    This function will be used to encode the input message using
*    HMacMd5 hash algorithm.
*
*    As per the RFC2104 the HMacMd5 algorithm can be specified
*        ---------------------------------------
*         MD5(K XOR opad, MD5(K XOR ipad, text))
*        ---------------------------------------
*
*********************************************************************/
QByteArray qEncodeHmacMd5(QByteArray &key, const QByteArray &message)
{
    Q_ASSERT_X(!(message.isEmpty()),"qEncodeHmacMd5", "Empty message check");
    Q_ASSERT_X(!(key.isEmpty()),"qEncodeHmacMd5", "Empty key check");

    QCryptographicHash hash(QCryptographicHash::Md5);
    QByteArray hMsg;

    QByteArray iKeyPad(blockSize, 0x36);
    QByteArray oKeyPad(blockSize, 0x5c);

    hash.reset();
    // Adjust the key length to blockSize

    if(blockSize < key.length()) {
        hash.addData(key);
        key = hash.result(); //MD5 will always return 16 bytes length output
    }

    //Key will be <= 16 or 20 bytes as hash function (MD5 or SHA hash algorithms)
    //key size can be max of Block size only
    key = key.leftJustified(blockSize,0,true);

    //iKeyPad, oKeyPad and key are all of same size "blockSize"

    //xor of iKeyPad with Key and store the result into iKeyPad
    for(int i = 0; i<key.size();i++) {
        iKeyPad[i] = key[i]^iKeyPad[i];
    }

    //xor of oKeyPad with Key and store the result into oKeyPad
    for(int i = 0; i<key.size();i++) {
        oKeyPad[i] = key[i]^oKeyPad[i];
    }

    iKeyPad.append(message); // (K0 xor ipad) || text

    hash.reset();
    hash.addData(iKeyPad);
    hMsg = hash.result();
                    //Digest gen after pass-1: H((K0 xor ipad)||text)

    QByteArray hmacDigest;
    oKeyPad.append(hMsg);
    hash.reset();
    hash.addData(oKeyPad);
    hmacDigest = hash.result();
                    // H((K0 xor opad )|| H((K0 xor ipad) || text))

    /*hmacDigest should not be less than half the length of the HMAC output
      (to match the birthday attack bound) and not less than 80 bits
      (a suitable lower bound on the number of bits that need to be
      predicted by an attacker).
      Refer RFC 2104 for more details on truncation part */

    /*MD5 hash always returns 16 byte digest only and HMAC-MD5 spec
      (RFC 2104) also says digest length should be 16 bytes*/
    return hmacDigest;
}

static QByteArray qCreatev2Hash(const QHttpAuthenticatorPrivate *ctx,
                                QNtlmPhase3Block *phase3)
{
    Q_ASSERT(phase3 != 0);
    // since v2 Hash is need for both NTLMv2 and LMv2 it is calculated
    // only once and stored and reused
    if(phase3->v2Hash.size() == 0) {
        QCryptographicHash md4(QCryptographicHash::Md4);
        QByteArray passUnicode = qStringAsUcs2Le(ctx->password);
        md4.addData(passUnicode.data(), passUnicode.size());

        QByteArray hashKey = md4.result();
        Q_ASSERT(hashKey.size() == 16);
        // Assuming the user and domain is always unicode in challenge
        QByteArray message =
                qStringAsUcs2Le(ctx->extractedUser.toUpper()) +
                qStringAsUcs2Le(phase3->domainStr);

        phase3->v2Hash = qEncodeHmacMd5(hashKey, message);
    }
    return phase3->v2Hash;
}

static QByteArray clientChallenge(const QHttpAuthenticatorPrivate *ctx)
{
    Q_ASSERT(ctx->cnonce.size() >= 8);
    QByteArray clientCh = ctx->cnonce.right(8);
    return clientCh;
}

// caller has to ensure a valid targetInfoBuff
static QByteArray qExtractServerTime(const QByteArray& targetInfoBuff)
{
    QByteArray timeArray;
    QDataStream ds(targetInfoBuff);
    ds.setByteOrder(QDataStream::LittleEndian);

    quint16 avId;
    quint16 avLen;

    ds >> avId;
    ds >> avLen;
    while(avId != 0) {
        if(avId == AVTIMESTAMP) {
            timeArray.resize(avLen);
            //avLen size of QByteArray is allocated
            ds.readRawData(timeArray.data(), avLen);
            break;
        }
        ds.skipRawData(avLen);
        ds >> avId;
        ds >> avLen;
    }
    return timeArray;
}

static QByteArray qEncodeNtlmv2Response(const QHttpAuthenticatorPrivate *ctx,
                                        const QNtlmPhase2Block& ch,
                                        QNtlmPhase3Block *phase3)
{
    Q_ASSERT(phase3 != 0);
    // return value stored in phase3
    qCreatev2Hash(ctx, phase3);

    QByteArray temp;
    QDataStream ds(&temp, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << respversion;
    ds << hirespversion;

    //Reserved
    QByteArray reserved1(6, 0);
    ds.writeRawData(reserved1.constData(), reserved1.size());

    quint64 time = 0;
    QByteArray timeArray;

    if(ch.targetInfo.len)
    {
        timeArray = qExtractServerTime(ch.targetInfoBuff);
    }

    //if server sends time, use it instead of current time
    if(timeArray.size()) {
        ds.writeRawData(timeArray.constData(), timeArray.size());
    } else {
        QDateTime currentTime(QDate::currentDate(),
                              QTime::currentTime(), Qt::UTC);

        // number of seconds between 1601 and epoc(1970)
        // 369 years, 89 leap years
        // ((369 * 365) + 89) * 24 * 3600 = 11644473600

        time = Q_UINT64_C(currentTime.toTime_t() + 11644473600);

        // represented as 100 nano seconds
        time = Q_UINT64_C(time * 10000000);
        ds << time;
    }

    //8 byte client challenge
    QByteArray clientCh = clientChallenge(ctx);
    ds.writeRawData(clientCh.constData(), clientCh.size());

    //Reserved
    QByteArray reserved2(4, 0);
    ds.writeRawData(reserved2.constData(), reserved2.size());

    if (ch.targetInfo.len > 0) {
        ds.writeRawData(ch.targetInfoBuff.constData(),
                        ch.targetInfoBuff.size());
    }

    //Reserved
    QByteArray reserved3(4, 0);
    ds.writeRawData(reserved3.constData(), reserved3.size());

    QByteArray message((const char*)ch.challenge, sizeof(ch.challenge));
    message.append(temp);

    QByteArray ntChallengeResp = qEncodeHmacMd5(phase3->v2Hash, message);
    ntChallengeResp.append(temp);

    return ntChallengeResp;
}

static QByteArray qEncodeLmv2Response(const QHttpAuthenticatorPrivate *ctx,
                                      const QNtlmPhase2Block& ch,
                                      QNtlmPhase3Block *phase3)
{
    Q_ASSERT(phase3 != 0);
    // return value stored in phase3
    qCreatev2Hash(ctx, phase3);

    QByteArray message((const char*)ch.challenge, sizeof(ch.challenge));
    QByteArray clientCh = clientChallenge(ctx);

    message.append(clientCh);

    QByteArray lmChallengeResp = qEncodeHmacMd5(phase3->v2Hash, message);
    lmChallengeResp.append(clientCh);

    return lmChallengeResp;
}

static bool qNtlmDecodePhase2(const QByteArray& data, QNtlmPhase2Block& ch)
{
    Q_ASSERT(QNtlmPhase2BlockBase::Size == sizeof(QNtlmPhase2BlockBase));
    if (data.size() < QNtlmPhase2BlockBase::Size)
        return false;


    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    if (ds.readRawData(ch.magic, 8) < 8)
        return false;
    if (strncmp(ch.magic, "NTLMSSP", 8) != 0)
        return false;

    ds >> ch.type;
    if (ch.type != 2)
        return false;

    ds >> ch.targetName;
    ds >> ch.flags;
    if (ds.readRawData((char *)ch.challenge, 8) < 8)
        return false;
    ds >> ch.context[0] >> ch.context[1];
    ds >> ch.targetInfo;

    if (ch.targetName.len > 0) {
        if (ch.targetName.len + ch.targetName.offset >= (unsigned)data.size())
            return false;

        ch.targetNameStr = qStringFromUcs2Le(data.mid(ch.targetName.offset, ch.targetName.len));
    }

    if (ch.targetInfo.len > 0) {
        if (ch.targetInfo.len + ch.targetInfo.offset > (unsigned)data.size())
            return false;

        ch.targetInfoBuff = data.mid(ch.targetInfo.offset, ch.targetInfo.len);
    }

    return true;
}


static QByteArray qNtlmPhase3(QHttpAuthenticatorPrivate *ctx, const QByteArray& phase2data)
{
    QNtlmPhase2Block ch;
    if (!qNtlmDecodePhase2(phase2data, ch))
        return QByteArray();

    QByteArray rc;
    QDataStream ds(&rc, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    QNtlmPhase3Block pb;

    bool unicode = ch.flags & NTLMSSP_NEGOTIATE_UNICODE;

    pb.flags = NTLMSSP_NEGOTIATE_NTLM;
    if (unicode)
        pb.flags |= NTLMSSP_NEGOTIATE_UNICODE;
    else
        pb.flags |= NTLMSSP_NEGOTIATE_OEM;


    int offset = QNtlmPhase3BlockBase::Size;
    Q_ASSERT(QNtlmPhase3BlockBase::Size == sizeof(QNtlmPhase3BlockBase));

    // for kerberos style user@domain logins, NTLM domain string should be left empty
    if (ctx->userDomain.isEmpty() && !ctx->extractedUser.contains(QLatin1Char('@'))) {
        offset = qEncodeNtlmString(pb.domain, offset, ch.targetNameStr, unicode);
        pb.domainStr = ch.targetNameStr;
    } else {
        offset = qEncodeNtlmString(pb.domain, offset, ctx->userDomain, unicode);
        pb.domainStr = ctx->userDomain;
    }

    offset = qEncodeNtlmString(pb.user, offset, ctx->extractedUser, unicode);
    pb.userStr = ctx->extractedUser;

    offset = qEncodeNtlmString(pb.workstation, offset, ctx->workstation, unicode);
    pb.workstationStr = ctx->workstation;

    // Get LM response
#ifdef NTLMV1_CLIENT
    pb.lmResponseBuf = qEncodeLmResponse(ctx, ch);
#else
    if (ch.targetInfo.len > 0) {
        pb.lmResponseBuf = QByteArray();
    } else {
        pb.lmResponseBuf = qEncodeLmv2Response(ctx, ch, &pb);
    }
#endif
    offset = qEncodeNtlmBuffer(pb.lmResponse, offset, pb.lmResponseBuf);

    // Get NTLM response
#ifdef NTLMV1_CLIENT
    pb.ntlmResponseBuf = qEncodeNtlmResponse(ctx, ch);
#else
    pb.ntlmResponseBuf = qEncodeNtlmv2Response(ctx, ch, &pb);
#endif
    offset = qEncodeNtlmBuffer(pb.ntlmResponse, offset, pb.ntlmResponseBuf);


    // Encode and send
    ds << pb;

    return rc;
}



QT_END_NAMESPACE
