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

#ifndef QHTTP_H
#define QHTTP_H

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qpair.h>
#include <QtCore/qscopedpointer.h>

QT_BEGIN_HEADER

class QTcpSocket;
class QTimerEvent;
class QIODevice;
class QAuthenticator;
class QNetworkProxy;
class QSslError;

class QHttpPrivate;

class QHttpHeaderPrivate;
class QHttpHeader
{
public:
    QHttpHeader();
    QHttpHeader(const QHttpHeader &header);
    QHttpHeader(const QString &str);
    virtual ~QHttpHeader();

    QHttpHeader &operator=(const QHttpHeader &h);

    void setValue(const QString &key, const QString &value);
    void setValues(const QList<QPair<QString, QString> > &values);
    void addValue(const QString &key, const QString &value);
    QList<QPair<QString, QString> > values() const;
    bool hasKey(const QString &key) const;
    QStringList keys() const;
    QString value(const QString &key) const;
    QStringList allValues(const QString &key) const;
    void removeValue(const QString &key);
    void removeAllValues(const QString &key);

    // ### Qt 5: change to qint64
    bool hasContentLength() const;
    uint contentLength() const;
    void setContentLength(int len);

    bool hasContentType() const;
    QString contentType() const;
    void setContentType(const QString &type);

    virtual QString toString() const;
    bool isValid() const;

    virtual int majorVersion() const = 0;
    virtual int minorVersion() const = 0;

protected:
    virtual bool parseLine(const QString &line, int number);
    bool parse(const QString &str);
    void setValid(bool);

    QHttpHeader(QHttpHeaderPrivate &dd, const QString &str = QString());
    QHttpHeader(QHttpHeaderPrivate &dd, const QHttpHeader &header);
    QScopedPointer<QHttpHeaderPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(QHttpHeader)
};

class QHttpResponseHeaderPrivate;
class QHttpResponseHeader : public QHttpHeader
{
public:
    QHttpResponseHeader();
    QHttpResponseHeader(const QHttpResponseHeader &header);
    QHttpResponseHeader(const QString &str);
    QHttpResponseHeader(int code, const QString &text = QString(), int majorVer = 1, int minorVer = 1);
    QHttpResponseHeader &operator=(const QHttpResponseHeader &header);

    void setStatusLine(int code, const QString &text = QString(), int majorVer = 1, int minorVer = 1);

    int statusCode() const;
    QString reasonPhrase() const;

    int majorVersion() const;
    int minorVersion() const;

    QString toString() const;

protected:
    bool parseLine(const QString &line, int number);

private:
    Q_DECLARE_PRIVATE(QHttpResponseHeader)
    friend class QHttpPrivate;
};

class QHttpRequestHeaderPrivate;
class QHttpRequestHeader : public QHttpHeader
{
public:
    QHttpRequestHeader();
    QHttpRequestHeader(const QString &method, const QString &path, int majorVer = 1, int minorVer = 1);
    QHttpRequestHeader(const QHttpRequestHeader &header);
    QHttpRequestHeader(const QString &str);
    QHttpRequestHeader &operator=(const QHttpRequestHeader &header);

    void setRequest(const QString &method, const QString &path, int majorVer = 1, int minorVer = 1);

    QString method() const;
    QString path() const;

    int majorVersion() const;
    int minorVersion() const;

    QString toString() const;

protected:
    bool parseLine(const QString &line, int number);

private:
    Q_DECLARE_PRIVATE(QHttpRequestHeader)
};

class QHttp : public QObject
{
    Q_OBJECT

public:
    enum ConnectionMode {
        ConnectionModeHttp,
        ConnectionModeHttps
    };

    explicit QHttp(QObject *parent = 0);
    QHttp(const QString &hostname, quint16 port = 80, QObject *parent = 0);
    QHttp(const QString &hostname, ConnectionMode mode, quint16 port = 0, QObject *parent = 0);
    virtual ~QHttp();

    enum State {
        Unconnected,
        HostLookup,
        Connecting,
        Sending,
        Reading,
        Connected,
        Closing
    };
    enum Error {
        NoError,
        UnknownError,
        HostNotFound,
        ConnectionRefused,
        UnexpectedClose,
        InvalidResponseHeader,
        WrongContentLength,
        Aborted,
        AuthenticationRequiredError,
        ProxyAuthenticationRequiredError
    };

    int setHost(const QString &hostname, quint16 port = 80);
    int setHost(const QString &hostname, ConnectionMode mode, quint16 port = 0);

    int setSocket(QTcpSocket *socket);
    int setUser(const QString &username, const QString &password = QString());

#ifndef QT_NO_NETWORKPROXY
    int setProxy(const QString &host, int port,
                 const QString &username = QString(),
                 const QString &password = QString());
    int setProxy(const QNetworkProxy &proxy);
#endif

    int get(const QString &path, QIODevice *to=0);
    int post(const QString &path, QIODevice *data, QIODevice *to=0 );
    int post(const QString &path, const QByteArray &data, QIODevice *to=0);
    int head(const QString &path);
    int request(const QHttpRequestHeader &header, QIODevice *device=0, QIODevice *to=0);
    int request(const QHttpRequestHeader &header, const QByteArray &data, QIODevice *to=0);

    int closeConnection();
    int close();

    qint64 bytesAvailable() const;
    qint64 read(char *data, qint64 maxlen);
    QByteArray readAll();

    int currentId() const;
    QIODevice *currentSourceDevice() const;
    QIODevice *currentDestinationDevice() const;
    QHttpRequestHeader currentRequest() const;
    QHttpResponseHeader lastResponse() const;
    bool hasPendingRequests() const;
    void clearPendingRequests();

    State state() const;

    Error error() const;
    QString errorString() const;

public Q_SLOTS:
    void abort();

#ifndef QT_NO_OPENSSL
    void ignoreSslErrors();
#endif

Q_SIGNALS:
    void stateChanged(int);
    void responseHeaderReceived(const QHttpResponseHeader &resp);
    void readyRead(const QHttpResponseHeader &resp);

    // ### Qt 5: change to qint64
    void dataSendProgress(int, int);
    void dataReadProgress(int, int);

    void requestStarted(int);
    void requestFinished(int, bool);
    void done(bool);

#ifndef QT_NO_NETWORKPROXY
    void proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *);
#endif
    void authenticationRequired(const QString &hostname, quint16 port, QAuthenticator *);

#ifndef QT_NO_OPENSSL
    void sslErrors(const QList<QSslError> &errors);
#endif

private:
    Q_DISABLE_COPY(QHttp)
    QScopedPointer<QHttpPrivate> d;

    Q_PRIVATE_SLOT(d, void _q_startNextRequest())
    Q_PRIVATE_SLOT(d, void _q_slotReadyRead())
    Q_PRIVATE_SLOT(d, void _q_slotConnected())
    Q_PRIVATE_SLOT(d, void _q_slotError(QAbstractSocket::SocketError))
    Q_PRIVATE_SLOT(d, void _q_slotClosed())
    Q_PRIVATE_SLOT(d, void _q_slotBytesWritten(qint64 numBytes))
#ifndef QT_NO_OPENSSL
    Q_PRIVATE_SLOT(d, void _q_slotEncryptedBytesWritten(qint64 numBytes))
#endif
    Q_PRIVATE_SLOT(d, void _q_slotDoFinished())
    Q_PRIVATE_SLOT(d, void _q_slotSendRequest())
    Q_PRIVATE_SLOT(d, void _q_continuePost())

    friend class QHttpNormalRequest;
    friend class QHttpSetHostRequest;
    friend class QHttpSetSocketRequest;
    friend class QHttpSetUserRequest;
    friend class QHttpSetProxyRequest;
    friend class QHttpCloseRequest;
    friend class QHttpPGHRequest;
};

QT_END_HEADER

#endif // QHTTP_H
