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

#include "base/iconprovider.h"
#include "base/utils/misc.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "base/preferences.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/net/downloadmanager.h"
#include "btjson.h"
#include "prefjson.h"
#include "jsonutils.h"
#include "websessiondata.h"
#include "webapplication.h"

static const int API_VERSION = 9;
static const int API_VERSION_MIN = 9;

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
    ADD_ACTION(sync, torrent_peers);
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
    ADD_ACTION(command, setCategory);
    ADD_ACTION(command, getSavePath);
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

    QString token = request().posts["token"];

    bool equalUser = Utils::String::slowEquals(request().posts["username"].toUtf8(), pref->getWebUiUsername().toUtf8());
    bool equalPass = Utils::String::slowEquals(pass.toUtf8(), pref->getWebUiPassword().toUtf8());
    bool userAuthenticated = equalUser && equalPass;

    // check if the provided token matches one of our authentication tokens
    bool tokenAuthenticated = pref->isAuthenticationTokenValid(token);

    if (tokenAuthenticated || userAuthenticated) {
        sessionStart(token);
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

    QString url = IconProvider::instance()->getIconPath(args_.front());
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
//   - category (string): torrent category for filtering by it (empty string means "uncategorized"; no "category" param presented means "any category")
//   - sort (string): name of column for sorting by its value
//   - reverse (bool): enable reverse sorting
//   - limit (int): set limit number of torrents returned (if greater than 0, otherwise - unlimited)
//   - offset (int): set offset (if less than 0 - offset from end)
void WebApplication::action_query_torrents()
{
    CHECK_URI(0);
    const QStringMap& gets = request().gets;

    print(btjson::getTorrents(
        gets["filter"], gets["category"], gets["sort"], gets["reverse"] == "true",
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

// GET param:
//   - hash (string): torrent hash
//   - rid (int): last response id
void WebApplication::action_sync_torrent_peers()
{
    CHECK_URI(0);
    print(btjson::getSyncTorrentPeersData(request().gets["rid"].toInt(),
        request().gets["hash"],
        session()->syncTorrentPeersLastResponse,
        session()->syncTorrentPeersLastAcceptedResponse), Http::CONTENT_TYPE_JSON);
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
    QString urls = request().posts["urls"];
    QStringList list = urls.split('\n');
    QString savepath = request().posts["savepath"];
    QString category = request().posts["category"];
    QString cookie = request().posts["cookie"];
    QList<QNetworkCookie> cookies;
    if (!cookie.isEmpty()) {

        QStringList cookiesStr = cookie.split("; ");
        foreach (QString cookieStr, cookiesStr) {
            cookieStr = cookieStr.trimmed();
            int index = cookieStr.indexOf('=');
            if (index > 1) {
                QByteArray name = cookieStr.left(index).toLatin1();
                QByteArray value = cookieStr.right(cookieStr.length() - index - 1).toLatin1();
                QNetworkCookie c(name, value);
                cookies << c;
            }
        }
    }

    savepath = savepath.trimmed();
    category = category.trimmed();

    BitTorrent::AddTorrentParams params;
    params.savePath = savepath;
    params.category = category;

    foreach (QString url, list) {
        url = url.trimmed();
        if (!url.isEmpty()) {
            Net::DownloadManager::instance()->setCookiesFromUrl(cookies, QUrl::fromEncoded(url.toUtf8()));
            BitTorrent::Session::instance()->addTorrent(url, params);
        }
    }
}

void WebApplication::action_command_upload()
{
    qDebug() << Q_FUNC_INFO;
    CHECK_URI(0);
    QString savepath = request().posts["savepath"];
    QString category = request().posts["category"];

    savepath = savepath.trimmed();
    category = category.trimmed();

    foreach(const Http::UploadedFile& torrent, request().files) {
        QString filePath = saveTmpFile(torrent.data);

        if (!filePath.isEmpty()) {
            BitTorrent::TorrentInfo torrentInfo = BitTorrent::TorrentInfo::loadFromFile(filePath);
            if (!torrentInfo.isValid()) {
                status(415, "Unsupported Media Type");
                print(QObject::tr("Error: '%1' is not a valid torrent file.\n").arg(torrent.filename), Http::CONTENT_TYPE_TXT);
            }
            else {
                BitTorrent::AddTorrentParams params;
                params.savePath = savepath;
                params.category = category;
                if (!BitTorrent::Session::instance()->addTorrent(torrentInfo, params)) {
                    status(500, "Internal Server Error");
                    print(QObject::tr("Error: Could not add torrent to session."), Http::CONTENT_TYPE_TXT);
                }
            }
            // Clean up
            Utils::Fs::forceRemove(filePath);
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

    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
    if (torrent) {
        QList<BitTorrent::TrackerEntry> trackers;
        foreach (QString url, request().posts["urls"].split('\n')) {
            url = url.trimmed();
            if (!url.isEmpty())
                trackers << url;
        }
        torrent->addTrackers(trackers);
    }
}

void WebApplication::action_command_resumeAll()
{
    CHECK_URI(0);

    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->resume();
}

void WebApplication::action_command_pauseAll()
{
    CHECK_URI(0);

    foreach (BitTorrent::TorrentHandle *const torrent, BitTorrent::Session::instance()->torrents())
        torrent->pause();
}

void WebApplication::action_command_resume()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");

    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(request().posts["hash"]);
    if (torrent)
        torrent->resume();
}

void WebApplication::action_command_pause()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");

    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(request().posts["hash"]);
    if (torrent)
        torrent->pause();
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
    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);

    if (torrent && torrent->hasMetadata())
        torrent->setFilePriority(file_id, priority);
}

void WebApplication::action_command_getGlobalUpLimit()
{
    CHECK_URI(0);
    print(QByteArray::number(BitTorrent::Session::instance()->uploadRateLimit()), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_getGlobalDlLimit()
{
    CHECK_URI(0);
    print(QByteArray::number(BitTorrent::Session::instance()->downloadRateLimit()), Http::CONTENT_TYPE_TXT);
}

void WebApplication::action_command_setGlobalUpLimit()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("limit");
    qlonglong limit = request().posts["limit"].toLongLong();
    if (limit == 0) limit = -1;

    BitTorrent::Session::instance()->setUploadRateLimit(limit);
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

    BitTorrent::Session::instance()->setDownloadRateLimit(limit);
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
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->setUploadLimit(limit);
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
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->setDownloadLimit(limit);
    }
}

void WebApplication::action_command_toggleAlternativeSpeedLimits()
{
    CHECK_URI(0);
    BitTorrent::Session::instance()->changeSpeedLimitMode(!Preferences::instance()->isAltBandwidthEnabled());
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
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->toggleSequentialDownload();
    }
}

void WebApplication::action_command_toggleFirstLastPiecePrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->toggleFirstLastPiecePriority();
    }
}

void WebApplication::action_command_setSuperSeeding()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "value");
    bool value = request().posts["value"] == "true";
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->setSuperSeeding(value);
    }
}

void WebApplication::action_command_setForceStart()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "value");
    bool value = request().posts["value"] == "true";
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent)
            torrent->resume(value);
    }
}

void WebApplication::action_command_delete()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        BitTorrent::Session::instance()->deleteTorrent(hash, false);
}

void WebApplication::action_command_deletePerm()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");
    QStringList hashes = request().posts["hashes"].split("|");
    foreach (const QString &hash, hashes)
        BitTorrent::Session::instance()->deleteTorrent(hash, true);
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
    BitTorrent::Session::instance()->increaseTorrentsPriority(hashes);
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
    BitTorrent::Session::instance()->decreaseTorrentsPriority(hashes);
}

void WebApplication::action_command_topPrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

    QStringList hashes = request().posts["hashes"].split("|");
    BitTorrent::Session::instance()->topTorrentsPriority(hashes);
}

void WebApplication::action_command_bottomPrio()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes");

    if (!Preferences::instance()->isQueueingSystemEnabled()) {
        status(403, "Torrent queueing must be enabled");
        return;
    }

    QStringList hashes = request().posts["hashes"].split("|");
    BitTorrent::Session::instance()->bottomTorrentsPriority(hashes);
}

void WebApplication::action_command_recheck()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hash");

    BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(request().posts["hash"]);
    if (torrent)
        torrent->forceRecheck();
}

void WebApplication::action_command_setCategory()
{
    CHECK_URI(0);
    CHECK_PARAMETERS("hashes" << "category");

    QStringList hashes = request().posts["hashes"].split("|");
    QString category = request().posts["category"].trimmed();

    foreach (const QString &hash, hashes) {
        BitTorrent::TorrentHandle *const torrent = BitTorrent::Session::instance()->findTorrent(hash);
        if (torrent) {
            if (!torrent->setCategory(category)) {
                status(400, "Incorrect category name");
                return;
            }
        }
    }
}

void WebApplication::action_command_getSavePath()
{
    CHECK_URI(0);
    print(BitTorrent::Session::instance()->defaultSavePath());
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
    if (!pathItems.empty() && actions_.contains(pathItems.front())) {
        scope_ = pathItems.front();
        pathItems.pop_front();
    }

    if (!pathItems.empty() && actions_[scope_].contains(pathItems.front())) {
        action_ = pathItems.front();
        pathItems.pop_front();
    }

    args_ = pathItems;
}

WebApplication::WebApplication(QObject *parent)
    : AbstractWebApplication(parent)
{
}

QMap<QString, QMap<QString, WebApplication::Action> > WebApplication::actions_ = WebApplication::initializeActions();
