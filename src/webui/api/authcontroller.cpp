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

#include <QString>

#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/password.h"
#include "apierror.h"
#include "isessionmanager.h"

AuthController::AuthController(ISessionManager *sessionManager, IApplication *app, QObject *parent)
    : APIController(app, parent)
    , m_sessionManager {sessionManager}
{
}

void AuthController::setUsername(const QString &username)
{
    m_username = username;
}

void AuthController::setPasswordHash(const QByteArray &passwordHash)
{
    m_passwordHash = passwordHash;
}

void AuthController::loginAction()
{
    if (m_sessionManager->session())
    {
        setResult(u"Ok."_s);
        return;
    }

    const QString clientAddr = m_sessionManager->clientId();
    const QString usernameFromWeb = params()[u"username"_s];
    const QString passwordFromWeb = params()[u"password"_s];

    if (isBanned())
    {
        LogMsg(tr("WebAPI login failure. Reason: IP has been banned, IP: %1, username: %2")
                .arg(clientAddr, usernameFromWeb)
            , Log::WARNING);
        throw APIError(APIErrorType::AccessDenied
            , tr("Your IP address has been banned after too many failed authentication attempts."));
    }

    const bool usernameEqual = Utils::Password::slowEquals(usernameFromWeb.toUtf8(), m_username.toUtf8());
    const bool passwordEqual = Utils::Password::PBKDF2::verify(m_passwordHash, passwordFromWeb);

    if (usernameEqual && passwordEqual)
    {
        m_clientFailedLogins.remove(clientAddr);

        m_sessionManager->sessionStart();
        setResult(u"Ok."_s);
        LogMsg(tr("WebAPI login success. IP: %1").arg(clientAddr));
    }
    else
    {
        if (Preferences::instance()->getWebUIMaxAuthFailCount() > 0)
            increaseFailedAttempts();
        setResult(u"Fails."_s);
        LogMsg(tr("WebAPI login failure. Reason: invalid credentials, attempt count: %1, IP: %2, username: %3")
                .arg(QString::number(failedAttemptsCount()), clientAddr, usernameFromWeb)
            , Log::WARNING);
    }
}

void AuthController::logoutAction() const
{
    m_sessionManager->sessionEnd();
}

bool AuthController::isBanned() const
{
    const auto failedLoginIter = m_clientFailedLogins.constFind(m_sessionManager->clientId());
    if (failedLoginIter == m_clientFailedLogins.cend())
        return false;

    bool isBanned = (failedLoginIter->banTimer.remainingTime() >= 0);
    if (isBanned && failedLoginIter->banTimer.hasExpired())
    {
        m_clientFailedLogins.erase(failedLoginIter);
        isBanned = false;
    }

    return isBanned;
}

int AuthController::failedAttemptsCount() const
{
    return m_clientFailedLogins.value(m_sessionManager->clientId()).failedAttemptsCount;
}

void AuthController::increaseFailedAttempts()
{
    Q_ASSERT(Preferences::instance()->getWebUIMaxAuthFailCount() > 0);

    FailedLogin &failedLogin = m_clientFailedLogins[m_sessionManager->clientId()];
    ++failedLogin.failedAttemptsCount;

    if (failedLogin.failedAttemptsCount >= Preferences::instance()->getWebUIMaxAuthFailCount())
    {
        // Max number of failed attempts reached
        // Start ban period
        failedLogin.banTimer.setRemainingTime(Preferences::instance()->getWebUIBanDuration());
    }
}
