/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "authcontroller.h"

#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/password.h"
#include "apierror.h"
#include "isessionmanager.h"

constexpr int BAN_TIME = 3600000; // 1 hour
constexpr int MAX_AUTH_FAILED_ATTEMPTS = 5;

void AuthController::loginAction()
{
    if (sessionManager()->session()) {
        setResult(QLatin1String("Ok."));
        return;
    }

    const QString clientAddr {sessionManager()->clientId()};
    const QString usernameFromWeb {params()["username"]};
    const QString passwordFromWeb {params()["password"]};

    if (isBanned()) {
        LogMsg(tr("WebAPI login failure. Reason: IP has been banned, IP: %1, username: %2")
                .arg(clientAddr, usernameFromWeb)
            , Log::WARNING);
        throw APIError(APIErrorType::AccessDenied
                       , tr("Your IP address has been banned after too many failed authentication attempts."));
    }

    const Preferences *pref = Preferences::instance();

    const QString username {pref->getWebUiUsername()};
    const QByteArray secret {pref->getWebUIPassword()};
    const bool usernameEqual = Utils::Password::slowEquals(usernameFromWeb.toUtf8(), username.toUtf8());
    const bool passwordEqual = Utils::Password::PBKDF2::verify(secret, passwordFromWeb);

    if (usernameEqual && passwordEqual) {
        m_clientFailedLogins.remove(clientAddr);

        sessionManager()->sessionStart();
        setResult(QLatin1String("Ok."));
        LogMsg(tr("WebAPI login success. IP: %1").arg(clientAddr));
    }
    else {
        increaseFailedAttempts();
        setResult(QLatin1String("Fails."));
        LogMsg(tr("WebAPI login failure. Reason: invalid credentials, attempt count: %1, IP: %2, username: %3")
                .arg(QString::number(failedAttemptsCount()), clientAddr, usernameFromWeb)
            , Log::WARNING);
    }
}

void AuthController::logoutAction()
{
    sessionManager()->sessionEnd();
}

bool AuthController::isBanned() const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    const FailedLogin failedLogin = m_clientFailedLogins.value(sessionManager()->clientId());

    bool isBanned = (failedLogin.bannedAt > 0);
    if (isBanned && ((now - failedLogin.bannedAt) > BAN_TIME)) {
        m_clientFailedLogins.remove(sessionManager()->clientId());
        isBanned = false;
    }

    return isBanned;
}

int AuthController::failedAttemptsCount() const
{
    return m_clientFailedLogins.value(sessionManager()->clientId()).failedAttemptsCount;
}

void AuthController::increaseFailedAttempts()
{
    FailedLogin &failedLogin = m_clientFailedLogins[sessionManager()->clientId()];
    ++failedLogin.failedAttemptsCount;

    if (failedLogin.failedAttemptsCount == MAX_AUTH_FAILED_ATTEMPTS) {
        // Max number of failed attempts reached
        // Start ban period
        failedLogin.bannedAt = QDateTime::currentMSecsSinceEpoch() / 1000;
    }
}
