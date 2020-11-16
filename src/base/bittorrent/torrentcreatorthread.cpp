/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcreatorthread.h"

#include <fstream>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include "base/exceptions.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "ltunderlyingtype.h"

namespace
{
    // do not include files and folders whose
    // name starts with a .
    bool fileFilter(const std::string &f)
    {
        return !Utils::Fs::fileName(QString::fromStdString(f)).startsWith('.');
    }

#if (LIBTORRENT_VERSION_NUM >= 20000)
    lt::create_flags_t toNativeTorrentFormatFlag(const BitTorrent::TorrentFormat torrentFormat)
    {
        switch (torrentFormat)
        {
        case BitTorrent::TorrentFormat::V1:
            return lt::create_torrent::v1_only;
        case BitTorrent::TorrentFormat::Hybrid:
            return {};
        case BitTorrent::TorrentFormat::V2:
            return lt::create_torrent::v2_only;
        }
        return {};
    }
#endif
}

using namespace BitTorrent;

TorrentCreatorThread::TorrentCreatorThread(QObject *parent)
    : QThread(parent)
{
}

TorrentCreatorThread::~TorrentCreatorThread()
{
    requestInterruption();
    wait();
}

void TorrentCreatorThread::create(const TorrentCreatorParams &params)
{
    m_params = params;
    start();
}

void TorrentCreatorThread::sendProgressSignal(int currentPieceIdx, int totalPieces)
{
    emit updateProgress(static_cast<int>((currentPieceIdx * 100.) / totalPieces));
}

void TorrentCreatorThread::run()
{
    const QString creatorStr("qBittorrent " QBT_VERSION);

    emit updateProgress(0);

    try
    {
        const QString parentPath = Utils::Fs::branchPath(m_params.inputPath) + '/';

        // Adding files to the torrent
        lt::file_storage fs;
        if (QFileInfo(m_params.inputPath).isFile())
        {
            lt::add_files(fs, Utils::Fs::toNativePath(m_params.inputPath).toStdString(), fileFilter);
        }
        else
        {
            // need to sort the file names by natural sort order
            QStringList dirs = {m_params.inputPath};

            QDirIterator dirIter(m_params.inputPath, (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories);
            while (dirIter.hasNext())
            {
                dirIter.next();
                dirs += dirIter.filePath();
            }
            std::sort(dirs.begin(), dirs.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);

            QStringList fileNames;
            QHash<QString, qint64> fileSizeMap;

            for (const auto &dir : asConst(dirs))
            {
                QStringList tmpNames;  // natural sort files within each dir

                QDirIterator fileIter(dir, QDir::Files);
                while (fileIter.hasNext())
                {
                    fileIter.next();

                    const QString relFilePath = fileIter.filePath().mid(parentPath.length());
                    tmpNames += relFilePath;
                    fileSizeMap[relFilePath] = fileIter.fileInfo().size();
                }

                std::sort(tmpNames.begin(), tmpNames.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);
                fileNames += tmpNames;
            }

            for (const auto &fileName : asConst(fileNames))
                fs.add_file(fileName.toStdString(), fileSizeMap[fileName]);
        }

        if (isInterruptionRequested()) return;

#if (LIBTORRENT_VERSION_NUM >= 20000)
        lt::create_torrent newTorrent {fs, m_params.pieceSize, toNativeTorrentFormatFlag(m_params.torrentFormat)};
#else
        lt::create_torrent newTorrent(fs, m_params.pieceSize, m_params.paddedFileSizeLimit
            , (m_params.isAlignmentOptimized ? lt::create_torrent::optimize_alignment : lt::create_flags_t {}));
#endif

        // Add url seeds
        for (QString seed : asConst(m_params.urlSeeds))
        {
            seed = seed.trimmed();
            if (!seed.isEmpty())
                newTorrent.add_url_seed(seed.toStdString());
        }

        int tier = 0;
        for (const QString &tracker : asConst(m_params.trackers))
        {
            if (tracker.isEmpty())
                ++tier;
            else
                newTorrent.add_tracker(tracker.trimmed().toStdString(), tier);
        }

        if (isInterruptionRequested()) return;

        // calculate the hash for all pieces
        lt::set_piece_hashes(newTorrent, Utils::Fs::toNativePath(parentPath).toStdString()
            , [this, &newTorrent](const lt::piece_index_t n)
        {
            sendProgressSignal(static_cast<LTUnderlyingType<lt::piece_index_t>>(n), newTorrent.num_pieces());
        });
        // Set qBittorrent as creator and add user comment to
        // torrent_info structure
        newTorrent.set_creator(creatorStr.toUtf8().constData());
        newTorrent.set_comment(m_params.comment.toUtf8().constData());
        // Is private ?
        newTorrent.set_priv(m_params.isPrivate);

        if (isInterruptionRequested()) return;

        lt::entry entry = newTorrent.generate();

        // add source field
        if (!m_params.source.isEmpty())
            entry["info"]["source"] = m_params.source.toStdString();

        if (isInterruptionRequested()) return;

        // create the torrent
        QFile outfile {m_params.savePath};
        if (!outfile.open(QIODevice::WriteOnly))
        {
            throw RuntimeError
            {tr("Create new torrent file failed. Reason: %1")
                .arg(outfile.errorString())};
        }

        if (isInterruptionRequested()) return;

        lt::bencode(Utils::IO::FileDeviceOutputIterator {outfile}, entry);
        if (outfile.error() != QFileDevice::NoError)
        {
            throw RuntimeError
            {tr("Create new torrent file failed. Reason: %1")
                .arg(outfile.errorString())};
        }
        outfile.close();

        emit updateProgress(100);
        emit creationSuccess(m_params.savePath, parentPath);
    }
    catch (const std::exception &e)
    {
        emit creationFailure(e.what());
    }
}

#if (LIBTORRENT_VERSION_NUM >= 20000)
int TorrentCreatorThread::calculateTotalPieces(const QString &inputPath, const int pieceSize, const TorrentFormat torrentFormat)
#else
int TorrentCreatorThread::calculateTotalPieces(const QString &inputPath, const int pieceSize, const bool isAlignmentOptimized, const int paddedFileSizeLimit)
#endif
{
    if (inputPath.isEmpty())
        return 0;

    lt::file_storage fs;
    lt::add_files(fs, Utils::Fs::toNativePath(inputPath).toStdString(), fileFilter);

#if (LIBTORRENT_VERSION_NUM >= 20000)
    return lt::create_torrent {fs, pieceSize, toNativeTorrentFormatFlag(torrentFormat)}.num_pieces();
#else
    return lt::create_torrent(fs, pieceSize, paddedFileSizeLimit
        , (isAlignmentOptimized ? lt::create_torrent::optimize_alignment : lt::create_flags_t {})).num_pieces();
#endif
}
