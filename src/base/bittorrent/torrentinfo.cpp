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

#include <QDebug>
#include <QString>
#include <QList>
#include <QUrl>
#include <QDateTime>

#include <libtorrent/error_code.hpp>

#include "base/utils/misc.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "infohash.h"
#include "trackerentry.h"
#include "torrentinfo.h"

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

TorrentInfo TorrentInfo::loadFromFile(const QString &path, QString &error)
{
    error.clear();
    libt::error_code ec;
    TorrentInfo info(NativePtr(new libt::torrent_info(Utils::Fs::toNativePath(path).toStdString(), ec)));
    if (ec) {
        error = QString::fromUtf8(ec.message().c_str());
        qDebug("Cannot load .torrent file: %s", qPrintable(error));
    }

    return info;
}

TorrentInfo TorrentInfo::loadFromFile(const QString &path)
{
    QString error;
    return loadFromFile(path, error);
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
    return m_nativeInfo->file_at(index).offset;
}

QList<TrackerEntry> TorrentInfo::trackers() const
{
    if (!isValid()) return QList<TrackerEntry>();

    QList<TrackerEntry> trackers;
    foreach (const libt::announce_entry &tracker, m_nativeInfo->trackers())
        trackers.append(tracker);

    return trackers;
}

QList<QUrl> TorrentInfo::urlSeeds() const
{
    if (!isValid()) return QList<QUrl>();

    QList<QUrl> urlSeeds;
    foreach (const libt::web_seed_entry &webSeed, m_nativeInfo->web_seeds())
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

TorrentInfo::PieceRange TorrentInfo::filePieces(const QString& file) const
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
    const auto firstOffset = files.file_offset(fileIndex);
    return makeInterval(static_cast<int>(firstOffset / pieceLength()),
                        static_cast<int>((firstOffset + fileSize - 1) / pieceLength()));
}

void TorrentInfo::renameFile(uint index, const QString &newPath)
{
    if (!isValid()) return;
    nativeInfo()->rename_file(index, newPath.toStdString());
}

int BitTorrent::TorrentInfo::fileIndex(const QString& fileName) const
{
    // the check whether the object valid is not needed here
    // because filesCount() returns -1 in that case and the loop exits immediately
    for (int i = 0; i < filesCount(); ++i)
        if (fileName == filePath(i))
            return i;

    return -1;
}

TorrentInfo::NativePtr TorrentInfo::nativeInfo() const
{
    return m_nativeInfo;
}
