/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <type_traits>
#include <utility>

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QList>
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
#include "base/utils/thread.h"
#include "base/utils/version.h"
#include "api/isessionmanager.h"

inline const Utils::Version<3, 2> API_VERSION {2, 11, 4};

class QTimer;

class APIController;
class AuthController;
class FreeDiskSpaceChecker;
class WebApplication;

namespace BitTorrent
{
    class TorrentCreationManager;
}

class WebSession final : public ApplicationComponent<QObject>, public ISession
{
public:
    explicit WebSession(const QString &sid, IApplication *app);

    QString id() const override;

    bool hasExpired(qint64 seconds) const;
    void updateTimestamp();

    void registerAPIController(const QString &scope, APIController *controller);
    APIController *getAPIController(const QString &scope) const;

private:
    const QString m_sid;
    QElapsedTimer m_timer;  // timestamp
    QMap<QString, APIController *> m_apiControllers;
};

class WebApplication final : public ApplicationComponent<QObject>
        , public Http::IRequestHandler, public ISessionManager
        , private Http::ResponseBuilder
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebApplication)

public:
    explicit WebApplication(IApplication *app, QObject *parent = nullptr);
    ~WebApplication() override;

    Http::Response processRequest(const Http::Request &request, const Http::Environment &env) override;

    const Http::Request &request() const;
    const Http::Environment &env() const;

    void setUsername(const QString &username);
    void setPasswordHash(const QByteArray &passwordHash);

private:
    QString clientId() const override;
    WebSession *session() override;
    void sessionStart() override;
    void sessionEnd() override;

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

    bool isOriginTrustworthy() const;
    bool isCrossSiteRequest(const Http::Request &request) const;
    bool validateHostHeader(const QStringList &domains) const;

    // reverse proxy
    QHostAddress resolveClientAddress() const;

    // Persistent data
    QHash<QString, WebSession *> m_sessions;

    // Current data
    WebSession *m_currentSession = nullptr;
    Http::Request m_request;
    Http::Environment m_env;
    QHash<QString, QString> m_params;
    const QString m_cacheID;

    const QRegularExpression m_apiPathPattern {u"^/api/v2/(?<scope>[A-Za-z_][A-Za-z_0-9]*)/(?<action>[A-Za-z_][A-Za-z_0-9]*)$"_s};

    QSet<QString> m_publicAPIs;
    const QHash<std::pair<QString, QString>, QString> m_allowedMethod =
    {
        // <<controller name, action name>, HTTP method>
        {{u"app"_s, u"sendTestEmail"_s}, Http::METHOD_POST},
        {{u"app"_s, u"setCookies"_s}, Http::METHOD_POST},
        {{u"app"_s, u"setPreferences"_s}, Http::METHOD_POST},
        {{u"app"_s, u"shutdown"_s}, Http::METHOD_POST},
        {{u"auth"_s, u"login"_s}, Http::METHOD_POST},
        {{u"auth"_s, u"logout"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"addFeed"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"addFolder"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"markAsRead"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"moveItem"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"refreshItem"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"removeItem"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"removeRule"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"renameRule"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"setFeedURL"_s}, Http::METHOD_POST},
        {{u"rss"_s, u"setRule"_s}, Http::METHOD_POST},
        {{u"search"_s, u"delete"_s}, Http::METHOD_POST},
        {{u"search"_s, u"enablePlugin"_s}, Http::METHOD_POST},
        {{u"search"_s, u"installPlugin"_s}, Http::METHOD_POST},
        {{u"search"_s, u"start"_s}, Http::METHOD_POST},
        {{u"search"_s, u"stop"_s}, Http::METHOD_POST},
        {{u"search"_s, u"uninstallPlugin"_s}, Http::METHOD_POST},
        {{u"search"_s, u"updatePlugins"_s}, Http::METHOD_POST},
        {{u"torrentcreator"_s, u"addTask"_s}, Http::METHOD_POST},
        {{u"torrentcreator"_s, u"deleteTask"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"add"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"addPeers"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"addTags"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"addTrackers"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"addWebSeeds"_s}, Http::METHOD_POST},
        {{u"transfer"_s, u"banPeers"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"bottomPrio"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"createCategory"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"createTags"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"decreasePrio"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"delete"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"deleteTags"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"editCategory"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"editTracker"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"editWebSeed"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"filePrio"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"increasePrio"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"reannounce"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"recheck"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"removeCategories"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"removeTags"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"removeTrackers"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"removeWebSeeds"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"rename"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"renameFile"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"renameFolder"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setAutoManagement"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setCategory"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setDownloadLimit"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setDownloadPath"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setForceStart"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setLocation"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setSavePath"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setShareLimits"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setSSLParameters"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setSuperSeeding"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setTags"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"setUploadLimit"_s}, Http::METHOD_POST},
        {{u"transfer"_s, u"setDownloadLimit"_s}, Http::METHOD_POST},
        {{u"transfer"_s, u"setSpeedLimitsMode"_s}, Http::METHOD_POST},
        {{u"transfer"_s, u"setUploadLimit"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"start"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"stop"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"toggleFirstLastPiecePrio"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"toggleSequentialDownload"_s}, Http::METHOD_POST},
        {{u"transfer"_s, u"toggleSpeedLimitsMode"_s}, Http::METHOD_POST},
        {{u"torrents"_s, u"topPrio"_s}, Http::METHOD_POST},
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
    bool m_isLocalAuthEnabled = false;
    bool m_isAuthSubnetWhitelistEnabled = false;
    QList<Utils::Net::Subnet> m_authSubnetWhitelist;
    int m_sessionTimeout = 0;
    QString m_sessionCookieName;

    // security related
    QStringList m_domainList;
    bool m_isCSRFProtectionEnabled = true;
    bool m_isSecureCookieEnabled = true;
    bool m_isHostHeaderValidationEnabled = true;
    bool m_isHttpsEnabled = false;

    // Reverse proxy
    bool m_isReverseProxySupportEnabled = false;
    QList<Utils::Net::Subnet> m_trustedReverseProxyList;
    QHostAddress m_clientAddress;

    QList<Http::Header> m_prebuiltHeaders;

    Utils::Thread::UniquePtr m_workerThread;
    FreeDiskSpaceChecker *m_freeDiskSpaceChecker = nullptr;
    QTimer *m_freeDiskSpaceCheckingTimer = nullptr;
    BitTorrent::TorrentCreationManager *m_torrentCreationManager = nullptr;
};
