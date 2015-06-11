/*
 * Bittorrent Client using Qt and libtorrent.
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

#include <boost/bind.hpp>
#include <iostream>
#include <fstream>

#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "core/utils/string.h"
#include "torrentcreatorthread.h"

namespace libt = libtorrent;
using namespace BitTorrent;

// do not include files and folders whose
// name starts with a .
bool fileFilter(const std::string &f)
{
    return (libt::filename(f)[0] != '.');
}

TorrentCreatorThread::TorrentCreatorThread(QObject *parent)
    : QThread(parent)
{
}

TorrentCreatorThread::~TorrentCreatorThread()
{
    m_abort = true;
    wait();
}

void TorrentCreatorThread::create(const QString &inputPath, const QString &savePath, const QStringList &trackers,
                                  const QStringList &urlSeeds, const QString &comment, bool isPrivate, int pieceSize)
{
    m_inputPath = Utils::Fs::fromNativePath(inputPath);
    m_savePath = Utils::Fs::fromNativePath(savePath);
    if (QFile(m_savePath).exists())
        Utils::Fs::forceRemove(m_savePath);
    m_trackers = trackers;
    m_urlSeeds = urlSeeds;
    m_comment = comment;
    m_private = isPrivate;
    m_pieceSize = pieceSize;
    m_abort = false;

    start();
}

void TorrentCreatorThread::sendProgressSignal(int numHashes, int numPieces)
{
    emit updateProgress(static_cast<int>((numHashes * 100.) / numPieces));
}

void TorrentCreatorThread::abortCreation()
{
    m_abort = true;
}

void TorrentCreatorThread::run()
{
    emit updateProgress(0);

    QString creator_str("qBittorrent " VERSION);
    try {
        libt::file_storage fs;
        // Adding files to the torrent
        libt::add_files(fs, Utils::String::toStdString(Utils::Fs::toNativePath(m_inputPath)), fileFilter);
        if (m_abort) return;

        libt::create_torrent t(fs, m_pieceSize);

        // Add url seeds
        foreach (const QString &seed, m_urlSeeds)
            t.add_url_seed(Utils::String::toStdString(seed.trimmed()));

        int tier = 0;
        bool newline = false;
        foreach (const QString &tracker, m_trackers) {
            if (tracker.isEmpty()) {
                if (newline)
                    continue;
                ++tier;
                newline = true;
                continue;
            }
            t.add_tracker(Utils::String::toStdString(tracker.trimmed()), tier);
            newline = false;
        }
        if (m_abort) return;

        // calculate the hash for all pieces
        const QString parentPath = Utils::Fs::branchPath(m_inputPath) + "/";
        libt::set_piece_hashes(t, Utils::String::toStdString(Utils::Fs::toNativePath(parentPath)), boost::bind(&TorrentCreatorThread::sendProgressSignal, this, _1, t.num_pieces()));
        // Set qBittorrent as creator and add user comment to
        // torrent_info structure
        t.set_creator(creator_str.toUtf8().constData());
        t.set_comment(m_comment.toUtf8().constData());
        // Is private ?
        t.set_priv(m_private);
        if (m_abort) return;

        // create the torrent and print it to out
        qDebug("Saving to %s", qPrintable(m_savePath));
#ifdef _MSC_VER
        wchar_t *savePathW = new wchar_t[m_savePath.length() + 1];
        int len = Utils::Fs::toNativePath(m_savePath).toWCharArray(savePathW);
        savePathW[len] = L'\0';
        std::ofstream outfile(savePathW, std::ios_base::out | std::ios_base::binary);
        delete[] savePathW;
#else
        std::ofstream outfile(Utils::Fs::toNativePath(m_savePath).toLocal8Bit().constData(), std::ios_base::out | std::ios_base::binary);
#endif
        if (outfile.fail())
            throw std::exception();

        libt::bencode(std::ostream_iterator<char>(outfile), t.generate());
        outfile.close();

        emit updateProgress(100);
        emit creationSuccess(m_savePath, parentPath);
    }
    catch (std::exception& e) {
        emit creationFailure(Utils::String::fromStdString(e.what()));
    }
}
