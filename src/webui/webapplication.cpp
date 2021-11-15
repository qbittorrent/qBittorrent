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

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QUrl>

#include "base/algorithm.h"
#include "base/global.h"
#include "base/http/httperror.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/types.h"
#include "base/utils/bytearray.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "api/apierror.h"
#include "api/appcontroller.h"
#include "api/authcontroller.h"
#include "api/logcontroller.h"
#include "api/rsscontroller.h"
#include "api/searchcontroller.h"
#include "api/synccontroller.h"
#include "api/torrentscontroller.h"
#include "api/transfercontroller.h"

const int MAX_ALLOWED_FILESIZE = 10 * 1024 * 1024;
const char C_SID[] = "SID"; // name of session id cookie

const QString PATH_PREFIX_ICONS {QStringLiteral("/icons/")};
const QString WWW_FOLDER {QStringLiteral(":/www")};
const QString PUBLIC_FOLDER {QStringLiteral("/public")};
const QString PRIVATE_FOLDER {QStringLiteral("/private")};

namespace
{
    QStringMap parseCookie(const QStringView cookieStr)
    {
        // [rfc6265] 4.2.1. Syntax
        QStringMap ret;
        const QList<QStringView> cookies = cookieStr.split(u';', Qt::SkipEmptyParts);

        for (const auto &cookie : cookies)
        {
            const int idx = cookie.indexOf('=');
            if (idx < 0)
                continue;

            const QString name = cookie.left(idx).trimmed().toString();
            const QString value = Utils::String::unquote(cookie.mid(idx + 1).trimmed()).toString();
            ret.insert(name, value);
        }
        return ret;
    }

    QUrl urlFromHostHeader(const QString &hostHeader)
    {
        if (!hostHeader.contains(QLatin1String("://")))
            return {QLatin1String("http://") + hostHeader};
        return hostHeader;
    }

    QString getCachingInterval(QString contentType)
    {
        contentType = contentType.toLower();

        if (contentType.startsWith(QLatin1String("image/")))
            return QLatin1String("private, max-age=604800");  // 1 week

        if ((contentType == Http::CONTENT_TYPE_CSS)
            || (contentType == Http::CONTENT_TYPE_JS))
            {
            // short interval in case of program update
            return QLatin1String("private, max-age=43200");  // 12 hrs
        }

        return QLatin1String("no-store");
    }
}

WebApplication::WebApplication(QObject *parent)
    : QObject(parent)
    , m_cacheID {QString::number(Utils::Random::rand(), 36)}
{
    registerAPIController(QLatin1String("app"), new AppController(this, this));
    registerAPIController(QLatin1String("auth"), new AuthController(this, this));
    registerAPIController(QLatin1String("log"), new LogController(this, this));
    registerAPIController(QLatin1String("rss"), new RSSController(this, this));
    registerAPIController(QLatin1String("search"), new SearchController(this, this));
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
    const QStringList pathItems {request().path.split('/', Qt::SkipEmptyParts)};
    if (pathItems.contains(".") || pathItems.contains(".."))
        throw InternalServerErrorHTTPError();

    if (!m_isAltUIUsed)
    {
        if (request().path.startsWith(PATH_PREFIX_ICONS))
        {
            const Path imageFilename {request().path.mid(PATH_PREFIX_ICONS.size())};
            sendFile(Path(":/icons") / imageFilename);
            return;
        }
    }

    const QString path
    {
        (request().path != QLatin1String("/")
                ? request().path
                : QLatin1String("/index.html"))
    };

    Path localPath = m_rootFolder
                / Path(session() ? PRIVATE_FOLDER : PUBLIC_FOLDER)
                / Path(path);
    if (!localPath.exists() && session())
    {
        // try to send public file if there is no private one
        localPath = m_rootFolder / Path(PUBLIC_FOLDER) / Path(path);
    }

    if (m_isAltUIUsed)
    {
#ifdef Q_OS_UNIX
        if (!Utils::Fs::isRegularFile(localPath))
        {
            status(500, "Internal Server Error");
            print(tr("Unacceptable file type, only regular file is allowed."), Http::CONTENT_TYPE_TXT);
            return;
        }
#endif

        QFileInfo fileInfo {localPath.data()};
        while (Path(fileInfo.filePath()) != m_rootFolder)
        {
            if (fileInfo.isSymLink())
                throw InternalServerErrorHTTPError(tr("Symlinks inside alternative UI folder are forbidden."));

            fileInfo.setFile(fileInfo.path());
        }
    }

    sendFile(localPath);
}

void WebApplication::translateDocument(QString &data) const
{
    const QRegularExpression regex("QBT_TR\\((([^\\)]|\\)(?!QBT_TR))+)\\)QBT_TR\\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\\]");

    int i = 0;
    bool found = true;
    while (i < data.size() && found)
    {
        QRegularExpressionMatch regexMatch;
        i = data.indexOf(regex, i, &regexMatch);
        if (i >= 0)
        {
            const QString sourceText = regexMatch.captured(1);
            const QString context = regexMatch.captured(3);

            const QString loadedText = m_translationFileLoaded
                ? m_translator.translate(context.toUtf8().constData(), sourceText.toUtf8().constData())
                : QString();
            // `loadedText` is empty when translation is not provided
            // it should fallback to `sourceText`
            QString translation = loadedText.isEmpty() ? sourceText : loadedText;

            // Use HTML code for quotes to prevent issues with JS
            translation.replace('\'', "&#39;");
            translation.replace('\"', "&#34;");

            data.replace(i, regexMatch.capturedLength(), translation);
            i += translation.length();
        }
        else
        {
            found = false; // no more translatable strings
        }

        data.replace(QLatin1String("${LANG}"), m_currentLocale.left(2));
        data.replace(QLatin1String("${CACHEID}"), m_cacheID);
    }
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
    const QRegularExpressionMatch match = m_apiPathPattern.match(request().path);
    if (!match.hasMatch())
    {
        sendWebUIFile();
        return;
    }

    const QString action = match.captured(QLatin1String("action"));
    const QString scope = match.captured(QLatin1String("scope"));

    APIController *controller = m_apiControllers.value(scope);
    if (!controller)
        throw NotFoundHTTPError();

    if (!session() && !isPublicAPI(scope, action))
        throw ForbiddenHTTPError();

    DataMap data;
    for (const Http::UploadedFile &torrent : request().files)
        data[torrent.filename] = torrent.data;

    try
    {
        const QVariant result = controller->run(action, m_params, data);
        switch (result.userType())
        {
        case QMetaType::QJsonDocument:
            print(result.toJsonDocument().toJson(QJsonDocument::Compact), Http::CONTENT_TYPE_JSON);
            break;
        case QMetaType::QString:
        default:
            print(result.toString(), Http::CONTENT_TYPE_TXT);
            break;
        }
    }
    catch (const APIError &error)
    {
        // re-throw as HTTPError
        switch (error.type())
        {
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

void WebApplication::configure()
{
    const auto *pref = Preferences::instance();

    const bool isAltUIUsed = pref->isAltWebUiEnabled();
    const Path rootFolder = (!isAltUIUsed ? Path(WWW_FOLDER) : pref->getWebUiRootFolder());
    if ((isAltUIUsed != m_isAltUIUsed) || (rootFolder != m_rootFolder))
    {
        m_isAltUIUsed = isAltUIUsed;
        m_rootFolder = rootFolder;
        m_translatedFiles.clear();
        if (!m_isAltUIUsed)
            LogMsg(tr("Using built-in Web UI."));
        else
            LogMsg(tr("Using custom Web UI. Location: \"%1\".").arg(m_rootFolder.toString()));
    }

    const QString newLocale = pref->getLocale();
    if (m_currentLocale != newLocale)
    {
        m_currentLocale = newLocale;
        m_translatedFiles.clear();

        m_translationFileLoaded = m_translator.load((m_rootFolder / Path("translations/webui_") + newLocale).data());
        if (m_translationFileLoaded)
        {
            LogMsg(tr("Web UI translation for selected locale (%1) has been successfully loaded.")
                   .arg(newLocale));
        }
        else
        {
            LogMsg(tr("Couldn't load Web UI translation for selected locale (%1).").arg(newLocale), Log::WARNING);
        }
    }

    m_isLocalAuthEnabled = pref->isWebUiLocalAuthEnabled();
    m_isAuthSubnetWhitelistEnabled = pref->isWebUiAuthSubnetWhitelistEnabled();
    m_authSubnetWhitelist = pref->getWebUiAuthSubnetWhitelist();
    m_sessionTimeout = pref->getWebUISessionTimeout();

    m_domainList = pref->getServerDomains().split(';', Qt::SkipEmptyParts);
    std::for_each(m_domainList.begin(), m_domainList.end(), [](QString &entry) { entry = entry.trimmed(); });

    m_isCSRFProtectionEnabled = pref->isWebUiCSRFProtectionEnabled();
    m_isSecureCookieEnabled = pref->isWebUiSecureCookieEnabled();
    m_isHostHeaderValidationEnabled = pref->isWebUIHostHeaderValidationEnabled();
    m_isHttpsEnabled = pref->isWebUiHttpsEnabled();

    m_prebuiltHeaders.clear();
    m_prebuiltHeaders.push_back({QLatin1String(Http::HEADER_X_XSS_PROTECTION), QLatin1String("1; mode=block")});
    m_prebuiltHeaders.push_back({QLatin1String(Http::HEADER_X_CONTENT_TYPE_OPTIONS), QLatin1String("nosniff")});

    if (!m_isAltUIUsed)
        m_prebuiltHeaders.push_back({QLatin1String(Http::HEADER_REFERRER_POLICY), QLatin1String("same-origin")});

    const bool isClickjackingProtectionEnabled = pref->isWebUiClickjackingProtectionEnabled();
    if (isClickjackingProtectionEnabled)
        m_prebuiltHeaders.push_back({QLatin1String(Http::HEADER_X_FRAME_OPTIONS), QLatin1String("SAMEORIGIN")});

    const QString contentSecurityPolicy =
        (m_isAltUIUsed
            ? QLatin1String("")
            : QLatin1String("default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; object-src 'none'; form-action 'self';"))
        + (isClickjackingProtectionEnabled ? QLatin1String(" frame-ancestors 'self';") : QLatin1String(""))
        + (m_isHttpsEnabled ? QLatin1String(" upgrade-insecure-requests;") : QLatin1String(""));
    if (!contentSecurityPolicy.isEmpty())
        m_prebuiltHeaders.push_back({QLatin1String(Http::HEADER_CONTENT_SECURITY_POLICY), contentSecurityPolicy});

    if (pref->isWebUICustomHTTPHeadersEnabled())
    {
        const QString customHeaders = pref->getWebUICustomHTTPHeaders();
        const QList<QStringView> customHeaderLines = QStringView(customHeaders).trimmed().split(u'\n', Qt::SkipEmptyParts);

        for (const QStringView line : customHeaderLines)
        {
            const int idx = line.indexOf(':');
            if (idx < 0)
            {
                // require separator `:` to be present even if `value` field can be empty
                LogMsg(tr("Missing ':' separator in WebUI custom HTTP header: \"%1\"").arg(line.toString()), Log::WARNING);
                continue;
            }

            const QString header = line.left(idx).trimmed().toString();
            const QString value = line.mid(idx + 1).trimmed().toString();
            m_prebuiltHeaders.push_back({header, value});
        }
    }

    m_isReverseProxySupportEnabled = pref->isWebUIReverseProxySupportEnabled();
    if (m_isReverseProxySupportEnabled)
    {
        m_trustedReverseProxyList.clear();

        const QStringList proxyList = pref->getWebUITrustedReverseProxiesList().split(';', Qt::SkipEmptyParts);

        for (const QString &proxy : proxyList)
        {
            QHostAddress ip;
            if (ip.setAddress(proxy))
                m_trustedReverseProxyList.push_back(ip);
        }

        if (m_trustedReverseProxyList.isEmpty())
            m_isReverseProxySupportEnabled = false;
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

void WebApplication::sendFile(const Path &path)
{
    const QDateTime lastModified = Utils::Fs::lastModified(path);

    // find translated file in cache
    const auto it = m_translatedFiles.constFind(path);
    if ((it != m_translatedFiles.constEnd()) && (lastModified <= it->lastModified))
    {
        print(it->data, it->mimeType);
        setHeader({Http::HEADER_CACHE_CONTROL, getCachingInterval(it->mimeType)});
        return;
    }

    QFile file {path.data()};
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug("File %s was not found!", qUtf8Printable(path.toString()));
        throw NotFoundHTTPError();
    }

    if (file.size() > MAX_ALLOWED_FILESIZE)
    {
        qWarning("%s: exceeded the maximum allowed file size!", qUtf8Printable(path.toString()));
        throw InternalServerErrorHTTPError(tr("Exceeded the maximum allowed file size (%1)!")
                                           .arg(Utils::Misc::friendlyUnit(MAX_ALLOWED_FILESIZE)));
    }

    QByteArray data {file.readAll()};
    file.close();

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFileNameAndData(path.data(), data);
    const bool isTranslatable = mimeType.inherits(QLatin1String("text/plain"));

    // Translate the file
    if (isTranslatable)
    {
        QString dataStr {data};
        translateDocument(dataStr);
        data = dataStr.toUtf8();

        m_translatedFiles[path] = {data, mimeType.name(), lastModified}; // caching translated file
    }

    print(data, mimeType.name());
    setHeader({Http::HEADER_CACHE_CONTROL, getCachingInterval(mimeType.name())});
}

Http::Response WebApplication::processRequest(const Http::Request &request, const Http::Environment &env)
{
    m_currentSession = nullptr;
    m_request = request;
    m_env = env;
    m_params.clear();

    if (m_request.method == Http::METHOD_GET)
    {
        for (auto iter = m_request.query.cbegin(); iter != m_request.query.cend(); ++iter)
            m_params[iter.key()] = QString::fromUtf8(iter.value());
    }
    else
    {
        m_params = m_request.posts;
    }

    // clear response
    clear();

    try
    {
        // block suspicious requests
        if ((m_isCSRFProtectionEnabled && isCrossSiteRequest(m_request))
            || (m_isHostHeaderValidationEnabled && !validateHostHeader(m_domainList)))
            {
            throw UnauthorizedHTTPError();
        }

        // reverse proxy resolve client address
        m_clientAddress = resolveClientAddress();

        sessionInitialize();
        doProcessRequest();
    }
    catch (const HTTPError &error)
    {
        status(error.statusCode(), error.statusText());
        print((!error.message().isEmpty() ? error.message() : error.statusText()), Http::CONTENT_TYPE_TXT);
    }

    for (const Http::Header &prebuiltHeader : asConst(m_prebuiltHeaders))
        setHeader(prebuiltHeader);

    return response();
}

QString WebApplication::clientId() const
{
    return m_clientAddress.toString();
}

void WebApplication::sessionInitialize()
{
    Q_ASSERT(!m_currentSession);

    const QString sessionId {parseCookie(m_request.headers.value(QLatin1String("cookie"))).value(C_SID)};

    // TODO: Additional session check

    if (!sessionId.isEmpty())
    {
        m_currentSession = m_sessions.value(sessionId);
        if (m_currentSession)
        {
            if (m_currentSession->hasExpired(m_sessionTimeout))
            {
                // session is outdated - removing it
                delete m_sessions.take(sessionId);
                m_currentSession = nullptr;
            }
            else
            {
                m_currentSession->updateTimestamp();
            }
        }
        else
        {
            qDebug() << Q_FUNC_INFO << "session does not exist!";
        }
    }

    if (!m_currentSession && !isAuthNeeded())
        sessionStart();
}

QString WebApplication::generateSid() const
{
    QString sid;

    do
    {
        const quint32 tmp[] =
        {Utils::Random::rand(), Utils::Random::rand(), Utils::Random::rand()
                , Utils::Random::rand(), Utils::Random::rand(), Utils::Random::rand()};
        sid = QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(tmp)).toBase64();
    }
    while (m_sessions.contains(sid));

    return sid;
}

bool WebApplication::isAuthNeeded()
{
    if (!m_isLocalAuthEnabled && Utils::Net::isLoopbackAddress(m_clientAddress))
        return false;
    if (m_isAuthSubnetWhitelistEnabled && Utils::Net::isIPInRange(m_clientAddress, m_authSubnetWhitelist))
        return false;
    return true;
}

bool WebApplication::isPublicAPI(const QString &scope, const QString &action) const
{
    return m_publicAPIs.contains(QString::fromLatin1("%1/%2").arg(scope, action));
}

void WebApplication::sessionStart()
{
    Q_ASSERT(!m_currentSession);

    // remove outdated sessions
    Algorithm::removeIf(m_sessions, [this](const QString &, const WebSession *session)
    {
        if (session->hasExpired(m_sessionTimeout))
        {
            delete session;
            return true;
        }

        return false;
    });

    m_currentSession = new WebSession(generateSid());
    m_sessions[m_currentSession->id()] = m_currentSession;

    QNetworkCookie cookie(C_SID, m_currentSession->id().toUtf8());
    cookie.setHttpOnly(true);
    cookie.setSecure(m_isSecureCookieEnabled && m_isHttpsEnabled);
    cookie.setPath(QLatin1String("/"));
    QByteArray cookieRawForm = cookie.toRawForm();
    if (m_isCSRFProtectionEnabled)
        cookieRawForm.append("; SameSite=Strict");
    setHeader({Http::HEADER_SET_COOKIE, cookieRawForm});
}

void WebApplication::sessionEnd()
{
    Q_ASSERT(m_currentSession);

    QNetworkCookie cookie(C_SID);
    cookie.setPath(QLatin1String("/"));
    cookie.setExpirationDate(QDateTime::currentDateTime().addDays(-1));

    delete m_sessions.take(m_currentSession->id());
    m_currentSession = nullptr;

    setHeader({Http::HEADER_SET_COOKIE, cookie.toRawForm()});
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

    if (originValue.isEmpty() && refererValue.isEmpty())
    {
        // owasp.org recommends to block this request, but doing so will inevitably lead Web API users to spoof headers
        // so lets be permissive here
        return false;
    }

    // sent with CORS requests, as well as with POST requests
    if (!originValue.isEmpty())
    {
        const bool isInvalid = !isSameOrigin(urlFromHostHeader(targetOrigin), originValue);
        if (isInvalid)
            LogMsg(tr("WebUI: Origin header & Target origin mismatch! Source IP: '%1'. Origin header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString(), originValue, targetOrigin)
                   , Log::WARNING);
        return isInvalid;
    }

    if (!refererValue.isEmpty())
    {
        const bool isInvalid = !isSameOrigin(urlFromHostHeader(targetOrigin), refererValue);
        if (isInvalid)
            LogMsg(tr("WebUI: Referer header & Target origin mismatch! Source IP: '%1'. Referer header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString(), refererValue, targetOrigin)
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
    if ((requestPort != -1) && (m_env.localPort != requestPort))
    {
        LogMsg(tr("WebUI: Invalid Host header, port mismatch. Request source IP: '%1'. Server port: '%2'. Received Host header: '%3'")
               .arg(m_env.clientAddress.toString()).arg(m_env.localPort)
               .arg(m_request.headers[Http::HEADER_HOST])
                , Log::WARNING);
        return false;
    }

    // try matching host header with local address
    const bool sameAddr = m_env.localAddress.isEqual(QHostAddress(requestHost));

    if (sameAddr)
        return true;

    // try matching host header with domain list
    for (const auto &domain : domains)
    {
        const QRegularExpression domainRegex {Utils::String::wildcardToRegexPattern(domain), QRegularExpression::CaseInsensitiveOption};
        if (requestHost.contains(domainRegex))
            return true;
    }

    LogMsg(tr("WebUI: Invalid Host header. Request source IP: '%1'. Received Host header: '%2'")
           .arg(m_env.clientAddress.toString(), m_request.headers[Http::HEADER_HOST])
            , Log::WARNING);
    return false;
}

QHostAddress WebApplication::resolveClientAddress() const
{
    if (!m_isReverseProxySupportEnabled)
        return m_env.clientAddress;

    // Only reverse proxy can overwrite client address
    if (!m_trustedReverseProxyList.contains(m_env.clientAddress))
        return m_env.clientAddress;

    const QString forwardedFor = m_request.headers.value(Http::HEADER_X_FORWARDED_FOR);

    if (!forwardedFor.isEmpty())
    {
        // client address is the 1st global IP in X-Forwarded-For or, if none available, the 1st IP in the list
        const QStringList remoteIpList = forwardedFor.split(',', Qt::SkipEmptyParts);

        if (!remoteIpList.isEmpty())
        {
            QHostAddress clientAddress;

            for (const QString &remoteIp : remoteIpList)
            {
                if (clientAddress.setAddress(remoteIp) && clientAddress.isGlobal())
                    return clientAddress;
            }

            if (clientAddress.setAddress(remoteIpList[0]))
                return clientAddress;
        }
    }

    return m_env.clientAddress;
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

bool WebSession::hasExpired(const qint64 seconds) const
{
    if (seconds <= 0)
        return false;
    return m_timer.hasExpired(seconds * 1000);
}

void WebSession::updateTimestamp()
{
    m_timer.start();
}

QVariant WebSession::getData(const QString &id) const
{
    return m_data.value(id);
}

void WebSession::setData(const QString &id, const QVariant &data)
{
    m_data[id] = data;
}
