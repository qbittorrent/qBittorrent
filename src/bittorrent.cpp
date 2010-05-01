/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QDir>
#include <QDateTime>
#include <QString>
#include <QSettings>
#include <stdlib.h>

#include "filesystemwatcher.h"
#include "bittorrent.h"
#include "misc.h"
#include "downloadthread.h"
#include "filterparserthread.h"
#include "preferences.h"
#include "scannedfoldersmodel.h"
#ifndef DISABLE_GUI
#include "geoip.h"
#endif
#include "torrentpersistentdata.h"
#include "httpserver.h"
#include "bandwidthscheduler.h"
#include <libtorrent/extensions/ut_metadata.hpp>
#ifdef LIBTORRENT_0_15
#include <libtorrent/extensions/lt_trackers.hpp>
#endif
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
//#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <boost/filesystem/exception.hpp>
#include <queue>

const int MAX_TRACKER_ERRORS = 2;
const float MAX_RATIO = 100.;
enum ProxyType {HTTP=1, SOCKS5=2, HTTP_PW=3, SOCKS5_PW=4, SOCKS4=5};
enum VersionType { NORMAL,ALPHA,BETA,RELEASE_CANDIDATE,DEVEL };

// Main constructor
Bittorrent::Bittorrent()
  : m_scanFolders(ScanFoldersModel::instance(this)),
  preAllocateAll(false), addInPause(false), ratio_limit(-1),
  UPnPEnabled(false), NATPMPEnabled(false), LSDEnabled(false),
  DHTEnabled(false), current_dht_port(0), queueingEnabled(false),
  torrentExport(false), exiting(false)
#ifndef DISABLE_GUI
  , geoipDBLoaded(false), resolve_countries(false)
#endif
{
  // To avoid some exceptions
  fs::path::default_name_check(fs::no_check);
  // For backward compatibility
  // Move .qBittorrent content to XDG folder
  // TODO: Remove after some releases (introduced in v2.1.0)
  misc::moveToXDGFolders();
  // Creating Bittorrent session
  // Check if we should spoof utorrent
  QList<int> version;
  version << VERSION_MAJOR;
  version << VERSION_MINOR;
  version << VERSION_BUGFIX;
  version << VERSION_TYPE;
  QString peer_id = Preferences::getPeerID();
  if(peer_id.size() != 2) peer_id = "qB";
  if(peer_id != "qB") {
    QStringList peer_ver = Preferences::getClientVersion().split('.');
    while(peer_ver.size() < 4) {
      peer_ver << "0";
    }
    for(int i=0; i<peer_ver.size(); ++i) {
      QString ver = peer_ver.at(i);
      if(ver.size() != 1) {
        ver.truncate(1);
      }
      version.replace(i, ver.toInt());
    }
  }
  // Construct session
  s = new session(fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)), 0);
  std::cout << "Peer ID: " << fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)).to_string() << std::endl;
  addConsoleMessage("Peer ID: "+misc::toQString(fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)).to_string()));

  // Set severity level of libtorrent session
  s->set_alert_mask(alert::error_notification | alert::peer_notification | alert::port_mapping_notification | alert::storage_notification | alert::tracker_notification | alert::status_notification | alert::ip_block_notification);
  // Load previous state
  loadSessionState();
  // Enabling plugins
  //s->add_extension(&create_metadata_plugin);
  s->add_extension(&create_ut_metadata_plugin);
#ifdef LIBTORRENT_0_15
  s->add_extension(create_lt_trackers_plugin);
#endif
  if(Preferences::isPeXEnabled()) {
    PeXEnabled = true;
    s->add_extension(&create_ut_pex_plugin);
  } else {
    PeXEnabled = false;
  }
  s->add_extension(&create_smart_ban_plugin);
  timerAlerts = new QTimer();
  connect(timerAlerts, SIGNAL(timeout()), this, SLOT(readAlerts()));
  timerAlerts->start(3000);
  connect(&resumeDataTimer, SIGNAL(timeout()), this, SLOT(saveTempFastResumeData()));
  resumeDataTimer.start(180000); // 3min
  // To download from urls
  downloader = new downloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
  appendLabelToSavePath = Preferences::appendTorrentLabel();
#ifdef LIBTORRENT_0_15
  appendqBExtension = Preferences::useIncompleteFilesExtension();
#endif
  connect(m_scanFolders, SIGNAL(torrentsAdded(QStringList&)), this, SLOT(addTorrentsFromScanFolder(QStringList&)));
  // Apply user settings to Bittorrent session
  configureSession();
  qDebug("* BTSession constructed");
}

session_proxy Bittorrent::asyncDeletion() {
  qDebug("Bittorrent session async deletion IN");
  exiting = true;
  // Do some BT related saving
#ifndef LIBTORRENT_0_15
  saveDHTEntry();
#endif
  saveSessionState();
  saveFastResumeData();
  // Delete session
  session_proxy sp = s->abort();
  delete s;
  qDebug("Bittorrent session async deletion OUT");
  return sp;
}

// Main destructor
Bittorrent::~Bittorrent() {
  qDebug("BTSession destructor IN");
  if(!exiting) {
    // Do some BT related saving
 #ifndef LIBTORRENT_0_15
    saveDHTEntry();
#endif
    saveSessionState();
    saveFastResumeData();
    // Delete session
    session_proxy sp = s->abort();
    delete s;
  }
  // Delete our objects
  delete timerAlerts;
  if(BigRatioTimer)
    delete BigRatioTimer;
  if(filterParser)
    delete filterParser;
  delete downloader;
  if(bd_scheduler)
    delete bd_scheduler;
  // HTTP Server
  if(httpServer)
    delete httpServer;
  if(timerETA)
    delete timerETA;
  qDebug("BTSession destructor OUT");
}

void Bittorrent::preAllocateAllFiles(bool b) {
  const bool change = (preAllocateAll != b);
  if(change) {
    qDebug("PreAllocateAll changed, reloading all torrents!");
    preAllocateAll = b;
  }
}

ScanFoldersModel* Bittorrent::getScanFoldersModel() const {
  return m_scanFolders;
}

void Bittorrent::deleteBigRatios() {
  if(ratio_limit == -1) return;
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    const QTorrentHandle h(*torrentIT);
    if(!h.is_valid()) continue;
    if(h.is_seed()) {
      const QString hash = h.hash();
      const float ratio = getRealRatio(hash);
      if(ratio <= MAX_RATIO && ratio > ratio_limit) {
        addConsoleMessage(tr("%1 reached the maximum ratio you set.").arg(h.name()));
        deleteTorrent(hash);
        //emit torrent_ratio_deleted(fileName);
      }
    }
  }
}

void Bittorrent::setDownloadLimit(QString hash, long val) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid()) {
    h.set_download_limit(val);
  }
}

bool Bittorrent::isQueueingEnabled() const {
  return queueingEnabled;
}

void Bittorrent::setUploadLimit(QString hash, long val) {
  qDebug("Set upload limit rate to %ld", val);
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_valid()) {
    h.set_upload_limit(val);
  }
}

void Bittorrent::handleDownloadFailure(QString url, QString reason) {
  emit downloadFromUrlFailure(url, reason);
  // Clean up
  const QUrl &qurl = QUrl::fromEncoded(url.toLocal8Bit());
  const int index = url_skippingDlg.indexOf(qurl);
  if(index >= 0)
    url_skippingDlg.removeAt(index);
  if(savepath_fromurl.contains(qurl))
    savepath_fromurl.remove(qurl);
}

void Bittorrent::startTorrentsInPause(bool b) {
  addInPause = b;
}

void Bittorrent::setQueueingEnabled(bool enable) {
  if(queueingEnabled != enable) {
    qDebug("Queueing system is changing state...");
    queueingEnabled = enable;
  }
}

// Set BT session configuration
void Bittorrent::configureSession() {
  qDebug("Configuring session");
  // Downloads
  // * Save path
  defaultSavePath = Preferences::getSavePath();
  if(Preferences::isTempPathEnabled()) {
    setDefaultTempPath(Preferences::getTempPath());
  } else {
    setDefaultTempPath(QString::null);
  }
  setAppendLabelToSavePath(Preferences::appendTorrentLabel());
#ifdef LIBTORRENT_0_15
  setAppendqBExtension(Preferences::useIncompleteFilesExtension());
#endif
  preAllocateAllFiles(Preferences::preAllocateAllFiles());
  startTorrentsInPause(Preferences::addTorrentsInPause());
  // * Scan dirs
  const QStringList &scan_dirs = Preferences::getScanDirs();
  QVariantList downloadInDirList = Preferences::getDownloadInScanDirs();
  while(scan_dirs.size() > downloadInDirList.size()) {
    downloadInDirList << QVariant(false);
  }
  int i = 0;
  foreach (const QString &dir, scan_dirs) {
    m_scanFolders->addPath(dir, downloadInDirList.at(i).toBool());
    ++i;
  }
  // * Export Dir
  const bool newTorrentExport = Preferences::isTorrentExportEnabled();
  if(torrentExport != newTorrentExport) {
    torrentExport = newTorrentExport;
    if(torrentExport) {
      qDebug("Torrent export is enabled, exporting the current torrents");
      exportTorrentFiles(Preferences::getExportDir());
    }
  }
  // Connection
  // * Ports binding
  const unsigned short old_listenPort = getListenPort();
  const unsigned short new_listenPort = Preferences::getSessionPort();
  if(old_listenPort != new_listenPort) {
    setListeningPort(new_listenPort);
    addConsoleMessage(tr("qBittorrent is bound to port: TCP/%1", "e.g: qBittorrent is bound to port: 6881").arg(QString::number(new_listenPort)));
  }
  // * Global download limit
  const bool alternative_speeds = Preferences::isAltBandwidthEnabled();
  int down_limit;
  if(alternative_speeds)
    down_limit = Preferences::getAltGlobalDownloadLimit();
  else
    down_limit = Preferences::getGlobalDownloadLimit();
  if(down_limit <= 0) {
    // Download limit disabled
    setDownloadRateLimit(-1);
  } else {
    // Enabled
    setDownloadRateLimit(down_limit*1024);
  }
  int up_limit;
  if(alternative_speeds)
    up_limit = Preferences::getAltGlobalUploadLimit();
  else
    up_limit = Preferences::getGlobalUploadLimit();
  // * Global Upload limit
  if(up_limit <= 0) {
    // Upload limit disabled
    setUploadRateLimit(-1);
  } else {
    // Enabled
    setUploadRateLimit(up_limit*1024);
  }
  if(Preferences::isSchedulerEnabled()) {
    if(!bd_scheduler) {
      bd_scheduler = new BandwidthScheduler(this);
      connect(bd_scheduler, SIGNAL(switchToAlternativeMode(bool)), this, SLOT(useAlternativeSpeedsLimit(bool)));
    }
    bd_scheduler->start();
  } else {
    if(bd_scheduler) delete bd_scheduler;
  }
#ifndef DISABLE_GUI
  // Resolve countries
  qDebug("Loading country resolution settings");
  const bool new_resolv_countries = Preferences::resolvePeerCountries();
  if(resolve_countries != new_resolv_countries) {
    qDebug("in country reoslution settings");
    resolve_countries = new_resolv_countries;
    if(resolve_countries && !geoipDBLoaded) {
      qDebug("Loading geoip database");
      GeoIP::loadDatabase(s);
      geoipDBLoaded = true;
    }
    // Update torrent handles
    std::vector<torrent_handle> torrents = getTorrents();
    std::vector<torrent_handle>::iterator torrentIT;
    for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if(h.is_valid())
        h.resolve_countries(resolve_countries);
    }
  }
#endif
  // * UPnP
  if(Preferences::isUPnPEnabled()) {
    enableUPnP(true);
    addConsoleMessage(tr("UPnP support [ON]"), QString::fromUtf8("blue"));
  } else {
    enableUPnP(false);
    addConsoleMessage(tr("UPnP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * NAT-PMP
  if(Preferences::isNATPMPEnabled()) {
    enableNATPMP(true);
    addConsoleMessage(tr("NAT-PMP support [ON]"), QString::fromUtf8("blue"));
  } else {
    enableNATPMP(false);
    addConsoleMessage(tr("NAT-PMP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Session settings
  session_settings sessionSettings;
  QString peer_id = Preferences::getPeerID();
  if(peer_id.size() != 2) peer_id = "qB";
  if(peer_id == "UT") {
    QString version = Preferences::getClientVersion().replace(".", "");
    while(version.size() < 4)
      version.append("0");
    const QString build = Preferences::getClientBuild();
    sessionSettings.user_agent = QString("uTorrent/"+version+"("+build+")").toStdString();
  } else {
    if(peer_id == "AZ") {
      QStringList version = Preferences::getClientVersion().split(".");
      while(version.size() < 4)
        version << "0";
      sessionSettings.user_agent = QString("Azureus "+version.join(".")).toStdString();
    } else {
      if(peer_id == "KT") {
        sessionSettings.user_agent = QString("KTorrent/"+Preferences::getClientVersion()).toStdString();
      } else {
        sessionSettings.user_agent = "qBittorrent "VERSION;
      }
    }
  }
  std::cout << "HTTP user agent is " << sessionSettings.user_agent << std::endl;
  addConsoleMessage(tr("HTTP user agent is %1").arg(misc::toQString(sessionSettings.user_agent)));

  sessionSettings.upnp_ignore_nonrouters = true;
  sessionSettings.use_dht_as_fallback = false;
  // To prevent ISPs from blocking seeding
  sessionSettings.lazy_bitfields = true;
  // Speed up exit
  sessionSettings.stop_tracker_timeout = 1;
  //sessionSettings.announce_to_all_trackers = true;
  sessionSettings.auto_scrape_interval = 1200; // 20 minutes
#ifdef LIBTORRENT_0_15
  sessionSettings.announce_to_all_trackers = true;
  sessionSettings.announce_to_all_tiers = true; //uTorrent behavior
  sessionSettings.auto_scrape_min_interval = 900; // 15 minutes
#endif
  // To keep same behavior as in qBittorrent v1.2.0
  sessionSettings.rate_limit_ip_overhead = false;
  sessionSettings.cache_size = Preferences::diskCacheSize()*64;
  addConsoleMessage(tr("Using a disk cache size of %1 MiB").arg(Preferences::diskCacheSize()));
  // Queueing System
  if(Preferences::isQueueingSystemEnabled()) {
    sessionSettings.active_downloads = Preferences::getMaxActiveDownloads();
    sessionSettings.active_seeds = Preferences::getMaxActiveUploads();
    sessionSettings.active_limit = Preferences::getMaxActiveTorrents();
    sessionSettings.dont_count_slow_torrents = false;
    setQueueingEnabled(true);
  } else {
    sessionSettings.active_downloads = -1;
    sessionSettings.active_seeds = -1;
    sessionSettings.active_limit = -1;
    setQueueingEnabled(false);
  }
  // Outgoing ports
  sessionSettings.outgoing_ports = std::make_pair(Preferences::outgoingPortsMin(), Preferences::outgoingPortsMax());
  setSessionSettings(sessionSettings);
  // Ignore limits on LAN
  sessionSettings.ignore_limits_on_local_network = Preferences::ignoreLimitsOnLAN();
  // Include overhead in transfer limits
  sessionSettings.rate_limit_ip_overhead = Preferences::includeOverheadInLimits();
  // Bittorrent
  // * Max connections limit
  setMaxConnections(Preferences::getMaxConnecs());
  // * Max connections per torrent limit
  setMaxConnectionsPerTorrent(Preferences::getMaxConnecsPerTorrent());
  // * Max uploads per torrent limit
  setMaxUploadsPerTorrent(Preferences::getMaxUploadsPerTorrent());
  // * DHT
  if(Preferences::isDHTEnabled()) {
    // Set DHT Port
    if(enableDHT(true)) {
      int dht_port;
      if(Preferences::isDHTPortSameAsBT())
        dht_port = 0;
      else
        dht_port = Preferences::getDHTPort();
      setDHTPort(dht_port);
      if(dht_port == 0) dht_port = new_listenPort;
      addConsoleMessage(tr("DHT support [ON], port: UDP/%1").arg(dht_port), QString::fromUtf8("blue"));
    } else {
      addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("red"));
    }
  } else {
    enableDHT(false);
    addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("blue"));
  }
  // * PeX
  if(PeXEnabled) {
    addConsoleMessage(tr("PeX support [ON]"), QString::fromUtf8("blue"));
  } else {
    addConsoleMessage(tr("PeX support [OFF]"), QString::fromUtf8("red"));
  }
  if(PeXEnabled != Preferences::isPeXEnabled()) {
    addConsoleMessage(tr("Restart is required to toggle PeX support"), QString::fromUtf8("red"));
  }
  // * LSD
  if(Preferences::isLSDEnabled()) {
    enableLSD(true);
    addConsoleMessage(tr("Local Peer Discovery [ON]"), QString::fromUtf8("blue"));
  } else {
    enableLSD(false);
    addConsoleMessage(tr("Local Peer Discovery support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Encryption
  const int encryptionState = Preferences::getEncryptionSetting();
  // The most secure, rc4 only so that all streams and encrypted
  pe_settings encryptionSettings;
  encryptionSettings.allowed_enc_level = pe_settings::rc4;
  encryptionSettings.prefer_rc4 = true;
  switch(encryptionState) {
  case 0: //Enabled
    encryptionSettings.out_enc_policy = pe_settings::enabled;
    encryptionSettings.in_enc_policy = pe_settings::enabled;
    addConsoleMessage(tr("Encryption support [ON]"), QString::fromUtf8("blue"));
    break;
  case 1: // Forced
    encryptionSettings.out_enc_policy = pe_settings::forced;
    encryptionSettings.in_enc_policy = pe_settings::forced;
    addConsoleMessage(tr("Encryption support [FORCED]"), QString::fromUtf8("blue"));
    break;
  default: // Disabled
    encryptionSettings.out_enc_policy = pe_settings::disabled;
    encryptionSettings.in_enc_policy = pe_settings::disabled;
    addConsoleMessage(tr("Encryption support [OFF]"), QString::fromUtf8("blue"));
  }
  applyEncryptionSettings(encryptionSettings);
  // * Desired ratio
  setGlobalRatio(Preferences::getDesiredRatio());
  // * Maximum ratio
  setDeleteRatio(Preferences::getDeleteRatio());
  // Ip Filter
  FilterParserThread::processFilterList(s, Preferences::bannedIPs());
  if(Preferences::isFilteringEnabled()) {
    enableIPFilter(Preferences::getFilter());
  }else{
    disableIPFilter();
  }
  // Update Web UI
  if (Preferences::isWebUiEnabled()) {
    const quint16 port = Preferences::getWebUiPort();
    const QString username = Preferences::getWebUiUsername();
    const QString password = Preferences::getWebUiPassword();
    initWebUi(username, password, port);
  } else if(httpServer) {
    delete httpServer;
  }
  // * Proxy settings
  proxy_settings proxySettings;
  if(Preferences::isPeerProxyEnabled()) {
    qDebug("Enabling P2P proxy");
    proxySettings.hostname = Preferences::getPeerProxyIp().toStdString();
    qDebug("hostname is %s", proxySettings.hostname.c_str());
    proxySettings.port = Preferences::getPeerProxyPort();
    qDebug("port is %d", proxySettings.port);
    if(Preferences::isPeerProxyAuthEnabled()) {
      proxySettings.username = Preferences::getPeerProxyUsername().toStdString();
      proxySettings.password = Preferences::getPeerProxyPassword().toStdString();
      qDebug("username is %s", proxySettings.username.c_str());
      qDebug("password is %s", proxySettings.password.c_str());
    }
  }
  switch(Preferences::getPeerProxyType()) {
  case HTTP:
    qDebug("type: http");
    proxySettings.type = proxy_settings::http;
    break;
  case HTTP_PW:
    qDebug("type: http_pw");
    proxySettings.type = proxy_settings::http_pw;
    break;
  case SOCKS4:
    proxySettings.type = proxy_settings::socks4;
  case SOCKS5:
    qDebug("type: socks5");
    proxySettings.type = proxy_settings::socks5;
    break;
  case SOCKS5_PW:
    qDebug("type: socks5_pw");
    proxySettings.type = proxy_settings::socks5_pw;
    break;
  default:
    proxySettings.type = proxy_settings::none;
  }
  setPeerProxySettings(proxySettings);
  // HTTP Proxy
  proxy_settings http_proxySettings;
  qDebug("HTTP Communications proxy type: %d", Preferences::getHTTPProxyType());
  switch(Preferences::getHTTPProxyType()) {
  case HTTP_PW:
    http_proxySettings.type = proxy_settings::http_pw;
    http_proxySettings.username = Preferences::getHTTPProxyUsername().toStdString();
    http_proxySettings.password = Preferences::getHTTPProxyPassword().toStdString();
    http_proxySettings.hostname = Preferences::getHTTPProxyIp().toStdString();
    http_proxySettings.port = Preferences::getHTTPProxyPort();
    break;
  case HTTP:
    http_proxySettings.type = proxy_settings::http;
    http_proxySettings.hostname = Preferences::getHTTPProxyIp().toStdString();
    http_proxySettings.port = Preferences::getHTTPProxyPort();
    break;
  case SOCKS5:
    http_proxySettings.type = proxy_settings::socks5;
    http_proxySettings.hostname = Preferences::getHTTPProxyIp().toStdString();
    http_proxySettings.port = Preferences::getHTTPProxyPort();
    break;
  case SOCKS5_PW:
    http_proxySettings.type = proxy_settings::socks5_pw;
    http_proxySettings.username = Preferences::getHTTPProxyUsername().toStdString();
    http_proxySettings.password = Preferences::getHTTPProxyPassword().toStdString();
    http_proxySettings.hostname = Preferences::getHTTPProxyIp().toStdString();
    http_proxySettings.port = Preferences::getHTTPProxyPort();
    break;
  default:
    http_proxySettings.type = proxy_settings::none;
  }
  setHTTPProxySettings(http_proxySettings);
  qDebug("Session configured");
}

bool Bittorrent::initWebUi(QString username, QString password, int port) {
  if(httpServer) {
    if(httpServer->serverPort() != port) {
      httpServer->close();
    }
  } else {
    httpServer = new HttpServer(this, 3000, this);
  }
  httpServer->setAuthorization(username, password);
  bool success = true;
  if(!httpServer->isListening()) {
    success = httpServer->listen(QHostAddress::Any, port);
    if (success)
      addConsoleMessage(tr("The Web UI is listening on port %1").arg(port));
    else
      addConsoleMessage(tr("Web User Interface Error - Unable to bind Web UI to port %1").arg(port), "red");
  }
  return success; 
}

void Bittorrent::useAlternativeSpeedsLimit(bool alternative) {
  // Save new state to remember it on startup
  Preferences::setAltBandwidthEnabled(alternative);
  // Apply settings to the bittorrent session
  if(alternative) {
    s->set_download_rate_limit(Preferences::getAltGlobalDownloadLimit()*1024);
    s->set_upload_rate_limit(Preferences::getAltGlobalUploadLimit()*1024);
  } else {
    s->set_download_rate_limit(Preferences::getGlobalDownloadLimit()*1024);
    s->set_upload_rate_limit(Preferences::getGlobalUploadLimit()*1024);
  }
  emit alternativeSpeedsModeChanged(alternative);
}

void Bittorrent::takeETASamples() {
  bool change = false;;
  foreach(const QString &hash, ETA_samples.keys()) {
    const QTorrentHandle &h = getTorrentHandle(hash);
    if(h.is_valid() && !h.is_paused() && !h.is_seed()) {
      QList<int> samples = ETA_samples.value(h.hash(), QList<int>());
      if(samples.size() >= MAX_SAMPLES)
        samples.removeFirst();
      samples.append(h.download_payload_rate());
      ETA_samples[h.hash()] = samples;
      change = true;
    } else {
      ETA_samples.remove(hash);
    }
  }
  if(!change && timerETA) {
    delete timerETA;
  }
}

// This algorithm was inspired from KTorrent - http://www.ktorrent.org
// Calculate the ETA using a combination of several algorithms:
// GASA: Global Average Speed Algorithm
// CSA: Current Speed Algorithm
// WINX: Window of X Algorithm
qlonglong Bittorrent::getETA(QString hash) {
  const QTorrentHandle &h = getTorrentHandle(hash);
  if(!h.is_valid() || h.state() != torrent_status::downloading || !h.active_time())
    return -1;
  // See if the torrent is going to be completed soon
  const qulonglong bytes_left = h.actual_size() - h.total_wanted_done();
  if(h.actual_size() > 10485760L) { // Size > 10MiB
    if(h.progress() >= (float)0.99 && bytes_left < 10485760L) { // Progress>99% but less than 10MB left.
      // Compute by taking samples
      if(!ETA_samples.contains(h.hash())) {
        ETA_samples[h.hash()] = QList<int>();
      }
      if(!timerETA) {
        timerETA = new QTimer(this);
        connect(timerETA, SIGNAL(timeout()), this, SLOT(takeETASamples()));
        timerETA->start();
      } else {
        const QList<int> &samples = ETA_samples.value(h.hash(), QList<int>());
        const int nb_samples = samples.size();
        if(nb_samples > 3) {
          long sum_samples = 0;
          foreach(const int val, samples) {
            sum_samples += val;
          }
          // Use WINX
          return (qlonglong)(((double)bytes_left) / (((double)sum_samples) / ((double)nb_samples)));
        }
      }
    }
  }
  // Normal case: Use GASA
  double avg_speed = (double)h.all_time_download() / h.active_time();
  return (qlonglong) floor((double) (bytes_left) / avg_speed);
}

std::vector<torrent_handle> Bittorrent::getTorrents() const {
  return s->get_torrents();
}

// Return the torrent handle, given its hash
QTorrentHandle Bittorrent::getTorrentHandle(QString hash) const{
  return QTorrentHandle(s->find_torrent(misc::QStringToSha1(hash)));
}

bool Bittorrent::hasActiveTorrents() const {
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    const QTorrentHandle h(*torrentIT);
    if(h.is_valid() && !h.is_paused() && !h.is_queued())
      return true;
  }
  return false;
}

void Bittorrent::banIP(QString ip) {
  FilterParserThread::processFilterList(s, QStringList(ip));
  Preferences::banIP(ip);
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void Bittorrent::deleteTorrent(QString hash, bool delete_local_files) {
  qDebug("Deleting torrent with hash: %s", qPrintable(hash));
  const QTorrentHandle &h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  const QString &fileName = h.name();
  // Remove it from session
  if(delete_local_files)
    s->remove_torrent(h.get_torrent_handle(), session::delete_files);
  else
    s->remove_torrent(h.get_torrent_handle());
  // Remove it from torrent backup directory
  QDir torrentBackup(misc::BTBackupLocation());
  QStringList filters;
  filters << hash+".*";
  const QStringList &files = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  foreach(const QString &file, files) {
    torrentBackup.remove(file);
  }
  TorrentPersistentData::deletePersistentData(hash);
  // Remove tracker errors
  trackersInfos.remove(hash);
  if(delete_local_files)
    addConsoleMessage(tr("'%1' was removed from transfer list and hard disk.", "'xxx.avi' was removed...").arg(fileName));
  else
    addConsoleMessage(tr("'%1' was removed from transfer list.", "'xxx.avi' was removed...").arg(fileName));
  emit deletedTorrent(hash);
}

void Bittorrent::pauseAllTorrents() {
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(!h.is_paused()) {
      h.pause();
      emit pausedTorrent(h);
    }
  }
}

void Bittorrent::resumeAllTorrents() {
  std::vector<torrent_handle> torrents = getTorrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid()) continue;
    if(h.is_paused()) {
      h.resume();
      emit resumedTorrent(h);
    }
  }
}

void Bittorrent::pauseTorrent(QString hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_paused()) {
    h.pause();
    emit pausedTorrent(h);
  }
}

void Bittorrent::resumeTorrent(QString hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if(h.is_paused()) {
    h.resume();
    emit resumedTorrent(h);
  }
}

QTorrentHandle Bittorrent::addMagnetUri(QString magnet_uri, bool resumed) {
  QTorrentHandle h;
  const QString &hash = misc::magnetUriToHash(magnet_uri);
  if(hash.isEmpty()) {
    addConsoleMessage(tr("'%1' is not a valid magnet URI.").arg(magnet_uri));
    return h;
  }
  const QDir torrentBackup(misc::BTBackupLocation());
  if(resumed) {
    qDebug("Resuming magnet URI: %s", qPrintable(hash));
    // Load metadata
    if(QFile::exists(torrentBackup.path()+QDir::separator()+hash+QString(".torrent")))
      return addTorrent(torrentBackup.path()+QDir::separator()+hash+QString(".torrent"), false, false, true);
  } else {
    qDebug("Adding new magnet URI");
  }

  bool fastResume=false;
  Q_ASSERT(magnet_uri.startsWith("magnet:"));

  // Check if torrent is already in download list
  if(s->find_torrent(sha1_hash(hash.toLocal8Bit().constData())).is_valid()) {
    qDebug("/!\\ Torrent is already in download list");
    // Update info Bar
    addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(magnet_uri));
    return h;
  }

  add_torrent_params p;
  //Getting fast resume data if existing
  std::vector<char> buf;
  if(resumed) {
    const QString fastresume_path = torrentBackup.path()+QDir::separator()+hash+QString(".fastresume");
    qDebug("Trying to load fastresume data: %s", qPrintable(fastresume_path));
    if (load_file(fastresume_path.toLocal8Bit().constData(), buf) == 0) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfuly loaded");
    }
  }
  const QString &savePath = getSavePath(hash);
  if(!defaultTempPath.isEmpty() && resumed && !TorrentPersistentData::isSeed(hash)) {
    qDebug("addMagnetURI: Temp folder is enabled.");
    p.save_path = defaultTempPath.toLocal8Bit().constData();
    qDebug("addMagnetURI: using save_path: %s", qPrintable(defaultTempPath));
  } else {
    p.save_path = savePath.toLocal8Bit().constData();
    qDebug("addMagnetURI: using save_path: %s", qPrintable(savePath));
  }

#ifdef LIBTORRENT_0_15
  // Skip checking and directly start seeding (new in libtorrent v0.15)
  if(TorrentTempData::isSeedingMode(hash)){
    p.seed_mode=true;
  } else {
    p.seed_mode=false;
  }
#endif
  // Preallocate all?
  if(preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;
  // Start in pause
  p.paused = true;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = false; // Because it is added in paused state
  // Adding torrent to Bittorrent session
  try {
    h =  QTorrentHandle(add_magnet_uri(*s, magnet_uri.toStdString(), p));
  }catch(std::exception e){
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if(!h.is_valid()) {
    // No need to keep on, it failed.
    qDebug("/!\\ Error: Invalid handle");
    return h;
  }
  Q_ASSERT(h.hash() == hash);

  // If temp path is enabled, move torrent
  if(!defaultTempPath.isEmpty() && !resumed) {
    qDebug("Temp folder is enabled, moving new torrent to temp folder");
    h.move_storage(defaultTempPath);
  }

  // Connections limit per torrent
  h.set_max_connections(Preferences::getMaxConnecsPerTorrent());
  // Uploads limit per torrent
  h.set_max_uploads(Preferences::getMaxUploadsPerTorrent());
#ifndef DISABLE_GUI
  // Resolve countries
  h.resolve_countries(resolve_countries);
#endif
  // Load filtered files
  if(!resumed) {
    // Sequential download
    if(TorrentTempData::hasTempData(hash)) {
      qDebug("addMagnetURI Setting download as sequential (from tmp data)");
      h.set_sequential_download(TorrentTempData::isSequential(hash));
    }
    const QString &label = TorrentTempData::getLabel(hash);
    // Save persistent data for new torrent
    TorrentPersistentData::saveTorrentPersistentData(h, true);
    // Save Label
    if(!label.isEmpty()) {
      TorrentPersistentData::saveLabel(hash, label);
    }
    // Save save_path
    if(!defaultTempPath.isEmpty()) {
      qDebug("addTorrent: Saving save_path in persistent data: %s", qPrintable(savePath));
      TorrentPersistentData::saveSavePath(hash, savePath);
    }
  }
  if(!fastResume && (!addInPause || (Preferences::useAdditionDialog()))) {
    // Start torrent because it was added in paused state
    h.resume();
  }
  // Send torrent addition signal
  if(fastResume)
    addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(magnet_uri));
  else
    addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(magnet_uri));
  emit addedTorrent(h);
  return h;
}

// Add a torrent to the Bittorrent session
QTorrentHandle Bittorrent::addTorrent(QString path, bool fromScanDir, QString from_url, bool resumed) {
  QTorrentHandle h;
  bool fastResume=false;
  const QDir torrentBackup(misc::BTBackupLocation());
  QString hash;
  boost::intrusive_ptr<torrent_info> t;

  // Checking if BT_backup Dir exists
  // create it if it is not
  if(! torrentBackup.exists()) {
    if(! torrentBackup.mkpath(torrentBackup.path())) {
      std::cerr << "Couldn't create the directory: '" << qPrintable(torrentBackup.path()) << "'\n";
      exit(1);
    }
  }
  // Processing torrents
  const QString file = path.trimmed().replace("file://", "", Qt::CaseInsensitive);
  if(file.isEmpty()) {
    return h;
  }
  Q_ASSERT(!file.startsWith("http://", Qt::CaseInsensitive) && !file.startsWith("https://", Qt::CaseInsensitive) && !file.startsWith("ftp://", Qt::CaseInsensitive));

  qDebug("Adding %s to download list", qPrintable(file));
  try {
    // Getting torrent file informations
    t = new torrent_info(file.toLocal8Bit().constData());
    if(!t->is_valid())
      throw std::exception();
  } catch(std::exception&) {
    if(!from_url.isNull()) {
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(from_url), QString::fromUtf8("red"));
      //emit invalidTorrent(from_url);
      QFile::remove(file);
    }else{
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(file), QString::fromUtf8("red"));
      //emit invalidTorrent(file);
    }
    addConsoleMessage(tr("This file is either corrupted or this isn't a torrent."),QString::fromUtf8("red"));
    if(fromScanDir) {
      // Remove file
      QFile::remove(file);
    }
    return h;
  }
  hash = misc::toQString(t->info_hash());

  qDebug(" -> Hash: %s", qPrintable(hash));
  qDebug(" -> Name: %s", t->name().c_str());

  // Check if torrent is already in download list
  if(s->find_torrent(t->info_hash()).is_valid()) {
    qDebug("/!\\ Torrent is already in download list");
    // Update info Bar
    if(!from_url.isNull()) {
      // If download from url, remove temp file
      QFile::remove(file);
      addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(from_url));
      //emit duplicateTorrent(from_url);
    }else{
      addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(file));
      //emit duplicateTorrent(file);
    }
    // Check if the torrent contains trackers or url seeds we don't know about
    // and add them
    QTorrentHandle h_ex = getTorrentHandle(hash);
    if(h_ex.is_valid()) {
      std::vector<announce_entry> old_trackers = h_ex.trackers();
      std::vector<announce_entry> new_trackers = t->trackers();
      bool trackers_added = false;
      for(std::vector<announce_entry>::iterator it=new_trackers.begin();it!=new_trackers.end();it++) {
        std::string tracker_url = it->url;
        bool found = false;
        for(std::vector<announce_entry>::iterator itold=old_trackers.begin();itold!=old_trackers.end();itold++) {
          if(tracker_url == itold->url) {
            found = true;
            break;
          }
        }
        if(found) {
          trackers_added = true;
          announce_entry entry(tracker_url);
          h_ex.add_tracker(entry);
        }
      }
      if(trackers_added) {
        addConsoleMessage(tr("Note: new trackers were added to the existing torrent."));
      }
      bool urlseeds_added = false;
      const QStringList &old_urlseeds = h_ex.url_seeds();
      std::vector<std::string> new_urlseeds = t->url_seeds();
      for(std::vector<std::string>::iterator it = new_urlseeds.begin(); it != new_urlseeds.end(); it++) {
        const QString &new_url = misc::toQString(it->c_str());
        if(!old_urlseeds.contains(new_url)) {
          urlseeds_added = true;
          h_ex.add_url_seed(new_url);
        }
      }
      if(urlseeds_added) {
        addConsoleMessage(tr("Note: new URL seeds were added to the existing torrent."));
      }
    }
    if(fromScanDir) {
      // Delete torrent from scan dir
      QFile::remove(file);
    }
    return h;
  }
  add_torrent_params p;
  //Getting fast resume data if existing
  std::vector<char> buf;
  if(resumed) {
    const QString fastresume_path = torrentBackup.path()+QDir::separator()+hash+QString(".fastresume");
    qDebug("Trying to load fastresume data: %s", qPrintable(fastresume_path));
    if (load_file(fastresume_path.toLocal8Bit().constData(), buf) == 0) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfuly loaded");
    }
  }
  QString savePath;
  if(!from_url.isEmpty() && savepath_fromurl.contains(QUrl::fromEncoded(from_url.toLocal8Bit()))) {
    // Enforcing the save path defined before URL download (from RSS for example)
    savePath = savepath_fromurl.take(QUrl::fromEncoded(from_url.toLocal8Bit()));
  } else {
    savePath = getSavePath(hash, fromScanDir, path);
  }
  if(!defaultTempPath.isEmpty() && resumed && !TorrentPersistentData::isSeed(hash)) {
    qDebug("addTorrent::Temp folder is enabled.");
    p.save_path = defaultTempPath.toLocal8Bit().constData();
    qDebug("addTorrent: using save_path: %s", qPrintable(defaultTempPath));
  } else {
    p.save_path = savePath.toLocal8Bit().constData();
    qDebug("addTorrent: using save_path: %s", qPrintable(savePath));
  }

#ifdef LIBTORRENT_0_15
  // Skip checking and directly start seeding (new in libtorrent v0.15)
  if(TorrentTempData::isSeedingMode(hash)){
    p.seed_mode=true;
  } else {
    p.seed_mode=false;
  }
#endif
  p.ti = t;
  // Preallocate all?
  if(preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;
  // Start in pause
  p.paused = true;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = false; // Because it is added in paused state
  // Adding torrent to Bittorrent session
  try {
    h =  QTorrentHandle(s->add_torrent(p));
  }catch(std::exception e){
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if(!h.is_valid()) {
    // No need to keep on, it failed.
    qDebug("/!\\ Error: Invalid handle");
    // If download from url, remove temp file
    if(!from_url.isNull()) QFile::remove(file);
    return h;
  }

  // If temp path is enabled, move torrent
  if(!defaultTempPath.isEmpty() && !resumed) {
    qDebug("Temp folder is enabled, moving new torrent to temp folder");
    h.move_storage(defaultTempPath);
  }

  // Connections limit per torrent
  h.set_max_connections(Preferences::getMaxConnecsPerTorrent());
  // Uploads limit per torrent
  h.set_max_uploads(Preferences::getMaxUploadsPerTorrent());
#ifndef DISABLE_GUI
  // Resolve countries
  qDebug("AddTorrent: Resolve_countries: %d", (int)resolve_countries);
  h.resolve_countries(resolve_countries);
#endif
  if(!resumed) {
    // Sequential download
    if(TorrentTempData::hasTempData(hash)) {
      qDebug("addTorrent: Setting download as sequential (from tmp data)");
      h.prioritize_files(TorrentTempData::getFilesPriority(hash));
      h.set_sequential_download(TorrentTempData::isSequential(hash));
      // Import Files names from torrent addition dialog
      const QStringList &files_path = TorrentTempData::getFilesPath(hash);
      bool force_recheck = false;
      if(files_path.size() == h.num_files()) {
        for(int i=0; i<h.num_files(); ++i) {
          const QString &path = files_path.at(i);
          if(!force_recheck && QFile::exists(h.save_path()+QDir::separator()+path))
            force_recheck = true;
          h.rename_file(i, path);
        }
        // Force recheck
        if(force_recheck) h.force_recheck();
      }
    }
    const QString &label = TorrentTempData::getLabel(hash);
    // Save persistent data for new torrent
    TorrentPersistentData::saveTorrentPersistentData(h);
    // Save Label
    if(!label.isEmpty()) {
      TorrentPersistentData::saveLabel(hash, label);
    }
    // Save save_path
    if(!defaultTempPath.isEmpty()) {
      qDebug("addTorrent: Saving save_path in persistent data: %s", qPrintable(savePath));
      TorrentPersistentData::saveSavePath(hash, savePath);
    }
#ifdef LIBTORRENT_0_15
    // Append .!qB to incomplete files
    if(appendqBExtension)
      appendqBextensionToTorrent(h, true);
#endif
    // Backup torrent file
    const QString &newFile = torrentBackup.absoluteFilePath(hash + ".torrent");
    if(file != newFile) {
      // Delete file from torrentBackup directory in case it exists because
      // QFile::copy() do not overwrite
      QFile::remove(newFile);
      // Copy it to torrentBackup directory
      QFile::copy(file, newFile);
    }
    // Copy the torrent file to the export folder
    if(torrentExport) {
      QDir exportPath(Preferences::getExportDir());
      if(exportPath.exists() || exportPath.mkpath(exportPath.absolutePath())) {
        QString torrent_path = exportPath.absoluteFilePath(h.name()+".torrent");
        if(QFile::exists(torrent_path) && misc::sameFiles(file, torrent_path)) {
          // Append hash to torrent name to make it unique
          torrent_path = exportPath.absoluteFilePath(h.name()+"-"+h.hash()+".torrent");
        }
        QFile::copy(file, torrent_path);
        //h.save_torrent_file(torrent_path);
      }
    }
  }
  if(!fastResume && (!addInPause || (Preferences::useAdditionDialog() && !fromScanDir))) {
    // Start torrent because it was added in paused state
    h.resume();
  }
  // If download from url, remove temp file
  if(!from_url.isNull()) QFile::remove(file);
  // Delete from scan dir to avoid trying to download it again
  if(fromScanDir) {
    QFile::remove(file);
  }
  // Send torrent addition signal
  if(!from_url.isNull()) {
    if(fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(from_url));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(from_url));
  }else{
    if(fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(file));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(file));
  }
  emit addedTorrent(h);
  return h;
}

void Bittorrent::exportTorrentFiles(QString path) {
  Q_ASSERT(torrentExport);
  QDir exportDir(path);
  if(!exportDir.exists()) {
    if(!exportDir.mkpath(exportDir.absolutePath())) {
      std::cerr << "Error: Could not create torrent export directory: " << qPrintable(exportDir.absolutePath()) << std::endl;
      return;
    }
  }
  QDir torrentBackup(misc::BTBackupLocation());
  std::vector<torrent_handle> handles = s->get_torrents();
  std::vector<torrent_handle>::iterator itr;
  for(itr=handles.begin(); itr != handles.end(); itr++) {
    const QTorrentHandle h(*itr);
    if(!h.is_valid()) {
      std::cerr << "Torrent Export: torrent is invalid, skipping..." << std::endl;
      continue;
    }
    const QString &src_path = torrentBackup.absoluteFilePath(h.hash()+".torrent");
    if(QFile::exists(src_path)) {
      QString dst_path = exportDir.absoluteFilePath(h.name()+".torrent");
      if(QFile::exists(dst_path)) {
        if(!misc::sameFiles(src_path, dst_path)) {
          dst_path = exportDir.absoluteFilePath(h.name()+"-"+h.hash()+".torrent");
        } else {
          qDebug("Torrent Export: Destination file exists, skipping...");
          continue;
        }
      }
      qDebug("Export Torrent: %s -> %s", qPrintable(src_path), qPrintable(dst_path));
      QFile::copy(src_path, dst_path);
    } else {
      std::cerr << "Error: could not export torrent "<< qPrintable(h.hash()) << ", maybe it has not metadata yet." <<std::endl;
    }
  }
}

// Set the maximum number of opened connections
void Bittorrent::setMaxConnections(int maxConnec) {
  s->set_max_connections(maxConnec);
}

void Bittorrent::setMaxConnectionsPerTorrent(int max) {
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i) {
    QTorrentHandle h(handles[i]);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_max_connections(max);
  }
}

void Bittorrent::setMaxUploadsPerTorrent(int max) {
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();
  unsigned int nbHandles = handles.size();
  for(unsigned int i=0; i<nbHandles; ++i) {
    QTorrentHandle h(handles[i]);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      continue;
    }
    h.set_max_uploads(max);
  }
}

// Return DHT state
bool Bittorrent::isDHTEnabled() const{
  return DHTEnabled;
}

bool Bittorrent::isLSDEnabled() const{
  return LSDEnabled;
}

void Bittorrent::enableUPnP(bool b) {
  if(b) {
    if(!UPnPEnabled) {
      qDebug("Enabling UPnP");
      s->start_upnp();
      UPnPEnabled = true;  
    }
  } else {
    if(UPnPEnabled) {
      qDebug("Disabling UPnP");
      s->stop_upnp();
      UPnPEnabled = false;
    }
  }
}

void Bittorrent::enableNATPMP(bool b) {
  if(b) {
    if(!NATPMPEnabled) {
      qDebug("Enabling NAT-PMP");
      s->start_natpmp();
      NATPMPEnabled = true;
    }
  } else {
    if(NATPMPEnabled) {
      qDebug("Disabling NAT-PMP");
      s->stop_natpmp();
      NATPMPEnabled = false;
    }
  }
}

void Bittorrent::enableLSD(bool b) {
  if(b) {
    if(!LSDEnabled) {
      qDebug("Enabling LSD");
      s->start_lsd();
      LSDEnabled = true;
    }
  } else {
    if(LSDEnabled) {
      qDebug("Disabling LSD");
      s->stop_lsd();
      LSDEnabled = false;
    }
  }
}

void Bittorrent::loadSessionState() {
  const QString state_path = misc::cacheLocation()+QDir::separator()+QString::fromUtf8("ses_state");
#ifdef LIBTORRENT_0_15
  std::vector<char> in;
  if (load_file(state_path.toLocal8Bit().constData(), in) == 0)
  {
    lazy_entry e;
    if (lazy_bdecode(&in[0], &in[0] + in.size(), e) == 0)
      s->load_state(e);
  }
#else
  boost::filesystem::ifstream ses_state_file(state_path.toLocal8Bit().constData()
                                             , std::ios_base::binary);
  ses_state_file.unsetf(std::ios_base::skipws);
  s->load_state(bdecode(
      std::istream_iterator<char>(ses_state_file)
      , std::istream_iterator<char>()));
#endif
}

void Bittorrent::saveSessionState() {
  qDebug("Saving session state to disk...");
  const QString state_path = misc::cacheLocation()+QDir::separator()+QString::fromUtf8("ses_state");
#ifdef LIBTORRENT_0_15
  entry session_state;
  s->save_state(session_state);
  std::vector<char> out;
  bencode(std::back_inserter(out), session_state);
  file f;
  error_code ec;
  if (f.open(state_path.toLocal8Bit().data(), file::write_only, ec)) {
    if (ec) {
      file::iovec_t b = {&out[0], out.size()};
      f.writev(0, &b, 1, ec);
    }
  }
#else
  entry session_state = s->state();
  boost::filesystem::ofstream out(state_path.toLocal8Bit().constData()
                                  , std::ios_base::binary);
  out.unsetf(std::ios_base::skipws);
  bencode(std::ostream_iterator<char>(out), session_state);
#endif
}

// Enable DHT
bool Bittorrent::enableDHT(bool b) {
  if(b) {
    if(!DHTEnabled) {
#ifndef LIBTORRENT_0_15
      entry dht_state;
      const QString dht_state_path = misc::cacheLocation()+QDir::separator()+QString::fromUtf8("dht_state");
      if(QFile::exists(dht_state_path)) {
        boost::filesystem::ifstream dht_state_file(dht_state_path.toLocal8Bit().constData(), std::ios_base::binary);
        dht_state_file.unsetf(std::ios_base::skipws);
        try{
          dht_state = bdecode(std::istream_iterator<char>(dht_state_file), std::istream_iterator<char>());
        }catch (std::exception&) {}
      }
#endif
      try {
#ifdef LIBTORRENT_0_15
        s->start_dht();
#else
        s->start_dht(dht_state);
#endif
        s->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
        s->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
        s->add_dht_router(std::make_pair(std::string("router.bitcomet.com"), 6881));
        DHTEnabled = true;
        qDebug("DHT enabled");
      }catch(std::exception e) {
        qDebug("Could not enable DHT, reason: %s", e.what());
        return false;
      }
    }
  } else {
    if(DHTEnabled) {
      DHTEnabled = false;
      s->stop_dht();
      qDebug("DHT disabled");
    }
  }
  return true;
}

float Bittorrent::getRealRatio(QString hash) const{
  QTorrentHandle h = getTorrentHandle(hash);
  if(!h.is_valid()) {
    return 0.;
  }
  Q_ASSERT(h.total_done() >= 0);
  Q_ASSERT(h.all_time_upload() >= 0);
  if(h.total_done() == 0) {
    if(h.all_time_upload() == 0)
      return 0;
    return 101;
  }
  float ratio = (float)h.all_time_upload()/(float)h.total_done();
  Q_ASSERT(ratio >= 0.);
  if(ratio > 100.)
    ratio = 100.;
  return ratio;
}

void Bittorrent::saveTempFastResumeData() {
  std::vector<torrent_handle> torrents =  s->get_torrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid() || !h.has_metadata() || h.is_seed() || h.is_paused()) continue;
    if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
    qDebug("Saving fastresume data for %s", qPrintable(h.name()));
    h.save_resume_data();
  }
}

// Only save fast resume data for unfinished and unpaused torrents (Optimization)
// Called periodically and on exit
void Bittorrent::saveFastResumeData() {
  // Stop listening for alerts
  resumeDataTimer.stop();
  timerAlerts->stop();
  int num_resume_data = 0;
  // Pause session
  s->pause();
  std::vector<torrent_handle> torrents =  s->get_torrents();
  std::vector<torrent_handle>::iterator torrentIT;
  for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if(!h.is_valid() || !h.has_metadata()) continue;
    if(isQueueingEnabled())
      TorrentPersistentData::savePriority(h);
    // Actually with should save fast resume data for paused files too
    //if(h.is_paused()) continue;
    if(h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
    h.save_resume_data();
    ++num_resume_data;
  }
  while (num_resume_data > 0) {
    alert const* a = s->wait_for_alert(seconds(30));
    if (a == 0) {
      std::cerr << " aborting with " << num_resume_data << " outstanding "
          "torrents to save resume data for" << std::endl;
      break;
    }
    // Saving fastresume data can fail
    save_resume_data_failed_alert const* rda = dynamic_cast<save_resume_data_failed_alert const*>(a);
    if (rda) {
      --num_resume_data;
      s->pop_alert();
      try {
        // Remove torrent from session
        s->remove_torrent(rda->handle);
      }catch(libtorrent::libtorrent_exception){}
      continue;
    }
    save_resume_data_alert const* rd = dynamic_cast<save_resume_data_alert const*>(a);
    if (!rd) {
      s->pop_alert();
      continue;
    }
    // Saving fast resume data was successful
    --num_resume_data;
    if (!rd->resume_data) continue;
    QDir torrentBackup(misc::BTBackupLocation());
    const QTorrentHandle h(rd->handle);
    if(!h.is_valid()) continue;
    // Remove old fastresume file if it exists
    QFile::remove(torrentBackup.path()+QDir::separator()+ h.hash() + ".fastresume");
    const QString file = h.hash()+".fastresume";
    boost::filesystem::ofstream out(fs::path(torrentBackup.path().toLocal8Bit().constData()) / file.toLocal8Bit().constData(), std::ios_base::binary);
    out.unsetf(std::ios_base::skipws);
    bencode(std::ostream_iterator<char>(out), *rd->resume_data);
    // Remove torrent from session
    s->remove_torrent(rd->handle);
    s->pop_alert();
  }
}

QStringList Bittorrent::getConsoleMessages() const {
  return consoleMessages;
}

QStringList Bittorrent::getPeerBanMessages() const {
  return peerBanMessages;
}

#ifdef DISABLE_GUI
void Bittorrent::addConsoleMessage(QString msg, QString) {
#else
  void Bittorrent::addConsoleMessage(QString msg, QColor color) {
    if(consoleMessages.size() > 100) {
      consoleMessages.removeFirst();
    }
    consoleMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - <font color='") + color.name() +QString::fromUtf8("'><i>") + msg + QString::fromUtf8("</i></font>"));
#endif
    emit newConsoleMessage(QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss") + " - " + msg);
  }

  void Bittorrent::addPeerBanMessage(QString ip, bool from_ipfilter) {
    if(peerBanMessages.size() > 100) {
      peerBanMessages.removeFirst();
    }
    if(from_ipfilter)
      peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was blocked due to your IP filter</i>", "x.y.z.w was blocked").arg(ip));
    else
      peerBanMessages.append(QString::fromUtf8("<font color='grey'>")+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + QString::fromUtf8("</font> - ")+tr("<font color='red'>%1</font> <i>was banned due to corrupt pieces</i>", "x.y.z.w was banned").arg(ip));
  }

  bool Bittorrent::isFilePreviewPossible(QString hash) const{
    // See if there are supported files in the torrent
    const QTorrentHandle &h = getTorrentHandle(hash);
    if(!h.is_valid()) {
      qDebug("/!\\ Error: Invalid handle");
      return false;
    }
    const unsigned int nbFiles = h.num_files();
    for(unsigned int i=0; i<nbFiles; ++i) {
      QString extension = h.file_at(i).split('.').last();
      if(misc::isPreviewable(extension))
        return true;
    }
    return false;
  }

  void Bittorrent::addTorrentsFromScanFolder(QStringList &pathList) {
    foreach(const QString &file, pathList) {
      qDebug("File %s added", qPrintable(file));
      try {
        torrent_info t(file.toLocal8Bit().constData());
        if(t.is_valid())
          addTorrent(file, true);
      } catch(std::exception&) {
        qDebug("Ignoring incomplete torrent file: %s", qPrintable(file));
      }
    }
  }

  QString Bittorrent::getDefaultSavePath() const {
    return defaultSavePath;
  }

  bool Bittorrent::useTemporaryFolder() const {
    return !defaultTempPath.isEmpty();
  }

  void Bittorrent::setDefaultTempPath(QString temppath) {
    if(defaultTempPath == temppath)
      return;
    if(temppath.isEmpty()) {
      // Disabling temp dir
      // Moving all torrents to their destination folder
      std::vector<torrent_handle> torrents = getTorrents();
      std::vector<torrent_handle>::iterator torrentIT;
      for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        if(!h.is_valid()) continue;
        h.move_storage(getSavePath(h.hash()));
      }
    } else {
      // Moving all downloading torrents to temporary save path
      std::vector<torrent_handle> torrents = getTorrents();
      std::vector<torrent_handle>::iterator torrentIT;
      for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        if(!h.is_valid()) continue;
        if(!h.is_seed())
          h.move_storage(temppath);
      }
    }
    defaultTempPath = temppath;
  }

#ifdef LIBTORRENT_0_15
  void Bittorrent::appendqBextensionToTorrent(QTorrentHandle h, bool append) {
    if(!h.is_valid() || !h.has_metadata()) return;
    std::vector<size_type> fp;
    h.file_progress(fp);
    for(int i=0; i<h.num_files(); ++i) {
      if(append) {
        const qulonglong file_size = h.filesize_at(i);
        if(file_size > 0 && (fp[i]/(double)file_size) < 1.) {
          const QString &name = misc::toQString(h.get_torrent_info().file_at(i).path.string());
          if(!name.endsWith(".!qB")) {
            const QString new_name = name+".!qB";
            qDebug("Renaming %s to %s", qPrintable(name), qPrintable(new_name));
            h.rename_file(i, new_name);
          }
        }
      } else {
        QString name = misc::toQString(h.get_torrent_info().file_at(i).path.string());
        if(name.endsWith(".!qB")) {
          const QString old_name = name;
          name.chop(4);
          qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(name));
          h.rename_file(i, name);
        }
      }
    }
  }
#endif

  void Bittorrent::changeLabelInTorrentSavePath(QTorrentHandle h, QString old_label, QString new_label) {
    if(!h.is_valid()) return;
    if(!appendLabelToSavePath) return;
    const QString &old_save_path = TorrentPersistentData::getSavePath(h.hash());
    QDir old_dir(old_save_path);
    const bool move_storage = (old_dir == QDir(h.save_path()));
    if(!old_label.isEmpty()) {
      Q_ASSERT(old_dir.dirName() == old_label);
      QString path = old_save_path;
      // Cd UP
      if(path.endsWith(QDir::separator()))
        path.chop(1);
      QStringList path_items = path.split(QDir::separator());
      path_items.removeLast();
      old_dir = QDir(path_items.join(QDir::separator()));
    }
    QString new_save_path;
    if(new_label.isEmpty()) {
      new_save_path = old_dir.absolutePath();
    } else {
      new_save_path = old_dir.absoluteFilePath(new_label);
    }
    if(move_storage) {
      // Move storage
      h.move_storage(new_save_path);
    }
  }

  void Bittorrent::appendLabelToTorrentSavePath(QTorrentHandle h) {
    if(!h.is_valid()) return;
    const QString &label = TorrentPersistentData::getLabel(h.hash());
    if(label.isEmpty()) return;
    // Current save path
    const QString &old_save_path = TorrentPersistentData::getSavePath(h.hash());
    const QDir old_dir(old_save_path);
    if(old_dir.dirName() != label) {
      const QString &new_save_path = old_dir.absoluteFilePath(label);
      if(old_dir == QDir(h.save_path())) {
        // Move storage
        h.move_storage(new_save_path);
      }
    }
  }

  void Bittorrent::setAppendLabelToSavePath(bool append) {
    if(appendLabelToSavePath != append) {
      appendLabelToSavePath = !appendLabelToSavePath;
      if(appendLabelToSavePath) {
        // Move torrents storage to sub folder with label name
        std::vector<torrent_handle> torrents = getTorrents();
        std::vector<torrent_handle>::iterator torrentIT;
        for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
          QTorrentHandle h = QTorrentHandle(*torrentIT);
          appendLabelToTorrentSavePath(h);
        }
      }
    }
  }

#ifdef LIBTORRENT_0_15
  void Bittorrent::setAppendqBExtension(bool append) {
    if(appendqBExtension != append) {
      appendqBExtension = !appendqBExtension;
      // append or remove .!qB extension for incomplete files
      std::vector<torrent_handle> torrents = getTorrents();
      std::vector<torrent_handle>::iterator torrentIT;
      for(torrentIT = torrents.begin(); torrentIT != torrents.end(); torrentIT++) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        appendqBextensionToTorrent(h, appendqBExtension);
      }
    }
  }
#endif

  // Set the ports range in which is chosen the port the Bittorrent
  // session will listen to
  void Bittorrent::setListeningPort(int port) {
    std::pair<int,int> ports(port, port);
    s->listen_on(ports);
  }

  // Set download rate limit
  // -1 to disable
  void Bittorrent::setDownloadRateLimit(long rate) {
    qDebug("Setting a global download rate limit at %ld", rate);
    s->set_download_rate_limit(rate);
  }

  session* Bittorrent::getSession() const{
    return s;
  }

  // Set upload rate limit
  // -1 to disable
  void Bittorrent::setUploadRateLimit(long rate) {
    qDebug("set upload_limit to %fkb/s", rate/1024.);
    s->set_upload_rate_limit(rate);
  }

  // libtorrent allow to adjust ratio for each torrent
  // This function will apply to same ratio to all torrents
  void Bittorrent::setGlobalRatio(float ratio) {
    if(ratio != -1 && ratio < 1.) ratio = 1.;
    if(ratio == -1) {
      // 0 means unlimited for libtorrent
      ratio = 0;
    }
    std::vector<torrent_handle> handles = s->get_torrents();
    std::vector<torrent_handle>::iterator it;
    for(it = handles.begin(); it != handles.end(); it++) {
      QTorrentHandle h(*it);
      if(!h.is_valid()) {
        qDebug("/!\\ Error: Invalid handle");
        continue;
      }
      h.set_ratio(ratio);
    }
  }

  // Torrents will a ratio superior to the given value will
  // be automatically deleted
  void Bittorrent::setDeleteRatio(float ratio) {
    if(ratio != -1 && ratio < 1.) ratio = 1.;
    if(ratio_limit == -1 && ratio != -1) {
      Q_ASSERT(!BigRatioTimer);
      BigRatioTimer = new QTimer(this);
      connect(BigRatioTimer, SIGNAL(timeout()), this, SLOT(deleteBigRatios()));
      BigRatioTimer->start(5000);
    } else {
      if(ratio_limit != -1 && ratio == -1) {
        delete BigRatioTimer;
      }
    }
    if(ratio_limit != ratio) {
      ratio_limit = ratio;
      qDebug("* Set deleteRatio to %.1f", ratio_limit);
      deleteBigRatios();
    }
  }

  // Set DHT port (>= 1 or 0 if same as BT)
  void Bittorrent::setDHTPort(int dht_port) {
    if(dht_port >= 0) {
      if(dht_port == current_dht_port) return;
      struct dht_settings DHTSettings;
      DHTSettings.service_port = dht_port;
      s->set_dht_settings(DHTSettings);
      current_dht_port = dht_port;
      qDebug("Set DHT Port to %d", dht_port);
    }
  }

  // Enable IP Filtering
  void Bittorrent::enableIPFilter(QString filter) {
    qDebug("Enabling IPFiler");
    if(!filterParser) {
      filterParser = new FilterParserThread(this, s);
    }
    if(filterPath.isEmpty() || filterPath != filter) {
      filterPath = filter;
      filterParser->processFilterFile(filter);
    }
  }

  // Disable IP Filtering
  void Bittorrent::disableIPFilter() {
    qDebug("Disabling IPFilter");
    s->set_ip_filter(ip_filter());
    if(filterParser) {
      delete filterParser;
    }
    filterPath = "";
  }

  // Set BT session settings (user_agent)
  void Bittorrent::setSessionSettings(const session_settings &sessionSettings) {
    qDebug("Set session settings");
    s->set_settings(sessionSettings);
  }

  // Set Proxy
  void Bittorrent::setPeerProxySettings(const proxy_settings &proxySettings) {
    qDebug("Set Peer Proxy settings");
    s->set_peer_proxy(proxySettings);
    s->set_dht_proxy(proxySettings);
  }

  void Bittorrent::setHTTPProxySettings(const proxy_settings &proxySettings) {
    s->set_tracker_proxy(proxySettings);
    s->set_web_seed_proxy(proxySettings);
    QString proxy_str;
    switch(proxySettings.type) {
    case proxy_settings::http_pw:
      proxy_str = "http://"+misc::toQString(proxySettings.username)+":"+misc::toQString(proxySettings.password)+"@"+misc::toQString(proxySettings.hostname)+":"+QString::number(proxySettings.port);
      break;
    case proxy_settings::http:
      proxy_str = "http://"+misc::toQString(proxySettings.hostname)+":"+QString::number(proxySettings.port);
      break;
    case proxy_settings::socks5:
      proxy_str = misc::toQString(proxySettings.hostname)+":"+QString::number(proxySettings.port);
      break;
    case proxy_settings::socks5_pw:
      proxy_str = misc::toQString(proxySettings.username)+":"+misc::toQString(proxySettings.password)+"@"+misc::toQString(proxySettings.hostname)+":"+QString::number(proxySettings.port);
      break;
    default:
      qDebug("Disabling HTTP communications proxy");
#ifdef Q_WS_WIN
      putenv("http_proxy=");
      putenv("sock_proxy=");
#else
      unsetenv("http_proxy");
      unsetenv("sock_proxy");
#endif
      return;
    }
    // We need this for urllib in search engine plugins
#ifdef Q_WS_WIN
    char proxystr[512];
    if(proxySettings.type == proxy_settings::socks5 || proxySettings.type == proxy_settings::socks5_pw)
      snprintf(proxystr, 512, "sock_proxy=%s", proxy_str.toLocal8Bit().constData());
    else
      snprintf(proxystr, 512, "http_proxy=%s", proxy_str.toLocal8Bit().constData());
    putenv(proxystr);
#else
    qDebug("HTTP communications proxy string: %s", qPrintable(proxy_str));
    if(proxySettings.type == proxy_settings::socks5 || proxySettings.type == proxy_settings::socks5_pw)
      setenv("sock_proxy", proxy_str.toLocal8Bit().constData(), 1);
    else
      setenv("http_proxy", proxy_str.toLocal8Bit().constData(), 1);
#endif
  }

  void Bittorrent::recursiveTorrentDownload(const QTorrentHandle &h) {
    for(int i=0; i<h.get_torrent_info().num_files(); ++i) {
      const QString &torrent_relpath = misc::toQString(h.get_torrent_info().file_at(i).path.string());
      if(torrent_relpath.endsWith(".torrent")) {
        addConsoleMessage(tr("Recursive download of file %1 embedded in torrent %2", "Recursive download of test.torrent embedded in torrent test2").arg(torrent_relpath).arg(h.name()));
        const QString torrent_fullpath = h.save_path()+QDir::separator()+torrent_relpath;
        try {
          boost::intrusive_ptr<torrent_info> t = new torrent_info(torrent_fullpath.toLocal8Bit().constData());
          const QString &sub_hash = misc::toQString(t->info_hash());
          // Passing the save path along to the sub torrent file
          TorrentTempData::setSavePath(sub_hash, h.save_path());
          addTorrent(torrent_fullpath);
        } catch(std::exception&) {
          qDebug("Caught error loading torrent");
          addConsoleMessage(tr("Unable to decode %1 torrent file.").arg(torrent_fullpath), QString::fromUtf8("red"));
        }
      }
    }
  }

  // Read alerts sent by the Bittorrent session
  void Bittorrent::readAlerts() {
    // look at session alerts and display some infos
    std::auto_ptr<alert> a = s->pop_alert();
    while (a.get()) {
      if (torrent_finished_alert* p = dynamic_cast<torrent_finished_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(h.is_valid()) {
          emit finishedTorrent(h);
          const QString &hash = h.hash();
#ifdef LIBTORRENT_0_15
          // Remove .!qB extension if necessary
          if(appendqBExtension)
            appendqBextensionToTorrent(h, false);
#endif
          qDebug("A torrent has finished downloading");
          // Move to download directory if necessary
          if(!defaultTempPath.isEmpty()) {
            qDebug("A torrent has finished downloading and will be moved to the final download location");
            // Check if directory is different
            const QDir current_dir(h.save_path());
            const QDir save_dir(getSavePath(hash));
            if(current_dir != save_dir) {
              qDebug("current dir is different that final destination, actually move the storage...");
              h.move_storage(save_dir.path());
            }
          }
          const bool was_already_seeded = TorrentPersistentData::isSeed(hash);
          if(!was_already_seeded) {
            h.save_resume_data();
            qDebug("Checking if the torrent contains torrent files to download");
            // Check if there are torrent files inside
            for(int i=0; i<h.get_torrent_info().num_files(); ++i) {
              const QString &torrent_relpath = misc::toQString(h.get_torrent_info().file_at(i).path.string());
              if(torrent_relpath.endsWith(".torrent")) {
                qDebug("Found possible recursive torrent download.");
                const QString torrent_fullpath = h.save_path()+QDir::separator()+torrent_relpath;
                qDebug("Full subtorrent path is %s", qPrintable(torrent_fullpath));
                try {
                  boost::intrusive_ptr<torrent_info> t = new torrent_info(torrent_fullpath.toLocal8Bit().constData());
                  if(t->is_valid()) {
                    qDebug("emitting recursiveTorrentDownloadPossible()");
                    emit recursiveTorrentDownloadPossible(h);
                    break;
                  }
                } catch(std::exception&) {
                  qDebug("Caught error loading torrent");
                  addConsoleMessage(tr("Unable to decode %1 torrent file.").arg(torrent_fullpath), QString::fromUtf8("red"));
                }
              }
            }
          }
          // Recheck if the user asked to
          if(Preferences::recheckTorrentsOnCompletion() && !was_already_seeded) {
            // Remember finished state
            TorrentPersistentData::saveSeedStatus(h);
            h.force_recheck();
          } else {
            // Remember finished state
            TorrentPersistentData::saveSeedStatus(h);
          }
          qDebug("Received finished alert for %s", qPrintable(h.name()));
        }
      }
      else if (save_resume_data_alert* p = dynamic_cast<save_resume_data_alert*>(a.get())) {
        const QDir torrentBackup(misc::BTBackupLocation());
        const QTorrentHandle h(p->handle);
        if(h.is_valid()) {
          const QString file = h.hash()+".fastresume";
          // Delete old fastresume file if necessary
          if(QFile::exists(file))
            QFile::remove(file);
          qDebug("Saving fastresume data in %s", qPrintable(file));
          if (p->resume_data) {
            boost::filesystem::ofstream out(fs::path(torrentBackup.path().toLocal8Bit().constData()) / file.toLocal8Bit().constData(), std::ios_base::binary);
            out.unsetf(std::ios_base::skipws);
            bencode(std::ostream_iterator<char>(out), *p->resume_data);
          }
        }
      }
      else if (storage_moved_alert* p = dynamic_cast<storage_moved_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(h.is_valid()) {
          // Attempt to remove old folder if empty
          const QString& old_save_path = TorrentPersistentData::getSavePath(h.hash());
          const QString new_save_path = QString::fromLocal8Bit(p->path.c_str());
          qDebug("Torrent moved from %s to %s", qPrintable(old_save_path), qPrintable(new_save_path));
          qDebug("Attempting to remove %s", qPrintable(old_save_path));
          QDir().rmpath(old_save_path);
          if(new_save_path != defaultTempPath)
            TorrentPersistentData::saveSavePath(h.hash(), new_save_path);
          emit savePathChanged(h);
          //h.force_recheck();
        }
      }
      else if (metadata_received_alert* p = dynamic_cast<metadata_received_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(h.is_valid()) {
          qDebug("Received metadata for %s", qPrintable(h.hash()));
#ifdef LIBTORRENT_0_15
          // Append .!qB to incomplete files
          if(appendqBExtension)
            appendqBextensionToTorrent(h, true);
#endif
          emit metadataReceived(h);
          // Save metadata
          const QDir torrentBackup(misc::BTBackupLocation());
          if(!QFile::exists(torrentBackup.path()+QDir::separator()+h.hash()+QString(".torrent")))
            h.save_torrent_file(torrentBackup.path()+QDir::separator()+h.hash()+QString(".torrent"));
          // Copy the torrent file to the export folder
          if(torrentExport) {
            QDir exportPath(Preferences::getExportDir());
            if(exportPath.exists() || exportPath.mkpath(exportPath.absolutePath())) {
              QString torrent_path = exportPath.absoluteFilePath(h.name()+".torrent");
              bool duplicate = false;
              if(QFile::exists(torrent_path)) {
                // Append hash to torrent name to make it unique
                torrent_path = exportPath.absoluteFilePath(h.name()+"-"+h.hash()+".torrent");
                duplicate = true;
              }
              h.save_torrent_file(torrent_path);
              if(duplicate) {
                // Remove duplicate file if indentical
                if(misc::sameFiles(exportPath.absoluteFilePath(h.name()+".torrent"), torrent_path)) {
                  QFile::remove(torrent_path);
                }
              }
            }
          }
          if(h.is_paused()) {
            // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
            // and the torrent can be paused when metadata is received
            emit pausedTorrent(h);
          }
        }
      }
      else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(h.is_valid()) {
          h.auto_managed(false);
          std::cerr << "File Error: " << p->message().c_str() << std::endl;
          addConsoleMessage(tr("An I/O error occured, '%1' paused.").arg(h.name()));
          addConsoleMessage(tr("Reason: %1").arg(misc::toQString(p->message())));
          if(h.is_valid()) {
            emit fullDiskError(h, misc::toQString(p->message()));
            h.pause();
            emit pausedTorrent(h);
          }
        }
      }
#ifdef LIBTORRENT_0_15
      else if (file_completed_alert* p = dynamic_cast<file_completed_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(appendqBExtension) {
          QString name = misc::toQString(h.get_torrent_info().file_at(p->index).path.string());
          if(name.endsWith(".!qB")) {
            const QString old_name = name;
            name.chop(4);
            qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(name));
            h.rename_file(p->index, name);
          }
        }
      }
#endif
      else if (torrent_paused_alert* p = dynamic_cast<torrent_paused_alert*>(a.get())) {
        if(p->handle.is_valid()) {
          p->handle.save_resume_data();
        }
      }
      else if (tracker_error_alert* p = dynamic_cast<tracker_error_alert*>(a.get())) {
        // Level: fatal
        QTorrentHandle h(p->handle);
        if(h.is_valid()){
          // Authentication
          if(p->status_code != 401) {
            qDebug("Received a tracker error for %s: %s", p->url.c_str(), p->msg.c_str());
            const QString &tracker_url = misc::toQString(p->url);
            QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
            TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
            data.last_message = misc::toQString(p->msg);
#ifndef LIBTORRENT_0_15
            data.verified = false;
            ++data.fail_count;
#endif
            trackers_data.insert(tracker_url, data);
            trackersInfos[h.hash()] = trackers_data;
          } else {
            emit trackerAuthenticationRequired(h);
          }
        }
      }
      else if (tracker_reply_alert* p = dynamic_cast<tracker_reply_alert*>(a.get())) {
        const QTorrentHandle h(p->handle);
        if(h.is_valid()){
          qDebug("Received a tracker reply from %s (Num_peers=%d)", p->url.c_str(), p->num_peers);
          // Connection was successful now. Remove possible old errors
          QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
          const QString &tracker_url = misc::toQString(p->url);
          TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
          data.last_message = ""; // Reset error/warning message
          data.num_peers = p->num_peers;
#ifndef LIBTORRENT_0_15
          data.fail_count = 0;
          data.verified = true;
#endif
          trackers_data.insert(tracker_url, data);
          trackersInfos[h.hash()] = trackers_data;
        }
      } else if (tracker_warning_alert* p = dynamic_cast<tracker_warning_alert*>(a.get())) {
        const QTorrentHandle h(p->handle);
        if(h.is_valid()){
          // Connection was successful now but there is a warning message
          QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
          const QString &tracker_url = misc::toQString(p->url);
          TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
          data.last_message = misc::toQString(p->msg); // Store warning message
#ifndef LIBTORRENT_0_15
          data.verified = true;
          data.fail_count = 0;
#endif
          trackers_data.insert(tracker_url, data);
          trackersInfos[h.hash()] = trackers_data;
          qDebug("Received a tracker warning from %s: %s", p->url.c_str(), p->msg.c_str());
        }
      }
      else if (portmap_error_alert* p = dynamic_cast<portmap_error_alert*>(a.get())) {
        addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping failure, message: %1").arg(misc::toQString(p->message())), "red");
        //emit UPnPError(QString(p->msg().c_str()));
      }
      else if (portmap_alert* p = dynamic_cast<portmap_alert*>(a.get())) {
        qDebug("UPnP Success, msg: %s", p->message().c_str());
        addConsoleMessage(tr("UPnP/NAT-PMP: Port mapping successful, message: %1").arg(misc::toQString(p->message())), "blue");
        //emit UPnPSuccess(QString(p->msg().c_str()));
      }
      else if (peer_blocked_alert* p = dynamic_cast<peer_blocked_alert*>(a.get())) {
        addPeerBanMessage(QString(p->ip.to_string().c_str()), true);
        //emit peerBlocked(QString::fromUtf8(p->ip.to_string().c_str()));
      }
      else if (peer_ban_alert* p = dynamic_cast<peer_ban_alert*>(a.get())) {
        addPeerBanMessage(QString(p->ip.address().to_string().c_str()), false);
        //emit peerBlocked(QString::fromUtf8(p->ip.to_string().c_str()));
      }
      else if (fastresume_rejected_alert* p = dynamic_cast<fastresume_rejected_alert*>(a.get())) {
        const QTorrentHandle h(p->handle);
        if(h.is_valid()){
          qDebug("/!\\ Fast resume failed for %s, reason: %s", qPrintable(h.name()), p->message().c_str());
          addConsoleMessage(tr("Fast resume data was rejected for torrent %1, checking again...").arg(h.name()), QString::fromUtf8("red"));
          addConsoleMessage(tr("Reason: %1").arg(misc::toQString(p->message())));
          //emit fastResumeDataRejected(h.name());
        }
      }
      else if (url_seed_alert* p = dynamic_cast<url_seed_alert*>(a.get())) {
        addConsoleMessage(tr("Url seed lookup failed for url: %1, message: %2").arg(misc::toQString(p->url)).arg(misc::toQString(p->message())), QString::fromUtf8("red"));
        //emit urlSeedProblem(QString::fromUtf8(p->url.c_str()), QString::fromUtf8(p->msg().c_str()));
      }
      else if (torrent_checked_alert* p = dynamic_cast<torrent_checked_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if(h.is_valid()){
          const QString &hash = h.hash();
          qDebug("%s have just finished checking", qPrintable(hash));
          // Save seed status
          TorrentPersistentData::saveSeedStatus(h);
          // Move to temp directory if necessary
          if(!h.is_seed() && !defaultTempPath.isEmpty()) {
            // Check if directory is different
            const QDir current_dir(h.save_path());
            const QDir save_dir(getSavePath(h.hash()));
            if(current_dir == save_dir) {
              h.move_storage(defaultTempPath);
            }
          }
          emit torrentFinishedChecking(h);
          if(torrentsToPausedAfterChecking.contains(hash)) {
            torrentsToPausedAfterChecking.removeOne(hash);
            h.pause();
            emit pausedTorrent(h);
          }
        }
      }
      a = s->pop_alert();
    }
  }

  void Bittorrent::recheckTorrent(QString hash) {
    QTorrentHandle h = getTorrentHandle(hash);
    if(h.is_valid() && h.has_metadata()) {
      if(h.is_paused()) {
        if(!torrentsToPausedAfterChecking.contains(h.hash())) {
          torrentsToPausedAfterChecking << h.hash();
          h.resume();
        }
      }
      h.force_recheck();
    }
  }

  QHash<QString, TrackerInfos> Bittorrent::getTrackersInfo(QString hash) const{
    return trackersInfos.value(hash, QHash<QString, TrackerInfos>());
  }

  int Bittorrent::getListenPort() const{
    qDebug("LISTEN PORT: %d", s->listen_port());
    return s->listen_port();
  }

  session_status Bittorrent::getSessionStatus() const{
    return s->status();
  }

  QString Bittorrent::getSavePath(QString hash, bool fromScanDir, QString filePath) {
    QString savePath;
    if(TorrentTempData::hasTempData(hash)) {
      savePath = TorrentTempData::getSavePath(hash);
      if(savePath.isEmpty()) {
        savePath = defaultSavePath;
      }
      if(appendLabelToSavePath) {
        qDebug("appendLabelToSavePath is true");
        const QString &label = TorrentTempData::getLabel(hash);
        if(!label.isEmpty()) {
          const QDir save_dir(savePath);
          if(save_dir.dirName() != label) {
            savePath = save_dir.absoluteFilePath(label);
          }
        }
      }
      qDebug("getSavePath, got save_path from temp data: %s", qPrintable(savePath));
    } else {
      savePath = TorrentPersistentData::getSavePath(hash);
      if(savePath.isEmpty()) {
        if(fromScanDir && m_scanFolders->downloadInTorrentFolder(filePath))
          savePath = QFileInfo(filePath).dir().path();
        else
          savePath = defaultSavePath;
      }
      if(!fromScanDir && appendLabelToSavePath) {
        const QString &label = TorrentPersistentData::getLabel(hash);
        if(!label.isEmpty()) {
          QDir save_dir(savePath);
          if(save_dir.dirName() != label) {
            savePath = save_dir.absoluteFilePath(label);
          }
        }
      }
      qDebug("getSavePath, got save_path from persistent data: %s", qPrintable(savePath));
    }
    // Clean path
    savePath = misc::expandPath(savePath);
    // Checking if savePath Dir exists
    // create it if it is not
    QDir saveDir(savePath);
    if(!saveDir.exists()) {
      if(!saveDir.mkpath(saveDir.absolutePath())) {
        std::cerr << "Couldn't create the save directory: " << qPrintable(saveDir.path()) << "\n";
        // XXX: Do something else?
      }
    }
    return savePath;
  }

  // Take an url string to a torrent file,
  // download the torrent file to a tmp location, then
  // add it to download list
  void Bittorrent::downloadFromUrl(QString url) {
    addConsoleMessage(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(url)
#ifndef DISABLE_GUI
                      , QPalette::WindowText
#endif
                      );
    //emit aboutToDownloadFromUrl(url);
    // Launch downloader thread
    downloader->downloadTorrentUrl(url);
  }

  void Bittorrent::downloadFromURLList(const QStringList& urls) {
    foreach(const QString &url, urls) {
      downloadFromUrl(url);
    }
  }

  void Bittorrent::addMagnetSkipAddDlg(QString uri) {
    addMagnetUri(uri, false);
  }

  void Bittorrent::downloadUrlAndSkipDialog(QString url, QString save_path) {
    //emit aboutToDownloadFromUrl(url);
    const QUrl &qurl = QUrl::fromEncoded(url.toLocal8Bit());
    if(!save_path.isEmpty())
      savepath_fromurl[qurl] = save_path;
    url_skippingDlg << qurl;
    // Launch downloader thread
    downloader->downloadUrl(url);
  }

  // Add to Bittorrent session the downloaded torrent file
  void Bittorrent::processDownloadedFile(QString url, QString file_path) {
    const int index = url_skippingDlg.indexOf(QUrl::fromEncoded(url.toLocal8Bit()));
    if(index < 0) {
      // Add file to torrent download list
      emit newDownloadedTorrent(file_path, url);
    } else {
      url_skippingDlg.removeAt(index);
      addTorrent(file_path, false, url, false);
    }
  }

  // Return current download rate for the BT
  // session. Payload means that it only take into
  // account "useful" part of the rate
  float Bittorrent::getPayloadDownloadRate() const{
    const session_status &sessionStatus = s->status();
    return sessionStatus.payload_download_rate;
  }

  // Return current upload rate for the BT
  // session. Payload means that it only take into
  // account "useful" part of the rate
  float Bittorrent::getPayloadUploadRate() const{
    const session_status &sessionStatus = s->status();
    return sessionStatus.payload_upload_rate;
  }

#ifndef LIBTORRENT_0_15
  // Save DHT entry to hard drive
  void Bittorrent::saveDHTEntry() {
    // Save DHT entry
    if(DHTEnabled) {
      try{
        entry dht_state = s->dht_state();
        const QString dht_path = misc::cacheLocation()+QDir::separator()+QString::fromUtf8("dht_state");
        boost::filesystem::ofstream out(dht_path.toLocal8Bit().constData(), std::ios_base::binary);
        out.unsetf(std::ios_base::skipws);
        bencode(std::ostream_iterator<char>(out), dht_state);
        qDebug("DHT entry saved");
      }catch (std::exception& e) {
        std::cerr << e.what() << "\n";
      }
    }
  }
#endif

  void Bittorrent::applyEncryptionSettings(pe_settings se) {
    qDebug("Applying encryption settings");
    s->set_pe_settings(se);
  }

  // Will fast resume torrents in
  // backup directory
  void Bittorrent::startUpTorrents() {
    qDebug("Resuming unfinished torrents");
    const QDir torrentBackup(misc::BTBackupLocation());
    const QStringList &known_torrents = TorrentPersistentData::knownTorrents();

    // Safety measure because some people reported torrent loss since
    // we switch the v1.5 way of resuming torrents on startup
    QStringList filters;
    filters << "*.torrent";
    const QStringList &torrents_on_hd = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
    foreach(QString hash, torrents_on_hd) {
      hash.chop(8); // remove trailing .torrent
      if(!known_torrents.contains(hash)) {
        std::cerr << "ERROR Detected!!! Adding back torrent " << qPrintable(hash) << " which got lost for some reason." << std::endl;
        addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
      }
    }
    // End of safety measure

    qDebug("Starting up torrents");
    if(isQueueingEnabled()) {
      priority_queue<QPair<int, QString>, vector<QPair<int, QString> >, std::greater<QPair<int, QString> > > torrent_queue;
      foreach(const QString &hash, known_torrents) {
        QString filePath;
        if(TorrentPersistentData::isMagnet(hash)) {
          filePath = TorrentPersistentData::getMagnetUri(hash);
        } else {
          filePath = torrentBackup.path()+QDir::separator()+hash+".torrent";
        }
        const int prio = TorrentPersistentData::getPriority(hash);
        torrent_queue.push(qMakePair(prio, hash));
      }
      // Resume downloads
      while(!torrent_queue.empty()) {
        const QString hash = torrent_queue.top().second;
        torrent_queue.pop();
        qDebug("Starting up torrent %s", qPrintable(hash));
        if(TorrentPersistentData::isMagnet(hash)) {
          addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
        } else {
          addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
        }
      }
    } else {
      // Resume downloads
      foreach(const QString &hash, known_torrents) {
        qDebug("Starting up torrent %s", qPrintable(hash));
        if(TorrentPersistentData::isMagnet(hash))
          addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
        else
          addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
      }
    }
    qDebug("Unfinished torrents resumed");
  }
