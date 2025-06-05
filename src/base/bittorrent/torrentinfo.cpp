/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentinfo.h"

#include <libtorrent/version.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>

#include "base/global.h"
#include "base/path.h"
#include "infohash.h"
#include "trackerentry.h"

using namespace BitTorrent;

const int TORRENTINFO_TYPEID = qRegisterMetaType<TorrentInfo>();

TorrentInfo::TorrentInfo(const lt::torrent_info &nativeInfo)
    : m_nativeInfo {std::make_shared<const lt::torrent_info>(nativeInfo)}
{
    Q_ASSERT(m_nativeInfo->is_valid() && (m_nativeInfo->num_files() > 0));

    const lt::file_storage &fileStorage = m_nativeInfo->orig_files();
    m_nativeIndexes.reserve(fileStorage.num_files());
    for (const lt::file_index_t nativeIndex : fileStorage.file_range())
    {
        if (!fileStorage.pad_file_at(nativeIndex))
            m_nativeIndexes.append(nativeIndex);
    }
}

TorrentInfo &TorrentInfo::operator=(const TorrentInfo &other)
{
    if (this != &other)
    {
        m_nativeInfo = other.m_nativeInfo;
        m_nativeIndexes = other.m_nativeIndexes;
    }
    return *this;
}

bool TorrentInfo::isValid() const
{
    return (m_nativeInfo != nullptr);
}

InfoHash TorrentInfo::infoHash() const
{
    if (!isValid()) return {};

#ifdef QBT_USES_LIBTORRENT2
    return m_nativeInfo->info_hashes();
#else
    return m_nativeInfo->info_hash();
#endif
}

QString TorrentInfo::name() const
{
    if (!isValid()) return {};

    return QString::fromStdString(m_nativeInfo->orig_files().name());
}

bool TorrentInfo::isPrivate() const
{
    if (!isValid()) return false;

    return m_nativeInfo->priv();
}

qlonglong TorrentInfo::totalSize() const
{
    if (!isValid()) return -1;

    return m_nativeInfo->total_size();
}

int TorrentInfo::filesCount() const
{
    if (!isValid()) return -1;

    return m_nativeIndexes.size();
}

int TorrentInfo::pieceLength() const
{
    if (!isValid()) return -1;

    return m_nativeInfo->piece_length();
}

int TorrentInfo::pieceLength(const int index) const
{
    if (!isValid()) return -1;

    return m_nativeInfo->piece_size(lt::piece_index_t {index});
}

int TorrentInfo::piecesCount() const
{
    if (!isValid()) return -1;

    return m_nativeInfo->num_pieces();
}

Path TorrentInfo::filePath(const int index) const
{
    if (!isValid()) return {};

    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_nativeIndexes.size());
    if ((index < 0) || (index >= m_nativeIndexes.size()))
        return {};

    return Path(m_nativeInfo->orig_files().file_path(m_nativeIndexes[index]));
}

PathList TorrentInfo::filePaths() const
{
    PathList list;
    list.reserve(filesCount());
    for (int i = 0; i < filesCount(); ++i)
        list << filePath(i);

    return list;
}

qlonglong TorrentInfo::fileSize(const int index) const
{
    if (!isValid()) return -1;

    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_nativeIndexes.size());
    if ((index < 0) || (index >= m_nativeIndexes.size()))
        return -1;

    return m_nativeInfo->orig_files().file_size(m_nativeIndexes[index]);
}

qlonglong TorrentInfo::fileOffset(const int index) const
{
    if (!isValid()) return -1;

    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_nativeIndexes.size());
    if ((index < 0) || (index >= m_nativeIndexes.size()))
        return -1;

    return m_nativeInfo->orig_files().file_offset(m_nativeIndexes[index]);
}

QByteArray TorrentInfo::rawData() const
{
    if (!isValid()) return {};
#ifdef QBT_USES_LIBTORRENT2
    const lt::span<const char> infoSection {m_nativeInfo->info_section()};
    return {infoSection.data(), static_cast<int>(infoSection.size())};
#else
    return {m_nativeInfo->metadata().get(), m_nativeInfo->metadata_size()};
#endif
}

PathList TorrentInfo::filesForPiece(const int pieceIndex) const
{
    // no checks here because fileIndicesForPiece() will return an empty list
    const QList<int> fileIndices = fileIndicesForPiece(pieceIndex);

    PathList res;
    res.reserve(fileIndices.size());
    std::transform(fileIndices.begin(), fileIndices.end(), std::back_inserter(res)
                   , [this](int i) { return filePath(i); });

    return res;
}

QList<int> TorrentInfo::fileIndicesForPiece(const int pieceIndex) const
{
    if (!isValid() || (pieceIndex < 0) || (pieceIndex >= piecesCount()))
        return {};

    const std::vector<lt::file_slice> files = m_nativeInfo->map_block(
                lt::piece_index_t {pieceIndex}, 0, m_nativeInfo->piece_size(lt::piece_index_t {pieceIndex}));
    QList<int> res;
    res.reserve(static_cast<decltype(res)::size_type>(files.size()));
    for (const lt::file_slice &fileSlice : files)
    {
        const int index = m_nativeIndexes.indexOf(fileSlice.file_index);
        if (index >= 0)
            res.append(index);
    }

    return res;
}

QList<QByteArray> TorrentInfo::pieceHashes() const
{
    if (!isValid())
        return {};

    const int count = piecesCount();
    QList<QByteArray> hashes;
    hashes.reserve(count);

    for (int i = 0; i < count; ++i)
        hashes += {m_nativeInfo->hash_for_piece_ptr(lt::piece_index_t {i}), SHA1Hash::length()};

    return hashes;
}

TorrentInfo::PieceRange TorrentInfo::filePieces(const Path &filePath) const
{
    return filePieces(fileIndex(filePath));
}

TorrentInfo::PieceRange TorrentInfo::filePieces(const int fileIndex) const
{
    if (!isValid())
        return {};

    if ((fileIndex < 0) || (fileIndex >= filesCount()))
        return {};

    const lt::file_storage &files = m_nativeInfo->orig_files();
    const auto fileSize = files.file_size(m_nativeIndexes[fileIndex]);
    const auto fileOffset = files.file_offset(m_nativeIndexes[fileIndex]);

    const int beginIdx = (fileOffset / pieceLength());
    const int endIdx = ((fileOffset + fileSize - 1) / pieceLength());

    if (fileSize <= 0)
        return {beginIdx, 0};
    return makeInterval(beginIdx, endIdx);
}

bool TorrentInfo::matchesInfoHash(const InfoHash &otherInfoHash) const
{
    if (!isValid())
        return false;

    const InfoHash thisInfoHash = infoHash();

    if (thisInfoHash.v1().isValid() && otherInfoHash.v1().isValid()
            && (thisInfoHash.v1() != otherInfoHash.v1()))
    {
        return false;
    }

    if (thisInfoHash.v2().isValid() && otherInfoHash.v2().isValid()
            && (thisInfoHash.v2() != otherInfoHash.v2()))
    {
        return false;
    }

    if (!thisInfoHash.v1().isValid() && otherInfoHash.v1().isValid())
        return false;

    if (!thisInfoHash.v2().isValid() && otherInfoHash.v2().isValid())
        return false;

    return true;
}

int TorrentInfo::fileIndex(const Path &filePath) const
{
    // the check whether the object is valid is not needed here
    // because if filesCount() returns -1 the loop exits immediately
    for (int i = 0; i < filesCount(); ++i)
    {
        if (filePath == this->filePath(i))
            return i;
    }

    return -1;
}

std::shared_ptr<lt::torrent_info> TorrentInfo::nativeInfo() const
{
    if (!isValid())
        return nullptr;

    return std::make_shared<lt::torrent_info>(*m_nativeInfo);
}

QList<lt::file_index_t> TorrentInfo::nativeIndexes() const
{
    return m_nativeIndexes;
}
