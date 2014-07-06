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

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <math.h>
#include "fs_utils.h"
#include "misc.h"
#include "preferences.h"
#include "qtorrenthandle.h"
#include "torrentpersistentdata.h"
#include <libtorrent/version.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#if LIBTORRENT_VERSION_NUM < 1600
#include <boost/date_time/posix_time/posix_time_types.hpp>
#endif

#ifdef Q_WS_WIN
#include <Windows.h>
#endif

using namespace libtorrent;
using namespace std;

static QPair<int, int> get_file_extremity_pieces(const torrent_info& t, int file_index)
{
  const int num_pieces = t.num_pieces();
  const int piece_size = t.piece_length();
  const file_entry& file = t.file_at(file_index);

  // Determine the first and last piece of the file
  int first_piece = floor((file.offset + 1) / (float) piece_size);
  Q_ASSERT(first_piece >= 0 && first_piece < num_pieces);
  qDebug("First piece of the file is %d/%d", first_piece, num_pieces - 1);

  int num_pieces_in_file = ceil(file.size / (float) piece_size);
  int last_piece = first_piece + num_pieces_in_file - 1;
  Q_ASSERT(last_piece >= 0 && last_piece < num_pieces);
  qDebug("last piece of the file is %d/%d", last_piece, num_pieces - 1);

  return qMakePair(first_piece, last_piece);
}

QTorrentHandle::QTorrentHandle(const torrent_handle& h): torrent_handle(h) {}

//
// Getters
//

QString QTorrentHandle::hash() const {
  return misc::toQString(torrent_handle::info_hash());
}

QString QTorrentHandle::name() const {
  QString name = TorrentPersistentData::getName(hash());
  if (name.isEmpty()) {
    name = misc::toQStringU(torrent_handle::name());
  }
  return name;
}

QString QTorrentHandle::creation_date() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  boost::optional<time_t> t = torrent_handle::get_torrent_info().creation_date();
  return t ? misc::toQString(*t) : "";
#else
  boost::optional<boost::posix_time::ptime> boostDate = torrent_handle::get_torrent_info().creation_date();
  return boostDate ? misc::toQString(*boostDate) : "";
#endif
}

QString QTorrentHandle::next_announce() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::userFriendlyDuration(torrent_handle::status(0x0).next_announce.total_seconds());
#else
  return misc::userFriendlyDuration(torrent_handle::status().next_announce.total_seconds());
#endif
}

qlonglong QTorrentHandle::next_announce_s() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).next_announce.total_seconds();
#else
  return torrent_handle::status().next_announce.total_seconds();
#endif
}

float QTorrentHandle::progress() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status st = torrent_handle::status(query_accurate_download_counters);
#else
  torrent_status st = torrent_handle::status();
#endif
  if (!st.total_wanted)
    return 0.;
  if (st.total_wanted_done == st.total_wanted)
    return 1.;
  float progress = (float) st.total_wanted_done / (float) st.total_wanted;
  Q_ASSERT(progress >= 0. && progress <= 1.);
  return progress;
}

bitfield QTorrentHandle::pieces() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(torrent_handle::query_pieces).pieces;
#else
  return torrent_handle::status().pieces;
#endif
}

QString QTorrentHandle::current_tracker() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::toQString(torrent_handle::status(0x0).current_tracker);
#else
  return misc::toQString(torrent_handle::status().current_tracker);
#endif
}

bool QTorrentHandle::is_paused() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status st = torrent_handle::status(0x0);
  return st.paused && !st.auto_managed;
#else
  return torrent_handle::is_paused() && !torrent_handle::is_auto_managed();
#endif
}

bool QTorrentHandle::is_queued() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status st = torrent_handle::status(0x0);
  return st.paused && st.auto_managed;
#else
  return torrent_handle::is_paused() && torrent_handle::is_auto_managed();
#endif
}

size_type QTorrentHandle::total_size() const {
  return torrent_handle::get_torrent_info().total_size();
}

size_type QTorrentHandle::piece_length() const {
  return torrent_handle::get_torrent_info().piece_length();
}

int QTorrentHandle::num_pieces() const {
  return torrent_handle::get_torrent_info().num_pieces();
}

bool QTorrentHandle::first_last_piece_first() const {
  const torrent_info& t = get_torrent_info();

  // Get int first media file
  int index = 0;
  for (index = 0; index < t.num_files(); ++index) {
#if LIBTORRENT_VERSION_NUM >= 1600
    QString path = misc::toQStringU(t.file_at(index).path);
#else
    QString path = misc::toQStringU(t.file_at(index).path.string());
#endif
    const QString ext = fsutils::fileExtension(path);
    if (misc::isPreviewable(ext) && torrent_handle::file_priority(index) > 0)
      break;
  }

  if (index >= t.num_files()) // No media file
    return false;


  QPair<int, int> extremities = get_file_extremity_pieces (t, index);

  return (torrent_handle::piece_priority(extremities.first) == 7)
      && (torrent_handle::piece_priority(extremities.second) == 7);
}

size_type QTorrentHandle::total_wanted_done() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(query_accurate_download_counters).total_wanted_done;
#else
  return torrent_handle::status().total_wanted_done;
#endif
}

size_type QTorrentHandle::total_wanted() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).total_wanted;
#else
  return torrent_handle::status().total_wanted;
#endif
}

qreal QTorrentHandle::download_payload_rate() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).download_payload_rate;
#else
  return torrent_handle::status().download_payload_rate;
#endif
}

qreal QTorrentHandle::upload_payload_rate() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).upload_payload_rate;
#else
  return torrent_handle::status().upload_payload_rate;
#endif
}

int QTorrentHandle::num_peers() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_peers;
#else
  return torrent_handle::status().num_peers;
#endif
}

int QTorrentHandle::num_seeds() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_seeds;
#else
  return torrent_handle::status().num_seeds;
#endif
}

int QTorrentHandle::num_complete() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_complete;
#else
  return torrent_handle::status().num_complete;
#endif
}

int QTorrentHandle::num_incomplete() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_incomplete;
#else
  return torrent_handle::status().num_incomplete;
#endif
}

QString QTorrentHandle::save_path() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::toQStringU(torrent_handle::save_path()).replace("\\", "/");
#else
  return misc::toQStringU(torrent_handle::save_path().string()).replace("\\", "/");
#endif
}

QString QTorrentHandle::save_path_parsed() const {
    QString p;
    if (has_metadata() && num_files() == 1) {
      p = firstFileSavePath();
    } else {
      p = TorrentPersistentData::getSavePath(hash());
      if (p.isEmpty())
        p = save_path();
    }
#if defined(Q_WS_WIN) || defined(Q_OS_OS2)
    p.replace("/", "\\");
#endif
    return p;
}

QStringList QTorrentHandle::url_seeds() const {
  QStringList res;
  try {
    const std::set<std::string> existing_seeds = torrent_handle::url_seeds();

    std::set<std::string>::const_iterator it = existing_seeds.begin();
    std::set<std::string>::const_iterator itend = existing_seeds.end();
    for ( ; it != itend; ++it) {
      qDebug("URL Seed: %s", it->c_str());
      res << misc::toQString(*it);
    }
  } catch(std::exception &e) {
    std::cout << "ERROR: Failed to convert the URL seed" << std::endl;
  }
  return res;
}

// get the size of the torrent without the filtered files
size_type QTorrentHandle::actual_size() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(query_accurate_download_counters).total_wanted;
#else
  return torrent_handle::status().total_wanted;
#endif
}

bool QTorrentHandle::has_filtered_pieces() const {
  const std::vector<int> piece_priorities = torrent_handle::piece_priorities();
  foreach (const int priority, piece_priorities) {
    if (priority == 0)
      return true;
  }
  return false;
}

int QTorrentHandle::num_files() const {
  return torrent_handle::get_torrent_info().num_files();
}

QString QTorrentHandle::filename_at(unsigned int index) const {
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
#if LIBTORRENT_VERSION_NUM >= 1600
  return fsutils::fileName(filepath_at(index));
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path.leaf());
#endif
}

size_type QTorrentHandle::filesize_at(unsigned int index) const {
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
  return torrent_handle::get_torrent_info().file_at(index).size;
}

QString QTorrentHandle::filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path);
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().file_at(index).path.string());
#endif
}

QString QTorrentHandle::orig_filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::toQStringU(torrent_handle::get_torrent_info().orig_files().at(index).path);
#else
  return misc::toQStringU(torrent_handle::get_torrent_info().orig_files().at(index).path.string());
#endif
}

torrent_status::state_t QTorrentHandle::state() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).state;
#else
  return torrent_handle::status().state;
#endif
}

QString QTorrentHandle::creator() const {
  return misc::toQStringU(torrent_handle::get_torrent_info().creator());
}

QString QTorrentHandle::comment() const {
  return misc::toQStringU(torrent_handle::get_torrent_info().comment());
}

size_type QTorrentHandle::total_failed_bytes() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).total_failed_bytes;
#else
  return torrent_handle::status().total_failed_bytes;
#endif
}

size_type QTorrentHandle::total_redundant_bytes() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).total_redundant_bytes;
#else
  return torrent_handle::status().total_redundant_bytes;
#endif
}

bool QTorrentHandle::is_checking() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status st = torrent_handle::status(0x0);
#else
  torrent_status st = torrent_handle::status();
#endif
  return st.state == torrent_status::checking_files || st.state == torrent_status::checking_resume_data;
}

size_type QTorrentHandle::total_done() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(query_accurate_download_counters).total_done;
#else
  return torrent_handle::status().total_done;
#endif
}

size_type QTorrentHandle::all_time_download() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).all_time_download;
#else
  return torrent_handle::status().all_time_download;
#endif
}

size_type QTorrentHandle::all_time_upload() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).all_time_upload;
#else
  return torrent_handle::status().all_time_upload;
#endif
}

size_type QTorrentHandle::total_payload_download() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).total_payload_download;
#else
  return torrent_handle::status().total_payload_download;
#endif
}

size_type QTorrentHandle::total_payload_upload() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).total_payload_upload;
#else
  return torrent_handle::status().total_payload_upload;
#endif
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::absolute_files_path() const {
  QDir saveDir(save_path());
  QStringList res;
  for (int i = 0; i<num_files(); ++i) {
    res << QDir::cleanPath(saveDir.absoluteFilePath(filepath_at(i)));
  }
  return res;
}

QStringList QTorrentHandle::absolute_files_path_uneeded() const {
  QDir saveDir(save_path());
  QStringList res;
  std::vector<int> fp = torrent_handle::file_priorities();
  for (uint i = 0; i < fp.size(); ++i) {
    if (fp[i] == 0) {
      const QString file_path = QDir::cleanPath(saveDir.absoluteFilePath(filepath_at(i)));
      if (file_path.contains(".unwanted"))
        res << file_path;
    }
  }
  return res;
}

bool QTorrentHandle::has_missing_files() const {
  const QStringList paths = absolute_files_path();
  foreach (const QString &path, paths) {
    if (!QFile::exists(path)) return true;
  }
  return false;
}

int QTorrentHandle::queue_position() const {
  if (torrent_handle::queue_position() < 0)
    return -1;
  return torrent_handle::queue_position()+1;
}

int QTorrentHandle::num_uploads() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_uploads;
#else
  return torrent_handle::status().num_uploads;
#endif
}

bool QTorrentHandle::is_seed() const {
  // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
  //return torrent_handle::is_seed();
  // May suffer from approximation problems
  //return (progress() == 1.);
  // This looks safe
  torrent_status::state_t st = state();
  return (st == torrent_status::finished || st == torrent_status::seeding);
}

bool QTorrentHandle::is_auto_managed() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status status = torrent_handle::status(0x0);
  return status.auto_managed;
#else
  return torrent_handle::is_auto_managed();
#endif
}

bool QTorrentHandle::is_sequential_download() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status status = torrent_handle::status(0x0);
  return status.sequential_download;
#else
  return torrent_handle::is_sequential_download();
#endif
}

qlonglong QTorrentHandle::active_time() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).active_time;
#else
  return torrent_handle::status().active_time;
#endif
}

qlonglong QTorrentHandle::seeding_time() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).seeding_time;
#else
  return torrent_handle::status().seeding_time;
#endif
}

int QTorrentHandle::num_connections() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).num_connections;
#else
  return torrent_handle::status().num_connections;
#endif
}

int QTorrentHandle::connections_limit() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).connections_limit;
#else
  return torrent_handle::status().connections_limit;
#endif
}

bool QTorrentHandle::priv() const {
  if (!has_metadata())
    return false;
  return torrent_handle::get_torrent_info().priv();
}

QString QTorrentHandle::firstFileSavePath() const {
  Q_ASSERT(has_metadata());
  QString fsave_path = TorrentPersistentData::getSavePath(hash());
  if (fsave_path.isEmpty())
    fsave_path = save_path();
  fsave_path.replace("\\", "/");
  if (!fsave_path.endsWith("/"))
    fsave_path += "/";
  fsave_path += filepath_at(0);
  // Remove .!qB extension
  if (fsave_path.endsWith(".!qB", Qt::CaseInsensitive))
    fsave_path.chop(4);
  return fsave_path;
}

QString QTorrentHandle::root_path() const
{
  if (num_files() < 2)
    return save_path();
   QString first_filepath = filepath_at(0);
   const int slashIndex = first_filepath.indexOf(QRegExp("[/\\\\]"));
   if (slashIndex >= 0)
     return QDir(save_path()).absoluteFilePath(first_filepath.left(slashIndex));
   return save_path();
}

bool QTorrentHandle::has_error() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  torrent_status st = torrent_handle::status(0x0);
  return st.paused && !st.error.empty();
#else
  return torrent_handle::is_paused() && !torrent_handle::status().error.empty();
#endif
}

QString QTorrentHandle::error() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return misc::toQString(torrent_handle::status(0x0).error);
#else
  return misc::toQString(torrent_handle::status().error);
#endif
}

void QTorrentHandle::downloading_pieces(bitfield &bf) const {
  std::vector<partial_piece_info> queue;
  torrent_handle::get_download_queue(queue);

  std::vector<partial_piece_info>::const_iterator it = queue.begin();
  std::vector<partial_piece_info>::const_iterator itend = queue.end();
  for ( ; it!= itend; ++it) {
    bf.set_bit(it->piece_index);
  }
  return;
}

bool QTorrentHandle::has_metadata() const {
#if LIBTORRENT_VERSION_NUM >= 1600
  return torrent_handle::status(0x0).has_metadata;
#else
  return torrent_handle::has_metadata();
#endif
}

float QTorrentHandle::distributed_copies() const {
#if LIBTORRENT_VERSION_NUM >= 1600
return torrent_handle::status(query_distributed_copies).distributed_copies;
#else
  return torrent_handle::status().distributed_copies;
#endif
}

void QTorrentHandle::file_progress(std::vector<size_type>& fp) const {
  torrent_handle::file_progress(fp, torrent_handle::piece_granularity);
}

//
// Setters
//

void QTorrentHandle::pause() const {
  torrent_handle::auto_managed(false);
  torrent_handle::pause();
  torrent_handle::save_resume_data();
}

void QTorrentHandle::resume() const {
  if (has_error())
    torrent_handle::clear_error();

  const QString torrent_hash = hash();
  bool has_persistant_error = TorrentPersistentData::hasError(torrent_hash);
  TorrentPersistentData::setErrorState(torrent_hash, false);
  bool temp_path_enabled = Preferences().isTempPathEnabled();
  if (has_persistant_error && temp_path_enabled) {
    // Torrent was supposed to be seeding, checking again in final destination
    qDebug("Resuming a torrent with error...");
    const QString final_save_path = TorrentPersistentData::getSavePath(torrent_hash);
    qDebug("Torrent final path is: %s", qPrintable(final_save_path));
    if (!final_save_path.isEmpty())
      move_storage(final_save_path);
  }
  torrent_handle::auto_managed(true);
  torrent_handle::resume();
  if (has_persistant_error && temp_path_enabled) {
    // Force recheck
    torrent_handle::force_recheck();
  }
}

void QTorrentHandle::remove_url_seed(const QString& seed) const {
  torrent_handle::remove_url_seed(seed.toStdString());
}

void QTorrentHandle::add_url_seed(const QString& seed) const {
  const std::string str_seed = seed.toStdString();
  qDebug("calling torrent_handle::add_url_seed(%s)", str_seed.c_str());
  torrent_handle::add_url_seed(str_seed);
}

void QTorrentHandle::set_tracker_login(const QString& username, const QString& password) const {
  torrent_handle::set_tracker_login(std::string(username.toLocal8Bit().constData()), std::string(password.toLocal8Bit().constData()));
}

void QTorrentHandle::move_storage(const QString& new_path) const {
  QString hashstr = hash();

  if (TorrentTempData::isMoveInProgress(hashstr)) {
    qDebug("enqueue move storage to %s", qPrintable(new_path));
    TorrentTempData::enqueueMove(hashstr, new_path);
  }
  else {
    QString old_path = save_path();
    qDebug("move storage: %s to %s", qPrintable(old_path), qPrintable(new_path));
    if (QDir(old_path) == QDir(new_path))
      return;

    TorrentTempData::startMove(hashstr, old_path, new_path);

    // Create destination directory if necessary
    // or move_storage() will fail...
    QDir().mkpath(new_path);
    // Actually move the storage
    torrent_handle::move_storage(new_path.toUtf8().constData());
  }
}

bool QTorrentHandle::save_torrent_file(const QString& path) const {
  if (!has_metadata()) return false;

  const torrent_info& t = torrent_handle::get_torrent_info();

  entry meta = bdecode(t.metadata().get(),
                       t.metadata().get() + t.metadata_size());
  entry torrent_entry(entry::dictionary_t);
  torrent_entry["info"] = meta;
  if (!torrent_handle::trackers().empty())
    torrent_entry["announce"] = torrent_handle::trackers().front().url;

  vector<char> out;
  bencode(back_inserter(out), torrent_entry);
  QFile torrent_file(path);
  if (!out.empty() && torrent_file.open(QIODevice::WriteOnly)) {
    torrent_file.write(&out[0], out.size());
    torrent_file.close();
    return true;
  }

  return false;
}

void QTorrentHandle::file_priority(int index, int priority) const {
  vector<int> priorities = torrent_handle::file_priorities();
  if (priorities[index] != priority) {
    priorities[index] = priority;
    prioritize_files(priorities);
  }
}

void QTorrentHandle::prioritize_files(const vector<int> &files) const {
  if ((int)files.size() != torrent_handle::get_torrent_info().num_files()) return;
  qDebug() << Q_FUNC_INFO;
  bool was_seed = is_seed();
  vector<size_type> progress;
  file_progress(progress);
  qDebug() << Q_FUNC_INFO << "Changing files priorities...";
  torrent_handle::prioritize_files(files);
  qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";
  for (uint i = 0; i < files.size(); ++i) {
    // Move unwanted files to a .unwanted subfolder
    if (files[i] == 0) {
      QString old_abspath = QDir(save_path()).absoluteFilePath(filepath_at(i));
      QString parent_abspath = fsutils::branchPath(old_abspath);
      // Make sure the file does not already exists
      if (QDir(parent_abspath).dirName() != ".unwanted") {
        QString unwanted_abspath = parent_abspath+"/.unwanted";
        QString new_abspath = unwanted_abspath+"/"+filename_at(i);
        qDebug() << "Unwanted path is" << unwanted_abspath;
        if (QFile::exists(new_abspath)) {
          qWarning() << "File" << new_abspath << "already exists at destination.";
          continue;
        }
        bool created = QDir().mkpath(unwanted_abspath);
#ifdef Q_WS_WIN
        qDebug() << "unwanted folder was created:" << created;
        if (created) {
          // Hide the folder on Windows
          qDebug() << "Hiding folder (Windows)";
          wstring win_path =  unwanted_abspath.replace("/","\\").toStdWString();
          DWORD dwAttrs = GetFileAttributesW(win_path.c_str());
          bool ret = SetFileAttributesW(win_path.c_str(), dwAttrs|FILE_ATTRIBUTE_HIDDEN);
          Q_ASSERT(ret != 0); Q_UNUSED(ret);
        }
#else
        Q_UNUSED(created);
#endif
        QString parent_path = fsutils::branchPath(filepath_at(i));
        if (!parent_path.isEmpty() && !parent_path.endsWith("/"))
          parent_path += "/";
        rename_file(i, parent_path+".unwanted/"+filename_at(i));
      }
    }
    // Move wanted files back to their original folder
    if (files[i] > 0) {
      QString parent_relpath = fsutils::branchPath(filepath_at(i));
      if (QDir(parent_relpath).dirName() == ".unwanted") {
        QString old_name = filename_at(i);
        QString new_relpath = fsutils::branchPath(parent_relpath);
        if (new_relpath.isEmpty())
            rename_file(i, old_name);
        else
            rename_file(i, QDir(new_relpath).filePath(old_name));
        // Remove .unwanted directory if empty
        qDebug() << "Attempting to remove .unwanted folder at " << QDir(save_path() + QDir::separator() + new_relpath).absoluteFilePath(".unwanted");
        QDir(save_path() + QDir::separator() + new_relpath).rmdir(".unwanted");
      }
    }
  }

  if (was_seed && !is_seed()) {
    qDebug() << "Torrent is no longer SEEDING";
    // Save seed status
    TorrentPersistentData::saveSeedStatus(*this);
    // Move to temp folder if necessary
    const Preferences pref;
    if (pref.isTempPathEnabled()) {
      QString tmp_path = pref.getTempPath();
      qDebug() << "tmp folder is enabled, move torrent to " << tmp_path << " from " << save_path();
      move_storage(tmp_path);
    }
  }
}

void QTorrentHandle::prioritize_first_last_piece(int file_index, bool b) const {
  // Determine the priority to set
  int prio = b ? 7 : torrent_handle::file_priority(file_index);

  QPair<int, int> extremities = get_file_extremity_pieces (get_torrent_info(), file_index);
  piece_priority(extremities.first, prio);
  piece_priority(extremities.second, prio);
}

void QTorrentHandle::prioritize_first_last_piece(bool b) const {
  if (!has_metadata()) return;
  // Download first and last pieces first for all media files in the torrent
  const uint nbfiles = num_files();
  for (uint index = 0; index < nbfiles; ++index) {
    const QString path = filepath_at(index);
    const QString ext = fsutils::fileExtension(path);
    if (misc::isPreviewable(ext) && torrent_handle::file_priority(index) > 0) {
      qDebug() << "File" << path << "is previewable, toggle downloading of first/last pieces first";
      prioritize_first_last_piece(index, b);
    }
  }
}

void QTorrentHandle::rename_file(int index, const QString& name) const {
  qDebug() << Q_FUNC_INFO << index << name;
  torrent_handle::rename_file(index, std::string(name.toUtf8().constData()));
}

//
// Operators
//

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const {
  return info_hash() == new_h.info_hash();
}
