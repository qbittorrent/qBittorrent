/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012, Christophe Dumez <chris@qbittorrent.org>
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

#include <QDebug>
#ifdef DISABLE_GUI
#include <QCoreApplication>
#else
#include <QApplication>
#endif
#include <QTimer>
#include <QCryptographicHash>
#include <queue>
#include <vector>
#include <libtorrent/session.hpp>
#ifndef DISABLE_GUI
#include "iconprovider.h"
#endif
#include "misc.h"
#include "fs_utils.h"
#include "preferences.h"
#include "btjson.h"
#include "prefjson.h"
#include "qbtsession.h"
#include "requesthandler.h"

using namespace libtorrent;

static const int API_VERSION = 2;
static const int API_VERSION_MIN = 2;

const QString WWW_FOLDER = ":/www/public/";
const QString PRIVATE_FOLDER = ":/www/private/";
const QString DEFAULT_SCOPE = "public";
const QString SCOPE_IMAGES = "images";
const QString SCOPE_THEME = "theme";
const QString DEFAULT_ACTION = "index";
const QString WEBUI_ACTION = "webui";
const QString VERSION_INFO = "version";
const QString MAX_AGE_MONTH = "public, max-age=2592000";

#define ADD_ACTION(scope, action) actions[#scope][#action] = &RequestHandler::action_##scope##_##action

QMap<QString, QMap<QString, RequestHandler::Action> > RequestHandler::initializeActions()
{
    QMap<QString,QMap<QString, RequestHandler::Action> > actions;

    ADD_ACTION(public, webui);
    ADD_ACTION(public, index);
    ADD_ACTION(public, login);
    ADD_ACTION(public, logout);
    ADD_ACTION(public, theme);
    ADD_ACTION(public, images);
    ADD_ACTION(json, torrents);
    ADD_ACTION(json, preferences);
    ADD_ACTION(json, transferInfo);
    ADD_ACTION(json, propertiesGeneral);
    ADD_ACTION(json, propertiesTrackers);
    ADD_ACTION(json, propertiesFiles);
    ADD_ACTION(command, shutdown);
    ADD_ACTION(command, download);
    ADD_ACTION(command, upload);
    ADD_ACTION(command, addTrackers);
    ADD_ACTION(command, resumeAll);
    ADD_ACTION(command, pauseAll);
    ADD_ACTION(command, resume);
    ADD_ACTION(command, pause);
    ADD_ACTION(command, setPreferences);
    ADD_ACTION(command, setFilePrio);
    ADD_ACTION(command, getGlobalUpLimit);
    ADD_ACTION(command, getGlobalDlLimit);
    ADD_ACTION(command, setGlobalUpLimit);
    ADD_ACTION(command, setGlobalDlLimit);
    ADD_ACTION(command, getTorrentUpLimit);
    ADD_ACTION(command, getTorrentDlLimit);
    ADD_ACTION(command, setTorrentUpLimit);
    ADD_ACTION(command, setTorrentDlLimit);
    ADD_ACTION(command, toggleSequentialDownload);
    ADD_ACTION(command, toggleFirstLastPiecePrio);
    ADD_ACTION(command, delete);
    ADD_ACTION(command, deletePerm);
    ADD_ACTION(command, increasePrio);
    ADD_ACTION(command, decreasePrio);
    ADD_ACTION(command, topPrio);
    ADD_ACTION(command, bottomPrio);
    ADD_ACTION(command, recheck);
    ADD_ACTION(version, api);
    ADD_ACTION(version, api_min);
    ADD_ACTION(version, qbittorrent);

    return actions;
}

void RequestHandler::action_public_index()
{
    QString path;

    if (!args_.isEmpty()) {
        if (args_.back() == "favicon.ico")
            path = ":/Icons/skin/qbittorrent16.png";
        else
            path = WWW_FOLDER + args_.join("/");
    }

    printFile(path);
}

void RequestHandler::action_public_webui()
{
    if (!sessionActive())
        printFile(PRIVATE_FOLDER + "login.html");
    else
        printFile(PRIVATE_FOLDER + "index.html");
}

void RequestHandler::action_public_login()
{
    const Preferences* const pref = Preferences::instance();
    QCryptographicHash md5(QCryptographicHash::Md5);

    md5.addData(request().posts["password"].toLocal8Bit());
    QString pass = md5.result().toHex();

    bool equalUser = misc::slowEquals(request().posts["username"].toUtf8(), pref->getWebUiUsername().toUtf8());
    bool equalPass = misc::slowEquals(pass.toUtf8(), pref->getWebUiPassword().toUtf8());

    if (equalUser && equalPass) {
        sessionStart();
        print(QByteArray("Ok."), CONTENT_TYPE_TXT);
    }
    else {
        QString addr = env().clientAddress.toString();
        increaseFailedAttempts();
        qDebug("client IP: %s (%d failed attempts)", qPrintable(addr), failedAttempts());
        print(QByteArray("Fails."), CONTENT_TYPE_TXT);
    }
}

void RequestHandler::action_public_logout()
{
    sessionEnd();
}

void RequestHandler::action_public_theme()
{
    if (args_.size() != 1) {
        status(404, "Not Found");
        return;
    }

#ifdef DISABLE_GUI
    QString url = ":/Icons/oxygen/" + args_.front() + ".png";
#else
    QString url = IconProvider::instance()->getIconPath(args_.front());
#endif
    qDebug() << Q_FUNC_INFO << "There icon:" << url;

    printFile(url);
    header(HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
}

void RequestHandler::action_public_images()
{
    const QString path = ":/Icons/" + args_.join("/");
    printFile(path);
    header(HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
}

// GET params:
//   - filter (string): all, downloading, completed, paused, active, inactive
//   - label (string): torrent label for filtering by it (empty string means "unlabeled"; no "label" param presented means "any label")
//   - sort (string): name of column for sorting by its value
//   - reverse (bool): enable reverse sorting
//   - limit (int): set limit number of torrents returned (if greater than 0, otherwise - unlimited)
//   - offset (int): set offset (if less than 0 - offset from end)
void RequestHandler::action_json_torrents()
{
    const QStringMap& gets = request().gets;

    print(btjson::getTorrents(
        gets["filter"], gets["label"], gets["sort"], gets["reverse"] == "true",
        gets["limit"].toInt(), gets["offset"].toInt()
        ), CONTENT_TYPE_JS);
}

void RequestHandler::action_json_preferences()
{
    print(prefjson::getPreferences(), CONTENT_TYPE_JS);
}

void RequestHandler::action_json_transferInfo()
{
    print(btjson::getTransferInfo(), CONTENT_TYPE_JS);
}

void RequestHandler::action_json_propertiesGeneral()
{
    print(btjson::getPropertiesForTorrent(args_.front()), CONTENT_TYPE_JS);
}

void RequestHandler::action_json_propertiesTrackers()
{
    print(btjson::getTrackersForTorrent(args_.front()), CONTENT_TYPE_JS);
}

void RequestHandler::action_json_propertiesFiles()
{
    print(btjson::getFilesForTorrent(args_.front()), CONTENT_TYPE_JS);
}

void RequestHandler::action_version_api()
{
    print(QString::number(API_VERSION), CONTENT_TYPE_TXT);
}

void RequestHandler::action_version_api_min()
{
    print(QString::number(API_VERSION_MIN), CONTENT_TYPE_TXT);
}

void RequestHandler::action_version_qbittorrent()
{
    print(QString(VERSION), CONTENT_TYPE_TXT);
}

void RequestHandler::action_command_shutdown()
{
    qDebug() << "Shutdown request from Web UI";
    // Special case handling for shutdown, we
    // need to reply to the Web UI before
    // actually shutting down.

    QTimer::singleShot(0, qApp, SLOT(quit()));
}

void RequestHandler::action_command_download()
{
    QString urls = request().posts["urls"];
    QStringList list = urls.split('\n');

    foreach (QString url, list) {
        url = url.trimmed();
        if (!url.isEmpty()) {
            if (url.startsWith("bc://bt/", Qt::CaseInsensitive)) {
                qDebug("Converting bc link to magnet link");
                url = misc::bcLinkToMagnet(url);
            }
            else if (url.startsWith("magnet:", Qt::CaseInsensitive)) {
                QBtSession::instance()->addMagnetSkipAddDlg(url);
            }
            else {
                qDebug("Downloading url: %s", qPrintable(url));
                QBtSession::instance()->downloadUrlAndSkipDialog(url);
            }
        }
    }
}

void RequestHandler::action_command_upload()
{
    qDebug() << Q_FUNC_INFO;

    foreach(const UploadedFile& torrent, request().files) {
        QString filePath = saveTmpFile(torrent.data);

        if (!filePath.isEmpty()) {
            QTorrentHandle h = QBtSession::instance()->addTorrent(filePath);
            if (!h.is_valid()) {
                status(415, "Internal Server Error");
                print(QObject::tr("Error: '%1' is not a valid torrent file.\n").arg(torrent.filename), CONTENT_TYPE_TXT);
            }
            // Clean up
            fsutils::forceRemove(filePath);
        }
        else {
            qWarning() << "I/O Error: Could not create temporary file";
            status(500, "Internal Server Error");
            print(QObject::tr("I/O Error: Could not create temporary file."), CONTENT_TYPE_TXT);
        }
    }
}

void RequestHandler::action_command_addTrackers()
{
    QString hash = request().posts["hash"];

    if (!hash.isEmpty()) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

        if (h.is_valid() && h.has_metadata()) {
            QString urls = request().posts["urls"];
            QStringList list = urls.split('\n');

            foreach (const QString& url, list) {
                announce_entry e(url.toStdString());
                h.add_tracker(e);
            }
        }
    }
}

void RequestHandler::action_command_resumeAll()
{
    QBtSession::instance()->resumeAllTorrents();
}

void RequestHandler::action_command_pauseAll()
{
    QBtSession::instance()->pauseAllTorrents();
}

void RequestHandler::action_command_resume()
{
    QBtSession::instance()->resumeTorrent(request().posts["hash"]);
}

void RequestHandler::action_command_pause()
{
    QBtSession::instance()->pauseTorrent(request().posts["hash"]);
}

void RequestHandler::action_command_setPreferences()
{
    prefjson::setPreferences(request().posts["json"]);
}

void RequestHandler::action_command_setFilePrio()
{
    QString hash = request().posts["hash"];
    int file_id = request().posts["id"].toInt();
    int priority = request().posts["priority"].toInt();
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid() && h.has_metadata())
        h.file_priority(file_id, priority);
}

void RequestHandler::action_command_getGlobalUpLimit()
{
    print(QByteArray::number(QBtSession::instance()->getSession()->settings().upload_rate_limit));
}

void RequestHandler::action_command_getGlobalDlLimit()
{
    print(QByteArray::number(QBtSession::instance()->getSession()->settings().download_rate_limit));
}

void RequestHandler::action_command_setGlobalUpLimit()
{
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;

    QBtSession::instance()->setUploadRateLimit(limit);
    Preferences::instance()->setGlobalUploadLimit(limit / 1024.);
}

void RequestHandler::action_command_setGlobalDlLimit()
{
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;

    QBtSession::instance()->setDownloadRateLimit(limit);
    Preferences::instance()->setGlobalDownloadLimit(limit / 1024.);
}

void RequestHandler::action_command_getTorrentUpLimit()
{
    QString hash = request().posts["hash"];
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid())
        print(QByteArray::number(h.upload_limit()));
}

void RequestHandler::action_command_getTorrentDlLimit()
{
    QString hash = request().posts["hash"];
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid())
        print(QByteArray::number(h.download_limit()));
}

void RequestHandler::action_command_setTorrentUpLimit()
{
    QString hash = request().posts["hash"];
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid())
        h.set_upload_limit(limit);
}

void RequestHandler::action_command_setTorrentDlLimit()
{
    QString hash = request().posts["hash"];
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid())
        h.set_download_limit(limit);
}

void RequestHandler::action_command_toggleSequentialDownload()
{
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            h.toggleSequentialDownload();
        }
        catch(invalid_handle&) {}
    }
}

void RequestHandler::action_command_toggleFirstLastPiecePrio()
{
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            h.toggleFirstLastPiecePrio();
        }
        catch(invalid_handle&) {}
    }
}

void RequestHandler::action_command_delete()
{
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        QBtSession::instance()->deleteTorrent(hash, false);
}

void RequestHandler::action_command_deletePerm()
{
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        QBtSession::instance()->deleteTorrent(hash, true);
}

void RequestHandler::action_command_increasePrio()
{
    QStringList hashes = request().posts["hashes"].split("|");

    std::priority_queue<QPair<int, QTorrentHandle>,
                        std::vector<QPair<int, QTorrentHandle> >,
                        std::greater<QPair<int, QTorrentHandle> > > torrent_queue;

    // Sort torrents by priority
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            if (!h.is_seed())
                torrent_queue.push(qMakePair(h.queue_position(), h));
        }
        catch(invalid_handle&) {}
    }

    // Increase torrents priority (starting with the ones with highest priority)
    while(!torrent_queue.empty()) {
        QTorrentHandle h = torrent_queue.top().second;

        try {
            h.queue_position_up();
        }
        catch(invalid_handle&) {}

        torrent_queue.pop();
    }
}

void RequestHandler::action_command_decreasePrio()
{
    QStringList hashes = request().posts["hashes"].split("|");

    std::priority_queue<QPair<int, QTorrentHandle>,
                        std::vector<QPair<int, QTorrentHandle> >,
                        std::less<QPair<int, QTorrentHandle> > > torrent_queue;

    // Sort torrents by priority
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

            if (!h.is_seed())
                torrent_queue.push(qMakePair(h.queue_position(), h));
        }
        catch(invalid_handle&) {}
    }

    // Decrease torrents priority (starting with the ones with lowest priority)
    while(!torrent_queue.empty()) {
        QTorrentHandle h = torrent_queue.top().second;

        try {
            h.queue_position_down();
        }
        catch(invalid_handle&) {}

        torrent_queue.pop();
    }
}

void RequestHandler::action_command_topPrio()
{
    foreach (const QString &hash, request().posts["hashes"].split("|")) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid()) h.queue_position_top();
    }
}

void RequestHandler::action_command_bottomPrio()
{
    foreach (const QString &hash, request().posts["hashes"].split("|")) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid()) h.queue_position_bottom();
    }
}

void RequestHandler::action_command_recheck()
{
    QBtSession::instance()->recheckTorrent(request().posts["hash"]);
}

bool RequestHandler::isPublicScope()
{
    return (scope_ == DEFAULT_SCOPE || scope_ == VERSION_INFO);
}

void RequestHandler::processRequest()
{
    if (args_.contains(".") || args_.contains("..")) {
        qDebug() << Q_FUNC_INFO << "Invalid path:" << request().path;
        status(404, "Not Found");
        return;
    }

    if (!isPublicScope() && !sessionActive()) {
        status(403, "Forbidden");
        return;
    }

    if (actions_.value(scope_).value(action_) != 0) {
        (this->*(actions_[scope_][action_]))();
    }
    else {
        status(404, "Not Found");
        qDebug() << Q_FUNC_INFO << "Resource not found:" << request().path;
    }
}

void RequestHandler::parsePath()
{
    if(request().path == "/") action_ = WEBUI_ACTION;

    // check action for requested path
    QStringList pathItems = request().path.split('/', QString::SkipEmptyParts);
    if (!pathItems.empty()) {
        if (actions_.contains(pathItems.front())) {
            scope_ = pathItems.front();
            pathItems.pop_front();
        }
    }

    if (!pathItems.empty()) {
        if (actions_[scope_].contains(pathItems.front())) {
            action_ = pathItems.front();
            pathItems.pop_front();
        }
    }

    args_ = pathItems;
}

RequestHandler::RequestHandler(const HttpRequest &request, const HttpEnvironment &env, WebApplication *app)
    : AbstractRequestHandler(request, env, app), scope_(DEFAULT_SCOPE), action_(DEFAULT_ACTION)
{
    parsePath();
}

QMap<QString, QMap<QString, RequestHandler::Action> > RequestHandler::actions_ = RequestHandler::initializeActions();
