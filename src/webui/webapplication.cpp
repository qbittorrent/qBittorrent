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

#include <QDebug>
#include <QCoreApplication>
#include <QTimer>
#include <QCryptographicHash>
#include <queue>
#include <vector>
#include <libtorrent/session.hpp>
#ifndef DISABLE_GUI
// TODO: Drop GUI dependency!
#include "iconprovider.h"
#endif
#include "misc.h"
#include "fs_utils.h"
#include "preferences.h"
#include "btjson.h"
#include "prefjson.h"
#include "qbtsession.h"
#include "websessiondata.h"
#include "webapplication.h"

using namespace libtorrent;

static const int API_VERSION = 4;
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

#define ADD_ACTION(scope, action) actions[#scope][#action] = &WebApplication::action_##scope##_##action

QMap<QString, QMap<QString, WebApplication::Action> > WebApplication::initializeActions()
{
    QMap<QString,QMap<QString, WebApplication::Action> > actions;

    ADD_ACTION(public, webui);
    ADD_ACTION(public, index);
    ADD_ACTION(public, login);
    ADD_ACTION(public, logout);
    ADD_ACTION(public, theme);
    ADD_ACTION(public, images);
    ADD_ACTION(query, torrents);
    ADD_ACTION(query, preferences);
    ADD_ACTION(query, transferInfo);
    ADD_ACTION(query, propertiesGeneral);
    ADD_ACTION(query, propertiesTrackers);
    ADD_ACTION(query, propertiesWebSeeds);
    ADD_ACTION(query, propertiesFiles);
    ADD_ACTION(sync, maindata);
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
    ADD_ACTION(command, getTorrentsUpLimit);
    ADD_ACTION(command, getTorrentsDlLimit);
    ADD_ACTION(command, setTorrentsUpLimit);
    ADD_ACTION(command, setTorrentsDlLimit);
    ADD_ACTION(command, alternativeSpeedLimitsEnabled);
    ADD_ACTION(command, toggleAlternativeSpeedLimits);
    ADD_ACTION(command, toggleSequentialDownload);
    ADD_ACTION(command, toggleFirstLastPiecePrio);
    ADD_ACTION(command, setSuperSeeding);
    ADD_ACTION(command, setForceStart);
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

#define CHECK_URI(ARGS_NUM) \
    if (args_.size() != ARGS_NUM) { \
        status(404, "Not Found"); \
        return; \
    }
#define CHECK_PARAMETERS(PARAMETERS) \
    QStringList parameters; \
    parameters << PARAMETERS; \
    if (parameters.size() != request().posts.size()) { \
        status(400, "Bad Request"); \
        return; \
    } \
    foreach (QString key, request().posts.keys()) { \
        if (!parameters.contains(key, Qt::CaseInsensitive)) { \
            status(400, "Bad Request"); \
            return; \
        } \
    }

void WebApplication::action_public_index()
{
    QString path;

    if (!args_.isEmpty()) {
        if (args_.back() == "favicon.ico")
            path = ":/icons/skin/qbittorrent16.png";
        else
            path = WWW_FOLDER + args_.join("/");
    }

    printFile(path);
}

void WebApplication::action_public_webui()
{
    if (!sessionActive())
        printFile(PRIVATE_FOLDER + "login.html");
    else
        printFile(PRIVATE_FOLDER + "index.html");
}

void WebApplication::action_public_login()
{
    const Preferences* const pref = Preferences::instance();
    QCryptographicHash md5(QCryptographicHash::Md5);

    md5.addData(request().posts["password"].toLocal8Bit());
    QString pass = md5.result().toHex();

    bool equalUser = misc::slowEquals(request().posts["username"].toUtf8(), pref->getWebUiUsername().toUtf8());
    bool equalPass = misc::slowEquals(pass.toUtf8(), pref->getWebUiPassword().toUtf8());

    if (equalUser && equalPass) {
        sessionStart();
        print(QByteArray("Ok."), Http::CONTENT_TYPE_TXT);
    }
    else {
        QString addr = env().clientAddress.toString();
        increaseFailedAttempts();
        qDebug("client IP: %s (%d failed attempts)", qPrintable(addr), failedAttempts());
        print(QByteArray("Fails."), Http::CONTENT_TYPE_TXT);
    }
}

void WebApplication::action_public_logout()
{
    CHECK_URI(0);
    sessionEnd();
}

void WebApplication::action_public_theme()
{
    if (args_.size() != 1) {
        status(404, "Not Found");
        return;
    }

#ifdef DISABLE_GUI
    QString url = ":/icons/oxygen/" + args_.front() + ".png";
#else
    QString url = IconProvider::instance()->getIconPath(args_.front());
#endif
    qDebug() << Q_FUNC_INFO << "There icon:" << url;

    printFile(url);
    header(Http::HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
}

void WebApplication::action_public_images()
{
    const QString path = ":/icons/" + args_.join("/");
    printFile(path);
    header(Http::HEADER_CACHE_CONTROL, MAX_AGE_MONTH);
}

// GET params:
//   - filter (string): all, downloading, seeding, completed, paused, resumed, active, inactive
//   - label (string): torrent label for filtering by it (empty string means "unlabeled"; no "label" param presented means "any label")
//   - sort (string): name of column for sorting by its value
//   - reverse (bool): enable reverse sorting
//   - limit (int): set limit number of torrents returned (if greater than 0, otherwise - unlimited)
//   - offset (int): set offset (if less than 0 - offset from end)
void WebApplication::action_query_torrents()
{
    CHECK_URI(0);
    const QStringMap& gets = request().gets;

    print(btjson::getTorrents(
        gets["filter"], gets["label"], gets["sort"], gets["reverse"] == "true",
        gets["limit"].toInt(), gets["offset"].toInt()
        ), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_preferences()
{
    CHECK_URI(0);
    print(prefjson::getPreferences(), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_transferInfo()
{
    CHECK_URI(0);
    print(btjson::getTransferInfo(), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_propertiesGeneral()
{
    CHECK_URI(1);
    print(btjson::getPropertiesForTorrent(args_.front()), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_propertiesTrackers()
{
    CHECK_URI(1);
    print(btjson::getTrackersForTorrent(args_.front()), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_propertiesWebSeeds()
{
    CHECK_URI(1);
    print(btjson::getWebSeedsForTorrent(args_.front()), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_query_propertiesFiles()
{
    CHECK_URI(1);
    print(btjson::getFilesForTorrent(args_.front()), Http::CONTENT_TYPE_JSON);
}

// GET param:
//   - rid (int): last response id
void WebApplication::action_sync_maindata()
{
    CHECK_URI(0);
    print(btjson::getSyncMainData(request().gets["rid"].toInt(),
        session()->syncMainDataLastResponse,
        session()->syncMainDataLastAcceptedResponse), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_version_api()
{
    CHECK_URI(0);
    print(QString::number(API_VERSION), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_version_api_min()
{
    CHECK_URI(0);
    print(QString::number(API_VERSION_MIN), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_version_qbittorrent()
{
    CHECK_URI(0);
    print(QString(VERSION), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_shutdown()
{
    qDebug() << "Shutdown request from Web UI";
    CHECK_URI(0);

    // Special case handling for shutdown, we
    // need to reply to the Web UI before
    // actually shutting down.
    QTimer::singleShot(100, qApp, SLOT(quit()));
}

void WebApplication::action_command_download()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("urls");
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

void WebApplication::action_command_upload()
{
    qDebug() << Q_FUNC_INFO;
    CHECK_URI(0);

    foreach(const Http::UploadedFile& torrent, request().files) {
        QString filePath = saveTmpFile(torrent.data);

        if (!filePath.isEmpty()) {
            QTorrentHandle h = QBtSession::instance()->addTorrent(filePath);
            if (!h.is_valid()) {
                status(415, "Internal Server Error");
                print(QObject::tr("Error: '%1' is not a valid torrent file.\n").arg(torrent.filename), Http::CONTENT_TYPE_TXT);
            }
            // Clean up
            fsutils::forceRemove(filePath);
        }
        else {
            qWarning() << "I/O Error: Could not create temporary file";
            status(500, "Internal Server Error");
            print(QObject::tr("I/O Error: Could not create temporary file."), Http::CONTENT_TYPE_TXT);
        }
    }
}

void WebApplication::action_command_addTrackers()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash" << "urls");
    QString hash = request().posts["hash"];

    if (!hash.isEmpty()) {
        QString urls = request().posts["urls"];
        QStringList list = urls.split('\n');
        QBtSession::instance()->addTrackersAndUrlSeeds(hash, list, QStringList());
    }
}

void WebApplication::action_command_resumeAll()
{
    CHECK_URI(0);
    QBtSession::instance()->resumeAllTorrents();
}

void WebApplication::action_command_pauseAll()
{
    CHECK_URI(0);
    QBtSession::instance()->pauseAllTorrents();
}

void WebApplication::action_command_resume()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");
    QBtSession::instance()->resumeTorrent(request().posts["hash"]);
}

void WebApplication::action_command_pause()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");
    QBtSession::instance()->pauseTorrent(request().posts["hash"]);
}

void WebApplication::action_command_setPreferences()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("json");
    prefjson::setPreferences(request().posts["json"]);
}

void WebApplication::action_command_setFilePrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash" << "id" << "priority");
    QString hash = request().posts["hash"];
    int file_id = request().posts["id"].toInt();
    int priority = request().posts["priority"].toInt();
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (h.is_valid() && h.has_metadata())
        h.file_priority(file_id, priority);
}

void WebApplication::action_command_getGlobalUpLimit()
{
    CHECK_URI(0);
    print(QByteArray::number(QBtSession::instance()->getSession()->settings().upload_rate_limit), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_getGlobalDlLimit()
{
    CHECK_URI(0);
    print(QByteArray::number(QBtSession::instance()->getSession()->settings().download_rate_limit), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_setGlobalUpLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("limit");
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;

    QBtSession::instance()->setUploadRateLimit(limit);
    if (Preferences::instance()->isAltBandwidthEnabled())
        Preferences::instance()->setAltGlobalUploadLimit(limit / 1024.);
    else
        Preferences::instance()->setGlobalUploadLimit(limit / 1024.);
}

void WebApplication::action_command_setGlobalDlLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("limit");
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;

    QBtSession::instance()->setDownloadRateLimit(limit);
    if (Preferences::instance()->isAltBandwidthEnabled())
        Preferences::instance()->setAltGlobalDownloadLimit(limit / 1024.);
    else
        Preferences::instance()->setGlobalDownloadLimit(limit / 1024.);
}

void WebApplication::action_command_getTorrentsUpLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    print(btjson::getTorrentsRatesLimits(hashes, false), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_command_getTorrentsDlLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    print(btjson::getTorrentsRatesLimits(hashes, true), Http::CONTENT_TYPE_JSON);
}

void WebApplication::action_command_setTorrentsUpLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "limit");

    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0)
        limit = -1;

    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid())
        h.set_upload_limit(limit);
    }
}

void WebApplication::action_command_setTorrentsDlLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "limit");

    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0)
        limit = -1;

    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid())
        h.set_download_limit(limit);
    }
}

void WebApplication::action_command_toggleAlternativeSpeedLimits()
{
    CHECK_URI(0);
    QBtSession::instance()->useAlternativeSpeedsLimit(!Preferences::instance()->isAltBandwidthEnabled());
}

void WebApplication::action_command_alternativeSpeedLimitsEnabled()
{
    CHECK_URI(0);
    print(QByteArray::number(Preferences::instance()->isAltBandwidthEnabled()), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_toggleSequentialDownload()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            h.toggleSequentialDownload();
        }
        catch(invalid_handle&) {}
    }
}

void WebApplication::action_command_toggleFirstLastPiecePrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            h.toggleFirstLastPiecePrio();
        }
        catch(invalid_handle&) {}
    }
}

void WebApplication::action_command_setSuperSeeding()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "value");
    bool value = request().posts["value"] == "true";
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        try {
            QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
            h.super_seeding(value);
        }
        catch(invalid_handle&) {}
    }
}

void WebApplication::action_command_setForceStart()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "value");
    bool value = request().posts["value"] == "true";
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid())
            QBtSession::instance()->resumeTorrent(hash, value);
    }
}

void WebApplication::action_command_delete()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        QBtSession::instance()->deleteTorrent(hash, false);
}

void WebApplication::action_command_deletePerm()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        QBtSession::instance()->deleteTorrent(hash, true);
}

void WebApplication::action_command_increasePrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

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

void WebApplication::action_command_decreasePrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

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

void WebApplication::action_command_topPrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

    foreach (const QString &hash, request().posts["hashes"].split("|")) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid()) h.queue_position_top();
    }
}

void WebApplication::action_command_bottomPrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

    foreach (const QString &hash, request().posts["hashes"].split("|")) {
        QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
        if (h.is_valid()) h.queue_position_bottom();
    }
}

void WebApplication::action_command_recheck()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");
    QBtSession::instance()->recheckTorrent(request().posts["hash"]);
}

bool WebApplication::isPublicScope()
{
    return (scope_ == DEFAULT_SCOPE || scope_ == VERSION_INFO);
}

void WebApplication::processRequest()
{
    scope_ = DEFAULT_SCOPE;
    action_ = DEFAULT_ACTION;

    parsePath();

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

void WebApplication::parsePath()
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

WebApplication::WebApplication(QObject *parent)
    : AbstractWebApplication(parent)
{
}

QMap<QString, QMap<QString, WebApplication::Action> > WebApplication::actions_ = WebApplication::initializeActions();
