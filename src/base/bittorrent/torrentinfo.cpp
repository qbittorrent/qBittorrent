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

#include <libtorrent/error_code.hpp>

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QUrl>

#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "infohash.h"
#include "trackerentry.h"

namespace libt = libtorrent;
using namespace BitTorrent;

TorrentInfo::TorrentInfo(NativeConstPtr nativeInfo)
{
    m_nativeInfo = boost::const_pointer_cast<libt::torrent_info>(nativeInfo);
}

TorrentInfo::TorrentInfo(const TorrentInfo &other)
    : m_nativeInfo(other.m_nativeInfo)
{
}

TorrentInfo &TorrentInfo::operator=(const TorrentInfo &other)
{
    m_nativeInfo = other.m_nativeInfo;
    return *this;
}

TorrentInfo TorrentInfo::load(const QByteArray &data, QString *error) noexcept
{
    // 2-step construction to overcome default limits of `depth_limit` & `token_limit` which are
    // used in `torrent_info()` constructor
    const int depthLimit = 100;
    const int tokenLimit = 10000000;
    libt::error_code ec;

    libt::bdecode_node node;
    bdecode(data.constData(), (data.constData() + data.size()), node, ec
        , nullptr, depthLimit, tokenLimit);
    if (ec) {
        if (error)
            *error = QString::fromStdString(ec.message());
        return TorrentInfo();
    }

    TorrentInfo info {NativePtr(new libt::torrent_info(node, ec))};
    if (ec) {
        if (error)
            *error = QString::fromStdString(ec.message());
        return TorrentInfo();
    }

    return info;
}

TorrentInfo TorrentInfo::loadFromFile(const QString &path, QString *error) noexcept
{
    if (error)
        error->clear();

    QFile file {path};
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return TorrentInfo();
    }

    const qint64 fileSizeLimit = 100 * 1024 * 1024;  // 100 MB
    if (file.size() > fileSizeLimit) {
        if (error)
            *error = tr("File size exceeds max limit %1").arg(fileSizeLimit);
        return TorrentInfo();
    }

    QByteArray data;
    try {
        data = file.readAll();
    }
    catch (const std::bad_alloc &e) {
        if (error)
            *error = tr("Torrent file read error: %1").arg(e.what());
        return TorrentInfo();
    }
    if (data.size() != file.size()) {
        if (error)
            *error = tr("Torrent file read error: size mismatch");
        return TorrentInfo();
    }

    file.close();

    return load(data, error);
}

bool TorrentInfo::isValid() const
{
    return (m_nativeInfo && m_nativeInfo->is_valid() && (m_nativeInfo->num_files() > 0));
}

InfoHash TorrentInfo::hash() const
{
    if (!isValid()) return InfoHash();
    return m_nativeInfo->info_hash();
}

QString TorrentInfo::name() const
{
    if (!isValid()) return QString();
    return QString::fromStdString(m_nativeInfo->name());
}

QDateTime TorrentInfo::creationDate() const
{
    if (!isValid()) return QDateTime();
    boost::optional<time_t> t = m_nativeInfo->creation_date();
    return t ? QDateTime::fromTime_t(*t) : QDateTime();
}

QString TorrentInfo::creator() const
{
    if (!isValid()) return QString();
    return QString::fromStdString(m_nativeInfo->creator());
}

QString TorrentInfo::comment() const
{
    if (!isValid()) return QString();
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
    return m_nativeInfo->num_files();
}

int TorrentInfo::pieceLength() const
{
    if (!isValid()) return -1;
    return m_nativeInfo->piece_length();
}

int TorrentInfo::pieceLength(int index) const
{
    if (!isValid()) return -1;
    return m_nativeInfo->piece_size(index);
}

int TorrentInfo::piecesCount() const
{
    if (!isValid()) return -1;
    return m_nativeInfo->num_pieces();
}

QString TorrentInfo::filePath(int index) const
{
    if (!isValid()) return QString();
    return Utils::Fs::fromNativePath(QString::fromStdString(m_nativeInfo->files().file_path(index)));
}

QStringList TorrentInfo::filePaths() const
{
    QStringList list;
    for (int i = 0; i < filesCount(); ++i)
        list << filePath(i);

    return list;
}

QString TorrentInfo::fileName(int index) const
{
    return Utils::Fs::fileName(filePath(index));
}

QString TorrentInfo::origFilePath(int index) const
{
    if (!isValid()) return QString();
    return Utils::Fs::fromNativePath(QString::fromStdString(m_nativeInfo->orig_files().file_path(index)));
}

qlonglong TorrentInfo::fileSize(int index) const
{
    if (!isValid()) return -1;
    return m_nativeInfo->files().file_size(index);
}

qlonglong TorrentInfo::fileOffset(int index) const
{
    if (!isValid()) return -1;
    return m_nativeInfo->files().file_offset(index);
}

QList<TrackerEntry> TorrentInfo::trackers() const
{
    if (!isValid()) return QList<TrackerEntry>();

    QList<TrackerEntry> trackers;
    for (const libt::announce_entry &tracker : m_nativeInfo->trackers())
        trackers.append(tracker);

    return trackers;
}

QList<QUrl> TorrentInfo::urlSeeds() const
{
    if (!isValid()) return QList<QUrl>();

    QList<QUrl> urlSeeds;
    for (const libt::web_seed_entry &webSeed : m_nativeInfo->web_seeds())
        if (webSeed.type == libt::web_seed_entry::url_seed)
            urlSeeds.append(QUrl(webSeed.url.c_str()));

    return urlSeeds;
}

QByteArray TorrentInfo::metadata() const
{
    if (!isValid()) return QByteArray();
    return QByteArray(m_nativeInfo->metadata().get(), m_nativeInfo->metadata_size());
}

QStringList TorrentInfo::filesForPiece(int pieceIndex) const
{
    // no checks here because fileIndicesForPiece() will return an empty list
    QVector<int> fileIndices = fileIndicesForPiece(pieceIndex);

    QStringList res;
    res.reserve(fileIndices.size());
    std::transform(fileIndices.begin(), fileIndices.end(), std::back_inserter(res),
        [this](int i) { return filePath(i); });

    return res;
}

QVector<int> TorrentInfo::fileIndicesForPiece(int pieceIndex) const
{
    if (!isValid() || (pieceIndex < 0) || (pieceIndex >= piecesCount()))
        return QVector<int>();

    std::vector<libt::file_slice> files(
        nativeInfo()->map_block(pieceIndex, 0, nativeInfo()->piece_size(pieceIndex)));
    QVector<int> res;
    res.reserve(int(files.size()));
    std::transform(files.begin(), files.end(), std::back_inserter(res),
        [](const libt::file_slice &s) { return s.file_index; });

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
        hashes += { m_nativeInfo->hash_for_piece_ptr(i), libtorrent::sha1_hash::size };

    return hashes;
}

TorrentInfo::PieceRange TorrentInfo::filePieces(const QString &file) const
{
    if (!isValid()) // if we do not check here the debug message will be printed, which would be not correct
        return {};

    int index = fileIndex(file);
    if (index == -1) {
        qDebug() << "Filename" << file << "was not found in torrent" << name();
        return {};
    }
    return filePieces(index);
}

TorrentInfo::PieceRange TorrentInfo::filePieces(int fileIndex) const
{
    if (!isValid())
        return {};

    if ((fileIndex < 0) || (fileIndex >= filesCount())) {
        qDebug() << "File index (" << fileIndex << ") is out of range for torrent" << name();
        return {};
    }

    const libt::file_storage &files = nativeInfo()->files();
    const auto fileSize = files.file_size(fileIndex);
    const auto fileOffset = files.file_offset(fileIndex);
    return makeInterval(static_cast<int>(fileOffset / pieceLength()),
                        static_cast<int>((fileOffset + fileSize - 1) / pieceLength()));
}

void TorrentInfo::renameFile(const int index, const QString &newPath)
{
    if (!isValid()) return;
    nativeInfo()->rename_file(index, Utils::Fs::toNativePath(newPath).toStdString());
}

int BitTorrent::TorrentInfo::fileIndex(const QString &fileName) const
{
    // the check whether the object is valid is not needed here
    // because if filesCount() returns -1 the loop exits immediately
    for (int i = 0; i < filesCount(); ++i)
        if (fileName == filePath(i))
            return i;

    return -1;
}

QString TorrentInfo::rootFolder() const
{
    QString rootFolder;
    for (int i = 0; i < filesCount(); ++i) {
        const QString filePath = this->filePath(i);
        if (QDir::isAbsolutePath(filePath)) continue;

        const auto filePathElements = filePath.splitRef('/');
        // if at least one file has no root folder, no common root folder exists
        if (filePathElements.count() <= 1) return "";

        if (rootFolder.isEmpty())
            rootFolder = filePathElements.at(0).toString();
        else if (rootFolder != filePathElements.at(0))
            return "";
    }

    return rootFolder;
}

bool TorrentInfo::hasRootFolder() const
{
    return !rootFolder().isEmpty();
}

void TorrentInfo::stripRootFolder()
{
    if (!hasRootFolder()) return;

    libtorrent::file_storage files = m_nativeInfo->files();

    // Solution for case of renamed root folder
    std::string testName = filePath(0).split('/').value(0).toStdString();
    if (files.name() != testName) {
        files.set_name(testName);
        for (int i = 0; i < files.num_files(); ++i)
            files.rename_file(i, files.file_path(i));
    }

    files.set_name("");
    m_nativeInfo->remap_files(files);
}

TorrentInfo::NativePtr TorrentInfo::nativeInfo() const
{
    return m_nativeInfo;
}
