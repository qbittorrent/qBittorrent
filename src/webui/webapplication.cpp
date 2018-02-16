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

#include "webapplication.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegExp>
#include <QTimer>
#include <QUrl>

#include "base/http/httperror.h"
#include "base/iconprovider.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "api/apierror.h"
#include "api/appcontroller.h"
#include "api/authcontroller.h"
#include "api/logcontroller.h"
#include "api/rsscontroller.h"
#include "api/synccontroller.h"
#include "api/torrentscontroller.h"
#include "api/transfercontroller.h"

constexpr int MAX_ALLOWED_FILESIZE = 10 * 1024 * 1024;

const QString PATH_PREFIX_IMAGES {"/images/"};
const QString PATH_PREFIX_THEME {"/theme/"};
const QString WWW_FOLDER {":/www"};
const QString PUBLIC_FOLDER {"/public"};
const QString PRIVATE_FOLDER {"/private"};
const QString MAX_AGE_MONTH {"public, max-age=2592000"};

namespace
{
    QStringMap parseCookie(const QString &cookieStr)
    {
        // [rfc6265] 4.2.1. Syntax
        QStringMap ret;
        const QVector<QStringRef> cookies = cookieStr.splitRef(';', QString::SkipEmptyParts);

        for (const auto &cookie : cookies) {
            const int idx = cookie.indexOf('=');
            if (idx < 0)
                continue;

            const QString name = cookie.left(idx).trimmed().toString();
            const QString value = Utils::String::unquote(cookie.mid(idx + 1).trimmed()).toString();
            ret.insert(name, value);
        }
        return ret;
    }

    void translateDocument(QString &data)
    {
        const QRegExp regex("QBT_TR\\((([^\\)]|\\)(?!QBT_TR))+)\\)QBT_TR(\\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\\])");
        const QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
        int i = 0;
        bool found = true;

        const QString locale = Preferences::instance()->getLocale();
        bool isTranslationNeeded = !locale.startsWith("en") || locale.startsWith("en_AU") || locale.startsWith("en_GB");

        while (i < data.size() && found) {
            i = regex.indexIn(data, i);
            if (i >= 0) {
                //qDebug("Found translatable string: %s", regex.cap(1).toUtf8().data());
                QByteArray word = regex.cap(1).toUtf8();

                QString translation = word;
                if (isTranslationNeeded) {
                    QString context = regex.cap(4);
                    translation = qApp->translate(context.toUtf8().constData(), word.constData(), 0, 1);
                }
                // Remove keyboard shortcuts
                translation.replace(mnemonic, "");

                // Use HTML code for quotes to prevent issues with JS
                translation.replace("'", "&#39;");
                translation.replace("\"", "&#34;");

                data.replace(i, regex.matchedLength(), translation);
                i += translation.length();
            }
            else {
                found = false; // no more translatable strings
            }

            data.replace(QLatin1String("${LANG}"), locale.left(2));
            data.replace(QLatin1String("${VERSION}"), QBT_VERSION);
        }
    }

    inline QUrl urlFromHostHeader(const QString &hostHeader)
    {
        if (!hostHeader.contains(QLatin1String("://")))
            return QUrl(QLatin1String("http://") + hostHeader);
        return hostHeader;
    }
}

WebApplication::WebApplication(QObject *parent)
    : QObject(parent)
{
    registerAPIController(QLatin1String("app"), new AppController(this, this));
    registerAPIController(QLatin1String("auth"), new AuthController(this, this));
    registerAPIController(QLatin1String("log"), new LogController(this, this));
    registerAPIController(QLatin1String("rss"), new RSSController(this, this));
    registerAPIController(QLatin1String("sync"), new SyncController(this, this));
    registerAPIController(QLatin1String("torrents"), new TorrentsController(this, this));
    registerAPIController(QLatin1String("transfer"), new TransferController(this, this));

    declarePublicAPI(QLatin1String("auth/login"));

    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &WebApplication::configure);
}

WebApplication::~WebApplication()
{
    // cleanup sessions data
    qDeleteAll(m_sessions);
}

void WebApplication::sendWebUIFile()
{
    const QStringList pathItems {request().path.split('/', QString::SkipEmptyParts)};
    if (pathItems.contains(".") || pathItems.contains(".."))
        throw InternalServerErrorHTTPError();

    if (!m_isAltUIUsed) {
        if (request().path.startsWith(PATH_PREFIX_IMAGES)) {
            const QString imageFilename {request().path.mid(PATH_PREFIX_IMAGES.size())};
            sendFile(QLatin1String(":/icons/") + imageFilename);
            header(Http::HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
            return;
        }

        if (request().path.startsWith(PATH_PREFIX_THEME)) {
            const QString iconId {request().path.mid(PATH_PREFIX_THEME.size())};
            sendFile(IconProvider::instance()->getIconPath(iconId));
            header(Http::HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
            return;
        }
    }

    const QString path {
        (request().path != QLatin1String("/")
                ? request().path
                : (session()
                   ? QLatin1String("/index.html")
                   : QLatin1String("/login.html")))
    };

    QString localPath {
        m_rootFolder
                + (session() ? PRIVATE_FOLDER : PUBLIC_FOLDER)
                + path
    };

    QFileInfo fileInfo {localPath};

    if (!fileInfo.exists() && session()) {
        // try to send public file if there is no private one
        localPath = m_rootFolder + PUBLIC_FOLDER + path;
        fileInfo.setFile(localPath);
    }

    if (m_isAltUIUsed) {
#ifdef Q_OS_UNIX
        if (!Utils::Fs::isRegularFile(localPath)) {
            status(500, "Internal Server Error");
            print(tr("Unacceptable file type, only regular file is allowed."), Http::CONTENT_TYPE_TXT);
            return;
        }
#endif

        while (fileInfo.filePath() != m_rootFolder) {
            if (fileInfo.isSymLink())
                throw InternalServerErrorHTTPError(tr("Symlinks inside alternative UI folder are forbidden."));

            fileInfo.setFile(fileInfo.path());
        }
    }

    sendFile(localPath);
}

WebSession *WebApplication::session()
{
    return m_currentSession;
}

const Http::Request &WebApplication::request() const
{
    return m_request;
}

const Http::Environment &WebApplication::env() const
{
    return m_env;
}

void WebApplication::doProcessRequest()
{
    QStringMap *params = &((request().method == QLatin1String("GET"))
                ? m_request.gets : m_request.posts);
    QString scope, action;

    const auto findAPICall = [&]() -> bool
    {
        QRegularExpressionMatch match = m_apiPathPattern.match(request().path);
        if (!match.hasMatch()) return false;

        action = match.captured(QLatin1String("action"));
        scope = match.captured(QLatin1String("scope"));
        return true;
    };

    const auto findLegacyAPICall = [&]() -> bool
    {
        QRegularExpressionMatch match = m_apiLegacyPathPattern.match(request().path);
        if (!match.hasMatch()) return false;

        struct APICompatInfo
        {
            QString scope;
            QString action;
        };
        const QMap<QString, APICompatInfo> APICompatMapping {
            {"sync/maindata", {"sync", "maindata"}},
            {"sync/torrent_peers", {"sync", "torrentPeers"}},

            {"login", {"auth", "login"}},
            {"logout", {"auth", "logout"}},

            {"command/shutdown", {"app", "shutdown"}},
            {"query/preferences", {"app", "preferences"}},
            {"command/setPreferences", {"app", "setPreferences"}},
            {"command/getSavePath", {"app", "defaultSavePath"}},
            {"version/qbittorrent", {"app", "version"}},

            {"query/getLog", {"log", "main"}},
            {"query/getPeerLog", {"log", "peers"}},

            {"query/torrents", {"torrents", "info"}},
            {"query/propertiesGeneral", {"torrents", "properties"}},
            {"query/propertiesTrackers", {"torrents", "trackers"}},
            {"query/propertiesWebSeeds", {"torrents", "webseeds"}},
            {"query/propertiesFiles", {"torrents", "files"}},
            {"query/getPieceHashes", {"torrents", "pieceHashes"}},
            {"query/getPieceStates", {"torrents", "pieceStates"}},
            {"command/resume", {"torrents", "resume"}},
            {"command/pause", {"torrents", "pause"}},
            {"command/recheck", {"torrents", "recheck"}},
            {"command/resumeAll", {"torrents", "resume"}},
            {"command/pauseAll", {"torrents", "pause"}},
            {"command/rename", {"torrents", "rename"}},
            {"command/download", {"torrents", "add"}},
            {"command/upload", {"torrents", "add"}},
            {"command/delete", {"torrents", "delete"}},
            {"command/deletePerm", {"torrents", "delete"}},
            {"command/addTrackers", {"torrents", "addTrackers"}},
            {"command/setFilePrio", {"torrents", "filePrio"}},
            {"command/setCategory", {"torrents", "setCategory"}},
            {"command/addCategory", {"torrents", "createCategory"}},
            {"command/removeCategories", {"torrents", "removeCategories"}},
            {"command/getTorrentsUpLimit", {"torrents", "uploadLimit"}},
            {"command/getTorrentsDlLimit", {"torrents", "downloadLimit"}},
            {"command/setTorrentsUpLimit", {"torrents", "setUploadLimit"}},
            {"command/setTorrentsDlLimit", {"torrents", "setDownloadLimit"}},
            {"command/increasePrio", {"torrents", "increasePrio"}},
            {"command/decreasePrio", {"torrents", "decreasePrio"}},
            {"command/topPrio", {"torrents", "topPrio"}},
            {"command/bottomPrio", {"torrents", "bottomPrio"}},
            {"command/setLocation", {"torrents", "setLocation"}},
            {"command/setAutoTMM", {"torrents", "setAutoManagement"}},
            {"command/setSuperSeeding", {"torrents", "setSuperSeeding"}},
            {"command/setForceStart", {"torrents", "setForceStart"}},
            {"command/toggleSequentialDownload", {"torrents", "toggleSequentialDownload"}},
            {"command/toggleFirstLastPiecePrio", {"torrents", "toggleFirstLastPiecePrio"}},

            {"query/transferInfo", {"transfer", "info"}},
            {"command/alternativeSpeedLimitsEnabled", {"transfer", "speedLimitsMode"}},
            {"command/toggleAlternativeSpeedLimits", {"transfer", "toggleSpeedLimitsMode"}},
            {"command/getGlobalUpLimit", {"transfer", "uploadLimit"}},
            {"command/getGlobalDlLimit", {"transfer", "downloadLimit"}},
            {"command/setGlobalUpLimit", {"transfer", "setUploadLimit"}},
            {"command/setGlobalDlLimit", {"transfer", "setDownloadLimit"}}
        };

        const QString legacyAction {match.captured(QLatin1String("action"))};
        const APICompatInfo compatInfo = APICompatMapping.value(legacyAction);

        scope = compatInfo.scope;
        action = compatInfo.action;

        if (legacyAction == QLatin1String("command/delete"))
            (*params)["deleteFiles"] = "false";
        else if (legacyAction == QLatin1String("command/deletePerm"))
            (*params)["deleteFiles"] = "true";

        const QString hash {match.captured(QLatin1String("hash"))};
        (*params)[QLatin1String("hash")] = hash;

        return true;
    };

    if (!findAPICall())
        findLegacyAPICall();

    APIController *controller = m_apiControllers.value(scope);
    if (!controller) {
        if (request().path == QLatin1String("/version/api")) {
            print(QString(COMPAT_API_VERSION), Http::CONTENT_TYPE_TXT);
            return;
        }

        if (request().path == QLatin1String("/version/api_min")) {
            print(QString(COMPAT_API_VERSION_MIN), Http::CONTENT_TYPE_TXT);
            return;
        }

        sendWebUIFile();
    }
    else {
        if (!session() && !isPublicAPI(scope, action))
            throw ForbiddenHTTPError();

        DataMap data;
        for (const Http::UploadedFile &torrent : request().files)
            data[torrent.filename] = torrent.data;

        try {
            const QVariant result = controller->run(action, *params, data);
            switch (result.userType()) {
            case QMetaType::QString:
                print(result.toString(), Http::CONTENT_TYPE_TXT);
                break;
            case QMetaType::QJsonDocument:
                print(result.toJsonDocument().toJson(QJsonDocument::Compact), Http::CONTENT_TYPE_JSON);
                break;
            default:
                print(result.toString(), Http::CONTENT_TYPE_TXT);
                break;
            }
        }
        catch (const APIError &error) {
            // re-throw as HTTPError
            switch (error.type()) {
            case APIErrorType::AccessDenied:
                throw ForbiddenHTTPError(error.message());
            case APIErrorType::BadData:
                throw UnsupportedMediaTypeHTTPError(error.message());
            case APIErrorType::BadParams:
                throw BadRequestHTTPError(error.message());
            case APIErrorType::Conflict:
                throw ConflictHTTPError(error.message());
            case APIErrorType::NotFound:
                throw NotFoundHTTPError(error.message());
            default:
                Q_ASSERT(false);
            }
        }
    }
}

void WebApplication::configure()
{
    const auto pref = Preferences::instance();

    m_domainList = Preferences::instance()->getServerDomains().split(';', QString::SkipEmptyParts);
    std::for_each(m_domainList.begin(), m_domainList.end(), [](QString &entry) { entry = entry.trimmed(); });

    const QString rootFolder = Utils::Fs::expandPathAbs(
                !pref->isAltWebUiEnabled() ? WWW_FOLDER : pref->getWebUiRootFolder());
    if (rootFolder != m_rootFolder) {
        m_translatedFiles.clear();
        m_rootFolder = rootFolder;
    }
}

void WebApplication::registerAPIController(const QString &scope, APIController *controller)
{
    Q_ASSERT(controller);
    Q_ASSERT(!m_apiControllers.value(scope));

    m_apiControllers[scope] = controller;
}

void WebApplication::declarePublicAPI(const QString &apiPath)
{
    m_publicAPIs << apiPath;
}

void WebApplication::sendFile(const QString &path)
{
    const QDateTime lastModified {QFileInfo(path).lastModified()};

    // find translated file in cache
    auto it = m_translatedFiles.constFind(path);
    if ((it != m_translatedFiles.constEnd()) && (lastModified <= (*it).lastModified)) {
        print((*it).data, QMimeDatabase().mimeTypeForFileNameAndData(path, (*it).data).name());
        return;
    }

    QFile file {path};
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug("File %s was not found!", qUtf8Printable(path));
        throw NotFoundHTTPError();
    }
    
    if (file.size() > MAX_ALLOWED_FILESIZE) {
        qWarning("%s: exceeded the maximum allowed file size!", qUtf8Printable(path));
        throw InternalServerErrorHTTPError(tr("Exceeded the maximum allowed file size (%1)!")
                                           .arg(Utils::Misc::friendlyUnit(MAX_ALLOWED_FILESIZE)));
    }

    QByteArray data {file.readAll()};
    file.close();

    const QMimeType type {QMimeDatabase().mimeTypeForFileNameAndData(path, data)};
    const bool isTranslatable {type.inherits(QLatin1String("text/plain"))};

    // Translate the file
    if (isTranslatable) {
        QString dataStr {data};
        translateDocument(dataStr);
        data = dataStr.toUtf8();

        m_translatedFiles[path] = {data, lastModified}; // caching translated file
    }

    print(data, type.name());
}

Http::Response WebApplication::processRequest(const Http::Request &request, const Http::Environment &env)
{
    m_currentSession = nullptr;
    m_request = request;
    m_env = env;

    // clear response
    clear();

    try {
        // block cross-site requests
        if (isCrossSiteRequest(m_request) || !validateHostHeader(m_domainList))
            throw UnauthorizedHTTPError();

        sessionInitialize();
        doProcessRequest();
    }
    catch (const HTTPError &error) {
        status(error.statusCode(), error.statusText());
        if (!error.message().isEmpty())
            print(error.message(), Http::CONTENT_TYPE_TXT);
    }

    // avoid clickjacking attacks
    header(Http::HEADER_X_FRAME_OPTIONS, "SAMEORIGIN");
    header(Http::HEADER_X_XSS_PROTECTION, "1; mode=block");
    header(Http::HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff");
    header(Http::HEADER_CONTENT_SECURITY_POLICY, "default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; object-src 'none';");

    return response();
}

QString WebApplication::clientId() const
{
    return env().clientAddress.toString();
}

void WebApplication::sessionInitialize()
{
    Q_ASSERT(!m_currentSession);

    const QString sessionId {parseCookie(m_request.headers.value(QLatin1String("cookie"))).value(C_SID)};

    // TODO: Additional session check

    if (!sessionId.isEmpty()) {
        m_currentSession = m_sessions.value(sessionId);
        if (m_currentSession) {
            const uint now = QDateTime::currentDateTime().toTime_t();
            if ((now - m_currentSession->m_timestamp) > INACTIVE_TIME) {
                // session is outdated - removing it
                delete m_sessions.take(sessionId);
                m_currentSession = nullptr;
            }
            else {
                m_currentSession->updateTimestamp();
            }
        }
        else {
            qDebug() << Q_FUNC_INFO << "session does not exist!";
        }
    }

    if (!m_currentSession && !isAuthNeeded())
        sessionStart();
}

QString WebApplication::generateSid() const
{
    QString sid;

    do {
        const size_t size = 6;
        quint32 tmp[size];

        for (size_t i = 0; i < size; ++i)
            tmp[i] = Utils::Random::rand();

        sid = QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(quint32) * size).toBase64();
    }
    while (m_sessions.contains(sid));

    return sid;
}

bool WebApplication::isAuthNeeded()
{
    qDebug("Checking auth rules against client address %s", qPrintable(m_env.clientAddress.toString()));
    const Preferences *pref = Preferences::instance();
    if (!pref->isWebUiLocalAuthEnabled() && Utils::Net::isLoopbackAddress(m_env.clientAddress))
        return false;
    if (pref->isWebUiAuthSubnetWhitelistEnabled() && Utils::Net::isIPInRange(m_env.clientAddress, pref->getWebUiAuthSubnetWhitelist()))
        return false;
    return true;
}

bool WebApplication::isPublicAPI(const QString &scope, const QString &action) const
{
    return m_publicAPIs.contains(QString::fromLatin1("%1/%2").arg(scope).arg(action));
}

void WebApplication::sessionStart()
{
    Q_ASSERT(!m_currentSession);

    // remove outdated sessions
    const uint now = QDateTime::currentDateTime().toTime_t();
    foreach (const auto session, m_sessions) {
        if ((now - session->timestamp()) > INACTIVE_TIME)
            delete m_sessions.take(session->id());
    }

    m_currentSession = new WebSession(generateSid());
    m_sessions[m_currentSession->id()] = m_currentSession;

    QNetworkCookie cookie(C_SID, m_currentSession->id().toUtf8());
    cookie.setHttpOnly(true);
    cookie.setPath(QLatin1String("/"));
    header(Http::HEADER_SET_COOKIE, cookie.toRawForm());
}

void WebApplication::sessionEnd()
{
    Q_ASSERT(m_currentSession);

    QNetworkCookie cookie(C_SID);
    cookie.setPath(QLatin1String("/"));
    cookie.setExpirationDate(QDateTime::currentDateTime().addDays(-1));

    delete m_sessions.take(m_currentSession->id());
    m_currentSession = nullptr;

    header(Http::HEADER_SET_COOKIE, cookie.toRawForm());
}

bool WebApplication::isCrossSiteRequest(const Http::Request &request) const
{
    // https://www.owasp.org/index.php/Cross-Site_Request_Forgery_(CSRF)_Prevention_Cheat_Sheet#Verifying_Same_Origin_with_Standard_Headers

    const auto isSameOrigin = [](const QUrl &left, const QUrl &right) -> bool
    {
        // [rfc6454] 5. Comparing Origins
        return ((left.port() == right.port())
                // && (left.scheme() == right.scheme())  // not present in this context
                && (left.host() == right.host()));
    };

    const QString targetOrigin = request.headers.value(Http::HEADER_X_FORWARDED_HOST, request.headers.value(Http::HEADER_HOST));
    const QString originValue = request.headers.value(Http::HEADER_ORIGIN);
    const QString refererValue = request.headers.value(Http::HEADER_REFERER);

    if (originValue.isEmpty() && refererValue.isEmpty()) {
        // owasp.org recommends to block this request, but doing so will inevitably lead Web API users to spoof headers
        // so lets be permissive here
        return false;
    }

    // sent with CORS requests, as well as with POST requests
    if (!originValue.isEmpty()) {
        const bool isInvalid = !isSameOrigin(urlFromHostHeader(targetOrigin), originValue);
        if (isInvalid)
            LogMsg(tr("WebUI: Origin header & Target origin mismatch! Source IP: '%1'. Origin header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString()).arg(originValue).arg(targetOrigin)
                   , Log::WARNING);
        return isInvalid;
    }

    if (!refererValue.isEmpty()) {
        const bool isInvalid = !isSameOrigin(urlFromHostHeader(targetOrigin), refererValue);
        if (isInvalid)
            LogMsg(tr("WebUI: Referer header & Target origin mismatch! Source IP: '%1'. Referer header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString()).arg(refererValue).arg(targetOrigin)
                   , Log::WARNING);
        return isInvalid;
    }

    return true;
}

bool WebApplication::validateHostHeader(const QStringList &domains) const
{
    const QUrl hostHeader = urlFromHostHeader(m_request.headers[Http::HEADER_HOST]);
    const QString requestHost = hostHeader.host();

    // (if present) try matching host header's port with local port
    const int requestPort = hostHeader.port();
    if ((requestPort != -1) && (m_env.localPort != requestPort)) {
        LogMsg(tr("WebUI: Invalid Host header, port mismatch. Request source IP: '%1'. Server port: '%2'. Received Host header: '%3'")
               .arg(m_env.clientAddress.toString()).arg(m_env.localPort)
               .arg(m_request.headers[Http::HEADER_HOST])
                , Log::WARNING);
        return false;
    }

    // try matching host header with local address
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    const bool sameAddr = m_env.localAddress.isEqual(QHostAddress(requestHost));
#else
    const auto equal = [](const Q_IPV6ADDR &l, const Q_IPV6ADDR &r) -> bool
    {
        for (int i = 0; i < 16; ++i) {
            if (l[i] != r[i])
                return false;
        }
        return true;
    };
    const bool sameAddr = equal(m_env.localAddress.toIPv6Address(), QHostAddress(requestHost).toIPv6Address());
#endif

    if (sameAddr)
        return true;

    // try matching host header with domain list
    for (const auto &domain : domains) {
        QRegExp domainRegex(domain, Qt::CaseInsensitive, QRegExp::Wildcard);
        if (requestHost.contains(domainRegex))
            return true;
    }

    LogMsg(tr("WebUI: Invalid Host header. Request source IP: '%1'. Received Host header: '%2'")
           .arg(m_env.clientAddress.toString()).arg(m_request.headers[Http::HEADER_HOST])
            , Log::WARNING);
    return false;
}

// WebSession

WebSession::WebSession(const QString &sid)
    : m_sid {sid}
{
    updateTimestamp();
}

QString WebSession::id() const
{
    return m_sid;
}

uint WebSession::timestamp() const
{
    return m_timestamp;
}

QVariant WebSession::getData(const QString &id) const
{
    return m_data.value(id);
}

void WebSession::setData(const QString &id, const QVariant &data)
{
    m_data[id] = data;
}

void WebSession::updateTimestamp()
{
    m_timestamp = QDateTime::currentDateTime().toTime_t();
}
