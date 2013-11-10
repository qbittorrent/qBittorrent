/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012, Christophe Dumez
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

#include "btjson.h"
#include "jsondict.h"
#include "jsonlist.h"
#include "misc.h"
#include "fs_utils.h"
#include "qbtsession.h"
#include "torrentpersistentdata.h"

#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
#include <QElapsedTimer>
#endif

using namespace libtorrent;

#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)

#define CACHED_VARIABLE(VARTYPE, VAR, DUR) \
  static VARTYPE VAR; \
  static QElapsedTimer cacheTimer; \
  static bool initialized = false; \
  if (initialized && !cacheTimer.hasExpired(DUR)) \
    return VAR.toString(); \
  initialized = true; \
  cacheTimer.start(); \
  VAR.clear()

#define CACHED_VARIABLE_FOR_HASH(VARTYPE, VAR, DUR, HASH) \
  static VARTYPE VAR; \
  static QString prev_hash; \
  static QElapsedTimer cacheTimer; \
  if (prev_hash == HASH && !cacheTimer.hasExpired(DUR)) \
    return VAR.toString(); \
  prev_hash = HASH; \
  cacheTimer.start(); \
  VAR.clear()

#else
// We don't support caching for Qt < 4.7 at the moment
#define CACHED_VARIABLE(VARTYPE, VAR, DUR) \
  VARTYPE VAR

#define CACHED_VARIABLE_FOR_HASH(VARTYPE, VAR, DUR, HASH) \
  VARTYPE VAR

#endif

// Numerical constants
static const int CACHE_DURATION_MS = 1500; // 1500ms

// Torrent keys
static const char KEY_TORRENT_HASH[] = "hash";
static const char KEY_TORRENT_NAME[] = "name";
static const char KEY_TORRENT_SIZE[] = "size";
static const char KEY_TORRENT_PROGRESS[] = "progress";
static const char KEY_TORRENT_DLSPEED[] = "dlspeed";
static const char KEY_TORRENT_UPSPEED[] = "upspeed";
static const char KEY_TORRENT_PRIORITY[] = "priority";
static const char KEY_TORRENT_SEEDS[] = "num_seeds";
static const char KEY_TORRENT_LEECHS[] = "num_leechs";
static const char KEY_TORRENT_RATIO[] = "ratio";
static const char KEY_TORRENT_ETA[] = "eta";
static const char KEY_TORRENT_STATE[] = "state";

// Tracker keys
static const char KEY_TRACKER_URL[] = "url";
static const char KEY_TRACKER_STATUS[] = "status";
static const char KEY_TRACKER_MSG[] = "msg";
static const char KEY_TRACKER_PEERS[] = "num_peers";

// Torrent keys (Properties)
static const char KEY_PROP_SAVE_PATH[] = "save_path";
static const char KEY_PROP_CREATION_DATE[] = "creation_date";
static const char KEY_PROP_PIECE_SIZE[] = "piece_size";
static const char KEY_PROP_COMMENT[] = "comment";
static const char KEY_PROP_WASTED[] = "total_wasted";
static const char KEY_PROP_UPLOADED[] = "total_uploaded";
static const char KEY_PROP_DOWNLOADED[] = "total_downloaded";
static const char KEY_PROP_UP_LIMIT[] = "up_limit";
static const char KEY_PROP_DL_LIMIT[] = "dl_limit";
static const char KEY_PROP_TIME_ELAPSED[] = "time_elapsed";
static const char KEY_PROP_CONNECT_COUNT[] = "nb_connections";
static const char KEY_PROP_RATIO[] = "share_ratio";

// File keys
static const char KEY_FILE_NAME[] = "name";
static const char KEY_FILE_SIZE[] = "size";
static const char KEY_FILE_PROGRESS[] = "progress";
static const char KEY_FILE_PRIORITY[] = "priority";
static const char KEY_FILE_IS_SEED[] = "is_seed";

// TransferInfo keys
static const char KEY_TRANSFER_DLSPEED[] = "dl_info";
static const char KEY_TRANSFER_UPSPEED[] = "up_info";

static JsonDict toJson(const QTorrentHandle& h)
{
  JsonDict ret;
  ret.add(KEY_TORRENT_HASH, h.hash());
  ret.add(KEY_TORRENT_NAME, h.name());
  ret.add(KEY_TORRENT_SIZE, misc::friendlyUnit(h.actual_size())); // FIXME: Should pass as Number, not formatted String (for sorting).
  ret.add(KEY_TORRENT_PROGRESS, (double)h.progress());
  ret.add(KEY_TORRENT_DLSPEED, misc::friendlyUnit(h.download_payload_rate(), true)); // FIXME: Should be passed as a Number
  ret.add(KEY_TORRENT_UPSPEED, misc::friendlyUnit(h.upload_payload_rate(), true)); // FIXME: Should be passed as a Number
  if (QBtSession::instance()->isQueueingEnabled() && h.queue_position() >= 0)
    ret.add(KEY_TORRENT_PRIORITY, QString::number(h.queue_position()));
  else
    ret.add(KEY_TORRENT_PRIORITY, "*");
  QString seeds = QString::number(h.num_seeds());
  if (h.num_complete() > 0)
    seeds += " ("+QString::number(h.num_complete())+")";
  ret.add(KEY_TORRENT_SEEDS, seeds);
  QString leechs = QString::number(h.num_peers() - h.num_seeds());
  if (h.num_incomplete() > 0)
    leechs += " ("+QString::number(h.num_incomplete())+")";
  ret.add(KEY_TORRENT_LEECHS, leechs);
  const qreal ratio = QBtSession::instance()->getRealRatio(h.hash());
  ret.add(KEY_TORRENT_RATIO, (ratio > 100.) ? QString::fromUtf8("∞") : misc::accurateDoubleToString(ratio, 1));
  QString eta;
  QString state;
  if (h.is_paused()) {
    if (h.has_error())
      state = "error";
    else
      state = h.is_seed() ? "pausedUP" : "pausedDL";
  } else {
    if (QBtSession::instance()->isQueueingEnabled() && h.is_queued())
      state = h.is_seed() ? "queuedUP" : "queuedDL";
    else {
      switch (h.state()) {
      case torrent_status::finished:
      case torrent_status::seeding:
        state = h.upload_payload_rate() > 0 ? "uploading" : "stalledUP";
        break;
      case torrent_status::allocating:
      case torrent_status::checking_files:
      case torrent_status::queued_for_checking:
      case torrent_status::checking_resume_data:
        state = h.is_seed() ? "checkingUP" : "checkingDL";
        break;
      case torrent_status::downloading:
      case torrent_status::downloading_metadata:
        state = h.download_payload_rate() > 0 ? "downloading" : "stalledDL";
        eta = misc::userFriendlyDuration(QBtSession::instance()->getETA(h.hash()));
        break;
      default:
        qWarning("Unrecognized torrent status, should not happen!!! status was %d", h.state());
      }
    }
  }
  ret.add(KEY_TORRENT_ETA, eta.isEmpty() ? QString::fromUtf8("∞") : eta);
  ret.add(KEY_TORRENT_STATE, state);

  return ret;
}

/**
 * Returns all the torrents in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "hash": Torrent hash
 *   - "name": Torrent name
 *   - "size": Torrent size
 *   - "progress: Torrent progress
 *   - "dlspeed": Torrent download speed
 *   - "upspeed": Torrent upload speed
 *   - "priority": Torrent priority ('*' if queuing is disabled)
 *   - "num_seeds": Torrent seed count
 *   - "num_leechs": Torrent leecher count
 *   - "ratio": Torrent share ratio
 *   - "eta": Torrent ETA
 *   - "state": Torrent state
 */
QString btjson::getTorrents()
{
  CACHED_VARIABLE(JsonList, torrent_list, CACHE_DURATION_MS);
  std::vector<torrent_handle> torrents = QBtSession::instance()->getTorrents();
  std::vector<torrent_handle>::const_iterator it = torrents.begin();
  std::vector<torrent_handle>::const_iterator end = torrents.end();
  for( ; it != end; ++it) {
    torrent_list.append(toJson(QTorrentHandle(*it)));
  }
  return torrent_list.toString();
}

/**
 * Returns the trackers for a torrent in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "url": Tracker URL
 *   - "status": Tracker status
 *   - "num_peers": Tracker peer count
 *   - "msg": Tracker message (last)
 */
QString btjson::getTrackersForTorrent(const QString& hash)
{
  CACHED_VARIABLE_FOR_HASH(JsonList, tracker_list, CACHE_DURATION_MS, hash);
  try {
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    QHash<QString, TrackerInfos> trackers_data = QBtSession::instance()->getTrackersInfo(hash);
    std::vector<announce_entry> vect_trackers = h.trackers();
    std::vector<announce_entry>::const_iterator it = vect_trackers.begin();
    std::vector<announce_entry>::const_iterator end = vect_trackers.end();
    for (; it != end; ++it) {
      JsonDict tracker_dict;
      const QString tracker_url = misc::toQString(it->url);
      tracker_dict.add(KEY_TRACKER_URL, tracker_url);
      const TrackerInfos data = trackers_data.value(tracker_url, TrackerInfos(tracker_url));
      QString status;
      if (it->verified)
        status = tr("Working");
      else {
        if (it->updating && it->fails == 0)
          status = tr("Updating...");
        else
          status = it->fails > 0 ? tr("Not working") : tr("Not contacted yet");
      }
      tracker_dict.add(KEY_TRACKER_STATUS, status);
      tracker_dict.add(KEY_TRACKER_PEERS, QString::number(trackers_data.value(tracker_url, TrackerInfos(tracker_url)).num_peers));
      tracker_dict.add(KEY_TRACKER_MSG, data.last_message.trimmed());

      tracker_list.append(tracker_dict);
    }
  } catch(const std::exception& e) {
    qWarning() << Q_FUNC_INFO << "Invalid torrent: " << e.what();
    return QString();
  }

  return tracker_list.toString();
}

/**
 * Returns the properties for a torrent in JSON format.
 *
 * The return value is a JSON-formatted dictionary.
 * The dictionary keys are:
 *   - "save_path": Torrent save path
 *   - "creation_date": Torrent creation date
 *   - "piece_size": Torrent piece size
 *   - "comment": Torrent comment
 *   - "total_wasted": Total data wasted for torrent
 *   - "total_uploaded": Total data uploaded for torrent
 *   - "total_downloaded": Total data uploaded for torrent
 *   - "up_limit": Torrent upload limit
 *   - "dl_limit": Torrent download limit
 *   - "time_elapsed": Torrent elapsed time
 *   - "nb_connections": Torrent connection count
 *   - "share_ratio": Torrent share ratio
 */
QString btjson::getPropertiesForTorrent(const QString& hash)
{
  CACHED_VARIABLE_FOR_HASH(JsonDict, data, CACHE_DURATION_MS, hash);
  try {
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);

    if (!h.has_metadata())
      return QString();

    // Save path
    QString save_path = fsutils::toNativePath(TorrentPersistentData::getSavePath(hash));
    if (save_path.isEmpty())
      save_path = fsutils::toNativePath(h.save_path());
    data.add(KEY_PROP_SAVE_PATH, save_path);
    data.add(KEY_PROP_CREATION_DATE, h.creation_date());
    data.add(KEY_PROP_PIECE_SIZE, misc::friendlyUnit(h.piece_length()));
    data.add(KEY_PROP_COMMENT, h.comment());
    data.add(KEY_PROP_WASTED, misc::friendlyUnit(h.total_failed_bytes() + h.total_redundant_bytes()));
    data.add(KEY_PROP_UPLOADED, QString(misc::friendlyUnit(h.all_time_upload()) + " (" + misc::friendlyUnit(h.total_payload_upload()) + " " + tr("this session") + ")"));
    data.add(KEY_PROP_DOWNLOADED, QString(misc::friendlyUnit(h.all_time_download()) + " (" + misc::friendlyUnit(h.total_payload_download()) + " " + tr("this session") + ")"));
    data.add(KEY_PROP_UP_LIMIT, h.upload_limit() <= 0 ? QString::fromUtf8("∞") : misc::friendlyUnit(h.upload_limit(), true));
    data.add(KEY_PROP_DL_LIMIT, h.download_limit() <= 0 ? QString::fromUtf8("∞") : misc::friendlyUnit(h.download_limit(), true));
    QString elapsed_txt = misc::userFriendlyDuration(h.active_time());
    if (h.is_seed())
      elapsed_txt += " ("+tr("Seeded for %1", "e.g. Seeded for 3m10s").arg(misc::userFriendlyDuration(h.seeding_time()))+")";
    data.add(KEY_PROP_TIME_ELAPSED, elapsed_txt);
    data.add(KEY_PROP_CONNECT_COUNT, QString(QString::number(h.num_connections()) + " (" + tr("%1 max", "e.g. 10 max").arg(QString::number(h.connections_limit())) + ")"));
    const qreal ratio = QBtSession::instance()->getRealRatio(h.hash());
    data.add(KEY_PROP_RATIO, ratio > 100. ? QString::fromUtf8("∞") : misc::accurateDoubleToString(ratio, 1));
  } catch(const std::exception& e) {
    qWarning() << Q_FUNC_INFO << "Invalid torrent: " << e.what();
    return QString();
  }

  return data.toString();
}

/**
 * Returns the files in a torrent in JSON format.
 *
 * The return value is a JSON-formatted list of dictionaries.
 * The dictionary keys are:
 *   - "name": File name
 *   - "size": File size
 *   - "progress": File progress
 *   - "priority": File priority
 *   - "is_seed": Flag indicating if torrent is seeding/complete
 */
QString btjson::getFilesForTorrent(const QString& hash)
{
  CACHED_VARIABLE_FOR_HASH(JsonList, file_list, CACHE_DURATION_MS, hash);
  try {
    QTorrentHandle h = QBtSession::instance()->getTorrentHandle(hash);
    if (!h.has_metadata())
      return QString();

    const std::vector<int> priorities = h.file_priorities();
    std::vector<size_type> fp;
    h.file_progress(fp);
    for (int i = 0; i < h.num_files(); ++i) {
      JsonDict file_dict;
      QString fileName = h.filename_at(i);
      if (fileName.endsWith(".!qB", Qt::CaseInsensitive))
        fileName.chop(4);
      file_dict.add(KEY_FILE_NAME, fsutils::toNativePath(fileName));
      const size_type size = h.filesize_at(i);
      file_dict.add(KEY_FILE_SIZE, misc::friendlyUnit(size));
      file_dict.add(KEY_FILE_PROGRESS, (size > 0) ? (fp[i] / (double) size) : 1.);
      file_dict.add(KEY_FILE_PRIORITY, priorities[i]);
      if (i == 0)
        file_dict.add(KEY_FILE_IS_SEED, h.is_seed());

      file_list.append(file_dict);
    }
  } catch (const std::exception& e) {
    qWarning() << Q_FUNC_INFO << "Invalid torrent: " << e.what();
    return QString();
  }

  return file_list.toString();
}

/**
 * Returns the global transfer information in JSON format.
 *
 * The return value is a JSON-formatted dictionary.
 * The dictionary keys are:
 *   - "dl_info": Global download info
 *   - "up_info": Global upload info
 */
QString btjson::getTransferInfo()
{
  CACHED_VARIABLE(JsonDict, info, CACHE_DURATION_MS);
  session_status sessionStatus = QBtSession::instance()->getSessionStatus();
  info.add(KEY_TRANSFER_DLSPEED, tr("D: %1/s - T: %2", "Download speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_download_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_download)));
  info.add(KEY_TRANSFER_UPSPEED, tr("U: %1/s - T: %2", "Upload speed: x KiB/s - Transferred: x MiB").arg(misc::friendlyUnit(sessionStatus.payload_upload_rate)).arg(misc::friendlyUnit(sessionStatus.total_payload_upload)));
  return info.toString();
}
