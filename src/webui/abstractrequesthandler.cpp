/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012, Christophe Dumez <chris@qbittorrent.org>
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

#include <QDebug>
#include <QDir>
#include <QTemporaryFile>
#include <QNetworkCookie>
#include "preferences.h"
#include "webapplication.h"
#include "abstractrequesthandler.h"

AbstractRequestHandler::AbstractRequestHandler(const HttpRequest &request, const HttpEnvironment &env, WebApplication *app)
    : app_(app), session_(0), request_(request), env_(env)
{
    sessionInitialize();
    if (!sessionActive() && !isAuthNeeded())
        sessionStart();
}

HttpResponse AbstractRequestHandler::run()
{
    response_ = HttpResponse();

    if (isBanned()) {
        status(403, "Forbidden");
        print(QObject::tr("Your IP address has been banned after too many failed authentication attempts."), CONTENT_TYPE_TXT);
    }
    else {
        processRequest();
    }

    return response_;
}

bool AbstractRequestHandler::isAuthNeeded()
{
    return
        (
        env_.clientAddress != QHostAddress::LocalHost &&
        env_.clientAddress != QHostAddress::LocalHostIPv6
        ) || Preferences::instance()->isWebUiLocalAuthEnabled();
}

void AbstractRequestHandler::status(uint code, const QString& text)
{
    response_.status = HttpResponseStatus(code, text);
}

void AbstractRequestHandler::header(const QString& name, const QString& value)
{
    response_.headers[name] = value;
}

void AbstractRequestHandler::print(const QString& text, const QString& type)
{
    print_impl(text.toUtf8(), type);
}

void AbstractRequestHandler::print(const QByteArray& data, const QString& type)
{
    print_impl(data, type);
}

void AbstractRequestHandler::print_impl(const QByteArray& data, const QString& type)
{
    if (!response_.headers.contains(HEADER_CONTENT_TYPE))
        response_.headers[HEADER_CONTENT_TYPE] = type;

    response_.content += data;
}

void AbstractRequestHandler::printFile(const QString& path)
{
    QByteArray data;
    QString type;

    if (!app_->readFile(path, data, type)) {
        status(404, "Not Found");
        return;
    }

    print(data, type);
}

void AbstractRequestHandler::sessionInitialize()
{
    app_->sessionInitialize(this);
}

void AbstractRequestHandler::sessionStart()
{
    if (app_->sessionStart(this)) {
        QNetworkCookie cookie(C_SID.toUtf8(), session_->id.toUtf8());
        cookie.setPath("/");
        header(HEADER_SET_COOKIE, cookie.toRawForm());
    }
}

void AbstractRequestHandler::sessionEnd()
{
    if (sessionActive()) {
        QNetworkCookie cookie(C_SID.toUtf8(), session_->id.toUtf8());
        cookie.setPath("/");
        cookie.setExpirationDate(QDateTime::currentDateTime());

        if (app_->sessionEnd(this))
            header(HEADER_SET_COOKIE, cookie.toRawForm());
    }
}

bool AbstractRequestHandler::isBanned() const
{
    return app_->isBanned(this);
}

int AbstractRequestHandler::failedAttempts() const
{
    return app_->failedAttempts(this);
}

void AbstractRequestHandler::resetFailedAttempts()
{
    app_->resetFailedAttempts(this);
}

void AbstractRequestHandler::increaseFailedAttempts()
{
    app_->increaseFailedAttempts(this);
}

QString AbstractRequestHandler::saveTmpFile(const QByteArray &data)
{
    QTemporaryFile tmpfile(QDir::temp().absoluteFilePath("qBT-XXXXXX.torrent"));
    tmpfile.setAutoRemove(false);
    if (tmpfile.open()) {
        tmpfile.write(data);
        tmpfile.close();
        return tmpfile.fileName();
    }

    qWarning() << "I/O Error: Could not create temporary file";
    return QString();
}
