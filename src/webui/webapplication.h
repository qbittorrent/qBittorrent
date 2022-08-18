/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014, 2017, 2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <type_traits>
#include <utility>

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QTranslator>

#include "base/applicationcomponent.h"
#include "base/global.h"
#include "base/http/irequesthandler.h"
#include "base/http/responsebuilder.h"
#include "base/http/types.h"
#include "base/path.h"
#include "base/utils/net.h"
#include "base/utils/version.h"
#include "api/isessionmanager.h"

inline const Utils::Version<3, 2> API_VERSION {2, 8, 14};

class APIController;
class AuthController;
class WebApplication;

class WebSession final : public QObject, public ApplicationComponent, public ISession
{
public:
    explicit WebSession(const QString &sid, IApplication *app);

    QString id() const override;

    bool hasExpired(qint64 seconds) const;
    void updateTimestamp();

    template <typename T>
    void registerAPIController(const QString &scope)
    {
        static_assert(std::is_base_of_v<APIController, T>, "Class should be derived from APIController.");
        m_apiControllers[scope] = new T(app(), this);
    }

    APIController *getAPIController(const QString &scope) const;

private:
    const QString m_sid;
    QElapsedTimer m_timer;  // timestamp
    QMap<QString, APIController *> m_apiControllers;
};

class WebApplication final
        : public QObject, public ApplicationComponent
        , public Http::IRequestHandler, public ISessionManager
        , private Http::ResponseBuilder
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebApplication)

public:
    explicit WebApplication(IApplication *app, QObject *parent = nullptr);
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

    void declarePublicAPI(const QString &apiPath);

    void sendFile(const Path &path);
    void sendWebUIFile();

    void translateDocument(QString &data) const;

    // Session management
    QString generateSid() const;
    void sessionInitialize();
    bool isAuthNeeded();
    bool isPublicAPI(const QString &scope, const QString &action) const;

    bool isCrossSiteRequest(const Http::Request &request) const;
    bool validateHostHeader(const QStringList &domains) const;

    QHostAddress resolveClientAddress() const;

    // Persistent data
    QHash<QString, WebSession *> m_sessions;

    // Current data
    WebSession *m_currentSession = nullptr;
    Http::Request m_request;
    Http::Environment m_env;
    QHash<QString, QString> m_params;
    const QString m_cacheID;

    const QRegularExpression m_apiPathPattern {u"^/api/v2/(?<scope>[A-Za-z_][A-Za-z_0-9]*)/(?<action>[A-Za-z_][A-Za-z_0-9]*)$"_qs};

    QSet<QString> m_publicAPIs;
    const QHash<std::pair<QString, QString>, QString> m_allowedMethod =
    {
        // <<controller name, action name>, HTTP method>
        {{u"app"_qs, u"setPreferences"_qs}, Http::METHOD_POST},
        {{u"app"_qs, u"shutdown"_qs}, Http::METHOD_POST},
        {{u"auth"_qs, u"login"_qs}, Http::METHOD_POST},
        {{u"auth"_qs, u"logout"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"addFeed"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"addFolder"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"markAsRead"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"moveItem"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"refreshItem"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"removeItem"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"removeRule"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"renameRule"_qs}, Http::METHOD_POST},
        {{u"rss"_qs, u"setRule"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"delete"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"enablePlugin"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"installPlugin"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"start"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"stop"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"uninstallPlugin"_qs}, Http::METHOD_POST},
        {{u"search"_qs, u"updatePlugins"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"add"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"addPeers"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"addTags"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"addTrackers"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"bottomPrio"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"createCategory"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"createTags"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"decreasePrio"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"delete"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"deleteTags"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"editCategory"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"editTracker"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"filePrio"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"increasePrio"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"pause"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"reannounce"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"recheck"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"removeCategories"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"removeTags"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"removeTrackers"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"rename"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"renameFile"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"renameFolder"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"resume"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setAutoManagement"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setCategory"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setDownloadLimit"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setDownloadPath"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setForceStart"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setLocation"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setSavePath"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setShareLimits"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setSuperSeeding"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"setUploadLimit"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"toggleFirstLastPiecePrio"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"toggleSequentialDownload"_qs}, Http::METHOD_POST},
        {{u"torrents"_qs, u"topPrio"_qs}, Http::METHOD_POST},
        {{u"transfer"_qs, u"banPeers"_qs}, Http::METHOD_POST},
        {{u"transfer"_qs, u"setDownloadLimit"_qs}, Http::METHOD_POST},
        {{u"transfer"_qs, u"setSpeedLimitsMode"_qs}, Http::METHOD_POST},
        {{u"transfer"_qs, u"setUploadLimit"_qs}, Http::METHOD_POST},
        {{u"transfer"_qs, u"toggleSpeedLimitsMode"_qs}, Http::METHOD_POST},
    };
    bool m_isAltUIUsed = false;
    Path m_rootFolder;

    struct TranslatedFile
    {
        QByteArray data;
        QString mimeType;
        QDateTime lastModified;
    };
    QHash<Path, TranslatedFile> m_translatedFiles;
    QString m_currentLocale;
    QTranslator m_translator;
    bool m_translationFileLoaded = false;

    AuthController *m_authController = nullptr;
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

    // Reverse proxy
    bool m_isReverseProxySupportEnabled;
    QVector<QHostAddress> m_trustedReverseProxyList;
    QHostAddress m_clientAddress;

    QVector<Http::Header> m_prebuiltHeaders;
};
