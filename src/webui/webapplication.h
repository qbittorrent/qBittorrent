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
#include <QHash>
#include <QMap>
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

constexpr Utils::Version<int, 3, 2> API_VERSION {2, 2, 0};

class APIController;
class WebApplication;

constexpr char C_SID[] = "SID"; // name of session id cookie
constexpr int INACTIVE_TIME = 900; // Session inactive time (in secs = 15 min.)

class WebSession : public ISession
{
    friend class WebApplication;

public:
    explicit WebSession(const QString &sid);

    QString id() const override;
    qint64 timestamp() const;

    QVariant getData(const QString &id) const override;
    void setData(const QString &id, const QVariant &data) override;

private:
    void updateTimestamp();

    const QString m_sid;
    qint64 m_timestamp;
    QVariantHash m_data;
};

class WebApplication
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

    void translateDocument(QString &data);

    // Session management
    QString generateSid() const;
    void sessionInitialize();
    bool isAuthNeeded();
    bool isPublicAPI(const QString &scope, const QString &action) const;

    bool isCrossSiteRequest(const Http::Request &request) const;
    bool validateHostHeader(const QStringList &domains) const;

    // Persistent data
    QMap<QString, WebSession *> m_sessions;

    // Current data
    WebSession *m_currentSession = nullptr;
    Http::Request m_request;
    Http::Environment m_env;
    QMap<QString, QString> m_params;

    const QRegularExpression m_apiPathPattern {(QLatin1String("^/api/v2/(?<scope>[A-Za-z_][A-Za-z_0-9]*)/(?<action>[A-Za-z_][A-Za-z_0-9]*)$"))};

    QHash<QString, APIController *> m_apiControllers;
    QSet<QString> m_publicAPIs;
    bool m_isAltUIUsed = false;
    QString m_rootFolder;

    struct TranslatedFile
    {
        QByteArray data;
        QDateTime lastModified;
    };
    QMap<QString, TranslatedFile> m_translatedFiles;
    QString m_currentLocale;
    QTranslator m_translator;
    bool m_translationFileLoaded = false;

    bool m_isLocalAuthEnabled;
    bool m_isAuthSubnetWhitelistEnabled;
    QList<Utils::Net::Subnet> m_authSubnetWhitelist;

    // security related
    QStringList m_domainList;
    bool m_isClickjackingProtectionEnabled;
    bool m_isCSRFProtectionEnabled;
    bool m_isHostHeaderValidationEnabled;
    bool m_isHttpsEnabled;
    QString m_contentSecurityPolicy;
};
