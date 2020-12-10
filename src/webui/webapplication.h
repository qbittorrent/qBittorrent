/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014, 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QTranslator>

#include "api/isessionmanager.h"
#include "base/http/irequesthandler.h"
#include "base/http/responsebuilder.h"
#include "base/http/types.h"
#include "base/utils/net.h"
#include "base/utils/version.h"

constexpr Utils::Version<int, 3, 2> API_VERSION {2, 7, 0};

class APIController;
class WebApplication;

constexpr char C_SID[] = "SID"; // name of session id cookie

class WebSession final : public ISession
{
public:
    explicit WebSession(const QString &sid);

    QString id() const override;

    bool hasExpired(qint64 seconds) const;
    void updateTimestamp();

    QVariant getData(const QString &id) const override;
    void setData(const QString &id, const QVariant &data) override;

private:
    const QString m_sid;
    QElapsedTimer m_timer;  // timestamp
    QVariantHash m_data;
};

class WebApplication final
        : public QObject, public Http::IRequestHandler, public ISessionManager
        , private Http::ResponseBuilder
{
    Q_OBJECT
    Q_DISABLE_COPY(WebApplication)

#ifndef Q_MOC_RUN
#define WEBAPI_PUBLIC
#define WEBAPI_PRIVATE
#endif

public:
    explicit WebApplication(QObject *parent = nullptr);
    ~WebApplication() override;

    Http::Response processRequest(const Http::Request &request, const Http::Environment &env) override;

    QString clientId() const override;
    WebSession *session() override;
    void sessionStart() override;
    void sessionEnd() override;

    const Http::Request &request() const;
    const Http::Environment &env() const;

private:
    void doProcessRequest();
    void configure();

    void registerAPIController(const QString &scope, APIController *controller);
    void declarePublicAPI(const QString &apiPath);

    void sendFile(const QString &path);
    void sendWebUIFile();

    void translateDocument(QString &data) const;

    // Session management
    QString generateSid() const;
    void sessionInitialize();
    bool isAuthNeeded();
    bool isPublicAPI(const QString &scope, const QString &action) const;

    bool isCrossSiteRequest(const Http::Request &request) const;
    bool validateHostHeader(const QStringList &domains) const;

    // Persistent data
    QHash<QString, WebSession *> m_sessions;

    // Current data
    WebSession *m_currentSession = nullptr;
    Http::Request m_request;
    Http::Environment m_env;
    QHash<QString, QString> m_params;
    const QString m_cacheID;

    const QRegularExpression m_apiPathPattern {QLatin1String("^/api/v2/(?<scope>[A-Za-z_][A-Za-z_0-9]*)/(?<action>[A-Za-z_][A-Za-z_0-9]*)$")};

    QHash<QString, APIController *> m_apiControllers;
    QSet<QString> m_publicAPIs;
    bool m_isAltUIUsed = false;
    QString m_rootFolder;

    struct TranslatedFile
    {
        QByteArray data;
        QString mimeType;
        QDateTime lastModified;
    };
    QHash<QString, TranslatedFile> m_translatedFiles;
    QString m_currentLocale;
    QTranslator m_translator;
    bool m_translationFileLoaded = false;

    bool m_isLocalAuthEnabled;
    bool m_isAuthSubnetWhitelistEnabled;
    QVector<Utils::Net::Subnet> m_authSubnetWhitelist;
    int m_sessionTimeout;

    // security related
    QStringList m_domainList;
    bool m_isCSRFProtectionEnabled;
    bool m_isSecureCookieEnabled;
    bool m_isHostHeaderValidationEnabled;
    bool m_isHttpsEnabled;

    QVector<Http::Header> m_prebuiltHeaders;
};
