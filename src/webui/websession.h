/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2024  Radu Carpa <radu.carpa@cern.ch>
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

#pragma once

#include <chrono>

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QString>

#include "api/isessionmanager.h"

using namespace std::chrono_literals;

class APIController;

enum class WebSessionType : qint8
{
    CookieBased,
    APIKeyBased
};

class WebSession : public QObject, public ISession
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebSession)

public:
    static WebSession *create(WebSessionType type, const QString &sid);

    explicit WebSession(const QString &sid);

    QString id() const override;

    virtual WebSessionType type() const = 0;
    virtual bool hasExpired(std::chrono::milliseconds duration) const = 0;

    QElapsedTimer timestamp() const;
    void updateTimestamp();

    void registerAPIController(const QString &scope, APIController *controller);
    APIController *getAPIController(const QString &scope) const;

private:
    const QString m_sid;
    QElapsedTimer m_timestamp;
    QMap<QString, APIController *> m_apiControllers;
};

class CookieBasedWebSession final : public WebSession
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CookieBasedWebSession)

public:
    using WebSession::WebSession;

    WebSessionType type() const override;
    bool hasExpired(std::chrono::milliseconds duration) const override;

    bool shouldRefreshCookie() const;
    void setCookieRefreshTime(std::chrono::seconds timeout);

private:
    QDeadlineTimer m_cookieRefreshTimer;
};

class APIKeyBasedWebSession final : public WebSession
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(APIKeyBasedWebSession)

public:
    using WebSession::WebSession;

    WebSessionType type() const override;
    bool hasExpired(std::chrono::milliseconds duration) const override;
};
