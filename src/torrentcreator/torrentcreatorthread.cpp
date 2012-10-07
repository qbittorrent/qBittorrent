/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <libtorrent/version.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/file.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/file_pool.hpp>
#include <libtorrent/create_torrent.hpp>
#include <QFile>

#include "torrentcreatorthread.h"
#include "fs_utils.h"

#if LIBTORRENT_VERSION_MINOR < 16
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#endif
#include <boost/bind.hpp>
#include <iostream>
#include <fstream>

using namespace libtorrent;
#if LIBTORRENT_VERSION_MINOR < 16
using namespace boost::filesystem;
#endif

// do not include files and folders whose
// name starts with a .
#if LIBTORRENT_VERSION_MINOR >= 16
bool file_filter(std::string const& f)
{
        if (filename(f)[0] == '.') return false;
        return true;
}
#else
bool file_filter(boost::filesystem::path const& filename)
{
  if (filename.leaf()[0] == '.') return false;
  std::cerr << filename << std::endl;
  return true;
}
#endif

void TorrentCreatorThread::create(QString _input_path, QString _save_path, QStringList _trackers, QStringList _url_seeds, QString _comment, bool _is_private, int _piece_size)
{
  input_path = _input_path;
  save_path = _save_path;
  if (QFile(save_path).exists())
    fsutils::forceRemove(save_path);
  trackers = _trackers;
  url_seeds = _url_seeds;
  comment = _comment;
  is_private = _is_private;
  piece_size = _piece_size;
#if LIBTORRENT_VERSION_MINOR < 16
  path::default_name_check(no_check);
#endif
  abort = false;
  start();
}

void sendProgressUpdateSignal(int i, int num, TorrentCreatorThread *parent) {
  parent->sendProgressSignal((int)(i*100./(float)num));
}

void TorrentCreatorThread::sendProgressSignal(int progress) {
  emit updateProgress(progress);
}

void TorrentCreatorThread::run() {
  emit updateProgress(0);
  QString creator_str("qBittorrent "VERSION);
  try {
    file_storage fs;
    // Adding files to the torrent
    libtorrent::add_files(fs, input_path.toUtf8().constData(), file_filter);
    if (abort) return;
    create_torrent t(fs, piece_size);

    // Add url seeds
    foreach (const QString &seed, url_seeds) {
      t.add_url_seed(seed.trimmed().toStdString());
    }
    foreach (const QString &tracker, trackers) {
      t.add_tracker(tracker.trimmed().toStdString());
    }
    if (abort) return;
    // calculate the hash for all pieces
    const QString parent_path = fsutils::branchPath(input_path);
    set_piece_hashes(t, parent_path.toUtf8().constData(), boost::bind<void>(&sendProgressUpdateSignal, _1, t.num_pieces(), this));
    // Set qBittorrent as creator and add user comment to
    // torrent_info structure
    t.set_creator(creator_str.toUtf8().constData());
    t.set_comment(comment.toUtf8().constData());
    // Is private ?
    t.set_priv(is_private);
    if (abort) return;
    // create the torrent and print it to out
    qDebug("Saving to %s", qPrintable(save_path));
    std::ofstream outfile(save_path.toLocal8Bit().constData());
    if (outfile.fail())
      throw std::exception();
    bencode(std::ostream_iterator<char>(outfile), t.generate());
    outfile.close();
    emit updateProgress(100);
    emit creationSuccess(save_path, parent_path);
  } catch (std::exception& e) {
    emit creationFailure(QString::fromLocal8Bit(e.what()));
  }
}
