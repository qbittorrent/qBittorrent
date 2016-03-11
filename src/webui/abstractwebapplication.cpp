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

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkCookie>
#include <QTemporaryFile>
#include <QTimer>

#include "base/preferences.h"
#include "websessiondata.h"
#include "abstractwebapplication.h"

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
    const QString token;
    uint timestamp;
    WebSessionData data;

    WebSession(const QString& id, const QString& token)
        : id(id), token(token)
    {
        updateTimestamp();
    }

    void updateTimestamp()
    {
        timestamp = QDateTime::currentDateTime().toTime_t();
    }

    bool hasAuthenticationToken()
    {
        return !token.isNull();
    }
};

// AbstractWebApplication

AbstractWebApplication::AbstractWebApplication(QObject *parent)
    : Http::ResponseBuilder(parent)
    , session_(0)
{
    QTimer *timer = new QTimer(this);
    timer->setInterval(60000); // 1 min.
    connect(timer, SIGNAL(timeout()), SLOT(removeInactiveSessions()));
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

    clear(); // clear response

    sessionInitialize();
    if (!sessionActive() && !isAuthNeeded())
        sessionStart();

    // if the session was authenticated using a token, then let's check if it's
    // still a valid token
    if (sessionActive() && session_->hasAuthenticationToken()) {
        if (!Preferences::instance()->isAuthenticationTokenValid(session_->token)) {
            sessionEnd();
        }
    }

    if (isBanned()) {
        status(403, "Forbidden");
        print(QObject::tr("Your IP address has been banned after too many failed authentication attempts."), Http::CONTENT_TYPE_TXT);
    }
    else {
        processRequest();
    }

    return response();
}

void AbstractWebApplication::UnbanTimerEvent()
{
    UnbanTimer* ubantimer = static_cast<UnbanTimer*>(sender());
    qDebug("Ban period has expired for %s", qPrintable(ubantimer->peerIp().toString()));
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

bool AbstractWebApplication::sessionInitialize()
{
    static const QString SID_START = QLatin1String(C_SID) + QLatin1String("=");

    if (session_ == 0)
    {
        QString cookie = request_.headers.value("cookie");
        //qDebug() << Q_FUNC_INFO << "cookie: " << cookie;

        QString sessionId;
        int pos = cookie.indexOf(SID_START);
        if (pos >= 0) {
            pos += SID_START.length();
            int end = cookie.indexOf(QRegExp("[,;]"), pos);
            sessionId = cookie.mid(pos, end >= 0 ? end - pos : end);
        }

        // TODO: Additional session check

        if (!sessionId.isNull()) {
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
            qDebug("File %s was not found!", qPrintable(path));
            return false;
        }

        data = file.readAll();
        file.close();

        // Translate the file
        if ((ext == "html") || ((ext == "js") && !path.endsWith("excanvas-compressed.js"))) {
            QString dataStr = QString::fromUtf8(data.constData());
            translateDocument(dataStr);

            if (path.endsWith("about.html") || path.endsWith("index.html") || path.endsWith("client.js"))
                dataStr.replace("${VERSION}", VERSION);

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

    qsrand(QDateTime::currentDateTime().toTime_t());
    do {
        const size_t size = 6;
        quint32 tmp[size];

        for (size_t i = 0; i < size; ++i)
            tmp[i] = qrand();

        sid = QByteArray::fromRawData(reinterpret_cast<const char *>(tmp), sizeof(quint32) * size).toBase64();
    }
    while (sessions_.contains(sid));

    return sid;
}

void AbstractWebApplication::translateDocument(QString& data)
{
    const QRegExp regex("QBT_TR\\((([^\\)]|\\)(?!QBT_TR))+)\\)QBT_TR(\\[CONTEXT=([a-zA-Z_][a-zA-Z0-9_]*)\\])?");
    const QRegExp mnemonic("\\(?&([a-zA-Z]?\\))?");
    const std::string contexts[] = {
        "TransferListFiltersWidget", "TransferListWidget", "PropertiesWidget",
        "HttpServer", "confirmDeletionDlg", "TrackerList", "TorrentFilesModel",
        "options_imp", "Preferences", "TrackersAdditionDlg", "ScanFoldersModel",
        "PropTabBar", "TorrentModel", "downloadFromURL", "MainWindow", "misc",
        "StatusBar", "AboutDlg", "about", "PeerListWidget", "StatusFiltersWidget",
        "CategoryFiltersList"
    };
    const size_t context_count = sizeof(contexts) / sizeof(contexts[0]);
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
                if (context.length() > 0) {
#ifndef QBT_USES_QT5
                    translation = qApp->translate(context.toUtf8().constData(), word.constData(), 0, QCoreApplication::UnicodeUTF8, 1);
#else
                    translation = qApp->translate(context.toUtf8().constData(), word.constData(), 0, 1);
#endif
                }
                else {
                    size_t context_index = 0;
                    while ((context_index < context_count) && (translation == word)) {
#ifndef QBT_USES_QT5
                        translation = qApp->translate(contexts[context_index].c_str(), word.constData(), 0, QCoreApplication::UnicodeUTF8, 1);
#else
                        translation = qApp->translate(contexts[context_index].c_str(), word.constData(), 0, 1);
#endif
                        ++context_index;
                    }
                }
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

bool AbstractWebApplication::sessionStart(const QString& token)
{
    if (session_ == 0) {
        session_ = new WebSession(generateSid(), token);
        sessions_[session_->id] = session_;

        QNetworkCookie cookie(C_SID, session_->id.toUtf8());
        cookie.setPath(QLatin1String("/"));
        header(Http::HEADER_SET_COOKIE, cookie.toRawForm());

        return true;
    }

    return false;
}

bool AbstractWebApplication::sessionEnd()
{
    if ((session_ != 0) && (sessions_.contains(session_->id))) {
        QNetworkCookie cookie(C_SID, session_->id.toUtf8());
        cookie.setPath(QLatin1String("/"));
        cookie.setExpirationDate(QDateTime::currentDateTime());

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
    QTemporaryFile tmpfile(QDir::temp().absoluteFilePath("qBT-XXXXXX.torrent"));
    tmpfile.setAutoRemove(false);
    if (tmpfile.open()) {
        tmpfile.write(data);
        tmpfile.close();
        return tmpfile.fileName();
    }

    qWarning() << "I/O Error: Could not create temporary file";
    return QString();
}

QStringMap AbstractWebApplication::initializeContentTypeByExtMap()
{
    QStringMap map;

    map["htm"] = Http::CONTENT_TYPE_HTML;
    map["html"] = Http::CONTENT_TYPE_HTML;
    map["css"] = Http::CONTENT_TYPE_CSS;
    map["gif"] = Http::CONTENT_TYPE_GIF;
    map["png"] = Http::CONTENT_TYPE_PNG;
    map["js"] = Http::CONTENT_TYPE_JS;

    return map;
}

const QStringMap AbstractWebApplication::CONTENT_TYPE_BY_EXT = AbstractWebApplication::initializeContentTypeByExtMap();
