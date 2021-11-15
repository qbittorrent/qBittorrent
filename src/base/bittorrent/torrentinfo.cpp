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

#include "torrentinfo.h"

#include <libtorrent/create_torrent.hpp>
#include <libtorrent/error_code.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include "base/global.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/misc.h"
#include "infohash.h"
#include "trackerentry.h"

using namespace BitTorrent;

const int torrentInfoId = qRegisterMetaType<TorrentInfo>();

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

TorrentInfo::TorrentInfo(const TorrentInfo &other)
    : m_nativeInfo {other.m_nativeInfo}
    , m_nativeIndexes {other.m_nativeIndexes}
{
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

nonstd::expected<TorrentInfo, QString> TorrentInfo::load(const QByteArray &data) noexcept
{
    // 2-step construction to overcome default limits of `depth_limit` & `token_limit` which are
    // used in `torrent_info()` constructor
    const int depthLimit = 100;
    const int tokenLimit = 10000000;

    lt::error_code ec;
    const lt::bdecode_node node = lt::bdecode(data, ec
        , nullptr, depthLimit, tokenLimit);
    if (ec)
        return nonstd::make_unexpected(QString::fromStdString(ec.message()));

    const lt::torrent_info nativeInfo {node, ec};
    if (ec)
        return nonstd::make_unexpected(QString::fromStdString(ec.message()));

    return TorrentInfo(nativeInfo);
}

nonstd::expected<TorrentInfo, QString> TorrentInfo::loadFromFile(const Path &path) noexcept
{
    QFile file {path.data()};
    if (!file.open(QIODevice::ReadOnly))
        return nonstd::make_unexpected(file.errorString());

    if (file.size() > MAX_TORRENT_SIZE)
        return nonstd::make_unexpected(tr("File size exceeds max limit %1").arg(Utils::Misc::friendlyUnit(MAX_TORRENT_SIZE)));

    QByteArray data;
    try
    {
        data = file.readAll();
    }
    catch (const std::bad_alloc &e)
    {
        return nonstd::make_unexpected(tr("Torrent file read error: %1").arg(e.what()));
    }

    if (data.size() != file.size())
        return nonstd::make_unexpected(tr("Torrent file read error: size mismatch"));

    file.close();

    return load(data);
}

nonstd::expected<void, QString> TorrentInfo::saveToFile(const Path &path) const
{
    if (!isValid())
        return nonstd::make_unexpected(tr("Invalid metadata"));

    try
    {
        const auto torrentCreator = lt::create_torrent(*m_nativeInfo);
        const lt::entry torrentEntry = torrentCreator.generate();
        const nonstd::expected<void, QString> result = Utils::IO::saveToFile(path, torrentEntry);
        if (!result)
            return result.get_unexpected();
    }
    catch (const lt::system_error &err)
    {
        return nonstd::make_unexpected(QString::fromLocal8Bit(err.what()));
    }

    return {};
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

QDateTime TorrentInfo::creationDate() const
{
    if (!isValid()) return {};

    const std::time_t date = m_nativeInfo->creation_date();
    return ((date != 0) ? QDateTime::fromSecsSinceEpoch(date) : QDateTime());
}

QString TorrentInfo::creator() const
{
    if (!isValid()) return {};

    return QString::fromStdString(m_nativeInfo->creator());
}

QString TorrentInfo::comment() const
{
    if (!isValid()) return {};

    return QString::fromStdString(m_nativeInfo->comment());
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

    return m_nativeInfo->orig_files().file_size(m_nativeIndexes[index]);
}

qlonglong TorrentInfo::fileOffset(const int index) const
{
    if (!isValid()) return -1;

    return m_nativeInfo->orig_files().file_offset(m_nativeIndexes[index]);
}

QVector<TrackerEntry> TorrentInfo::trackers() const
{
    if (!isValid()) return {};

    const std::vector<lt::announce_entry> trackers = m_nativeInfo->trackers();

    QVector<TrackerEntry> ret;
    ret.reserve(static_cast<decltype(ret)::size_type>(trackers.size()));

    for (const lt::announce_entry &tracker : trackers)
        ret.append({QString::fromStdString(tracker.url)});

    return ret;
}

QVector<QUrl> TorrentInfo::urlSeeds() const
{
    if (!isValid()) return {};

    const std::vector<lt::web_seed_entry> &nativeWebSeeds = m_nativeInfo->web_seeds();

    QVector<QUrl> urlSeeds;
    urlSeeds.reserve(static_cast<decltype(urlSeeds)::size_type>(nativeWebSeeds.size()));

    for (const lt::web_seed_entry &webSeed : nativeWebSeeds)
    {
        if (webSeed.type == lt::web_seed_entry::url_seed)
            urlSeeds.append(QUrl(webSeed.url.c_str()));
    }

    return urlSeeds;
}

QByteArray TorrentInfo::metadata() const
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
    const QVector<int> fileIndices = fileIndicesForPiece(pieceIndex);

    PathList res;
    res.reserve(fileIndices.size());
    std::transform(fileIndices.begin(), fileIndices.end(), std::back_inserter(res)
                   , [this](int i) { return filePath(i); });

    return res;
}

QVector<int> TorrentInfo::fileIndicesForPiece(const int pieceIndex) const
{
    if (!isValid() || (pieceIndex < 0) || (pieceIndex >= piecesCount()))
        return {};

    const std::vector<lt::file_slice> files = m_nativeInfo->map_block(
                lt::piece_index_t {pieceIndex}, 0, m_nativeInfo->piece_size(lt::piece_index_t {pieceIndex}));
    QVector<int> res;
    res.reserve(static_cast<decltype(res)::size_type>(files.size()));
    for (const lt::file_slice &fileSlice : files)
    {
        const int index = m_nativeIndexes.indexOf(fileSlice.file_index);
        if (index >= 0)
            res.append(index);
    }

    return res;
}

QVector<QByteArray> TorrentInfo::pieceHashes() const
{
    if (!isValid())
        return {};

    const int count = piecesCount();
    QVector<QByteArray> hashes;
    hashes.reserve(count);

    for (int i = 0; i < count; ++i)
        hashes += {m_nativeInfo->hash_for_piece_ptr(lt::piece_index_t {i}), SHA1Hash::length()};

    return hashes;
}

TorrentInfo::PieceRange TorrentInfo::filePieces(const Path &filePath) const
{
    if (!isValid()) // if we do not check here the debug message will be printed, which would be not correct
        return {};

    const int index = fileIndex(filePath);
    if (index == -1)
    {
        qDebug() << "Filename" << filePath.toString() << "was not found in torrent" << name();
        return {};
    }
    return filePieces(index);
}

TorrentInfo::PieceRange TorrentInfo::filePieces(const int fileIndex) const
{
    if (!isValid())
        return {};

    if ((fileIndex < 0) || (fileIndex >= filesCount()))
    {
        qDebug() << "File index (" << fileIndex << ") is out of range for torrent" << name();
        return {};
    }

    const lt::file_storage &files = m_nativeInfo->orig_files();
    const auto fileSize = files.file_size(m_nativeIndexes[fileIndex]);
    const auto fileOffset = files.file_offset(m_nativeIndexes[fileIndex]);

    const int beginIdx = (fileOffset / pieceLength());
    const int endIdx = ((fileOffset + fileSize - 1) / pieceLength());

    if (fileSize <= 0)
        return {beginIdx, 0};
    return makeInterval(beginIdx, endIdx);
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

TorrentContentLayout TorrentInfo::contentLayout() const
{
    if (!isValid())
        return TorrentContentLayout::Original;

    return detectContentLayout(filePaths());
}

std::shared_ptr<lt::torrent_info> TorrentInfo::nativeInfo() const
{
    if (!isValid())
        return nullptr;

    return std::make_shared<lt::torrent_info>(*m_nativeInfo);
}

QVector<lt::file_index_t> TorrentInfo::nativeIndexes() const
{
    return m_nativeIndexes;
}
