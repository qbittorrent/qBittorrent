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
#include <QNetworkInterface>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QProcess>
#include <stdlib.h>

#include "smtp.h"
#include "filesystemwatcher.h"
#include "torrentspeedmonitor.h"
#include "qbtsession.h"
#include "misc.h"
#include "fs_utils.h"
#include "downloadthread.h"
#include "filterparserthread.h"
#include "preferences.h"
#include "scannedfoldersmodel.h"
#ifndef DISABLE_GUI
#include "shutdownconfirm.h"
#include "geoipmanager.h"
#endif
#include "torrentpersistentdata.h"
#include "httpserver.h"
#include "qinisettings.h"
#include "bandwidthscheduler.h"
#include <libtorrent/version.hpp>
#include <libtorrent/extensions/ut_metadata.hpp>
#include <libtorrent/version.hpp>
#include <libtorrent/extensions/lt_trackers.hpp>
#include <libtorrent/extensions/ut_pex.hpp>
#include <libtorrent/extensions/smart_ban.hpp>
//#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/upnp.hpp>
#include <libtorrent/natpmp.hpp>
#if LIBTORRENT_VERSION_MINOR < 16
#include <boost/filesystem/exception.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#endif
#if LIBTORRENT_VERSION_MINOR > 15
#include "libtorrent/error_code.hpp"
#endif
#include <queue>
#include <string.h>
#include "dnsupdater.h"

using namespace libtorrent;

QBtSession* QBtSession::m_instance = 0;
const qreal QBtSession::MAX_RATIO = 9999.;

const int MAX_TRACKER_ERRORS = 2;

/* Converts a QString hash into a libtorrent sha1_hash */
static libtorrent::sha1_hash QStringToSha1(const QString& s) {
  QByteArray raw = s.toAscii();
  Q_ASSERT(raw.size() == 40);
  libtorrent::sha1_hash ret;
  from_hex(raw.constData(), 40, (char*)&ret[0]);
  return ret;
}

// Main constructor
QBtSession::QBtSession()
  : m_scanFolders(ScanFoldersModel::instance(this)),
    preAllocateAll(false), global_ratio_limit(-1),
    LSDEnabled(false),
    DHTEnabled(false), current_dht_port(0), queueingEnabled(false),
    m_torrentExportEnabled(false),
    m_finishedTorrentExportEnabled(false)
  #ifndef DISABLE_GUI
  , geoipDBLoaded(false), resolve_countries(false)
  #endif
  , m_tracker(0), m_shutdownAct(NO_SHUTDOWN),
    m_upnp(0), m_natpmp(0), m_dynDNSUpdater(0)
{
  BigRatioTimer = new QTimer(this);
  BigRatioTimer->setInterval(10000);
  connect(BigRatioTimer, SIGNAL(timeout()), SLOT(processBigRatios()));
  Preferences pref;
#if LIBTORRENT_VERSION_MINOR < 16
  // To avoid some exceptions
  boost::filesystem::path::default_name_check(boost::filesystem::no_check);
#endif
  // Creating Bittorrent session
  QList<int> version;
  version << VERSION_MAJOR;
  version << VERSION_MINOR;
  version << VERSION_BUGFIX;
  version << 0;
  const QString peer_id = "qB";
  // Construct session
  s = new session(fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)), 0);
  //std::cout << "Peer ID: " << fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)).to_string() << std::endl;
  addConsoleMessage("Peer ID: "+misc::toQString(fingerprint(peer_id.toLocal8Bit().constData(), version.at(0), version.at(1), version.at(2), version.at(3)).to_string()));

  // Set severity level of libtorrent session
  s->set_alert_mask(alert::error_notification | alert::peer_notification | alert::port_mapping_notification | alert::storage_notification | alert::tracker_notification | alert::status_notification | alert::ip_block_notification | alert::progress_notification);
  // Load previous state
  loadSessionState();
  // Enabling plugins
  //s->add_extension(&create_metadata_plugin);
  s->add_extension(&create_ut_metadata_plugin);
  if (pref.trackerExchangeEnabled())
    s->add_extension(&create_lt_trackers_plugin);
  if (pref.isPeXEnabled()) {
    PeXEnabled = true;
    s->add_extension(&create_ut_pex_plugin);
  } else {
    PeXEnabled = false;
  }
  s->add_extension(&create_smart_ban_plugin);
  timerAlerts = new QTimer(this);
  connect(timerAlerts, SIGNAL(timeout()), SLOT(readAlerts()));
  timerAlerts->start(1000);
  appendLabelToSavePath = pref.appendTorrentLabel();
  appendqBExtension = pref.useIncompleteFilesExtension();
  connect(m_scanFolders, SIGNAL(torrentsAdded(QStringList&)), SLOT(addTorrentsFromScanFolder(QStringList&)));
  // Apply user settings to Bittorrent session
  configureSession();
  // Torrent speed monitor
  m_speedMonitor = new TorrentSpeedMonitor(this);
  m_speedMonitor->start();
  // To download from urls
  downloader = new DownloadThread(this);
  connect(downloader, SIGNAL(downloadFinished(QString, QString)), SLOT(processDownloadedFile(QString, QString)));
  connect(downloader, SIGNAL(downloadFailure(QString, QString)), SLOT(handleDownloadFailure(QString, QString)));
  // Regular saving of fastresume data
  connect(&resumeDataTimer, SIGNAL(timeout()), SLOT(saveTempFastResumeData()));
  resumeDataTimer.start(170000); // 3min
  qDebug("* BTSession constructed");
}

// Main destructor
QBtSession::~QBtSession() {
  qDebug("BTSession destructor IN");
  delete m_speedMonitor;
  qDebug("Deleted the torrent speed monitor");
  // Do some BT related saving
  saveSessionState();
  saveFastResumeData();
  // Delete our objects
  if (m_tracker)
    delete m_tracker;
  delete timerAlerts;
  if (BigRatioTimer)
    delete BigRatioTimer;
  if (filterParser)
    delete filterParser;
  delete downloader;
  if (bd_scheduler)
    delete bd_scheduler;
  // HTTP Server
  if (httpServer)
    delete httpServer;
  qDebug("Deleting the session");
  delete s;
  qDebug("BTSession destructor OUT");
#ifndef DISABLE_GUI
  if (m_shutdownAct != NO_SHUTDOWN) {
    qDebug() << "Sending computer shutdown/suspend signal...";
    misc::shutdownComputer(m_shutdownAct == SUSPEND_COMPUTER);
  }
#endif
}

void QBtSession::preAllocateAllFiles(bool b) {
  const bool change = (preAllocateAll != b);
  if (change) {
    qDebug("PreAllocateAll changed, reloading all torrents!");
    preAllocateAll = b;
  }
}

void QBtSession::processBigRatios() {
  qDebug("Process big ratios...");
  std::vector<torrent_handle> torrents = s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    const QTorrentHandle h(*torrentIT);
    if (!h.is_valid()) continue;
    if (h.is_seed()) {
      const QString hash = h.hash();
      const qreal ratio = getRealRatio(hash);
      qreal ratio_limit = TorrentPersistentData::getRatioLimit(hash);
      if (ratio_limit == TorrentPersistentData::USE_GLOBAL_RATIO)
        ratio_limit = global_ratio_limit;
      if (ratio_limit == TorrentPersistentData::NO_RATIO_LIMIT)
        continue;
      qDebug("Ratio: %f (limit: %f)", ratio, ratio_limit);
      Q_ASSERT(ratio_limit >= 0.f);
      if (ratio <= MAX_RATIO && ratio >= ratio_limit) {
        if (high_ratio_action == REMOVE_ACTION) {
          addConsoleMessage(tr("%1 reached the maximum ratio you set.").arg(h.name()));
          addConsoleMessage(tr("Removing torrent %1...").arg(h.name()));
          deleteTorrent(hash);
        } else {
          // Pause it
          if (!h.is_paused()) {
            addConsoleMessage(tr("%1 reached the maximum ratio you set.").arg(h.name()));
            addConsoleMessage(tr("Pausing torrent %1...").arg(h.name()));
            pauseTorrent(hash);
          }
        }
        //emit torrent_ratio_deleted(fileName);
      }
    }
  }
}

void QBtSession::setDownloadLimit(QString hash, long val) {
  QTorrentHandle h = getTorrentHandle(hash);
  if (h.is_valid()) {
    h.set_download_limit(val);
  }
}

void QBtSession::setUploadLimit(QString hash, long val) {
  qDebug("Set upload limit rate to %ld", val);
  QTorrentHandle h = getTorrentHandle(hash);
  if (h.is_valid()) {
    h.set_upload_limit(val);
  }
}

void QBtSession::handleDownloadFailure(QString url, QString reason) {
  emit downloadFromUrlFailure(url, reason);
  // Clean up
  const QUrl qurl = QUrl::fromEncoded(url.toUtf8());
  url_skippingDlg.removeOne(qurl);
  savepathLabel_fromurl.remove(qurl);
}

void QBtSession::setQueueingEnabled(bool enable) {
  if (queueingEnabled != enable) {
    qDebug("Queueing system is changing state...");
    queueingEnabled = enable;
  }
}

// Set BT session configuration
void QBtSession::configureSession() {
  qDebug("Configuring session");
  const Preferences pref;
  // * Ports binding
  const unsigned short old_listenPort = getListenPort();
  const unsigned short new_listenPort = pref.getSessionPort();
  if (old_listenPort != new_listenPort) {
    qDebug("Session port changes in program preferences: %d -> %d", old_listenPort, new_listenPort);
    setListeningPort(new_listenPort);
    addConsoleMessage(tr("qBittorrent is bound to port: TCP/%1", "e.g: qBittorrent is bound to port: 6881").arg(QString::number(getListenPort())));
  }

  // Downloads
  // * Save path
  defaultSavePath = pref.getSavePath();
  if (pref.isTempPathEnabled()) {
    setDefaultTempPath(pref.getTempPath());
  } else {
    setDefaultTempPath(QString::null);
  }
  setAppendLabelToSavePath(pref.appendTorrentLabel());
  setAppendqBExtension(pref.useIncompleteFilesExtension());
  preAllocateAllFiles(pref.preAllocateAllFiles());
  // * Torrent export directory
  const bool torrentExportEnabled = pref.isTorrentExportEnabled();
  if (m_torrentExportEnabled != torrentExportEnabled) {
    m_torrentExportEnabled = torrentExportEnabled;
    if (m_torrentExportEnabled) {
      qDebug("Torrent export is enabled, exporting the current torrents");
      exportTorrentFiles(pref.getTorrentExportDir());
    }
  }
  // * Finished Torrent export directory
  const bool finishedTorrentExportEnabled = pref.isFinishedTorrentExportEnabled();
  if (m_finishedTorrentExportEnabled != finishedTorrentExportEnabled)
    m_finishedTorrentExportEnabled = finishedTorrentExportEnabled;
  // Connection
  // * Global download limit
  const bool alternative_speeds = pref.isAltBandwidthEnabled();
  int down_limit;
  if (alternative_speeds)
    down_limit = pref.getAltGlobalDownloadLimit();
  else
    down_limit = pref.getGlobalDownloadLimit();
  if (down_limit <= 0) {
    // Download limit disabled
    setDownloadRateLimit(-1);
  } else {
    // Enabled
    setDownloadRateLimit(down_limit*1024);
  }
  int up_limit;
  if (alternative_speeds)
    up_limit = pref.getAltGlobalUploadLimit();
  else
    up_limit = pref.getGlobalUploadLimit();
  // * Global Upload limit
  if (up_limit <= 0) {
    // Upload limit disabled
    setUploadRateLimit(-1);
  } else {
    // Enabled
    setUploadRateLimit(up_limit*1024);
  }
  if (pref.isSchedulerEnabled()) {
    if (!bd_scheduler) {
      bd_scheduler = new BandwidthScheduler(this);
      connect(bd_scheduler, SIGNAL(switchToAlternativeMode(bool)), this, SLOT(useAlternativeSpeedsLimit(bool)));
    }
    bd_scheduler->start();
  } else {
    if (bd_scheduler) delete bd_scheduler;
  }
#ifndef DISABLE_GUI
  // Resolve countries
  qDebug("Loading country resolution settings");
  const bool new_resolv_countries = pref.resolvePeerCountries();
  if (resolve_countries != new_resolv_countries) {
    qDebug("in country reoslution settings");
    resolve_countries = new_resolv_countries;
    if (resolve_countries && !geoipDBLoaded) {
      qDebug("Loading geoip database");
      GeoIPManager::loadDatabase(s);
      geoipDBLoaded = true;
    }
    // Update torrent handles
    std::vector<torrent_handle> torrents = s->get_torrents();

    std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
    std::vector<torrent_handle>::iterator torrentITend = torrents.end();
    for ( ; torrentIT != torrentITend; ++torrentIT) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if (h.is_valid())
        h.resolve_countries(resolve_countries);
    }
  }
#endif
  // * UPnP / NAT-PMP
  if (pref.isUPnPEnabled()) {
    enableUPnP(true);
    addConsoleMessage(tr("UPnP / NAT-PMP support [ON]"), QString::fromUtf8("blue"));
  } else {
    enableUPnP(false);
    addConsoleMessage(tr("UPnP / NAT-PMP support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Session settings
  session_settings sessionSettings = s->settings();
  sessionSettings.user_agent = "qBittorrent "VERSION;
  //std::cout << "HTTP user agent is " << sessionSettings.user_agent << std::endl;
  addConsoleMessage(tr("HTTP user agent is %1").arg(misc::toQString(sessionSettings.user_agent)));

  sessionSettings.upnp_ignore_nonrouters = true;
  sessionSettings.use_dht_as_fallback = false;
  // To prevent ISPs from blocking seeding
  sessionSettings.lazy_bitfields = true;
  // Speed up exit
  sessionSettings.stop_tracker_timeout = 1;
  //sessionSettings.announce_to_all_trackers = true;
  sessionSettings.auto_scrape_interval = 1200; // 20 minutes
  bool announce_to_all = pref.announceToAllTrackers();
  sessionSettings.announce_to_all_trackers = announce_to_all;
  sessionSettings.announce_to_all_tiers = announce_to_all;
  sessionSettings.auto_scrape_min_interval = 900; // 15 minutes
  int cache_size = pref.diskCacheSize();
  sessionSettings.cache_size = cache_size ? pref.diskCacheSize() * 64 : -1;
  qDebug() << "Using a disk cache size of" << pref.diskCacheSize() << "MiB";
  // Disable OS cache to avoid memory problems (uTorrent behavior)
#ifdef Q_WS_WIN
  // Fixes huge memory usage on Windows 7 (especially when checking files)
  sessionSettings.disk_io_write_mode = session_settings::disable_os_cache;
  sessionSettings.disk_io_read_mode = session_settings::disable_os_cache;
#endif
#if LIBTORRENT_VERSION_MINOR > 15
  sessionSettings.anonymous_mode = pref.isAnonymousModeEnabled();
  if (sessionSettings.anonymous_mode) {
    addConsoleMessage(tr("Anonymous mode [ON]"), "blue");
  } else {
    addConsoleMessage(tr("Anonymous mode [OFF]"), "blue");
  }
#endif
  // Queueing System
  if (pref.isQueueingSystemEnabled()) {
    sessionSettings.active_downloads = pref.getMaxActiveDownloads();
    sessionSettings.active_seeds = pref.getMaxActiveUploads();
    sessionSettings.active_limit = pref.getMaxActiveTorrents();
    sessionSettings.dont_count_slow_torrents = pref.ignoreSlowTorrentsForQueueing();
    setQueueingEnabled(true);
  } else {
    sessionSettings.active_downloads = -1;
    sessionSettings.active_seeds = -1;
    sessionSettings.active_limit = -1;
    setQueueingEnabled(false);
  }
  // Outgoing ports
  sessionSettings.outgoing_ports = std::make_pair(pref.outgoingPortsMin(), pref.outgoingPortsMax());
  // Ignore limits on LAN
  qDebug() << "Ignore limits on LAN" << pref.ignoreLimitsOnLAN();
  sessionSettings.ignore_limits_on_local_network = pref.ignoreLimitsOnLAN();
  // Include overhead in transfer limits
  sessionSettings.rate_limit_ip_overhead = pref.includeOverheadInLimits();
  // IP address to announce to trackers
  QString announce_ip = pref.getNetworkAddress();
  if (!announce_ip.isEmpty()) {
#if LIBTORRENT_VERSION_MINOR > 15
    sessionSettings.announce_ip = announce_ip.toStdString();
#else
    boost::system::error_code ec;
    boost::asio::ip::address addr = boost::asio::ip::address::from_string(announce_ip.toStdString(), ec);
    if (!ec) {
      addConsoleMessage(tr("Reporting IP address %1 to trackers...").arg(announce_ip));
      sessionSettings.announce_ip = addr;
    }
#endif
  }
  // Super seeding
  sessionSettings.strict_super_seeding = pref.isSuperSeedingEnabled();
#if LIBTORRENT_VERSION_MINOR > 15
  // * Max Half-open connections
  sessionSettings.half_open_limit = pref.getMaxHalfOpenConnections();
  // * Max connections limit
  sessionSettings.connections_limit = pref.getMaxConnecs();
#else
  // * Max Half-open connections
  s->set_max_half_open_connections(pref.getMaxHalfOpenConnections());
  // * Max connections limit
  setMaxConnections(pref.getMaxConnecs());
#endif
#if LIBTORRENT_VERSION_MINOR > 15
  // uTP
  if (pref.isuTPEnabled()) {
    sessionSettings.enable_incoming_utp = true;
    sessionSettings.enable_outgoing_utp = true;
  } else {
    sessionSettings.enable_incoming_utp = false;
    sessionSettings.enable_outgoing_utp = false;
  }
  // uTP rate limiting
  sessionSettings.rate_limit_utp = pref.isuTPRateLimited();
  if (sessionSettings.rate_limit_utp)
    sessionSettings.mixed_mode_algorithm = session_settings::prefer_tcp;
  else
    sessionSettings.mixed_mode_algorithm = session_settings::peer_proportional;
#endif
  qDebug() << "Settings SessionSettings";
  setSessionSettings(sessionSettings);
  // Bittorrent
  // * Max connections per torrent limit
  setMaxConnectionsPerTorrent(pref.getMaxConnecsPerTorrent());
  // * Max uploads per torrent limit
  setMaxUploadsPerTorrent(pref.getMaxUploadsPerTorrent());
  // * DHT
  if (pref.isDHTEnabled()) {
    // Set DHT Port
    if (enableDHT(true)) {
      int dht_port;
      if (pref.isDHTPortSameAsBT())
        dht_port = 0;
      else
        dht_port = pref.getDHTPort();
      setDHTPort(dht_port);
      if (dht_port == 0) dht_port = new_listenPort;
      addConsoleMessage(tr("DHT support [ON], port: UDP/%1").arg(dht_port), QString::fromUtf8("blue"));
    } else {
      addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("red"));
    }
  } else {
    enableDHT(false);
    addConsoleMessage(tr("DHT support [OFF]"), QString::fromUtf8("blue"));
  }
  // * PeX
  if (PeXEnabled) {
    addConsoleMessage(tr("PeX support [ON]"), QString::fromUtf8("blue"));
  } else {
    addConsoleMessage(tr("PeX support [OFF]"), QString::fromUtf8("red"));
  }
  if (PeXEnabled != pref.isPeXEnabled()) {
    addConsoleMessage(tr("Restart is required to toggle PeX support"), QString::fromUtf8("red"));
  }
  // * LSD
  if (pref.isLSDEnabled()) {
    enableLSD(true);
    addConsoleMessage(tr("Local Peer Discovery support [ON]"), QString::fromUtf8("blue"));
  } else {
    enableLSD(false);
    addConsoleMessage(tr("Local Peer Discovery support [OFF]"), QString::fromUtf8("blue"));
  }
  // * Encryption
  const int encryptionState = pref.getEncryptionSetting();
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
  // * Maximum ratio
  high_ratio_action = pref.getMaxRatioAction();
  setGlobalMaxRatio(pref.getGlobalMaxRatio());
  updateRatioTimer();
  // Ip Filter
  FilterParserThread::processFilterList(s, pref.bannedIPs());
  if (pref.isFilteringEnabled()) {
    enableIPFilter(pref.getFilter());
  }else{
    disableIPFilter();
  }
  // Update Web UI
  // Use a QTimer because the function can be called from qBtSession constructor
  QTimer::singleShot(0, this, SLOT(initWebUi()));
  // * Proxy settings
  proxy_settings proxySettings;
  if (pref.isProxyEnabled()) {
    qDebug("Enabling P2P proxy");
    proxySettings.hostname = pref.getProxyIp().toStdString();
    qDebug("hostname is %s", proxySettings.hostname.c_str());
    proxySettings.port = pref.getProxyPort();
    qDebug("port is %d", proxySettings.port);
    if (pref.isProxyAuthEnabled()) {
      proxySettings.username = pref.getProxyUsername().toStdString();
      proxySettings.password = pref.getProxyPassword().toStdString();
      qDebug("username is %s", proxySettings.username.c_str());
      qDebug("password is %s", proxySettings.password.c_str());
    }
  }
  switch(pref.getProxyType()) {
  case Proxy::HTTP:
    qDebug("type: http");
    proxySettings.type = proxy_settings::http;
    break;
  case Proxy::HTTP_PW:
    qDebug("type: http_pw");
    proxySettings.type = proxy_settings::http_pw;
    break;
  case Proxy::SOCKS4:
    proxySettings.type = proxy_settings::socks4;
  case Proxy::SOCKS5:
    qDebug("type: socks5");
    proxySettings.type = proxy_settings::socks5;
    break;
  case Proxy::SOCKS5_PW:
    qDebug("type: socks5_pw");
    proxySettings.type = proxy_settings::socks5_pw;
    break;
  default:
    proxySettings.type = proxy_settings::none;
  }
  setProxySettings(proxySettings);
  // Tracker
  if (pref.isTrackerEnabled()) {
    if (!m_tracker) {
      m_tracker = new QTracker(this);
    }
    if (m_tracker->start()) {
      addConsoleMessage(tr("Embedded Tracker [ON]"), QString::fromUtf8("blue"));
    } else {
      addConsoleMessage(tr("Failed to start the embedded tracker!"), QString::fromUtf8("red"));
    }
  } else {
    addConsoleMessage(tr("Embedded Tracker [OFF]"));
    if (m_tracker)
      delete m_tracker;
  }
  // * Scan dirs
  const QStringList scan_dirs = pref.getScanDirs();
  QList<bool> downloadInDirList = pref.getDownloadInScanDirs();
  while(scan_dirs.size() > downloadInDirList.size()) {
    downloadInDirList << false;
  }
  int i = 0;
  foreach (const QString &dir, scan_dirs) {
    qDebug() << "Adding scan dir" << dir << downloadInDirList.at(i);
    m_scanFolders->addPath(dir, downloadInDirList.at(i));
    ++i;
  }
  qDebug("Session configured");
}

void QBtSession::initWebUi() {
  Preferences pref;
  if (pref.isWebUiEnabled()) {
    const quint16 port = pref.getWebUiPort();
    const QString username = pref.getWebUiUsername();
    const QString password = pref.getWebUiPassword();

    if (httpServer) {
      if (httpServer->serverPort() != port) {
        httpServer->close();
      }
    } else {
      httpServer = new HttpServer(this);
    }

#ifndef QT_NO_OPENSSL
    if (pref.isWebUiHttpsEnabled()) {
      QSslCertificate cert(pref.getWebUiHttpsCertificate());
      QSslKey key;
      const QByteArray raw_key = pref.getWebUiHttpsKey();
      key = QSslKey(raw_key, QSsl::Rsa);
      if (!cert.isNull() && !key.isNull())
        httpServer->enableHttps(cert, key);
      else
        httpServer->disableHttps();
    } else {
      httpServer->disableHttps();
    }
#endif

    httpServer->setAuthorization(username, password);
    httpServer->setlocalAuthEnabled(pref.isWebUiLocalAuthEnabled());
    if (!httpServer->isListening()) {
      bool success = httpServer->listen(QHostAddress::Any, port);
      if (success)
        addConsoleMessage(tr("The Web UI is listening on port %1").arg(port));
      else
        addConsoleMessage(tr("Web User Interface Error - Unable to bind Web UI to port %1").arg(port), "red");
    }
    // DynDNS
    if (pref.isDynDNSEnabled()) {
      if (!m_dynDNSUpdater)
        m_dynDNSUpdater = new DNSUpdater(this);
      else
        m_dynDNSUpdater->updateCredentials();
    } else {
      if (m_dynDNSUpdater) {
        delete m_dynDNSUpdater;
        m_dynDNSUpdater = 0;
      }
    }
  } else {
    if (httpServer)
      delete httpServer;
    if (m_dynDNSUpdater) {
      delete m_dynDNSUpdater;
      m_dynDNSUpdater = 0;
    }
  }
}

void QBtSession::useAlternativeSpeedsLimit(bool alternative) {
  qDebug() << Q_FUNC_INFO << alternative;
  // Save new state to remember it on startup
  Preferences pref;
  pref.setAltBandwidthEnabled(alternative);
  // Apply settings to the bittorrent session
  int down_limit = alternative ? pref.getAltGlobalDownloadLimit() : pref.getGlobalDownloadLimit();
  if (down_limit <= 0) {
    down_limit = -1;
  } else {
    down_limit *= 1024;
  }
  setDownloadRateLimit(down_limit);
  // Upload rate
  int up_limit = alternative ? pref.getAltGlobalUploadLimit() : pref.getGlobalUploadLimit();
  if (up_limit <= 0) {
    up_limit = -1;
  } else {
    up_limit *= 1024;
  }
  setUploadRateLimit(up_limit);
  // Notify
  emit alternativeSpeedsModeChanged(alternative);
}

// Return the torrent handle, given its hash
QTorrentHandle QBtSession::getTorrentHandle(const QString &hash) const {
  return QTorrentHandle(s->find_torrent(QStringToSha1(hash)));
}

bool QBtSession::hasActiveTorrents() const {
  std::vector<torrent_handle> torrents = s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    const QTorrentHandle h(*torrentIT);
    if (h.is_valid() && !h.is_paused() && !h.is_queued())
      return true;
  }
  return false;
}

bool QBtSession::hasDownloadingTorrents() const {
  std::vector<torrent_handle> torrents = s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    if (torrentIT->is_valid()) {
      try {
        const torrent_status::state_t state = torrentIT->status().state;
        if (state != torrent_status::finished && state != torrent_status::seeding)
          return true;
      } catch(std::exception) {}
    }
  }
  return false;
}

void QBtSession::banIP(QString ip) {
  FilterParserThread::processFilterList(s, QStringList(ip));
  Preferences().banIP(ip);
}

// Delete a torrent from the session, given its hash
// permanent = true means that the torrent will be removed from the hard-drive too
void QBtSession::deleteTorrent(const QString &hash, bool delete_local_files) {
  qDebug("Deleting torrent with hash: %s", qPrintable(hash));
  const QTorrentHandle h = getTorrentHandle(hash);
  if (!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    return;
  }
  emit torrentAboutToBeRemoved(h);
  qDebug("h is valid, getting name or hash...");
  QString fileName;
  if (h.has_metadata())
    fileName = h.name();
  else
    fileName = h.hash();
  // Remove it from session
  if (delete_local_files) {
    if (h.has_metadata()) {
      QDir save_dir(h.save_path());
      if (save_dir != QDir(defaultSavePath) && (defaultTempPath.isEmpty() || save_dir != QDir(defaultTempPath))) {
        savePathsToRemove[hash] = save_dir.absolutePath();
        qDebug() << "Save path to remove (async): " << save_dir.absolutePath();
      }
    }
    s->remove_torrent(h, session::delete_files);
  } else {
    QStringList uneeded_files;
    if (h.has_metadata())
      uneeded_files = h.absolute_files_path_uneeded();
    s->remove_torrent(h);
    // Remove unneeded and incomplete files
    foreach (const QString &uneeded_file, uneeded_files) {
      qDebug("Removing uneeded file: %s", qPrintable(uneeded_file));
      fsutils::forceRemove(uneeded_file);
      const QString parent_folder = fsutils::branchPath(uneeded_file);
      qDebug("Attempt to remove parent folder (if empty): %s", qPrintable(parent_folder));
      QDir().rmpath(parent_folder);
    }
  }
  // Remove it from torrent backup directory
  QDir torrentBackup(fsutils::BTBackupLocation());
  QStringList filters;
  filters << hash+".*";
  const QStringList files = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  foreach (const QString &file, files) {
    fsutils::forceRemove(torrentBackup.absoluteFilePath(file));
  }
  TorrentPersistentData::deletePersistentData(hash);
  // Remove tracker errors
  trackersInfos.remove(hash);
  if (delete_local_files)
    addConsoleMessage(tr("'%1' was removed from transfer list and hard disk.", "'xxx.avi' was removed...").arg(fileName));
  else
    addConsoleMessage(tr("'%1' was removed from transfer list.", "'xxx.avi' was removed...").arg(fileName));
  qDebug("Torrent deleted.");
  emit deletedTorrent(hash);
  qDebug("Deleted signal emitted.");
}

void QBtSession::pauseAllTorrents() {
  std::vector<torrent_handle> torrents = s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    try {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if (!h.is_paused()) {
        h.pause();
        emit pausedTorrent(h);
      }
    } catch(invalid_handle&) {}
  }
}

std::vector<torrent_handle> QBtSession::getTorrents() const {
  return s->get_torrents();
}

void QBtSession::resumeAllTorrents() {
  std::vector<torrent_handle> torrents = s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    try {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if (h.is_paused()) {
        h.resume();
        emit resumedTorrent(h);
      }
    } catch(invalid_handle&) {}
  }
}

void QBtSession::pauseTorrent(const QString &hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if (!h.is_paused()) {
    h.pause();
    emit pausedTorrent(h);
  }
}

void QBtSession::resumeTorrent(const QString &hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if (h.is_paused()) {
    h.resume();
    emit resumedTorrent(h);
  }
}

bool QBtSession::loadFastResumeData(const QString &hash, std::vector<char> &buf) {
  const QString fastresume_path = QDir(fsutils::BTBackupLocation()).absoluteFilePath(hash+QString(".fastresume"));
  qDebug("Trying to load fastresume data: %s", qPrintable(fastresume_path));
  QFile fastresume_file(fastresume_path);
  if (fastresume_file.size() <= 0)
    return false;
  if (!fastresume_file.open(QIODevice::ReadOnly))
    return false;
  const QByteArray content = fastresume_file.readAll();
  const int content_size = content.size();
  Q_ASSERT(content_size > 0);
  buf.resize(content_size);
  memcpy(&buf[0], content.data(), content_size);
  fastresume_file.close();
  return true;
}

void QBtSession::loadTorrentSettings(QTorrentHandle& h) {
  Preferences pref;
  // Connections limit per torrent
  h.set_max_connections(pref.getMaxConnecsPerTorrent());
  // Uploads limit per torrent
  h.set_max_uploads(pref.getMaxUploadsPerTorrent());
#ifndef DISABLE_GUI
  // Resolve countries
  h.resolve_countries(resolve_countries);
#endif
}

QTorrentHandle QBtSession::addMagnetUri(QString magnet_uri, bool resumed, bool fromScanDir, const QString &filePath)
{
  Q_UNUSED(fromScanDir);
  Q_UNUSED(filePath);
  Preferences pref;
  QTorrentHandle h;
  const QString hash(misc::magnetUriToHash(magnet_uri));
  if (hash.isEmpty()) {
    addConsoleMessage(tr("'%1' is not a valid magnet URI.").arg(magnet_uri));
    return h;
  }
  const QDir torrentBackup(fsutils::BTBackupLocation());
  if (resumed) {
    // Load metadata
    const QString torrent_path = torrentBackup.absoluteFilePath(hash+".torrent");
    if (QFile::exists(torrent_path))
      return addTorrent(torrent_path, false, QString::null, true);
  }
  qDebug("Adding a magnet URI: %s", qPrintable(hash));
  Q_ASSERT(magnet_uri.startsWith("magnet:", Qt::CaseInsensitive));

  // Check for duplicate torrent
  if (s->find_torrent(QStringToSha1(hash)).is_valid()) {
    qDebug("/!\\ Torrent is already in download list");
    addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(magnet_uri));
    // Check if the torrent contains trackers or url seeds we don't know about
    // and add them
    QTorrentHandle h_ex = getTorrentHandle(hash);
    mergeTorrents(h_ex, magnet_uri);

    return h;
  }

  add_torrent_params p = initializeAddTorrentParams(hash);

  // Get save path
  QString savePath;
  if (!resumed && savepathLabel_fromurl.contains(magnet_uri)) {
    QPair<QString, QString> savePath_label = savepathLabel_fromurl.take(magnet_uri);
    if(!savePath_label.first.isEmpty())
      savePath = savePath_label.first;
    // Remember label
    if(!savePath_label.second.isEmpty())
      TorrentTempData::setLabel(hash, savePath_label.second);
  }
  if (savePath.isEmpty())
    savePath = getSavePath(hash, false);
  if (!defaultTempPath.isEmpty() && !TorrentPersistentData::isSeed(hash)) {
    qDebug("addMagnetURI: Temp folder is enabled.");
    QString torrent_tmp_path = defaultTempPath.replace("\\", "/");
    p.save_path = torrent_tmp_path.toUtf8().constData();
    // Check if save path exists, creating it otherwise
    if (!QDir(torrent_tmp_path).exists())
      QDir().mkpath(torrent_tmp_path);
    qDebug("addMagnetURI: using save_path: %s", qPrintable(torrent_tmp_path));
  } else {
    p.save_path = savePath.toUtf8().constData();
    // Check if save path exists, creating it otherwise
    if (!QDir(savePath).exists()) QDir().mkpath(savePath);
    qDebug("addMagnetURI: using save_path: %s", qPrintable(savePath));
  }

  qDebug("Adding magnet URI: %s", qPrintable(magnet_uri));

  // Adding torrent to Bittorrent session
  try {
    h =  QTorrentHandle(add_magnet_uri(*s, magnet_uri.toStdString(), p));
  }catch(std::exception e) {
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if (!h.is_valid()) {
    // No need to keep on, it failed.
    qDebug("/!\\ Error: Invalid handle");
    return h;
  }
  Q_ASSERT(h.hash() == hash);

  loadTorrentSettings(h);

  // Load filtered files
  if (!resumed) {
    loadTorrentTempData(h, savePath, true);
  }
  if (!pref.addTorrentsInPause()) {
    // Start torrent because it was added in paused state
    h.resume();
  }
  // Send torrent addition signal
  addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(magnet_uri));
  emit addedTorrent(h);
  return h;
}

// Add a torrent to the Bittorrent session
QTorrentHandle QBtSession::addTorrent(QString path, bool fromScanDir, QString from_url, bool resumed) {
  QTorrentHandle h;
  Preferences pref;

  // Check if BT_backup directory exists
  const QDir torrentBackup(fsutils::BTBackupLocation());
  if (!torrentBackup.exists()) return h;

  // Fix the input path if necessary
#ifdef Q_WS_WIN
  // Windows hack
  if (!path.endsWith(".torrent"))
    if (QFile::rename(path, path+".torrent")) path += ".torrent";
#endif
  if (path.startsWith("file:", Qt::CaseInsensitive))
    path = QUrl::fromEncoded(path.toLocal8Bit()).toLocalFile();
  if (path.isEmpty()) return h;

  Q_ASSERT(!misc::isUrl(path));

  qDebug("Adding %s to download list", qPrintable(path));
  boost::intrusive_ptr<torrent_info> t;
  try {
    qDebug() << "Loading torrent at" << path;
    // Getting torrent file informations
    t = new torrent_info(path.toUtf8().constData());
    if (!t->is_valid())
      throw std::exception();
  } catch(std::exception& e) {
    if (!from_url.isNull()) {
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(from_url), QString::fromUtf8("red"));
      addConsoleMessage(QString::fromLocal8Bit(e.what()), "red");
      //emit invalidTorrent(from_url);
      fsutils::forceRemove(path);
    }else{
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
      QString displayed_path = path;
      displayed_path.replace("/", "\\");
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(displayed_path), QString::fromUtf8("red"));
#else
      addConsoleMessage(tr("Unable to decode torrent file: '%1'", "e.g: Unable to decode torrent file: '/home/y/xxx.torrent'").arg(path), QString::fromUtf8("red"));
#endif
      //emit invalidTorrent(path);
    }
    addConsoleMessage(tr("This file is either corrupted or this isn't a torrent."),QString::fromUtf8("red"));
    if (fromScanDir) {
      // Remove file
      fsutils::forceRemove(path);
    }
    return h;
  }

  const QString hash = misc::toQString(t->info_hash());

  qDebug(" -> Hash: %s", qPrintable(hash));
  qDebug(" -> Name: %s", t->name().c_str());

  // Check for duplicate
  if (s->find_torrent(t->info_hash()).is_valid()) {
    qDebug("/!\\ Torrent is already in download list");
    // Update info Bar
    if (!from_url.isNull()) {
      addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(from_url));
    }else{
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
      QString displayed_path = path;
      displayed_path.replace("/", "\\");
      addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(displayed_path));
#else
      addConsoleMessage(tr("'%1' is already in download list.", "e.g: 'xxx.avi' is already in download list.").arg(path));
#endif
    }
    // Check if the torrent contains trackers or url seeds we don't know about
    // and add them
    QTorrentHandle h_ex = getTorrentHandle(hash);
    mergeTorrents(h_ex, t);

    // Delete file if temporary
    if (!from_url.isNull() || fromScanDir)
        fsutils::forceRemove(path);
    return h;
  }

  // Check number of files
  if (t->num_files() < 1) {
    addConsoleMessage(tr("Error: The torrent %1 does not contain any file.").arg(misc::toQStringU(t->name())));
    // Delete file if temporary
    if (!from_url.isNull() || fromScanDir)
        fsutils::forceRemove(path);
    return h;
  }

  // Actually add the torrent
  add_torrent_params p = initializeAddTorrentParams(hash);
  p.ti = t;

  // Get fast resume data if existing
  bool fastResume = false;
  std::vector<char> buf; // Needs to stay in the function scope
  if (resumed) {
    if (loadFastResumeData(hash, buf)) {
      fastResume = true;
      p.resume_data = &buf;
      qDebug("Successfully loaded fast resume data");
    }
  }
#if LIBTORRENT_VERSION_MINOR < 16
  else {
    // Generate fake resume data to make sure unwanted files
    // are not allocated
    if (preAllocateAll) {
      vector<int> fp;
      TorrentTempData::getFilesPriority(hash, fp);
      if ((int)fp.size() == t->num_files()) {
        entry rd = generateFilePriorityResumeData(t, fp);
        bencode(std::back_inserter(buf), rd);
        p.resume_data = &buf;
      }
    }
  }
#endif

  QString savePath;
  if (!from_url.isEmpty() && savepathLabel_fromurl.contains(QUrl::fromEncoded(from_url.toUtf8()))) {
    // Enforcing the save path defined before URL download (from RSS for example)
    QPair<QString, QString> savePath_label = savepathLabel_fromurl.take(QUrl::fromEncoded(from_url.toUtf8()));
    if (savePath_label.first.isEmpty())
      savePath = getSavePath(hash, fromScanDir, path);
    else
      savePath = savePath_label.first;
    // Remember label
    TorrentTempData::setLabel(hash, savePath_label.second);
  } else {
    savePath = getSavePath(hash, fromScanDir, path);
  }
  if (!defaultTempPath.isEmpty() && !TorrentPersistentData::isSeed(hash)) {
    qDebug("addTorrent::Temp folder is enabled.");
    QString torrent_tmp_path = defaultTempPath.replace("\\", "/");
    p.save_path = torrent_tmp_path.toUtf8().constData();
    // Check if save path exists, creating it otherwise
    if (!QDir(torrent_tmp_path).exists()) QDir().mkpath(torrent_tmp_path);
    qDebug("addTorrent: using save_path: %s", qPrintable(torrent_tmp_path));
  } else {
    p.save_path = savePath.toUtf8().constData();
    // Check if save path exists, creating it otherwise
    if (!QDir(savePath).exists()) QDir().mkpath(savePath);
    qDebug("addTorrent: using save_path: %s", qPrintable(savePath));
  }

  // Adding torrent to Bittorrent session
  try {
    h =  QTorrentHandle(s->add_torrent(p));
  }catch(std::exception e) {
    qDebug("Error: %s", e.what());
  }
  // Check if it worked
  if (!h.is_valid()) {
    qDebug("/!\\ Error: Invalid handle");
    if (!from_url.isNull())
        fsutils::forceRemove(path);
    return h;
  }

  loadTorrentSettings(h);

  if (!resumed) {
    qDebug("This is a NEW torrent (first time)...");
    loadTorrentTempData(h, savePath, false);

    // Append .!qB to incomplete files
    if (appendqBExtension)
      appendqBextensionToTorrent(h, true);

    // Backup torrent file
    const QString newFile = torrentBackup.absoluteFilePath(hash + ".torrent");
    if (path != newFile)
      QFile::copy(path, newFile);
    // Copy the torrent file to the export folder
    if (m_torrentExportEnabled)
      exportTorrentFile(h);
  }

  if (!fastResume && !pref.addTorrentsInPause()) {
    // Start torrent because it was added in paused state
    h.resume();
  }

  // If temporary file, remove it
  if (!from_url.isNull() || fromScanDir)
      fsutils::forceRemove(path);

  // Display console message
  if (!from_url.isNull()) {
    if (fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(from_url));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(from_url));
  }else{
    if (fastResume)
      addConsoleMessage(tr("'%1' resumed. (fast resume)", "'/home/y/xxx.torrent' was resumed. (fast resume)").arg(fsutils::toDisplayPath(path)));
    else
      addConsoleMessage(tr("'%1' added to download list.", "'/home/y/xxx.torrent' was added to download list.").arg(fsutils::toDisplayPath(path)));
  }

  // Send torrent addition signal
  emit addedTorrent(h);
  return h;
}

void QBtSession::exportTorrentFile(const QTorrentHandle& h, TorrentExportFolder folder) {
  Q_ASSERT((folder == RegularTorrentExportFolder && m_torrentExportEnabled) ||
           (folder == FinishedTorrentExportFolder && m_finishedTorrentExportEnabled));
  QString torrent_path = QDir(fsutils::BTBackupLocation()).absoluteFilePath(h.hash()+".torrent");
  QDir exportPath(folder == RegularTorrentExportFolder ? Preferences().getTorrentExportDir() : Preferences().getFinishedTorrentExportDir());
  if (exportPath.exists() || exportPath.mkpath(exportPath.absolutePath())) {
    QString new_torrent_path = exportPath.absoluteFilePath(h.name()+".torrent");
    if (QFile::exists(new_torrent_path) && fsutils::sameFiles(torrent_path, new_torrent_path)) {
      // Append hash to torrent name to make it unique
      new_torrent_path = exportPath.absoluteFilePath(h.name()+"-"+h.hash()+".torrent");
    }
    QFile::copy(torrent_path, new_torrent_path);
  }
}

add_torrent_params QBtSession::initializeAddTorrentParams(const QString &hash) {
  add_torrent_params p;

  // Seeding mode
  // Skip checking and directly start seeding (new in libtorrent v0.15)
  if (TorrentTempData::isSeedingMode(hash))
    p.seed_mode=true;
  else
    p.seed_mode=false;

  // Preallocation mode
  if (preAllocateAll)
    p.storage_mode = storage_mode_allocate;
  else
    p.storage_mode = storage_mode_sparse;

  // Priorities
/*#if LIBTORRENT_VERSION_MINOR > 15
  if (TorrentTempData::hasTempData(hash)) {
    std::vector<int> fp;
    TorrentTempData::getFilesPriority(hash, fp);
    if (!fp.empty()) {
      std::vector<boost::uint8_t> *fp_conv = new std::vector<boost::uint8_t>();
      for (uint i=0; i<fp.size(); ++i) {
        fp_conv->push_back(fp[i]);
      }
      p.file_priorities = fp_conv;
    }
  }
#endif*/

  // Start in pause
  p.paused = true;
  p.duplicate_is_error = false; // Already checked
  p.auto_managed = false; // Because it is added in paused state

  return p;
}

void QBtSession::loadTorrentTempData(QTorrentHandle &h, QString savePath, bool magnet) {
  qDebug("loadTorrentTempdata() - ENTER");
  const QString hash = h.hash();
  // Sequential download
  if (TorrentTempData::hasTempData(hash)) {
    // sequential download
    h.set_sequential_download(TorrentTempData::isSequential(hash));

    // The following is useless for newly added magnet
    if (!magnet) {
      // Files priorities
      vector<int> fp;
      TorrentTempData::getFilesPriority(hash, fp);
      h.prioritize_files(fp);

      // Prioritize first/last piece
      h.prioritize_first_last_piece(TorrentTempData::isSequential(hash));

      // Update file names
      const QStringList files_path = TorrentTempData::getFilesPath(hash);
      bool force_recheck = false;
      if (files_path.size() == h.num_files()) {
        for (int i=0; i<h.num_files(); ++i) {
          QString old_path = h.absolute_files_path().at(i);
          old_path.replace("\\", "/");
          if (!QFile::exists(old_path)) {
            // Remove old parent folder manually since we will
            // not get a file_renamed alert
            QStringList parts = old_path.split("/", QString::SkipEmptyParts);
            parts.removeLast();
            if (!parts.empty())
              QDir().rmpath(parts.join("/"));
          }
          const QString &path = files_path.at(i);
          if (!force_recheck && QDir(h.save_path()).exists(path))
            force_recheck = true;
          qDebug("Renaming file to %s", qPrintable(path));
          h.rename_file(i, path);
        }
        // Force recheck
        if (force_recheck) h.force_recheck();
      }
    }
  }
  // Save persistent data for new torrent
  qDebug("Saving torrent persistant data");
  if (defaultTempPath.isEmpty())
    TorrentPersistentData::saveTorrentPersistentData(h, QString::null, magnet);
  else
    TorrentPersistentData::saveTorrentPersistentData(h, savePath, magnet);
}

void QBtSession::mergeTorrents(QTorrentHandle& h_ex, const QString& magnet_uri)
{
  QList<QUrl> new_trackers = misc::magnetUriToTrackers(magnet_uri);
  bool trackers_added = false;
  foreach (const QUrl& new_tracker, new_trackers) {
    bool found = false;
    std::vector<announce_entry> existing_trackers = h_ex.trackers();
    foreach (const announce_entry& existing_tracker, existing_trackers) {
      if (new_tracker == QUrl(existing_tracker.url.c_str())) {
        found = true;
        break;
      }
    }

    if (!found) {
      h_ex.add_tracker(announce_entry(new_tracker.toString().toStdString()));
      trackers_added = true;
    }
  }
  if (trackers_added)
    addConsoleMessage(tr("Note: new trackers were added to the existing torrent."));
}

void QBtSession::mergeTorrents(QTorrentHandle &h_ex, boost::intrusive_ptr<torrent_info> t) {
  // Check if the torrent contains trackers or url seeds we don't know about
  // and add them
  if (!h_ex.is_valid()) return;
  std::vector<announce_entry> existing_trackers = h_ex.trackers();
  std::vector<announce_entry> new_trackers = t->trackers();
  bool trackers_added = false;
  foreach (const announce_entry& new_tracker, new_trackers) {
    std::string new_tracker_url = new_tracker.url;
    // Check if existing torrent has this tracker
    bool found = false;
    foreach (const announce_entry& existing_tracker, existing_trackers) {
      if (QUrl(new_tracker_url.c_str()) == QUrl(existing_tracker.url.c_str())) {
        found = true;
        break;
      }
    }

    if (!found) {
      h_ex.add_tracker(announce_entry(new_tracker_url));
      trackers_added = true;
    }
  }

  if (trackers_added)
    addConsoleMessage(tr("Note: new trackers were added to the existing torrent."));

  bool urlseeds_added = false;
  const QStringList old_urlseeds = h_ex.url_seeds();
#if LIBTORRENT_VERSION_MINOR > 15
  std::vector<web_seed_entry> new_urlseeds = t->web_seeds();

  std::vector<web_seed_entry>::iterator it = new_urlseeds.begin();
  std::vector<web_seed_entry>::iterator itend = new_urlseeds.end();
  for ( ; it != itend; ++it) {
    const QString new_url = misc::toQString(it->url.c_str());
    if (!old_urlseeds.contains(new_url)) {
      urlseeds_added = true;
      h_ex.add_url_seed(new_url);
    }
  }
#else
  std::vector<std::string> new_urlseeds = t->url_seeds();

  std::vector<std::string>::iterator it = new_urlseeds.begin();
  std::vector<std::string>::iterator itend = new_urlseeds.end();
  for ( ; it != itend; ++it) {
    const QString new_url = misc::toQString(it->c_str());
    if (!old_urlseeds.contains(new_url)) {
      urlseeds_added = true;
      h_ex.add_url_seed(new_url);
    }
  }
#endif
  if (urlseeds_added)
    addConsoleMessage(tr("Note: new URL seeds were added to the existing torrent."));
}

void QBtSession::exportTorrentFiles(QString path) {
  Q_ASSERT(m_torrentExportEnabled);
  QDir exportDir(path);
  if (!exportDir.exists()) {
    if (!exportDir.mkpath(exportDir.absolutePath())) {
      std::cerr << "Error: Could not create torrent export directory: " << qPrintable(exportDir.absolutePath()) << std::endl;
      return;
    }
  }
  QDir torrentBackup(fsutils::BTBackupLocation());
  std::vector<torrent_handle> handles = s->get_torrents();

  std::vector<torrent_handle>::iterator itr=handles.begin();
  std::vector<torrent_handle>::iterator itrend=handles.end();
  for ( ; itr != itrend; ++itr) {
    const QTorrentHandle h(*itr);
    if (!h.is_valid()) {
      std::cerr << "Torrent Export: torrent is invalid, skipping..." << std::endl;
      continue;
    }
    const QString src_path(torrentBackup.absoluteFilePath(h.hash()+".torrent"));
    if (QFile::exists(src_path)) {
      QString dst_path = exportDir.absoluteFilePath(h.name()+".torrent");
      if (QFile::exists(dst_path)) {
        if (!fsutils::sameFiles(src_path, dst_path)) {
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
void QBtSession::setMaxConnections(int maxConnec) {
#if LIBTORRENT_VERSION_MINOR > 15
  Q_UNUSED(maxConnec);
  Q_ASSERT(0); // Should not be used
#else
  qDebug() << Q_FUNC_INFO << maxConnec;
  s->set_max_connections(maxConnec);
#endif
}

void QBtSession::setMaxConnectionsPerTorrent(int max) {
  qDebug() << Q_FUNC_INFO << max;
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();

  std::vector<torrent_handle>::const_iterator it = handles.begin();
  std::vector<torrent_handle>::const_iterator itend = handles.end();
  for ( ; it != itend; ++it) {
    if (!it->is_valid())
      continue;
    try {
      it->set_max_connections(max);
    } catch(std::exception) {}
  }
}

void QBtSession::setMaxUploadsPerTorrent(int max) {
  qDebug() << Q_FUNC_INFO << max;
  // Apply this to all session torrents
  std::vector<torrent_handle> handles = s->get_torrents();

  std::vector<torrent_handle>::const_iterator it = handles.begin();
  std::vector<torrent_handle>::const_iterator itend = handles.end();
  for ( ; it != itend; ++it) {
    if (!it->is_valid())
      continue;
    try {
      it->set_max_uploads(max);
    } catch(std::exception) {}
  }
}

void QBtSession::enableUPnP(bool b) {
  Preferences pref;
  if (b) {
    if (!m_upnp) {
      qDebug("Enabling UPnP / NAT-PMP");
      m_upnp = s->start_upnp();
      m_natpmp = s->start_natpmp();
    }
    // Use UPnP/NAT-PMP for Web UI too
    if (pref.isWebUiEnabled() && pref.useUPnPForWebUIPort()) {
      const qint16 port = pref.getWebUiPort();
      m_upnp->add_mapping(upnp::tcp, port, port);
      m_natpmp->add_mapping(natpmp::tcp, port, port);
    }
  } else {
    if (m_upnp) {
      qDebug("Disabling UPnP / NAT-PMP");
      s->stop_upnp();
      s->stop_natpmp();
      m_upnp = 0;
      m_natpmp = 0;
    }
  }
}

void QBtSession::enableLSD(bool b) {
  if (b) {
    if (!LSDEnabled) {
      qDebug("Enabling Local Peer Discovery");
      s->start_lsd();
      LSDEnabled = true;
    }
  } else {
    if (LSDEnabled) {
      qDebug("Disabling Local Peer Discovery");
      s->stop_lsd();
      LSDEnabled = false;
    }
  }
}

void QBtSession::loadSessionState() {
  const QString state_path = fsutils::cacheLocation()+QDir::separator()+QString::fromUtf8("ses_state");
  if (!QFile::exists(state_path)) return;
  if (QFile(state_path).size() == 0) {
    // Remove empty invalid state file
    fsutils::forceRemove(state_path);
    return;
  }
  QFile state_file(state_path);
  if (!state_file.open(QIODevice::ReadOnly)) return;
  std::vector<char> in;
  const qint64 content_size = state_file.bytesAvailable();
  if (content_size <= 0) return;
  in.resize(content_size);
  state_file.read(&in[0], content_size);
  // bdecode
  lazy_entry e;
#if LIBTORRENT_VERSION_MINOR > 15
  libtorrent::error_code ec;
  lazy_bdecode(&in[0], &in[0] + in.size(), e, ec);
  if (!ec) {
#else
  if (lazy_bdecode(&in[0], &in[0] + in.size(), e) == 0) {
#endif
    s->load_state(e);
  }
}

void QBtSession::saveSessionState() {
  qDebug("Saving session state to disk...");
  const QString state_path = fsutils::cacheLocation()+QDir::separator()+QString::fromUtf8("ses_state");
  entry session_state;
  s->save_state(session_state);
  vector<char> out;
  bencode(back_inserter(out), session_state);
  QFile session_file(state_path);
  if (!out.empty() && session_file.open(QIODevice::WriteOnly)) {
    session_file.write(&out[0], out.size());
    session_file.close();
  }
}

// Enable DHT
bool QBtSession::enableDHT(bool b) {
  if (b) {
    if (!DHTEnabled) {
      try {
        qDebug() << "Starting DHT...";
        Q_ASSERT(!s->is_dht_running());
        s->start_dht();
        s->add_dht_router(std::make_pair(std::string("router.bittorrent.com"), 6881));
        s->add_dht_router(std::make_pair(std::string("router.utorrent.com"), 6881));
        s->add_dht_router(std::make_pair(std::string("dht.transmissionbt.com"), 6881));
        s->add_dht_router(std::make_pair(std::string("dht.aelitis.com"), 6881)); // Vuze
        DHTEnabled = true;
        qDebug("DHT enabled");
      }catch(std::exception e) {
        qDebug("Could not enable DHT, reason: %s", e.what());
        return false;
      }
    }
  } else {
    if (DHTEnabled) {
      DHTEnabled = false;
      s->stop_dht();
      qDebug("DHT disabled");
    }
  }
  return true;
}

qreal QBtSession::getRealRatio(const QString &hash) const {
  QTorrentHandle h = getTorrentHandle(hash);
  if (!h.is_valid()) {
    return 0.;
  }

  libtorrent::size_type all_time_upload = h.all_time_upload();
  libtorrent::size_type all_time_download = h.all_time_download();
  if (all_time_download == 0 && h.is_seed()) {
    // Purely seeded torrent
    all_time_download = h.total_done();
  }

  if (all_time_download == 0) {
    if (all_time_upload == 0)
      return 0;
    return MAX_RATIO+1;
  }

  qreal ratio = all_time_upload / (float) all_time_download;
  Q_ASSERT(ratio >= 0.);
  if (ratio > MAX_RATIO)
    ratio = MAX_RATIO;
  return ratio;
}

// Called periodically
void QBtSession::saveTempFastResumeData() {
  std::vector<torrent_handle> torrents =  s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    try {
      if (!h.is_valid() || !h.has_metadata() /*|| h.is_seed() || h.is_paused()*/) continue;
#if LIBTORRENT_VERSION_MINOR > 15
      if (!h.need_save_resume_data()) continue;
#endif
      if (h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
      qDebug("Saving fastresume data for %s", qPrintable(h.name()));
      h.save_resume_data();
    }catch(std::exception e) {}
  }
}

// Only save fast resume data for unfinished and unpaused torrents (Optimization)
// Called on exit
void QBtSession::saveFastResumeData() {
  qDebug("Saving fast resume data...");
  // Stop listening for alerts
  resumeDataTimer.stop();
  timerAlerts->stop();
  int num_resume_data = 0;
  // Pause session
  s->pause();
  std::vector<torrent_handle> torrents =  s->get_torrents();

  std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
  std::vector<torrent_handle>::iterator torrentITend = torrents.end();
  for ( ; torrentIT != torrentITend; ++torrentIT) {
    QTorrentHandle h = QTorrentHandle(*torrentIT);
    if (!h.is_valid())
      continue;
    try {
      if (isQueueingEnabled())
        TorrentPersistentData::savePriority(h);
      if (!h.has_metadata())
        continue;
      // Actually with should save fast resume data for paused files too
      //if (h.is_paused()) continue;
      if (h.state() == torrent_status::checking_files || h.state() == torrent_status::queued_for_checking) continue;
      h.save_resume_data();
      ++num_resume_data;
    } catch(libtorrent::invalid_handle&) {}
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
        if (rda->handle.is_valid())
          s->remove_torrent(rda->handle);
      }catch(libtorrent::libtorrent_exception) {}
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
    QDir torrentBackup(fsutils::BTBackupLocation());
    const QTorrentHandle h(rd->handle);
    if (!h.is_valid()) continue;
    try {
      // Remove old fastresume file if it exists
      vector<char> out;
      bencode(back_inserter(out), *rd->resume_data);
      const QString filepath = torrentBackup.absoluteFilePath(h.hash()+".fastresume");
      QFile resume_file(filepath);
      if (resume_file.exists())
        fsutils::forceRemove(filepath);
      if (!out.empty() && resume_file.open(QIODevice::WriteOnly)) {
        resume_file.write(&out[0], out.size());
        resume_file.close();
      }
      // Remove torrent from session
      s->remove_torrent(rd->handle);
      s->pop_alert();
    } catch(libtorrent::invalid_handle&) {}
  }
}

#ifdef DISABLE_GUI
void QBtSession::addConsoleMessage(QString msg, QString) {
  emit newConsoleMessage(QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss") + " - " + msg);
#else
void QBtSession::addConsoleMessage(QString msg, QColor color) {
  if (consoleMessages.size() > MAX_LOG_MESSAGES) {
    consoleMessages.removeFirst();
  }
  msg = "<font color='grey'>"+ QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + "</font> - <font color='" + color.name() + "'><i>" + msg + "</i></font>";
  consoleMessages.append(msg);
  emit newConsoleMessage(msg);
#endif
}

void QBtSession::addPeerBanMessage(QString ip, bool from_ipfilter) {
  if (peerBanMessages.size() > MAX_LOG_MESSAGES) {
    peerBanMessages.removeFirst();
  }
  QString msg;
  if (from_ipfilter)
    msg = "<font color='grey'>" + QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + "</font> - " + tr("<font color='red'>%1</font> <i>was blocked due to your IP filter</i>", "x.y.z.w was blocked").arg(ip);
  else
    msg = "<font color='grey'>" + QDateTime::currentDateTime().toString(QString::fromUtf8("dd/MM/yyyy hh:mm:ss")) + "</font> - " + tr("<font color='red'>%1</font> <i>was banned due to corrupt pieces</i>", "x.y.z.w was banned").arg(ip);
  peerBanMessages.append(msg);
  emit newBanMessage(msg);
}

bool QBtSession::isFilePreviewPossible(const QString &hash) const {
  // See if there are supported files in the torrent
  const QTorrentHandle h = getTorrentHandle(hash);
  if (!h.is_valid() || !h.has_metadata()) {
    return false;
  }
  const unsigned int nbFiles = h.num_files();
  for (unsigned int i=0; i<nbFiles; ++i) {
    const QString extension = fsutils::fileExtension(h.filename_at(i));
    if (misc::isPreviewable(extension))
      return true;
  }
  return false;
}

void QBtSession::addTorrentsFromScanFolder(QStringList &pathList) {
  foreach (const QString &file, pathList) {
    qDebug("File %s added", qPrintable(file));
    if (file.endsWith(".magnet")) {
      QFile f(file);
      if (!f.open(QIODevice::ReadOnly)) {
        qDebug("Failed to open magnet file: %s", qPrintable(f.errorString()));
      } else {
        const QString link = QString::fromLocal8Bit(f.readAll());
        addMagnetUri(link, false, true, file);
        f.remove();
      }
      continue;
    }
    try {
      torrent_info t(file.toUtf8().constData());
      if (t.is_valid())
        addTorrent(file, true);
    } catch(std::exception&) {
      qDebug("Ignoring incomplete torrent file: %s", qPrintable(file));
    }
  }
}

void QBtSession::setDefaultTempPath(QString temppath) {
  if (defaultTempPath == temppath)
    return;
  if (temppath.isEmpty()) {
    // Disabling temp dir
    // Moving all torrents to their destination folder
    std::vector<torrent_handle> torrents = s->get_torrents();

    std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
    std::vector<torrent_handle>::iterator torrentITend = torrents.end();
    for ( ; torrentIT != torrentITend; ++torrentIT) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if (!h.is_valid()) continue;
      h.move_storage(getSavePath(h.hash()));
    }
  } else {
    qDebug("Enabling default temp path...");
    // Moving all downloading torrents to temporary save path
    std::vector<torrent_handle> torrents = s->get_torrents();

    std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
    std::vector<torrent_handle>::iterator torrentITend = torrents.end();
    for ( ; torrentIT != torrentITend; ++torrentIT) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      if (!h.is_valid()) continue;
      if (!h.is_seed()) {
        QString torrent_tmp_path = temppath.replace("\\", "/");
        qDebug("Moving torrent to its temp save path: %s", qPrintable(torrent_tmp_path));
        h.move_storage(torrent_tmp_path);
      }
    }
  }
  defaultTempPath = temppath;
}

void QBtSession::appendqBextensionToTorrent(const QTorrentHandle &h, bool append) {
  if (!h.is_valid() || !h.has_metadata()) return;
  std::vector<size_type> fp;
  h.file_progress(fp);
  for (int i=0; i<h.num_files(); ++i) {
    if (append) {
      const qulonglong file_size = h.filesize_at(i);
      if (file_size > 0 && (fp[i]/(double)file_size) < 1.) {
        const QString name = h.filepath_at(i);
        if (!name.endsWith(".!qB")) {
          const QString new_name = name+".!qB";
          qDebug("Renaming %s to %s", qPrintable(name), qPrintable(new_name));
          h.rename_file(i, new_name);
        }
      }
    } else {
      QString name = h.filepath_at(i);
      if (name.endsWith(".!qB")) {
        const QString old_name = name;
        name.chop(4);
        qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(name));
        h.rename_file(i, name);
      }
    }
  }
}

void QBtSession::changeLabelInTorrentSavePath(const QTorrentHandle &h, QString old_label, QString new_label) {
  if (!h.is_valid()) return;
  if (!appendLabelToSavePath) return;
  QString old_save_path = TorrentPersistentData::getSavePath(h.hash());
  if (!old_save_path.startsWith(defaultSavePath)) return;
  QString new_save_path = fsutils::updateLabelInSavePath(defaultSavePath, old_save_path, old_label, new_label);
  if (new_save_path != old_save_path) {
    // Move storage
    qDebug("Moving storage to %s", qPrintable(new_save_path));
    QDir().mkpath(new_save_path);
    h.move_storage(new_save_path);
  }
}

void QBtSession::appendLabelToTorrentSavePath(const QTorrentHandle& h) {
  if (!h.is_valid()) return;
  const QString label = TorrentPersistentData::getLabel(h.hash());
  if (label.isEmpty()) return;
  // Current save path
  QString old_save_path = TorrentPersistentData::getSavePath(h.hash());
  QString new_save_path = fsutils::updateLabelInSavePath(defaultSavePath, old_save_path, "", label);
  if (old_save_path != new_save_path) {
    // Move storage
    QDir().mkpath(new_save_path);
    h.move_storage(new_save_path);
  }
}

void QBtSession::setAppendLabelToSavePath(bool append) {
  if (appendLabelToSavePath != append) {
    appendLabelToSavePath = !appendLabelToSavePath;
    if (appendLabelToSavePath) {
      // Move torrents storage to sub folder with label name
      std::vector<torrent_handle> torrents = s->get_torrents();

      std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
      std::vector<torrent_handle>::iterator torrentITend = torrents.end();
      for ( ; torrentIT != torrentITend; ++torrentIT) {
        QTorrentHandle h = QTorrentHandle(*torrentIT);
        appendLabelToTorrentSavePath(h);
      }
    }
  }
}

void QBtSession::setAppendqBExtension(bool append) {
  if (appendqBExtension != append) {
    appendqBExtension = !appendqBExtension;
    // append or remove .!qB extension for incomplete files
    std::vector<torrent_handle> torrents = s->get_torrents();

    std::vector<torrent_handle>::iterator torrentIT = torrents.begin();
    std::vector<torrent_handle>::iterator torrentITend = torrents.end();
    for ( ; torrentIT != torrentITend; ++torrentIT) {
      QTorrentHandle h = QTorrentHandle(*torrentIT);
      appendqBextensionToTorrent(h, appendqBExtension);
    }
  }
}

// Set the ports range in which is chosen the port the Bittorrent
// session will listen to
void QBtSession::setListeningPort(int port) {
  qDebug() << Q_FUNC_INFO << port;
  Preferences pref;
  std::pair<int,int> ports(port, port);
#if LIBTORRENT_VERSION_MINOR > 15
  libtorrent::error_code ec;
#endif
  const QString iface_name = pref.getNetworkInterface();
  if (iface_name.isEmpty()) {
#if LIBTORRENT_VERSION_MINOR > 15
    s->listen_on(ports, ec);
#else
    s->listen_on(ports);
#endif
    return;
  }
  // Attempt to listen on provided interface
  const QNetworkInterface network_iface = QNetworkInterface::interfaceFromName(iface_name);
  if (!network_iface.isValid()) {
    qDebug("Invalid network interface: %s", qPrintable(iface_name));
    addConsoleMessage(tr("The network interface defined is invalid: %1").arg(iface_name), "red");
    return;
  }
  QString ip;
  qDebug("This network interface has %d IP addresses", network_iface.addressEntries().size());
  foreach (const QNetworkAddressEntry &entry, network_iface.addressEntries()) {
    qDebug("Trying to listen on IP %s (%s)", qPrintable(entry.ip().toString()), qPrintable(iface_name));
#if LIBTORRENT_VERSION_MINOR > 15
    s->listen_on(ports, ec, entry.ip().toString().toAscii().constData());
    if (!ec) {
#else
    if (s->listen_on(ports, entry.ip().toString().toAscii().constData())) {
#endif
      ip = entry.ip().toString();
      break;
    }
  }
  if (s->is_listening()) {
    addConsoleMessage(tr("Listening on IP address %1 on network interface %2...").arg(ip).arg(iface_name));
  } else {
    qDebug("Failed to listen on any of the IP addresses");
    addConsoleMessage(tr("Failed to listen on network interface %1").arg(iface_name), "red");
  }
}

// Set download rate limit
// -1 to disable
void QBtSession::setDownloadRateLimit(long rate) {
  qDebug() << Q_FUNC_INFO << rate;
  Q_ASSERT(rate == -1 || rate >= 0);
#if LIBTORRENT_VERSION_MINOR > 15
  session_settings settings = s->settings();
  settings.download_rate_limit = rate;
  s->set_settings(settings);
#else
  s->set_download_rate_limit(rate);
#endif
}

// Set upload rate limit
// -1 to disable
void QBtSession::setUploadRateLimit(long rate) {
  qDebug() << Q_FUNC_INFO << rate;
  Q_ASSERT(rate == -1 || rate >= 0);
#if LIBTORRENT_VERSION_MINOR > 15
  session_settings settings = s->settings();
  settings.upload_rate_limit = rate;
  s->set_settings(settings);
#else
  s->set_upload_rate_limit(rate);
#endif
}

// Torrents will a ratio superior to the given value will
// be automatically deleted
void QBtSession::setGlobalMaxRatio(qreal ratio) {
  if (ratio < 0) ratio = -1.;
  if (global_ratio_limit != ratio) {
    global_ratio_limit = ratio;
    qDebug("* Set global deleteRatio to %.1f", global_ratio_limit);
    updateRatioTimer();
  }
}

void QBtSession::setMaxRatioPerTorrent(const QString &hash, qreal ratio)
{
  if (ratio < 0)
    ratio = -1;
  if (ratio > MAX_RATIO)
    ratio = MAX_RATIO;
  qDebug("* Set individual max ratio for torrent %s to %.1f.",
         qPrintable(hash), ratio);
  TorrentPersistentData::setRatioLimit(hash, ratio);
  updateRatioTimer();
}

void QBtSession::removeRatioPerTorrent(const QString &hash)
{
  qDebug("* Remove individual max ratio for torrent %s.", qPrintable(hash));
  TorrentPersistentData::setRatioLimit(hash, TorrentPersistentData::USE_GLOBAL_RATIO);
  updateRatioTimer();
}

qreal QBtSession::getMaxRatioPerTorrent(const QString &hash, bool *usesGlobalRatio) const
{
  qreal ratio_limit = TorrentPersistentData::getRatioLimit(hash);
  if (ratio_limit == TorrentPersistentData::USE_GLOBAL_RATIO) {
    ratio_limit = global_ratio_limit;
    *usesGlobalRatio = true;
  } else {
    *usesGlobalRatio = false;
  }
  return ratio_limit;
}

void QBtSession::updateRatioTimer()
{
  if (global_ratio_limit == -1 && !TorrentPersistentData::hasPerTorrentRatioLimit()) {
    if (BigRatioTimer->isActive())
      BigRatioTimer->stop();
  } else if (!BigRatioTimer->isActive()) {
    BigRatioTimer->start();
  }
}

// Set DHT port (>= 1 or 0 if same as BT)
void QBtSession::setDHTPort(int dht_port) {
  if (dht_port >= 0) {
    if (dht_port == current_dht_port) return;
    struct dht_settings DHTSettings;
    DHTSettings.service_port = dht_port;
    s->set_dht_settings(DHTSettings);
    current_dht_port = dht_port;
    qDebug("Set DHT Port to %d", dht_port);
  }
}

// Enable IP Filtering
void QBtSession::enableIPFilter(const QString &filter_path, bool force) {
  qDebug("Enabling IPFiler");
  if (!filterParser) {
    filterParser = new FilterParserThread(this, s);
    connect(filterParser.data(), SIGNAL(IPFilterParsed(int)), SLOT(handleIPFilterParsed(int)));
    connect(filterParser.data(), SIGNAL(IPFilterError()), SLOT(handleIPFilterError()));
  }
  if (filterPath.isEmpty() || filterPath != filter_path || force) {
    filterPath = filter_path;
    filterParser->processFilterFile(filter_path);
  }
}

// Disable IP Filtering
void QBtSession::disableIPFilter() {
  qDebug("Disabling IPFilter");
  s->set_ip_filter(ip_filter());
  if (filterParser) {
    disconnect(filterParser.data(), 0, this, 0);
    delete filterParser;
  }
  filterPath = "";
}

// Set BT session settings (user_agent)
void QBtSession::setSessionSettings(const session_settings &sessionSettings) {
  qDebug("Set session settings");
  s->set_settings(sessionSettings);
}

// Set Proxy
void QBtSession::setProxySettings(proxy_settings proxySettings) {
  qDebug() << Q_FUNC_INFO;

#if LIBTORRENT_VERSION_MINOR > 15
  proxySettings.proxy_peer_connections = Preferences().proxyPeerConnections();
  s->set_proxy(proxySettings);
#else
  s->set_tracker_proxy(proxySettings);
  proxy_settings peer_proxy;
  if (Preferences().proxyPeerConnections())
    peer_proxy = proxySettings;
  s->set_peer_proxy(peer_proxy);
  s->set_web_seed_proxy(peer_proxy);
  s->set_dht_proxy(peer_proxy);
#endif

  // Define environment variable
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
    qputenv("http_proxy", QByteArray());
    qputenv("sock_proxy", QByteArray());
    return;
  }
  // We need this for urllib in search engine plugins
  qDebug("HTTP communications proxy string: %s", qPrintable(proxy_str));
  if (proxySettings.type == proxy_settings::socks5 || proxySettings.type == proxy_settings::socks5_pw)
    qputenv("sock_proxy", proxy_str.toLocal8Bit());
  else
    qputenv("http_proxy", proxy_str.toLocal8Bit());
}

void QBtSession::recursiveTorrentDownload(const QTorrentHandle &h) {
  try {
    for (int i=0; i<h.num_files(); ++i) {
      const QString torrent_relpath = h.filepath_at(i);
      if (torrent_relpath.endsWith(".torrent")) {
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
        QString displayed_relpath = torrent_relpath;
        displayed_relpath.replace("/", "\\");
        addConsoleMessage(tr("Recursive download of file %1 embedded in torrent %2", "Recursive download of test.torrent embedded in torrent test2").arg(displayed_relpath).arg(h.name()));
#else
        addConsoleMessage(tr("Recursive download of file %1 embedded in torrent %2", "Recursive download of test.torrent embedded in torrent test2").arg(torrent_relpath).arg(h.name()));
#endif
        const QString torrent_fullpath = h.save_path()+QDir::separator()+torrent_relpath;

        boost::intrusive_ptr<torrent_info> t = new torrent_info(torrent_fullpath.toUtf8().constData());
        const QString sub_hash = misc::toQString(t->info_hash());
        // Passing the save path along to the sub torrent file
        TorrentTempData::setSavePath(sub_hash, h.save_path());
        addTorrent(torrent_fullpath);
      }
    }
  } catch(std::exception&) {
    qDebug("Caught error loading torrent");
  }
}

void QBtSession::cleanUpAutoRunProcess(int) {
  sender()->deleteLater();
}

void QBtSession::autoRunExternalProgram(const QTorrentHandle &h, bool async) {
  if (!h.is_valid()) return;
  QString program = Preferences().getAutoRunProgram().trimmed();
  if (program.isEmpty()) return;
  // Replace %f by torrent path
  QString torrent_path;
  if (h.num_files() == 1)
    torrent_path = h.firstFileSavePath();
  else
    torrent_path = h.save_path();
  program.replace("%f", torrent_path);
  // Replace %n by torrent name
  program.replace("%n", h.name());
  QProcess *process = new QProcess;
  if (async) {
    connect(process, SIGNAL(finished(int)), this, SLOT(cleanUpAutoRunProcess(int)));
    process->start(program);
  } else {
    process->execute(program);
    delete process;
  }
}

void QBtSession::sendNotificationEmail(const QTorrentHandle &h) {
  // Prepare mail content
  QString content = tr("Torrent name: %1").arg(h.name()) + "\n";
  content += tr("Torrent size: %1").arg(misc::friendlyUnit(h.actual_size())) + "\n";
  content += tr("Save path: %1").arg(TorrentPersistentData::getSavePath(h.hash())) + "\n\n";
  content += tr("The torrent was downloaded in %1.", "The torrent was downloaded in 1 hour and 20 seconds").arg(misc::userFriendlyDuration(h.active_time())) + "\n\n\n";
  content += tr("Thank you for using qBittorrent.") + "\n";
  // Send the notification email
  Smtp *sender = new Smtp(this);
  sender->sendMail("notification@qbittorrent.org", Preferences().getMailNotificationEmail(), tr("[qBittorrent] %1 has finished downloading").arg(h.name()), content);
}

// Read alerts sent by the Bittorrent session
void QBtSession::readAlerts() {
  // look at session alerts and display some infos
  std::auto_ptr<alert> a = s->pop_alert();
  while (a.get()) {
    try {
      if (torrent_finished_alert* p = dynamic_cast<torrent_finished_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          const QString hash = h.hash();
          qDebug("Got a torrent finished alert for %s", qPrintable(h.name()));
          // Remove .!qB extension if necessary
          if (appendqBExtension)
            appendqBextensionToTorrent(h, false);

          const bool was_already_seeded = TorrentPersistentData::isSeed(hash);
          qDebug("Was already seeded: %d", was_already_seeded);
          if (!was_already_seeded) {
            h.save_resume_data();
            qDebug("Checking if the torrent contains torrent files to download");
            // Check if there are torrent files inside
            for (int i=0; i<h.num_files(); ++i) {
              const QString torrent_relpath = h.filepath_at(i).replace("\\", "/");
              qDebug() << "File path:" << torrent_relpath;
              if (torrent_relpath.endsWith(".torrent", Qt::CaseInsensitive)) {
                qDebug("Found possible recursive torrent download.");
                const QString torrent_fullpath = h.save_path()+"/"+torrent_relpath;
                qDebug("Full subtorrent path is %s", qPrintable(torrent_fullpath));
                try {
                  boost::intrusive_ptr<torrent_info> t = new torrent_info(torrent_fullpath.toUtf8().constData());
                  if (t->is_valid()) {
                    qDebug("emitting recursiveTorrentDownloadPossible()");
                    emit recursiveTorrentDownloadPossible(h);
                    break;
                  }
                } catch(std::exception&) {
                  qDebug("Caught error loading torrent");
  #if defined(Q_WS_WIN) || defined(Q_OS_OS2)
                  QString displayed_path = torrent_fullpath;
                  displayed_path.replace("/", "\\");
                  addConsoleMessage(tr("Unable to decode %1 torrent file.").arg(displayed_path), QString::fromUtf8("red"));
  #else
                  addConsoleMessage(tr("Unable to decode %1 torrent file.").arg(torrent_fullpath), QString::fromUtf8("red"));
  #endif
                }
              }
            }
            // Move to download directory if necessary
            if (!defaultTempPath.isEmpty()) {
              // Check if directory is different
              const QDir current_dir(h.save_path());
              const QDir save_dir(getSavePath(hash));
              if (current_dir != save_dir) {
                qDebug("Moving torrent from the temp folder");
                h.move_storage(save_dir.absolutePath());
              }
            }
            // Remember finished state
            qDebug("Saving seed status");
            TorrentPersistentData::saveSeedStatus(h);
            // Recheck if the user asked to
            Preferences pref;
            if (pref.recheckTorrentsOnCompletion()) {
              h.force_recheck();
            }
            qDebug("Emitting finishedTorrent() signal");
            emit finishedTorrent(h);
            qDebug("Received finished alert for %s", qPrintable(h.name()));
  #ifndef DISABLE_GUI
            bool will_shutdown = (pref.shutdownWhenDownloadsComplete() ||
                                  pref.shutdownqBTWhenDownloadsComplete() ||
                                  pref.suspendWhenDownloadsComplete())
                && !hasDownloadingTorrents();
  #else
            bool will_shutdown = false;
  #endif
            // AutoRun program
            if (pref.isAutoRunEnabled())
              autoRunExternalProgram(h, will_shutdown);
            // Move .torrent file to another folder
            if (pref.isFinishedTorrentExportEnabled())
                exportTorrentFile(h, FinishedTorrentExportFolder);
            // Mail notification
            if (pref.isMailNotificationEnabled())
              sendNotificationEmail(h);
  #ifndef DISABLE_GUI
            // Auto-Shutdown
            if (will_shutdown) {
              bool suspend = pref.suspendWhenDownloadsComplete();
              bool shutdown = pref.shutdownWhenDownloadsComplete();
              // Confirm shutdown
              QString confirm_msg;
              if (suspend) {
                confirm_msg = tr("The computer will now go to sleep mode unless you cancel within the next 15 seconds...");
              } else if (shutdown) {
                confirm_msg = tr("The computer will now be switched off unless you cancel within the next 15 seconds...");
              } else {
                confirm_msg = tr("qBittorrent will now exit unless you cancel within the next 15 seconds...");
              }
              if (!ShutdownConfirmDlg::askForConfirmation(confirm_msg))
                return;
              // Actually shut down
              if (suspend || shutdown) {
                qDebug("Preparing for auto-shutdown because all downloads are complete!");
                // Disabling it for next time
                pref.setShutdownWhenDownloadsComplete(false);
                pref.setSuspendWhenDownloadsComplete(false);
                // Make sure preferences are synced before exiting
                if (suspend)
                  m_shutdownAct = SUSPEND_COMPUTER;
                else
                  m_shutdownAct = SHUTDOWN_COMPUTER;
              }
              qDebug("Exiting the application");
              qApp->exit();
              return;
            }
  #endif // DISABLE_GUI
          }
        }
      }
      else if (save_resume_data_alert* p = dynamic_cast<save_resume_data_alert*>(a.get())) {
        const QDir torrentBackup(fsutils::BTBackupLocation());
        const QTorrentHandle h(p->handle);
        if (h.is_valid() && p->resume_data) {
          const QString filepath = torrentBackup.absoluteFilePath(h.hash()+".fastresume");
          QFile resume_file(filepath);
          if (resume_file.exists())
            fsutils::forceRemove(filepath);
          qDebug("Saving fastresume data in %s", qPrintable(filepath));
          vector<char> out;
          bencode(back_inserter(out), *p->resume_data);
          if (!out.empty() && resume_file.open(QIODevice::WriteOnly)) {
            resume_file.write(&out[0], out.size());
            resume_file.close();
          }
        }
      }
      else if (file_renamed_alert* p = dynamic_cast<file_renamed_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          if (h.num_files() > 1) {
            // Check if folders were renamed
            QStringList old_path_parts = h.orig_filepath_at(p->index).split("/");
            old_path_parts.removeLast();
            QString old_path = old_path_parts.join("/");
            QStringList new_path_parts = misc::toQStringU(p->name).split("/");
            new_path_parts.removeLast();
            if (!new_path_parts.isEmpty() && old_path != new_path_parts.join("/")) {
              qDebug("Old_path(%s) != new_path(%s)", qPrintable(old_path), qPrintable(new_path_parts.join("/")));
              old_path = h.save_path()+"/"+old_path;
              qDebug("Detected folder renaming, attempt to delete old folder: %s", qPrintable(old_path));
              QDir().rmpath(old_path);
            }
          } else {
            // Single-file torrent
            // Renaming a file corresponds to changing the save path
            emit savePathChanged(h);
          }
        }
      }
      else if (torrent_deleted_alert* p = dynamic_cast<torrent_deleted_alert*>(a.get())) {
        qDebug("A torrent was deleted from the hard disk, attempting to remove the root folder too...");
        QString hash = misc::toQString(p->info_hash);
        if (!hash.isEmpty()) {
          if (savePathsToRemove.contains(hash)) {
            const QString dirpath = savePathsToRemove.take(hash);
            qDebug() << "Removing save path: " << dirpath << "...";
            bool ok = fsutils::smartRemoveEmptyFolderTree(dirpath);
            Q_UNUSED(ok);
            qDebug() << "Folder was removed: " << ok;
          }
        } else {
          // Fallback
          qDebug() << "hash is empty, use fallback to remove save path";
          foreach (const QString& key, savePathsToRemove.keys()) {
            // Attempt to delete
            if (QDir().rmdir(savePathsToRemove[key])) {
              savePathsToRemove.remove(key);
            }
          }
        }
      }
      else if (storage_moved_alert* p = dynamic_cast<storage_moved_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          // Attempt to remove old folder if empty
          const QString old_save_path = TorrentPersistentData::getPreviousPath(h.hash());
          const QString new_save_path = misc::toQStringU(p->path.c_str());
          qDebug("Torrent moved from %s to %s", qPrintable(old_save_path), qPrintable(new_save_path));
          QDir old_save_dir(old_save_path);
          if (old_save_dir != QDir(defaultSavePath) && old_save_dir != QDir(defaultTempPath)) {
            qDebug("Attempting to remove %s", qPrintable(old_save_path));
            QDir().rmpath(old_save_path);
          }
          if (defaultTempPath.isEmpty() || !new_save_path.startsWith(defaultTempPath)) {
            qDebug("Storage has been moved, updating save path to %s", qPrintable(new_save_path));
            TorrentPersistentData::saveSavePath(h.hash(), new_save_path);
          }
          emit savePathChanged(h);
          //h.force_recheck();
        }
      }
      else if (metadata_received_alert* p = dynamic_cast<metadata_received_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          qDebug("Received metadata for %s", qPrintable(h.hash()));
          // Save metadata
          const QDir torrentBackup(fsutils::BTBackupLocation());
          if (!QFile::exists(torrentBackup.absoluteFilePath(h.hash()+QString(".torrent"))))
            h.save_torrent_file(torrentBackup.absoluteFilePath(h.hash()+QString(".torrent")));
          // Copy the torrent file to the export folder
          if (m_torrentExportEnabled)
            exportTorrentFile(h);
          // Append .!qB to incomplete files
          if (appendqBExtension)
            appendqBextensionToTorrent(h, true);

          emit metadataReceived(h);
          if (h.is_paused()) {
            // XXX: Unfortunately libtorrent-rasterbar does not send a torrent_paused_alert
            // and the torrent can be paused when metadata is received
            emit pausedTorrent(h);
          }

        }
      }
      else if (file_error_alert* p = dynamic_cast<file_error_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          h.pause();
          std::cerr << "File Error: " << p->message().c_str() << std::endl;
          addConsoleMessage(tr("An I/O error occured, '%1' paused.").arg(h.name()));
          addConsoleMessage(tr("Reason: %1").arg(misc::toQString(p->message())));
          if (h.is_valid()) {
            emit fullDiskError(h, misc::toQString(p->message()));
            //h.pause();
            emit pausedTorrent(h);
          }
        }
      }
      else if (file_completed_alert* p = dynamic_cast<file_completed_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        qDebug("A file completed download in torrent %s", qPrintable(h.name()));
        if (appendqBExtension) {
          qDebug("appendqBTExtension is true");
          QString name = h.filepath_at(p->index);
          if (name.endsWith(".!qB")) {
            const QString old_name = name;
            name.chop(4);
            qDebug("Renaming %s to %s", qPrintable(old_name), qPrintable(name));
            h.rename_file(p->index, name);
          }
        }
      }
      else if (torrent_paused_alert* p = dynamic_cast<torrent_paused_alert*>(a.get())) {
        if (p->handle.is_valid()) {
          QTorrentHandle h(p->handle);
          if (!h.has_error())
            h.save_resume_data();
          emit pausedTorrent(h);
        }
      }
      else if (tracker_error_alert* p = dynamic_cast<tracker_error_alert*>(a.get())) {
        // Level: fatal
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          // Authentication
          if (p->status_code != 401) {
            qDebug("Received a tracker error for %s: %s", p->url.c_str(), p->msg.c_str());
            const QString tracker_url = misc::toQString(p->url);
            QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
            TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
            data.last_message = misc::toQString(p->msg);
            trackers_data.insert(tracker_url, data);
            trackersInfos[h.hash()] = trackers_data;
          } else {
            emit trackerAuthenticationRequired(h);
          }
        }
      }
      else if (tracker_reply_alert* p = dynamic_cast<tracker_reply_alert*>(a.get())) {
        const QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          qDebug("Received a tracker reply from %s (Num_peers=%d)", p->url.c_str(), p->num_peers);
          // Connection was successful now. Remove possible old errors
          QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
          const QString tracker_url = misc::toQString(p->url);
          TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
          data.last_message = ""; // Reset error/warning message
          data.num_peers = p->num_peers;
          trackers_data.insert(tracker_url, data);
          trackersInfos[h.hash()] = trackers_data;
        }
      } else if (tracker_warning_alert* p = dynamic_cast<tracker_warning_alert*>(a.get())) {
        const QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          // Connection was successful now but there is a warning message
          QHash<QString, TrackerInfos> trackers_data = trackersInfos.value(h.hash(), QHash<QString, TrackerInfos>());
          const QString tracker_url = misc::toQString(p->url);
          TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
          data.last_message = misc::toQString(p->msg); // Store warning message
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
        boost::system::error_code ec;
        string ip = p->ip.to_string(ec);
        if (!ec) {
          addPeerBanMessage(QString::fromAscii(ip.c_str()), true);
          //emit peerBlocked(QString::fromAscii(ip.c_str()));
        }
      }
      else if (peer_ban_alert* p = dynamic_cast<peer_ban_alert*>(a.get())) {
        boost::system::error_code ec;
        string ip = p->ip.address().to_string(ec);
        if (!ec) {
          addPeerBanMessage(QString::fromAscii(ip.c_str()), false);
          //emit peerBlocked(QString::fromAscii(ip.c_str()));
        }
      }
      else if (fastresume_rejected_alert* p = dynamic_cast<fastresume_rejected_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          qDebug("/!\\ Fast resume failed for %s, reason: %s", qPrintable(h.name()), p->message().c_str());
          if (p->error.value() == 134 && TorrentPersistentData::isSeed(h.hash()) && h.has_missing_files()) {
            const QString hash = h.hash();
            // Mismatching file size (files were probably moved
            addConsoleMessage(tr("File sizes mismatch for torrent %1, pausing it.").arg(h.name()));
            TorrentPersistentData::setErrorState(hash, true);
            pauseTorrent(hash);
          } else {
            addConsoleMessage(tr("Fast resume data was rejected for torrent %1, checking again...").arg(h.name()), QString::fromUtf8("red"));
            addConsoleMessage(tr("Reason: %1").arg(misc::toQString(p->message())));
          }
        }
      }
      else if (url_seed_alert* p = dynamic_cast<url_seed_alert*>(a.get())) {
        addConsoleMessage(tr("Url seed lookup failed for url: %1, message: %2").arg(misc::toQString(p->url)).arg(misc::toQString(p->message())), QString::fromUtf8("red"));
        //emit urlSeedProblem(QString::fromUtf8(p->url.c_str()), QString::fromUtf8(p->msg().c_str()));
      }
      else if (listen_succeeded_alert *p = dynamic_cast<listen_succeeded_alert*>(a.get())) {
        boost::system::error_code ec;
        qDebug() << "Sucessfully listening on" << p->endpoint.address().to_string(ec).c_str() << "/" << p->endpoint.port();
        // Force reannounce on all torrents because some trackers blacklist some ports
        std::vector<torrent_handle> torrents = s->get_torrents();

        std::vector<torrent_handle>::iterator it = torrents.begin();
        std::vector<torrent_handle>::iterator itend = torrents.end();
        for ( ; it != itend; ++it) {
          it->force_reannounce();
        }
        emit listenSucceeded();
      }
      else if (torrent_checked_alert* p = dynamic_cast<torrent_checked_alert*>(a.get())) {
        QTorrentHandle h(p->handle);
        if (h.is_valid()) {
          const QString hash = h.hash();
          qDebug("%s have just finished checking", qPrintable(hash));
          // Save seed status
          TorrentPersistentData::saveSeedStatus(h);
          // Move to temp directory if necessary
          if (!h.is_seed() && !defaultTempPath.isEmpty()) {
            // Check if directory is different
            const QDir current_dir(h.save_path());
            const QDir save_dir(getSavePath(h.hash()));
            if (current_dir == save_dir) {
              qDebug("Moving the torrent to the temp directory...");
              QString torrent_tmp_path = defaultTempPath.replace("\\", "/");
              h.move_storage(torrent_tmp_path);
            }
          }
          emit torrentFinishedChecking(h);
          if (torrentsToPausedAfterChecking.contains(hash)) {
            torrentsToPausedAfterChecking.removeOne(hash);
            h.pause();
            emit pausedTorrent(h);
          }
        }
      }
    } catch (const std::exception& e) {
      qWarning() << "Caught exception in readAlerts(): " << e.what();
    }

    a = s->pop_alert();
  }
}

void QBtSession::recheckTorrent(const QString &hash) {
  QTorrentHandle h = getTorrentHandle(hash);
  if (h.is_valid() && h.has_metadata()) {
    if (h.is_paused()) {
      if (!torrentsToPausedAfterChecking.contains(h.hash())) {
        torrentsToPausedAfterChecking << h.hash();
        h.resume();
      }
    }
    h.force_recheck();
  }
}

QHash<QString, TrackerInfos> QBtSession::getTrackersInfo(const QString &hash) const {
  return trackersInfos.value(hash, QHash<QString, TrackerInfos>());
}

int QBtSession::getListenPort() const {
  qDebug() << Q_FUNC_INFO << s->listen_port();
  return s->listen_port();
}

session_status QBtSession::getSessionStatus() const {
  return s->status();
}

QString QBtSession::getSavePath(const QString &hash, bool fromScanDir, QString filePath) {
  QString savePath;
  if (TorrentTempData::hasTempData(hash)) {
    savePath = TorrentTempData::getSavePath(hash);
    if (savePath.isEmpty()) {
      savePath = defaultSavePath;
    }
    if (appendLabelToSavePath) {
      qDebug("appendLabelToSavePath is true");
      const QString label = TorrentTempData::getLabel(hash);
      if (!label.isEmpty()) {
        savePath = fsutils::updateLabelInSavePath(defaultSavePath, savePath, "", label);
      }
    }
    qDebug("getSavePath, got save_path from temp data: %s", qPrintable(savePath));
  } else {
    savePath = TorrentPersistentData::getSavePath(hash);
    qDebug("SavePath got from persistant data is %s", qPrintable(savePath));
    if (savePath.isEmpty()) {
      if (fromScanDir && m_scanFolders->downloadInTorrentFolder(filePath)) {
        savePath = QFileInfo(filePath).dir().path();
      } else {
        savePath = defaultSavePath;
      }
    }
    if (!fromScanDir && appendLabelToSavePath) {
      const QString label = TorrentPersistentData::getLabel(hash);
      if (!label.isEmpty()) {
        qDebug("Torrent label is %s", qPrintable(label));
        savePath = fsutils::updateLabelInSavePath(defaultSavePath, savePath, "", label);
      }
    }
    qDebug("getSavePath, got save_path from persistent data: %s", qPrintable(savePath));
  }
  // Clean path
  savePath.replace("\\", "/");
  savePath = fsutils::expandPath(savePath);
  if (!savePath.endsWith("/"))
    savePath += "/";
  return savePath;
}

// Take an url string to a torrent file,
// download the torrent file to a tmp location, then
// add it to download list
void QBtSession::downloadFromUrl(const QString &url, const QList<QNetworkCookie>& cookies)
{
  addConsoleMessage(tr("Downloading '%1', please wait...", "e.g: Downloading 'xxx.torrent', please wait...").arg(url)
                  #ifndef DISABLE_GUI
                    , QPalette::WindowText
                  #endif
                    );
  //emit aboutToDownloadFromUrl(url);
  // Launch downloader thread
  downloader->downloadTorrentUrl(url, cookies);
}

void QBtSession::downloadFromURLList(const QStringList& urls) {
  foreach (const QString &url, urls) {
    downloadFromUrl(url);
  }
}

void QBtSession::addMagnetInteractive(const QString& uri)
{
  emit newMagnetLink(uri);
}

void QBtSession::addMagnetSkipAddDlg(const QString& uri, const QString& save_path, const QString& label) {
  if (!save_path.isEmpty() || !label.isEmpty())
    savepathLabel_fromurl[uri] = qMakePair(save_path, label);
  addMagnetUri(uri, false);
}

void QBtSession::downloadUrlAndSkipDialog(QString url, QString save_path, QString label) {
  //emit aboutToDownloadFromUrl(url);
  const QUrl qurl = QUrl::fromEncoded(url.toUtf8());
  if (!save_path.isEmpty() || !label.isEmpty())
    savepathLabel_fromurl[qurl] = qMakePair(save_path, label);
  url_skippingDlg << qurl;
  // Launch downloader thread
  downloader->downloadTorrentUrl(url);
}

// Add to Bittorrent session the downloaded torrent file
void QBtSession::processDownloadedFile(QString url, QString file_path) {
  Preferences pref;
  const int index = url_skippingDlg.indexOf(QUrl::fromEncoded(url.toUtf8()));
  if (index < 0) {
    // Add file to torrent download list
#ifdef Q_WS_WIN
    // Windows hack
    if (!file_path.endsWith(".torrent", Qt::CaseInsensitive)) {
      Q_ASSERT(QFile::exists(file_path));
      qDebug("Torrent name does not end with .torrent, from %s", qPrintable(file_path));
      if (QFile::rename(file_path, file_path+".torrent")) {
        file_path += ".torrent";
      } else {
        qDebug("Failed to rename torrent file!");
      }
    }
    qDebug("Downloading torrent at path: %s", qPrintable(file_path));
#endif
    emit newDownloadedTorrent(file_path, url);
  } else {
    url_skippingDlg.removeAt(index);
    QTorrentHandle h = addTorrent(file_path, false, url, false);
    // Pause torrent if necessary
    if (h.is_valid() && pref.addTorrentsInPause() && Preferences().useAdditionDialog())
        h.pause();
  }
}

// Return current download rate for the BT
// session. Payload means that it only take into
// account "useful" part of the rate
qreal QBtSession::getPayloadDownloadRate() const {
  return s->status().payload_download_rate;
}

// Return current upload rate for the BT
// session. Payload means that it only take into
// account "useful" part of the rate
qreal QBtSession::getPayloadUploadRate() const {
  return s->status().payload_upload_rate;
}

void QBtSession::applyEncryptionSettings(pe_settings se) {
  qDebug("Applying encryption settings");
  s->set_pe_settings(se);
}

// Will fast resume torrents in
// backup directory
void QBtSession::startUpTorrents() {
  qDebug("Resuming unfinished torrents");
  const QDir torrentBackup(fsutils::BTBackupLocation());
  const QStringList known_torrents = TorrentPersistentData::knownTorrents();

  // Safety measure because some people reported torrent loss since
  // we switch the v1.5 way of resuming torrents on startup
  QStringList filters;
  filters << "*.torrent";
  const QStringList torrents_on_hd = torrentBackup.entryList(filters, QDir::Files, QDir::Unsorted);
  foreach (QString hash, torrents_on_hd) {
    hash.chop(8); // remove trailing .torrent
    if (!known_torrents.contains(hash)) {
      qDebug("found torrent with hash: %s on hard disk", qPrintable(hash));
      std::cerr << "ERROR Detected!!! Adding back torrent " << qPrintable(hash) << " which got lost for some reason." << std::endl;
      addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
    }
  }
  // End of safety measure

  qDebug("Starting up torrents");
  if (isQueueingEnabled()) {
    priority_queue<QPair<int, QString>, vector<QPair<int, QString> >, std::greater<QPair<int, QString> > > torrent_queue;
    foreach (const QString &hash, known_torrents) {
      QString filePath;
      if (TorrentPersistentData::isMagnet(hash)) {
        filePath = TorrentPersistentData::getMagnetUri(hash);
      } else {
        filePath = torrentBackup.path()+QDir::separator()+hash+".torrent";
      }
      const int prio = TorrentPersistentData::getPriority(hash);
      torrent_queue.push(qMakePair(prio, hash));
    }
    qDebug("Priority_queue size: %ld", (long)torrent_queue.size());
    // Resume downloads
    while(!torrent_queue.empty()) {
      const QString hash = torrent_queue.top().second;
      torrent_queue.pop();
      qDebug("Starting up torrent %s", qPrintable(hash));
      if (TorrentPersistentData::isMagnet(hash)) {
        addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
      } else {
        addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
      }
    }
  } else {
    // Resume downloads
    foreach (const QString &hash, known_torrents) {
      qDebug("Starting up torrent %s", qPrintable(hash));
      if (TorrentPersistentData::isMagnet(hash))
        addMagnetUri(TorrentPersistentData::getMagnetUri(hash), true);
      else
        addTorrent(torrentBackup.path()+QDir::separator()+hash+".torrent", false, QString(), true);
    }
  }
  QIniSettings settings("qBittorrent", "qBittorrent");
  settings.setValue("ported_to_new_savepath_system", true);
  qDebug("Unfinished torrents resumed");
}

QBtSession * QBtSession::instance()
{
  if (!m_instance) {
    m_instance = new QBtSession;
  }
  return m_instance;
}

void QBtSession::drop()
{
  if (m_instance) {
    delete m_instance;
    m_instance = 0;
  }
}

qlonglong QBtSession::getETA(const QString &hash) const
{
  return m_speedMonitor->getETA(hash);
}

void QBtSession::handleIPFilterParsed(int ruleCount)
{
  addConsoleMessage(tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
  emit ipFilterParsed(false, ruleCount);
}

void QBtSession::handleIPFilterError()
{
  addConsoleMessage(tr("Error: Failed to parse the provided IP filter."), "red");
  emit ipFilterParsed(true, 0);
}

entry QBtSession::generateFilePriorityResumeData(boost::intrusive_ptr<torrent_info> &t, const std::vector<int> &fp)
{
  entry::dictionary_type rd;
  rd["file-format"] = "libtorrent resume file";
  rd["file-version"] = 1;
  rd["libtorrent-version"] = LIBTORRENT_VERSION;
  rd["allocation"] = "full";
  sha1_hash info_hash = t->info_hash();
  rd["info-hash"] = std::string((char*)info_hash.begin(), (char*)info_hash.end());
  // Priorities
  entry::list_type priorities;
  for (uint i=0; i<fp.size(); ++i) {
    priorities.push_back(entry(fp[i]));
  }
  rd["file_priority"] = entry(priorities);
  // files sizes (useless but required)
  entry::list_type sizes;
  for (int i=0; i<t->num_files(); ++i) {
    entry::list_type p;
    p.push_back(entry(0));
    p.push_back(entry(0));
    sizes.push_back(entry(p));
  }
  rd["file sizes"] = entry(sizes);
  // Slots
  entry::list_type tslots;
  for (int i=0; i<t->num_pieces(); ++i) {
    tslots.push_back(-1);
  }
  rd["slots"] = entry(tslots);

  entry::string_type pieces;
  pieces.resize(t->num_pieces());
  std::memset(&pieces[0], 0, pieces.size());
  rd["pieces"] = entry(pieces);

  entry ret(rd);
  Q_ASSERT(ret.type() == entry::dictionary_t);
  return ret;
}
