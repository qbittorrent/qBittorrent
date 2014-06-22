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

#ifdef Q_OS_WIN
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
#if LIBTORRENT_VERSION_NUM < 10000
    name = misc::toQStringU(torrent_handle::name());
#else
    name = misc::toQStringU(status(query_name).name);
#endif
  }
  return name;
}

QString QTorrentHandle::creation_date() const {
#if LIBTORRENT_VERSION_NUM < 10000
  boost::optional<time_t> t = torrent_handle::get_torrent_info().creation_date();
#else
  boost::optional<time_t> t = torrent_handle::torrent_file()->creation_date();
#endif
  return t ? misc::toQString(*t) : "";
}

QString QTorrentHandle::current_tracker() const {
  return misc::toQString(status(0x0).current_tracker);
}

bool QTorrentHandle::is_paused() const {
  return is_paused(status(0x0));
}

bool QTorrentHandle::is_queued() const {
  return is_queued(status(0x0));
}

size_type QTorrentHandle::total_size() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return torrent_handle::get_torrent_info().total_size();
#else
  return torrent_handle::torrent_file()->total_size();
#endif
}

size_type QTorrentHandle::piece_length() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return torrent_handle::get_torrent_info().piece_length();
#else
  return torrent_handle::torrent_file()->piece_length();
#endif
}

int QTorrentHandle::num_pieces() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return torrent_handle::get_torrent_info().num_pieces();
#else
  return torrent_handle::torrent_file()->num_pieces();
#endif
}

bool QTorrentHandle::first_last_piece_first() const {
#if LIBTORRENT_VERSION_NUM < 10000
  torrent_info const* t = &get_torrent_info();
#else
  boost::intrusive_ptr<torrent_info const> t = torrent_file();
#endif

  // Get int first media file
  int index = 0;
  for (index = 0; index < t->num_files(); ++index) {
    QString path = misc::toQStringU(t->file_at(index).path);
    const QString ext = fsutils::fileExtension(path);
    if (misc::isPreviewable(ext) && torrent_handle::file_priority(index) > 0)
      break;
  }

  if (index >= t->num_files()) // No media file
    return false;


  QPair<int, int> extremities = get_file_extremity_pieces(*t, index);

  return (torrent_handle::piece_priority(extremities.first) == 7)
      && (torrent_handle::piece_priority(extremities.second) == 7);
}

QString QTorrentHandle::save_path() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return fsutils::fromNativePath(misc::toQStringU(torrent_handle::save_path()));
#else
  return fsutils::fromNativePath(misc::toQStringU(status(torrent_handle::query_save_path).save_path));
#endif
}

QString QTorrentHandle::save_path_parsed() const {
    QString p;
    if (has_metadata() && num_files() == 1) {
      p = firstFileSavePath();
    } else {
      p = fsutils::fromNativePath(TorrentPersistentData::getSavePath(hash()));
      if (p.isEmpty())
        p = save_path();
    }
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
  return status(query_accurate_download_counters).total_wanted;
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
#if LIBTORRENT_VERSION_NUM < 10000
  return torrent_handle::get_torrent_info().num_files();
#else
  return torrent_handle::torrent_file()->num_files();
#endif
}

QString QTorrentHandle::filename_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM < 10000
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
#else
  Q_ASSERT(index < (unsigned int)torrent_handle::torrent_file()->num_files());
#endif
  return fsutils::fileName(filepath_at(index));
}

size_type QTorrentHandle::filesize_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM < 10000
  Q_ASSERT(index < (unsigned int)torrent_handle::get_torrent_info().num_files());
  return torrent_handle::get_torrent_info().files().file_size(index);
#else
  Q_ASSERT(index < (unsigned int)torrent_handle::torrent_file()->num_files());
  return torrent_handle::torrent_file()->files().file_size(index);
#endif
}

QString QTorrentHandle::filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM < 10000
  return fsutils::fromNativePath(misc::toQStringU(torrent_handle::get_torrent_info().files().file_path(index)));
#else
  return fsutils::fromNativePath(misc::toQStringU(torrent_handle::torrent_file()->files().file_path(index)));
#endif
}

QString QTorrentHandle::orig_filepath_at(unsigned int index) const {
#if LIBTORRENT_VERSION_NUM < 10000
  return fsutils::fromNativePath(misc::toQStringU(torrent_handle::get_torrent_info().orig_files().file_path(index)));
#else
  return fsutils::fromNativePath(misc::toQStringU(torrent_handle::torrent_file()->orig_files().file_path(index)));
#endif

}

torrent_status::state_t QTorrentHandle::state() const {
  return status(0x0).state;
}

QString QTorrentHandle::creator() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return misc::toQStringU(torrent_handle::get_torrent_info().creator());
#else
  return misc::toQStringU(torrent_handle::torrent_file()->creator());
#endif
}

QString QTorrentHandle::comment() const {
#if LIBTORRENT_VERSION_NUM < 10000
  return misc::toQStringU(torrent_handle::get_torrent_info().comment());
#else
  return misc::toQStringU(torrent_handle::torrent_file()->comment());
#endif
}

bool QTorrentHandle::is_checking() const {
  return is_checking(status(0x0));
}

// Return a list of absolute paths corresponding
// to all files in a torrent
QStringList QTorrentHandle::absolute_files_path() const {
  QDir saveDir(save_path());
  QStringList res;
  for (int i = 0; i<num_files(); ++i) {
    res << fsutils::expandPathAbs(saveDir.absoluteFilePath(filepath_at(i)));
  }
  return res;
}

QStringList QTorrentHandle::absolute_files_path_uneeded() const {
  QDir saveDir(save_path());
  QStringList res;
  std::vector<int> fp = torrent_handle::file_priorities();
  for (uint i = 0; i < fp.size(); ++i) {
    if (fp[i] == 0) {
      const QString file_path = fsutils::expandPathAbs(saveDir.absoluteFilePath(filepath_at(i)));
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
  return queue_position(status(0x0));
}

bool QTorrentHandle::is_seed() const {
  // Affected by bug http://code.rasterbar.com/libtorrent/ticket/402
  //return torrent_handle::is_seed();
  // May suffer from approximation problems
  //return (progress() == 1.);
  // This looks safe
  return is_seed(status(0x0));
}

bool QTorrentHandle::is_sequential_download() const {
  return status(0x0).sequential_download;
}

bool QTorrentHandle::priv() const {
  if (!has_metadata())
    return false;
#if LIBTORRENT_VERSION_NUM < 10000
  return torrent_handle::get_torrent_info().priv();
#else
  return torrent_handle::torrent_file()->priv();
#endif
}

QString QTorrentHandle::firstFileSavePath() const {
  Q_ASSERT(has_metadata());
  QString fsave_path = fsutils::fromNativePath(TorrentPersistentData::getSavePath(hash()));
  if (fsave_path.isEmpty())
    fsave_path = save_path();
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
   const int slashIndex = first_filepath.indexOf("/");
  if (slashIndex >= 0)
    return QDir(save_path()).absoluteFilePath(first_filepath.left(slashIndex));
  return save_path();
}

bool QTorrentHandle::has_error() const {
  return has_error(status(0x0));
}

QString QTorrentHandle::error() const {
  return misc::toQString(status(0x0).error);
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
  return status(0x0).has_metadata;
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
  if (QDir(save_path()) == QDir(new_path))
    return;

  TorrentPersistentData::setPreviousSavePath(hash(), save_path());
  // Create destination directory if necessary
  // or move_storage() will fail...
  QDir().mkpath(new_path);
  // Actually move the storage
  torrent_handle::move_storage(fsutils::toNativePath(new_path).toUtf8().constData());
}

bool QTorrentHandle::save_torrent_file(const QString& path) const {
  if (!has_metadata()) return false;

#if LIBTORRENT_VERSION_NUM < 10000
  torrent_info const* t = &get_torrent_info();
#else
  boost::intrusive_ptr<torrent_info const> t = torrent_file();
#endif

  entry meta = bdecode(t->metadata().get(),
                       t->metadata().get() + t->metadata_size());
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
#if LIBTORRENT_VERSION_NUM < 10000
  if ((int)files.size() != torrent_handle::get_torrent_info().num_files()) return;
#else
  if ((int)files.size() != torrent_handle::torrent_file()->num_files()) return;
#endif
  qDebug() << Q_FUNC_INFO;
  bool was_seed = is_seed();
  vector<size_type> progress;
  file_progress(progress);
  qDebug() << Q_FUNC_INFO << "Changing files priorities...";
  torrent_handle::prioritize_files(files);
  qDebug() << Q_FUNC_INFO << "Moving unwanted files to .unwanted folder and conversely...";

  QString spath = save_path();

  for (uint i = 0; i < files.size(); ++i) {
    // Move unwanted files to a .unwanted subfolder
    if (files[i] == 0) {
      QString old_abspath = QDir(spath).absoluteFilePath(filepath_at(i));
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
#ifdef Q_OS_WIN
        qDebug() << "unwanted folder was created:" << created;
        if (created) {
          // Hide the folder on Windows
          qDebug() << "Hiding folder (Windows)";
          wstring win_path =  fsutils::toNativePath(unwanted_abspath).toStdWString();
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
        qDebug() << "Attempting to remove .unwanted folder at " << QDir(spath + "/" + new_relpath).absoluteFilePath(".unwanted");
        QDir(spath + "/" + new_relpath).rmdir(".unwanted");
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
      qDebug() << "tmp folder is enabled, move torrent to " << tmp_path << " from " << spath;
      move_storage(tmp_path);
    }
  }
}

void QTorrentHandle::prioritize_first_last_piece(int file_index, bool b) const {
  // Determine the priority to set
  int prio = b ? 7 : torrent_handle::file_priority(file_index);

#if LIBTORRENT_VERSION_NUM < 10000
  torrent_info const* tf = &get_torrent_info();
#else
  boost::intrusive_ptr<torrent_info const> tf = torrent_file();
#endif

  QPair<int, int> extremities = get_file_extremity_pieces(*tf, file_index);
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
  torrent_handle::rename_file(index, std::string(fsutils::toNativePath(name).toUtf8().constData()));
}

//
// Operators
//

bool QTorrentHandle::operator ==(const QTorrentHandle& new_h) const {
  return info_hash() == new_h.info_hash();
}

bool QTorrentHandle::is_paused(const libtorrent::torrent_status &status) {
  return status.paused && !status.auto_managed;
}

int QTorrentHandle::queue_position(const libtorrent::torrent_status &status) {
  if (status.queue_position < 0)
    return -1;
  return status.queue_position+1;
}

bool QTorrentHandle::is_queued(const libtorrent::torrent_status &status) {
  return status.paused && status.auto_managed;
}

bool QTorrentHandle::is_seed(const libtorrent::torrent_status &status) {
  return status.state == torrent_status::finished
      || status.state == torrent_status::seeding;
}

bool QTorrentHandle::is_checking(const libtorrent::torrent_status &status) {
  return status.state == torrent_status::checking_files
      || status.state == torrent_status::checking_resume_data;
}

bool QTorrentHandle::has_error(const libtorrent::torrent_status &status) {
  return status.paused && !status.error.empty();
}

float QTorrentHandle::progress(const libtorrent::torrent_status &status) {
  if (!status.total_wanted)
    return 0.;
  if (status.total_wanted_done == status.total_wanted)
    return 1.;
  float progress = (float) status.total_wanted_done / (float) status.total_wanted;
  Q_ASSERT(progress >= 0.f && progress <= 1.f);
  return progress;
}
