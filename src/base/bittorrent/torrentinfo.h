/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <libtorrent/torrent_info.hpp>

#include <QCoreApplication>
#include <QtContainerFwd>

#include "base/3rdparty/expected.hpp"
#include "base/indexrange.h"
#include "base/pathfwd.h"
#include "torrentcontentlayout.h"

class QByteArray;
class QDateTime;
class QString;
class QUrl;

namespace BitTorrent
{
    class InfoHash;
    struct TrackerEntry;

    class TorrentInfo
    {
        Q_DECLARE_TR_FUNCTIONS(TorrentInfo)

    public:
        TorrentInfo() = default;
        TorrentInfo(const TorrentInfo &other);

        explicit TorrentInfo(const lt::torrent_info &nativeInfo);

        static nonstd::expected<TorrentInfo, QString> load(const QByteArray &data) noexcept;
        static nonstd::expected<TorrentInfo, QString> loadFromFile(const Path &path) noexcept;
        nonstd::expected<void, QString> saveToFile(const Path &path) const;

        TorrentInfo &operator=(const TorrentInfo &other);

        bool isValid() const;
        InfoHash infoHash() const;
        QString name() const;
        QDateTime creationDate() const;
        QString creator() const;
        QString comment() const;
        bool isPrivate() const;
        qlonglong totalSize() const;
        int filesCount() const;
        int pieceLength() const;
        int pieceLength(int index) const;
        int piecesCount() const;
        Path filePath(int index) const;
        PathList filePaths() const;
        qlonglong fileSize(int index) const;
        qlonglong fileOffset(int index) const;
        QVector<TrackerEntry> trackers() const;
        QVector<QUrl> urlSeeds() const;
        QByteArray metadata() const;
        PathList filesForPiece(int pieceIndex) const;
        QVector<int> fileIndicesForPiece(int pieceIndex) const;
        QVector<QByteArray> pieceHashes() const;

        using PieceRange = IndexRange<int>;
        // returns pair of the first and the last pieces into which
        // the given file extends (maybe partially).
        PieceRange filePieces(const Path &filePath) const;
        PieceRange filePieces(int fileIndex) const;

        std::shared_ptr<lt::torrent_info> nativeInfo() const;
        QVector<lt::file_index_t> nativeIndexes() const;

    private:
        // returns file index or -1 if fileName is not found
        int fileIndex(const Path &filePath) const;
        TorrentContentLayout contentLayout() const;

        std::shared_ptr<const lt::torrent_info> m_nativeInfo;

        // internal indexes of files (payload only, excluding any .pad files)
        // by which they are addressed in libtorrent
        QVector<lt::file_index_t> m_nativeIndexes;
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentInfo)
