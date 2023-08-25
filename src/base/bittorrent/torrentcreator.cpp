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

#include "torrentcreator.h"

#include <fstream>

#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QDirIterator>
#include <QFileInfo>
#include <QHash>

#include "addtorrentparams.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/version.h"
#include "lttypecast.h"
#include "session.h"

namespace
{
    // do not include files and folders whose
    // name starts with a .
    bool fileFilter(const std::string &f)
    {
        return !Path(f).filename().startsWith(u'.');
    }

#ifdef QBT_USES_LIBTORRENT2
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

nonstd::expected<BitTorrent::TorrentDescriptor, QString>
TorrentCreatorResult::startSeeding(bool ignoreShareLimits) const
{
    const auto loadResult = content.isEmpty()
                          ? TorrentDescriptor::loadFromFile(path)
                          : TorrentDescriptor::load(content);
    if (!loadResult)
        return loadResult;

    BitTorrent::AddTorrentParams params;
    params.savePath = branchPath;
    params.skipChecking = true;
    if (ignoreShareLimits)
    {
        params.ratioLimit = BitTorrent::Torrent::NO_RATIO_LIMIT;
        params.seedingTimeLimit = BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;
        params.inactiveSeedingTimeLimit = BitTorrent::Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT;
    }
    params.useAutoTMM = false;  // otherwise if it is on by default, it will overwrite `savePath` to the default save path

    Session::instance()->addTorrent(loadResult.value(), params);
    return loadResult;
}

TorrentCreator::TorrentCreator(const TorrentCreatorParams &params, QObject *parent)
    : QObject(parent)
    , m_params {params}
{
}

void TorrentCreator::sendProgressSignal(int currentPieceIdx, int totalPieces)
{
    emit updateProgress(static_cast<int>((currentPieceIdx * 100.) / totalPieces));
}

void TorrentCreator::checkInterruptionRequested() const
{
    if (isInterruptionRequested())
        throw RuntimeError(tr("Operation aborted"));
}

void TorrentCreator::requestInterruption()
{
    m_interruptionRequested.store(true, std::memory_order_relaxed);
}

bool TorrentCreator::isInterruptionRequested() const
{
    return m_interruptionRequested.load(std::memory_order_relaxed);
}

void TorrentCreator::run()
{
    emit updateProgress(0);

    try
    {
        const Path parentPath = m_params.inputPath.parentPath();
        const Utils::Compare::NaturalLessThan<Qt::CaseInsensitive> naturalLessThan {};

        // Adding files to the torrent
        lt::file_storage fs;
        if (QFileInfo(m_params.inputPath.data()).isFile())
        {
            lt::add_files(fs, m_params.inputPath.toString().toStdString(), fileFilter);
        }
        else
        {
            // need to sort the file names by natural sort order
            QStringList dirs = {m_params.inputPath.data()};

            QDirIterator dirIter {m_params.inputPath.data(), (QDir::AllDirs | QDir::NoDotAndDotDot), QDirIterator::Subdirectories};
            while (dirIter.hasNext())
            {
                dirIter.next();
                dirs.append(dirIter.filePath());
            }
            std::sort(dirs.begin(), dirs.end(), naturalLessThan);

            QStringList fileNames;
            QHash<QString, qint64> fileSizeMap;

            for (const QString &dir : asConst(dirs))
            {
                QStringList tmpNames;  // natural sort files within each dir

                QDirIterator fileIter {dir, QDir::Files};
                while (fileIter.hasNext())
                {
                    fileIter.next();

                    const auto relFilePath = parentPath.relativePathOf(Path(fileIter.filePath()));
                    tmpNames.append(relFilePath.toString());
                    fileSizeMap[tmpNames.last()] = fileIter.fileInfo().size();
                }

                std::sort(tmpNames.begin(), tmpNames.end(), naturalLessThan);
                fileNames += tmpNames;
            }

            for (const QString &fileName : asConst(fileNames))
                fs.add_file(fileName.toStdString(), fileSizeMap[fileName]);
        }

        checkInterruptionRequested();

#ifdef QBT_USES_LIBTORRENT2
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

        // calculate the hash for all pieces
        lt::set_piece_hashes(newTorrent, parentPath.toString().toStdString()
            , [this, &newTorrent](const lt::piece_index_t n)
        {
            checkInterruptionRequested();
            sendProgressSignal(LT::toUnderlyingType(n), newTorrent.num_pieces());
        });

        // Set qBittorrent as creator and add user comment to
        // torrent_info structure
        newTorrent.set_creator("qBittorrent " QBT_VERSION);
        newTorrent.set_comment(m_params.comment.toUtf8().constData());
        // Is private ?
        newTorrent.set_priv(m_params.isPrivate);

        checkInterruptionRequested();

        lt::entry entry = newTorrent.generate();

        // add source field
        if (!m_params.source.isEmpty())
            entry["info"]["source"] = m_params.source.toStdString();

        checkInterruptionRequested();

        BitTorrent::TorrentCreatorResult creatorResult;
        creatorResult.path = m_params.savePath;
        creatorResult.branchPath = parentPath;
        creatorResult.pieceSize = newTorrent.piece_length();
        if (!m_params.savePath.isEmpty())
        {
            // create the torrent file
            const nonstd::expected<void, QString> result = Utils::IO::saveToFile(m_params.savePath, entry);
            if (!result)
                throw RuntimeError(result.error());
        }
        else
        {
            lt::bencode(std::back_inserter(creatorResult.content), entry);
        }

        emit updateProgress(100);
        emit creationSuccess(creatorResult);
    }
    catch (const RuntimeError &err)
    {
        emit creationFailure(tr("Create new torrent file failed. Reason: %1.").arg(err.message()));
    }
    catch (const std::exception &err)
    {
        emit creationFailure(tr("Create new torrent file failed. Reason: %1.").arg(QString::fromLocal8Bit(err.what())));
    }
}

#ifdef QBT_USES_LIBTORRENT2
int TorrentCreator::calculateTotalPieces(const Path &inputPath, const int pieceSize, const TorrentFormat torrentFormat)
#else
int TorrentCreator::calculateTotalPieces(const Path &inputPath, const int pieceSize, const bool isAlignmentOptimized, const int paddedFileSizeLimit)
#endif
{
    if (inputPath.isEmpty())
        return 0;

    lt::file_storage fs;
    lt::add_files(fs, inputPath.toString().toStdString(), fileFilter);

#ifdef QBT_USES_LIBTORRENT2
    return lt::create_torrent {fs, pieceSize, toNativeTorrentFormatFlag(torrentFormat)}.num_pieces();
#else
    return lt::create_torrent(fs, pieceSize, paddedFileSizeLimit
        , (isAlignmentOptimized ? lt::create_torrent::optimize_alignment : lt::create_flags_t {})).num_pieces();
#endif
}
