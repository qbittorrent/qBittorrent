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
#include <QDir>

#include "torrentcreatorthread.h"
#include "fs_utils.h"

#include <boost/bind.hpp>
#include <iostream>
#include <fstream>

using namespace libtorrent;

// do not include files and folders whose
// name starts with a .
bool file_filter(std::string const& f)
{
        if (filename(f)[0] == '.') return false;
        return true;
}

void TorrentCreatorThread::create(QString _input_path, QString _save_path, QStringList _trackers, QStringList _url_seeds, QString _comment, bool _is_private, int _piece_size)
{
  input_path = fsutils::fromNativePath(_input_path);
  save_path = fsutils::fromNativePath(_save_path);
  if (QFile(save_path).exists())
    fsutils::forceRemove(save_path);
  trackers = _trackers;
  url_seeds = _url_seeds;
  comment = _comment;
  is_private = _is_private;
  piece_size = _piece_size;
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
    libtorrent::add_files(fs, fsutils::toNativePath(input_path).toUtf8().constData(), file_filter);
    if (abort) return;
    create_torrent t(fs, piece_size);

    // Add url seeds
    foreach (const QString &seed, url_seeds) {
      t.add_url_seed(seed.trimmed().toStdString());
    }
    int tier = 0;
    bool newline = false;
    foreach (const QString &tracker, trackers) {
      if (tracker.isEmpty()) {
        if (newline)
          continue;
        ++tier;
        newline = true;
        continue;
      }
      t.add_tracker(tracker.trimmed().toStdString(), tier);
      newline = false;
    }
    if (abort) return;
    // calculate the hash for all pieces
    const QString parent_path = fsutils::branchPath(input_path) + "/";
    set_piece_hashes(t, fsutils::toNativePath(parent_path).toUtf8().constData(), boost::bind<void>(&sendProgressUpdateSignal, _1, t.num_pieces(), this));
    // Set qBittorrent as creator and add user comment to
    // torrent_info structure
    t.set_creator(creator_str.toUtf8().constData());
    t.set_comment(comment.toUtf8().constData());
    // Is private ?
    t.set_priv(is_private);
    if (abort) return;
    // create the torrent and print it to out
    qDebug("Saving to %s", qPrintable(save_path));
#ifdef _MSC_VER
    wchar_t *wsave_path = new wchar_t[save_path.length()+1];
    int len = fsutils::toNativePath(save_path).toWCharArray(wsave_path);
    wsave_path[len] = '\0';
    std::ofstream outfile(wsave_path, std::ios_base::out|std::ios_base::binary);
    delete[] wsave_path;
#else
    std::ofstream outfile(fsutils::toNativePath(save_path).toLocal8Bit().constData(), std::ios_base::out|std::ios_base::binary);
#endif
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
