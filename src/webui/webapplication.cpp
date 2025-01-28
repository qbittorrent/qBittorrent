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

#include "webapplication.h"

#include <algorithm>
#include <chrono>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "base/algorithm.h"
#include "base/bittorrent/torrentcreationmanager.h"
#include "base/http/httperror.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/types.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
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
#include "api/torrentcreatorcontroller.h"
#include "api/torrentscontroller.h"
#include "api/transfercontroller.h"
#include "freediskspacechecker.h"

const int MAX_ALLOWED_FILESIZE = 10 * 1024 * 1024;
const QString DEFAULT_SESSION_COOKIE_NAME = u"SID"_s;

const QString WWW_FOLDER = u":/www"_s;
const QString PUBLIC_FOLDER = u"/public"_s;
const QString PRIVATE_FOLDER = u"/private"_s;

using namespace std::chrono_literals;

const std::chrono::seconds FREEDISKSPACE_CHECK_TIMEOUT = 30s;

namespace
{
    QStringMap parseCookie(const QStringView cookieStr)
    {
        // [rfc6265] 4.2.1. Syntax
        QStringMap ret;
        const QList<QStringView> cookies = cookieStr.split(u';', Qt::SkipEmptyParts);

        for (const auto &cookie : cookies)
        {
            const int idx = cookie.indexOf(u'=');
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
        if (!hostHeader.contains(u"://"))
            return {u"http://"_s + hostHeader};
        return hostHeader;
    }

    QString getCachingInterval(QString contentType)
    {
        contentType = contentType.toLower();

        if (contentType.startsWith(u"image/"))
            return u"private, max-age=604800"_s;  // 1 week

        if ((contentType == Http::CONTENT_TYPE_CSS) || (contentType == Http::CONTENT_TYPE_JS))
        {
            // short interval in case of program update
            return u"private, max-age=43200"_s;  // 12 hrs
        }

        return u"no-store"_s;
    }

    QString createLanguagesOptionsHtml()
    {
        // List language files
        const QStringList langFiles = QDir(u":/www/translations"_s)
            .entryList({u"webui_*.qm"_s}, QDir::Files, QDir::Name);

        QStringList languages;
        languages.reserve(langFiles.size());

        for (const QString &langFile : langFiles)
        {
            const auto langCode = QStringView(langFile).sliced(6).chopped(3); // remove "webui_" and ".qm"
            const QString entry = u"<option value=\"%1\">%2</option>"_s
                .arg(langCode, Utils::Misc::languageToLocalizedString(langCode));
            languages.append(entry);
        }

        return languages.join(u'\n');
    }

    bool isValidCookieName(const QString &cookieName)
    {
        if (cookieName.isEmpty() || (cookieName.size() > 128))
            return false;

        const QRegularExpression invalidNameRegex {u"[^a-zA-Z0-9_\\-]"_s};
        if (invalidNameRegex.match(cookieName).hasMatch())
            return false;

        return true;
    }
}

WebApplication::WebApplication(IApplication *app, QObject *parent)
    : ApplicationComponent(app, parent)
    , m_cacheID {QString::number(Utils::Random::rand(), 36)}
    , m_authController {new AuthController(this, app, this)}
    , m_workerThread {new QThread}
    , m_freeDiskSpaceChecker {new FreeDiskSpaceChecker}
    , m_freeDiskSpaceCheckingTimer {new QTimer(this)}
    , m_torrentCreationManager {new BitTorrent::TorrentCreationManager(app, this)}
{
    declarePublicAPI(u"auth/login"_s);

    configure();
    connect(Preferences::instance(), &Preferences::changed, this, &WebApplication::configure);

    m_sessionCookieName = Preferences::instance()->getWebAPISessionCookieName();
    if (!isValidCookieName(m_sessionCookieName))
    {
        if (!m_sessionCookieName.isEmpty())
        {
            LogMsg(tr("Unacceptable session cookie name is specified: '%1'. Default one is used.")
                   .arg(m_sessionCookieName), Log::WARNING);
        }
        m_sessionCookieName = DEFAULT_SESSION_COOKIE_NAME;
    }

    m_freeDiskSpaceChecker->moveToThread(m_workerThread.get());
    connect(m_workerThread.get(), &QThread::finished, m_freeDiskSpaceChecker, &QObject::deleteLater);
    m_workerThread->setObjectName("WebApplication m_workerThread");
    m_workerThread->start();

    m_freeDiskSpaceCheckingTimer->setInterval(FREEDISKSPACE_CHECK_TIMEOUT);
    m_freeDiskSpaceCheckingTimer->setSingleShot(true);
    connect(m_freeDiskSpaceCheckingTimer, &QTimer::timeout, m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::check);
    connect(m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::checked, m_freeDiskSpaceCheckingTimer, qOverload<>(&QTimer::start));
    QMetaObject::invokeMethod(m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::check);
}

WebApplication::~WebApplication()
{
    // cleanup sessions data
    qDeleteAll(m_sessions);
}

void WebApplication::sendWebUIFile()
{
    if (request().path.contains(u'\\'))
        throw BadRequestHTTPError();

    if (const QList<QStringView> pathItems = QStringView(request().path).split(u'/', Qt::SkipEmptyParts)
            ; pathItems.contains(u".") || pathItems.contains(u".."))
    {
        throw BadRequestHTTPError();
    }

    const QString path = (request().path != u"/")
        ? request().path
        : u"/index.html"_s;

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
        if (!Utils::Fs::isRegularFile(localPath))
            throw InternalServerErrorHTTPError(tr("Unacceptable file type, only regular file is allowed."));

        const QString rootFolder = m_rootFolder.data();

        QFileInfo fileInfo {localPath.parentPath().data()};
        while (fileInfo.path() != rootFolder)
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
    const QRegularExpression regex(u"QBT_TR\\((([^\\)]|\\)(?!QBT_TR))+)\\)QBT_TR\\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\\]"_s);

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

            // Escape quotes to workaround issues with HTML attributes
            // FIXME: this is a dirty workaround to deal with broken translation strings:
            // 1. Translation strings is the culprit of the issue, they should be fixed instead
            // 2. The escaped quote/string is wrong for JS. JS use backslash to escape the quote: "\""
            translation.replace(u'"', u"&#34;"_s);

            data.replace(i, regexMatch.capturedLength(), translation);
            i += translation.length();
        }
        else
        {
            found = false; // no more translatable strings
        }

        data.replace(u"${LANG}"_s, m_currentLocale.left(2));
        data.replace(u"${CACHEID}"_s, m_cacheID);
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

void WebApplication::setUsername(const QString &username)
{
    m_authController->setUsername(username);
}

void WebApplication::setPasswordHash(const QByteArray &passwordHash)
{
    m_authController->setPasswordHash(passwordHash);
}

void WebApplication::doProcessRequest()
{
    const QRegularExpressionMatch match = m_apiPathPattern.match(request().path);
    if (!match.hasMatch())
    {
        sendWebUIFile();
        return;
    }

    const QString action = match.captured(u"action"_s);
    const QString scope = match.captured(u"scope"_s);

    // Check public/private scope
    if (!session() && !isPublicAPI(scope, action))
        throw ForbiddenHTTPError();

    // Find matching API
    APIController *controller = nullptr;
    if (session())
        controller = session()->getAPIController(scope);
    if (!controller)
    {
        if (scope == u"auth")
            controller = m_authController;
        else
            throw NotFoundHTTPError();
    }

    // Filter HTTP methods
    const auto allowedMethodIter = m_allowedMethod.constFind({scope, action});
    if (allowedMethodIter == m_allowedMethod.cend())
    {
        // by default allow both GET, POST methods
        if ((m_request.method != Http::METHOD_GET) && (m_request.method != Http::METHOD_POST))
            throw MethodNotAllowedHTTPError();
    }
    else
    {
        if (*allowedMethodIter != m_request.method)
            throw MethodNotAllowedHTTPError();
    }

    DataMap data;
    for (const Http::UploadedFile &torrent : request().files)
        data[torrent.filename] = torrent.data;

    try
    {
        const APIResult result = controller->run(action, m_params, data);
        switch (result.data.userType())
        {
        case QMetaType::QJsonDocument:
            print(result.data.toJsonDocument().toJson(QJsonDocument::Compact), Http::CONTENT_TYPE_JSON);
            break;
        case QMetaType::QByteArray:
            {
                const auto resultData = result.data.toByteArray();
                print(resultData, (!result.mimeType.isEmpty() ? result.mimeType : Http::CONTENT_TYPE_TXT));
                if (!result.filename.isEmpty())
                {
                    setHeader({u"Content-Disposition"_s, u"attachment; filename=\"%1\""_s.arg(result.filename)});
                }
            }
            break;
        case QMetaType::QString:
        default:
            print(result.data.toString(), Http::CONTENT_TYPE_TXT);
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
            Q_UNREACHABLE();
            break;
        }
    }
}

void WebApplication::configure()
{
    const auto *pref = Preferences::instance();

    const bool isAltUIUsed = pref->isAltWebUIEnabled();
    const Path rootFolder = (!isAltUIUsed ? Path(WWW_FOLDER) : pref->getWebUIRootFolder());
    if ((isAltUIUsed != m_isAltUIUsed) || (rootFolder != m_rootFolder))
    {
        m_isAltUIUsed = isAltUIUsed;
        m_rootFolder = rootFolder;
        m_translatedFiles.clear();
        if (!m_isAltUIUsed)
            LogMsg(tr("Using built-in WebUI."));
        else
            LogMsg(tr("Using custom WebUI. Location: \"%1\".").arg(m_rootFolder.toString()));
    }

    const QString newLocale = pref->getLocale();
    if (m_currentLocale != newLocale)
    {
        m_currentLocale = newLocale;
        m_translatedFiles.clear();

        m_translationFileLoaded = m_translator.load((m_rootFolder / Path(u"translations/webui_"_s) + newLocale).data());
        if (m_translationFileLoaded)
        {
            LogMsg(tr("WebUI translation for selected locale (%1) has been successfully loaded.")
                   .arg(newLocale));
        }
        else
        {
            LogMsg(tr("Couldn't load WebUI translation for selected locale (%1).").arg(newLocale), Log::WARNING);
        }
    }

    m_isLocalAuthEnabled = pref->isWebUILocalAuthEnabled();
    m_isAuthSubnetWhitelistEnabled = pref->isWebUIAuthSubnetWhitelistEnabled();
    m_authSubnetWhitelist = pref->getWebUIAuthSubnetWhitelist();
    m_sessionTimeout = pref->getWebUISessionTimeout();

    m_domainList = pref->getServerDomains().split(u';', Qt::SkipEmptyParts);
    std::for_each(m_domainList.begin(), m_domainList.end(), [](QString &entry) { entry = entry.trimmed(); });

    m_isCSRFProtectionEnabled = pref->isWebUICSRFProtectionEnabled();
    m_isSecureCookieEnabled = pref->isWebUISecureCookieEnabled();
    m_isHostHeaderValidationEnabled = pref->isWebUIHostHeaderValidationEnabled();
    m_isHttpsEnabled = pref->isWebUIHttpsEnabled();

    m_prebuiltHeaders.clear();
    m_prebuiltHeaders.push_back({Http::HEADER_X_XSS_PROTECTION, u"1; mode=block"_s});
    m_prebuiltHeaders.push_back({Http::HEADER_X_CONTENT_TYPE_OPTIONS, u"nosniff"_s});

    if (!m_isAltUIUsed)
    {
        m_prebuiltHeaders.push_back({Http::HEADER_CROSS_ORIGIN_OPENER_POLICY, u"same-origin"_s});
        m_prebuiltHeaders.push_back({Http::HEADER_REFERRER_POLICY, u"same-origin"_s});
    }

    const bool isClickjackingProtectionEnabled = pref->isWebUIClickjackingProtectionEnabled();
    if (isClickjackingProtectionEnabled)
        m_prebuiltHeaders.push_back({Http::HEADER_X_FRAME_OPTIONS, u"SAMEORIGIN"_s});

    const QString contentSecurityPolicy =
        (m_isAltUIUsed
            ? QString()
            : u"default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; object-src 'none'; form-action 'self'; frame-src 'self' blob:;"_s)
        + (isClickjackingProtectionEnabled ? u" frame-ancestors 'self';"_s : QString())
        + (m_isHttpsEnabled ? u" upgrade-insecure-requests;"_s : QString());
    if (!contentSecurityPolicy.isEmpty())
        m_prebuiltHeaders.push_back({Http::HEADER_CONTENT_SECURITY_POLICY, contentSecurityPolicy});

    if (pref->isWebUICustomHTTPHeadersEnabled())
    {
        const QString customHeaders = pref->getWebUICustomHTTPHeaders();
        const QList<QStringView> customHeaderLines = QStringView(customHeaders).trimmed().split(u'\n', Qt::SkipEmptyParts);

        for (const QStringView line : customHeaderLines)
        {
            const int idx = line.indexOf(u':');
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
        const QStringList proxyList = pref->getWebUITrustedReverseProxiesList().split(u';', Qt::SkipEmptyParts);

        m_trustedReverseProxyList.clear();
        m_trustedReverseProxyList.reserve(proxyList.size());

        for (QString proxy : proxyList)
        {
            if (!proxy.contains(u'/'))
            {
                const QAbstractSocket::NetworkLayerProtocol protocol = QHostAddress(proxy).protocol();
                if (protocol == QAbstractSocket::IPv4Protocol)
                {
                    proxy.append(u"/32");
                }
                else if (protocol == QAbstractSocket::IPv6Protocol)
                {
                    proxy.append(u"/128");
                }
            }

            const std::optional<Utils::Net::Subnet> subnet = Utils::Net::parseSubnet(proxy);
            if (subnet)
                m_trustedReverseProxyList.push_back(subnet.value());
        }

        if (m_trustedReverseProxyList.isEmpty())
            m_isReverseProxySupportEnabled = false;
    }
}

void WebApplication::declarePublicAPI(const QString &apiPath)
{
    m_publicAPIs << apiPath;
}

void WebApplication::sendFile(const Path &path)
{
    const QDateTime lastModified = Utils::Fs::lastModified(path);

    // find translated file in cache
    if (!m_isAltUIUsed)
    {
        if (const auto it = m_translatedFiles.constFind(path);
            (it != m_translatedFiles.constEnd()) && (lastModified <= it->lastModified))
        {
            print(it->data, it->mimeType);
            setHeader({Http::HEADER_CACHE_CONTROL, getCachingInterval(it->mimeType)});
            return;
        }
    }

    const auto readResult = Utils::IO::readFile(path, MAX_ALLOWED_FILESIZE);
    if (!readResult)
    {
        const QString message = tr("Web server error. %1").arg(readResult.error().message);

        switch (readResult.error().status)
        {
        case Utils::IO::ReadError::NotExist:
            qDebug("%s", qUtf8Printable(message));
            // don't write log messages here to avoid exhausting the disk space
            throw NotFoundHTTPError();

        case Utils::IO::ReadError::ExceedSize:
            qWarning("%s", qUtf8Printable(message));
            LogMsg(message, Log::WARNING);
            throw InternalServerErrorHTTPError(readResult.error().message);

        case Utils::IO::ReadError::Failed:
        case Utils::IO::ReadError::SizeMismatch:
            LogMsg(message, Log::WARNING);
            throw InternalServerErrorHTTPError(readResult.error().message);
        }

        throw InternalServerErrorHTTPError(tr("Web server error. Unknown error."));
    }

    QByteArray data = readResult.value();
    const QMimeType mimeType = QMimeDatabase().mimeTypeForFileNameAndData(path.data(), data);
    const bool isTranslatable = !m_isAltUIUsed && mimeType.inherits(u"text/plain"_s);

    if (isTranslatable)
    {
        auto dataStr = QString::fromUtf8(data);
        // Translate the file
        translateDocument(dataStr);

        // Add the language options
        if (path == (m_rootFolder / Path(PRIVATE_FOLDER) / Path(u"views/preferences.html"_s)))
            dataStr.replace(u"${LANGUAGE_OPTIONS}"_s, createLanguagesOptionsHtml());

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

    const QString sessionId {parseCookie(m_request.headers.value(u"cookie"_s)).value(m_sessionCookieName)};

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
        sid = QString::fromLatin1(QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(tmp)).toBase64());
    }
    while (m_sessions.contains(sid));

    return sid;
}

bool WebApplication::isAuthNeeded()
{
    if (!m_isLocalAuthEnabled && m_clientAddress.isLoopback())
        return false;
    if (m_isAuthSubnetWhitelistEnabled && Utils::Net::isIPInSubnets(m_clientAddress, m_authSubnetWhitelist))
        return false;
    return true;
}

bool WebApplication::isPublicAPI(const QString &scope, const QString &action) const
{
    return m_publicAPIs.contains(u"%1/%2"_s.arg(scope, action));
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

    m_currentSession = new WebSession(generateSid(), app());
    m_sessions[m_currentSession->id()] = m_currentSession;

    m_currentSession->registerAPIController(u"app"_s, new AppController(app(), m_currentSession));
    m_currentSession->registerAPIController(u"log"_s, new LogController(app(), m_currentSession));
    m_currentSession->registerAPIController(u"torrentcreator"_s, new TorrentCreatorController(m_torrentCreationManager, app(), m_currentSession));
    m_currentSession->registerAPIController(u"rss"_s, new RSSController(app(), m_currentSession));
    m_currentSession->registerAPIController(u"search"_s, new SearchController(app(), m_currentSession));
    m_currentSession->registerAPIController(u"torrents"_s, new TorrentsController(app(), m_currentSession));
    m_currentSession->registerAPIController(u"transfer"_s, new TransferController(app(), m_currentSession));

    auto *syncController = new SyncController(app(), m_currentSession);
    syncController->updateFreeDiskSpace(m_freeDiskSpaceChecker->lastResult());
    connect(m_freeDiskSpaceChecker, &FreeDiskSpaceChecker::checked, syncController, &SyncController::updateFreeDiskSpace);
    m_currentSession->registerAPIController(u"sync"_s, syncController);

    QNetworkCookie cookie {m_sessionCookieName.toLatin1(), m_currentSession->id().toLatin1()};
    cookie.setHttpOnly(true);
    cookie.setSecure(m_isSecureCookieEnabled && isOriginTrustworthy());  // [rfc6265] 4.1.2.5. The Secure Attribute
    cookie.setPath(u"/"_s);
    if (m_isCSRFProtectionEnabled)
        cookie.setSameSitePolicy(QNetworkCookie::SameSite::Strict);
    else if (cookie.isSecure())
        cookie.setSameSitePolicy(QNetworkCookie::SameSite::None);
    setHeader({Http::HEADER_SET_COOKIE, QString::fromLatin1(cookie.toRawForm())});
}

void WebApplication::sessionEnd()
{
    Q_ASSERT(m_currentSession);

    QNetworkCookie cookie {m_sessionCookieName.toLatin1()};
    cookie.setPath(u"/"_s);
    cookie.setExpirationDate(QDateTime::currentDateTime().addDays(-1));

    delete m_sessions.take(m_currentSession->id());
    m_currentSession = nullptr;

    setHeader({Http::HEADER_SET_COOKIE, QString::fromLatin1(cookie.toRawForm())});
}

bool WebApplication::isOriginTrustworthy() const
{
    // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy

    if (m_isReverseProxySupportEnabled)
    {
        const QString forwardedProto = request().headers.value(Http::HEADER_X_FORWARDED_PROTO);
        if (forwardedProto.compare(u"https", Qt::CaseInsensitive) == 0)
            return true;
    }

    if (m_isHttpsEnabled)
        return true;

    return false;
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
        {
            LogMsg(tr("WebUI: Origin header & Target origin mismatch! Source IP: '%1'. Origin header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString(), originValue, targetOrigin)
                   , Log::WARNING);
        }
        return isInvalid;
    }

    if (!refererValue.isEmpty())
    {
        const bool isInvalid = !isSameOrigin(urlFromHostHeader(targetOrigin), refererValue);
        if (isInvalid)
        {
            LogMsg(tr("WebUI: Referer header & Target origin mismatch! Source IP: '%1'. Referer header: '%2'. Target origin: '%3'")
                   .arg(m_env.clientAddress.toString(), refererValue, targetOrigin)
                   , Log::WARNING);
        }
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
    if (!Utils::Net::isIPInSubnets(m_env.clientAddress, m_trustedReverseProxyList))
        return m_env.clientAddress;

    const QString forwardedFor = m_request.headers.value(Http::HEADER_X_FORWARDED_FOR);

    if (!forwardedFor.isEmpty())
    {
        // client address is the 1st global IP in X-Forwarded-For or, if none available, the 1st IP in the list
        const QStringList remoteIpList = forwardedFor.split(u',', Qt::SkipEmptyParts);

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

WebSession::WebSession(const QString &sid, IApplication *app)
    : ApplicationComponent(app)
    , m_sid {sid}
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

void WebSession::registerAPIController(const QString &scope, APIController *controller)
{
    Q_ASSERT(controller);
    m_apiControllers[scope] = controller;
}

APIController *WebSession::getAPIController(const QString &scope) const
{
    return m_apiControllers.value(scope);
}
