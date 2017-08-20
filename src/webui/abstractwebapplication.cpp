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

#include "abstractwebapplication.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkCookie>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/random.h"
#include "base/utils/string.h"
#include "websessiondata.h"

// UnbanTimer

class UnbanTimer: public QTimer
{
public:
    UnbanTimer(const QHostAddress& peer_ip, QObject *parent)
        : QTimer(parent), m_peerIp(peer_ip)
    {
        setSingleShot(true);
        setInterval(BAN_TIME);
    }

    inline const QHostAddress& peerIp() const { return m_peerIp; }

private:
    QHostAddress m_peerIp;
};

// WebSession

struct WebSession
{
    const QString id;
    uint timestamp;
    WebSessionData data;

    WebSession(const QString& id)
        : id(id)
    {
        updateTimestamp();
    }

    void updateTimestamp()
    {
        timestamp = QDateTime::currentDateTime().toTime_t();
    }
};

// AbstractWebApplication

AbstractWebApplication::AbstractWebApplication(QObject *parent)
    : Http::ResponseBuilder(parent)
    , session_(0)
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AbstractWebApplication::removeInactiveSessions);
    timer->start(60 * 1000);  // 1 min.

    reloadDomainList();
    connect(Preferences::instance(), &Preferences::changed, this, &AbstractWebApplication::reloadDomainList);
}

AbstractWebApplication::~AbstractWebApplication()
{
    // cleanup sessions data
    qDeleteAll(sessions_);
}

Http::Response AbstractWebApplication::processRequest(const Http::Request &request, const Http::Environment &env)
{
    session_ = 0;
    request_ = request;
    env_ = env;

    // clear response
    clear();

    // avoid clickjacking attacks
    header(Http::HEADER_X_FRAME_OPTIONS, "SAMEORIGIN");
    header(Http::HEADER_X_XSS_PROTECTION, "1; mode=block");
    header(Http::HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff");
    header(Http::HEADER_CONTENT_SECURITY_POLICY, "default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; object-src 'none';");

    // block cross-site requests
    if (isCrossSiteRequest(request_) || !validateHostHeader(request_, env, domainList)) {
        status(401, "Unauthorized");
        return response();
    }

    sessionInitialize();
    if (!sessionActive() && !isAuthNeeded())
        sessionStart();

    if (isBanned()) {
        status(403, "Forbidden");
        print(QObject::tr("Your IP address has been banned after too many failed authentication attempts."), Http::CONTENT_TYPE_TXT);
    }
    else {
        doProcessRequest();
    }

    return response();
}

void AbstractWebApplication::UnbanTimerEvent()
{
    UnbanTimer* ubantimer = static_cast<UnbanTimer*>(sender());
    qDebug("Ban period has expired for %s", qUtf8Printable(ubantimer->peerIp().toString()));
    clientFailedAttempts_.remove(ubantimer->peerIp());
    ubantimer->deleteLater();
}

void AbstractWebApplication::removeInactiveSessions()
{
    const uint now = QDateTime::currentDateTime().toTime_t();

    foreach (const QString &id, sessions_.keys()) {
        if ((now - sessions_[id]->timestamp) > INACTIVE_TIME)
            delete sessions_.take(id);
    }
}

void AbstractWebApplication::reloadDomainList()
{
    domainList = Preferences::instance()->getServerDomains().split(';', QString::SkipEmptyParts);
    std::for_each(domainList.begin(), domainList.end(), [](QString &entry){ entry = entry.trimmed(); });
}

bool AbstractWebApplication::sessionInitialize()
{
    if (session_ == 0)
    {
        const QString sessionId = parseCookie(request_).value(C_SID);

        // TODO: Additional session check

        if (!sessionId.isEmpty()) {
            if (sessions_.contains(sessionId)) {
                session_ = sessions_[sessionId];
                session_->updateTimestamp();
                return true;
            }
            else {
                qDebug() << Q_FUNC_INFO << "session does not exist!";
            }
        }
    }

    return false;
}

bool AbstractWebApplication::readFile(const QString& path, QByteArray &data, QString &type)
{
    QString ext = "";
    int index = path.lastIndexOf('.') + 1;
    if (index > 0)
        ext = path.mid(index);

    // find translated file in cache
    if (translatedFiles_.contains(path)) {
        data = translatedFiles_[path];
    }
    else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("File %s was not found!", qUtf8Printable(path));
            return false;
        }

        data = file.readAll();
        file.close();

        // Translate the file
        if ((ext == "html") || ((ext == "js") && !path.endsWith("excanvas-compressed.js"))) {
            QString dataStr = QString::fromUtf8(data.constData());
            translateDocument(dataStr);

            if (path.endsWith("about.html") || path.endsWith("index.html") || path.endsWith("client.js"))
                dataStr.replace("${VERSION}", QBT_VERSION);

            data = dataStr.toUtf8();
            translatedFiles_[path] = data; // cashing translated file
        }
    }

    type = CONTENT_TYPE_BY_EXT[ext];
    return true;
}

WebSessionData *AbstractWebApplication::session()
{
    Q_ASSERT(session_ != 0);
    return &session_->data;
}


QString AbstractWebApplication::generateSid()
{
    QString sid;

    do {
        const size_t size = 6;
        quint32 tmp[size];

        for (size_t i = 0; i < size; ++i)
            tmp[i] = Utils::Random::rand();

        sid = QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(quint32) * size).toBase64();
    }
    while (sessions_.contains(sid));

    return sid;
}

void AbstractWebApplication::translateDocument(QString& data)
{
    const QRegExp regex("QBT_TR\\((([^\\)]|\\)(?!QBT_TR))+)\\)QBT_TR(\\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\\])");
    const QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
    int i = 0;
    bool found = true;

    const QString locale = Preferences::instance()->getLocale();
    bool isTranslationNeeded = !locale.startsWith("en") || locale.startsWith("en_AU") || locale.startsWith("en_GB");

    while(i < data.size() && found) {
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
    }
}

bool AbstractWebApplication::isBanned() const
{
    return clientFailedAttempts_.value(env_.clientAddress, 0) >= MAX_AUTH_FAILED_ATTEMPTS;
}

int AbstractWebApplication::failedAttempts() const
{
    return clientFailedAttempts_.value(env_.clientAddress, 0);
}

void AbstractWebApplication::resetFailedAttempts()
{
    clientFailedAttempts_.remove(env_.clientAddress);
}

void AbstractWebApplication::increaseFailedAttempts()
{
    const int nb_fail = clientFailedAttempts_.value(env_.clientAddress, 0) + 1;

    clientFailedAttempts_[env_.clientAddress] = nb_fail;
    if (nb_fail == MAX_AUTH_FAILED_ATTEMPTS) {
        // Max number of failed attempts reached
        // Start ban period
        UnbanTimer* ubantimer = new UnbanTimer(env_.clientAddress, this);
        connect(ubantimer, SIGNAL(timeout()), SLOT(UnbanTimerEvent()));
        ubantimer->start();
    }
}

bool AbstractWebApplication::isAuthNeeded()
{
    return (env_.clientAddress != QHostAddress::LocalHost
            && env_.clientAddress != QHostAddress::LocalHostIPv6
            && env_.clientAddress != QHostAddress("::ffff:127.0.0.1"))
            || Preferences::instance()->isWebUiLocalAuthEnabled();
}

void AbstractWebApplication::printFile(const QString& path)
{
    QByteArray data;
    QString type;

    if (!readFile(path, data, type)) {
        status(404, "Not Found");
        return;
    }

    print(data, type);
}

bool AbstractWebApplication::sessionStart()
{
    if (session_ == 0) {
        session_ = new WebSession(generateSid());
        sessions_[session_->id] = session_;

        QNetworkCookie cookie(C_SID, session_->id.toUtf8());
        cookie.setHttpOnly(true);
        cookie.setPath(QLatin1String("/"));
        header(Http::HEADER_SET_COOKIE, cookie.toRawForm());

        return true;
    }

    return false;
}

bool AbstractWebApplication::sessionEnd()
{
    if ((session_ != 0) && (sessions_.contains(session_->id))) {
        QNetworkCookie cookie(C_SID);
        cookie.setPath(QLatin1String("/"));
        cookie.setExpirationDate(QDateTime::currentDateTime().addDays(-1));

        sessions_.remove(session_->id);
        delete session_;
        session_ = 0;

        header(Http::HEADER_SET_COOKIE, cookie.toRawForm());
        return true;
    }

    return false;
}

QString AbstractWebApplication::saveTmpFile(const QByteArray &data)
{
    QTemporaryFile tmpfile(Utils::Fs::tempPath() + "XXXXXX.torrent");
    tmpfile.setAutoRemove(false);
    if (tmpfile.open()) {
        tmpfile.write(data);
        tmpfile.close();
        return tmpfile.fileName();
    }

    qWarning() << "I/O Error: Could not create temporary file";
    return QString();
}

bool AbstractWebApplication::isCrossSiteRequest(const Http::Request &request) const
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
    if (!originValue.isEmpty())
        return !isSameOrigin(QUrl::fromUserInput(targetOrigin), originValue);

    if (!refererValue.isEmpty())
        return !isSameOrigin(QUrl::fromUserInput(targetOrigin), refererValue);

    return true;
}

bool AbstractWebApplication::validateHostHeader(const Http::Request &request, const Http::Environment &env, const QStringList &domains) const
{
    const QUrl hostHeader = QUrl::fromUserInput(request.headers.value(Http::HEADER_HOST));

    // (if present) try matching host header's port with local port
    const int requestPort = hostHeader.port();
    if ((requestPort != -1) && (env.localPort != requestPort))
        return false;

    // try matching host header with local address
    const QString requestHost = hostHeader.host();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    const bool sameAddr = env.localAddress.isEqual(QHostAddress(requestHost));
#else
    const auto equal = [](const Q_IPV6ADDR &l, const Q_IPV6ADDR &r) -> bool {
        for (int i = 0; i < 16; ++i) {
            if (l[i] != r[i])
                return false;
        }
        return true;
    };
    const bool sameAddr = equal(env.localAddress.toIPv6Address(), QHostAddress(requestHost).toIPv6Address());
#endif

    if (sameAddr)
        return true;

    // try matching host header with domain list
    for (const auto &domain : domains) {
        QRegExp domainRegex(domain, Qt::CaseInsensitive, QRegExp::Wildcard);
        if (requestHost.contains(domainRegex))
            return true;
    }

    return false;
}

const QStringMap AbstractWebApplication::CONTENT_TYPE_BY_EXT = {
    { "htm", Http::CONTENT_TYPE_HTML },
    { "html", Http::CONTENT_TYPE_HTML },
    { "css", Http::CONTENT_TYPE_CSS },
    { "gif", Http::CONTENT_TYPE_GIF },
    { "png", Http::CONTENT_TYPE_PNG },
    { "js", Http::CONTENT_TYPE_JS }
};

QStringMap AbstractWebApplication::parseCookie(const Http::Request &request) const
{
    // [rfc6265] 4.2.1. Syntax
    QStringMap ret;
    const QString cookieStr = request.headers.value(QLatin1String("cookie"));
    const QVector<QStringRef> cookies = cookieStr.splitRef(';', QString::SkipEmptyParts);

    for (const auto &cookie : cookies) {
        const int idx = cookie.indexOf('=');
        if (idx < 0)
            continue;

        const QString name = cookie.left(idx).trimmed().toString();
        const QString value = Utils::String::unquote(cookie.mid(idx + 1).trimmed())
                                  .toString();
        ret.insert(name, value);
    }
    return ret;
}
