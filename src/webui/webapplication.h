/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef WEBAPPLICATION_H
#define WEBAPPLICATION_H

#include <QObject>
#include <QMap>
#include <QHash>
#include "httptypes.h"
#include <QVariant>

struct WebSession
{
    const QString id;
    QVariantMap syncMainDataLastResponse;
    QVariantMap syncMainDataLastAcceptedResponse;
    WebSession(const QString& id): id(id) {}
};

const QString C_SID = "SID"; // name of session id cookie
const int BAN_TIME = 3600000; // 1 hour
const int MAX_AUTH_FAILED_ATTEMPTS = 5;

class AbstractRequestHandler;

class WebApplication: public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(WebApplication)

public:
    WebApplication(QObject* parent = 0);
    virtual ~WebApplication();

    static WebApplication* instance();

    bool isBanned(const AbstractRequestHandler* _this) const;
    int failedAttempts(const AbstractRequestHandler *_this) const;
    void resetFailedAttempts(AbstractRequestHandler* _this);
    void increaseFailedAttempts(AbstractRequestHandler* _this);

    bool sessionStart(AbstractRequestHandler* _this);
    bool sessionEnd(AbstractRequestHandler* _this);
    bool sessionInitialize(AbstractRequestHandler* _this);

    bool readFile(const QString &path, QByteArray& data, QString& type);

private slots:
    void UnbanTimerEvent();

private:
    QMap<QString, WebSession*> sessions_;
    QHash<QHostAddress, int> clientFailedAttempts_;
    QMap<QString, QByteArray> translatedFiles_;

    QString generateSid();
    static void translateDocument(QString& data);

    static const QStringMap CONTENT_TYPE_BY_EXT;
    static QStringMap initializeContentTypeByExtMap();
};

#endif // WEBAPPLICATION_H
