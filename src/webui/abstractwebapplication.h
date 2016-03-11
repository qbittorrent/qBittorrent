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

#ifndef ABSTRACTWEBAPPLICATION_H
#define ABSTRACTWEBAPPLICATION_H

#include <QObject>
#include <QMap>
#include <QHash>
#include "base/http/types.h"
#include "base/http/responsebuilder.h"
#include "base/http/irequesthandler.h"

struct WebSession;
struct WebSessionData;

const char C_SID[] = "SID"; // name of session id cookie
const int BAN_TIME = 3600000; // 1 hour
const int INACTIVE_TIME = 900; // Session inactive time (in secs = 15 min.)
const int MAX_AUTH_FAILED_ATTEMPTS = 5;

class AbstractWebApplication : public Http::ResponseBuilder, public Http::IRequestHandler
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractWebApplication)

public:
    explicit AbstractWebApplication(QObject *parent = 0);
    virtual ~AbstractWebApplication();

    Http::Response processRequest(const Http::Request &request, const Http::Environment &env);

protected:
    virtual void processRequest() = 0;

    bool isBanned() const;
    int failedAttempts() const;
    void resetFailedAttempts();
    void increaseFailedAttempts();

    void printFile(const QString &path);

    // Session management
    bool sessionActive() const { return session_ != 0; }
    bool sessionStart(const QString& token = QString());
    bool sessionEnd();

    bool isAuthNeeded();

    bool readFile(const QString &path, QByteArray &data, QString &type);

    // save data to temporary file on disk and return its name (or empty string if fails)
    static QString saveTmpFile(const QByteArray &data);

    WebSessionData *session();
    Http::Request request() const { return request_; }
    Http::Environment env() const { return env_; }

private slots:
    void UnbanTimerEvent();
    void removeInactiveSessions();

private:
    // Persistent data
    QMap<QString, WebSession *> sessions_;
    QHash<QHostAddress, int> clientFailedAttempts_;
    QMap<QString, QByteArray> translatedFiles_;

    // Current data
    WebSession *session_;
    Http::Request request_;
    Http::Environment env_;

    QString generateSid();
    bool sessionInitialize();

    static void translateDocument(QString &data);

    static const QStringMap CONTENT_TYPE_BY_EXT;
    static QStringMap initializeContentTypeByExtMap();
};

#endif // ABSTRACTWEBAPPLICATION_H
