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

#include <libtorrent/error_code.hpp>
#include "core/misc.h"
#include "core/fs_utils.h"
#include "torrentinfo.h"

namespace libt = libtorrent;
using namespace BitTorrent;

TorrentInfo::TorrentInfo(const libt::torrent_info *nativeInfo)
{
    if (!nativeInfo) {
        libt::error_code ec;
        m_nativeInfo = QSharedPointer<libtorrent::torrent_info>(new libt::torrent_info(libt::lazy_entry(), ec));
    }
    else {
        m_nativeInfo = QSharedPointer<libtorrent::torrent_info>(new libt::torrent_info(*nativeInfo));
    }
}

TorrentInfo::TorrentInfo(const TorrentInfo &other)
    : m_nativeInfo(other.m_nativeInfo)
{
}

TorrentInfo &TorrentInfo::operator=(const TorrentInfo &other)
{
    this->m_nativeInfo = other.m_nativeInfo;
    return *this;
}

TorrentInfo TorrentInfo::loadFromFile(const QString &path, QString &error)
{
    error.clear();
    libt::error_code ec;
    TorrentInfo info(new libt::torrent_info(path.toStdString(), ec));
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
    return (m_nativeInfo->is_valid() && (m_nativeInfo->num_files() > 0));
}

InfoHash TorrentInfo::hash() const
{
    return m_nativeInfo->info_hash();
}

QString TorrentInfo::name() const
{
    return misc::toQStringU(m_nativeInfo->name());
}

QDateTime TorrentInfo::creationDate() const
{
    boost::optional<time_t> t = m_nativeInfo->creation_date();
    return t ? QDateTime::fromTime_t(*t) : QDateTime();
}

QString TorrentInfo::creator() const
{
    return misc::toQStringU(m_nativeInfo->creator());
}

QString TorrentInfo::comment() const
{
    return misc::toQStringU(m_nativeInfo->comment());
}

bool TorrentInfo::isPrivate() const
{
    return m_nativeInfo->priv();
}

qlonglong TorrentInfo::totalSize() const
{
    return m_nativeInfo->total_size();
}

int TorrentInfo::filesCount() const
{
    return m_nativeInfo->num_files();
}

int TorrentInfo::pieceLength() const
{
    return m_nativeInfo->piece_length();
}

int TorrentInfo::piecesCount() const
{
    return m_nativeInfo->num_pieces();
}

QString TorrentInfo::filePath(uint index) const
{
    return fsutils::fromNativePath(misc::toQStringU(m_nativeInfo->files().file_path(index)));
}

QStringList TorrentInfo::filePaths() const
{
    QStringList list;
    for (int i = 0; i < filesCount(); ++i)
        list << filePath(i);

    return list;
}

QString TorrentInfo::fileName(uint index) const
{
    return fsutils::fileName(filePath(index));
}

QString TorrentInfo::origFilePath(uint index) const
{
    return fsutils::fromNativePath(misc::toQStringU(m_nativeInfo->orig_files().file_path(index)));
}

qlonglong TorrentInfo::fileSize(uint index) const
{
    return m_nativeInfo->files().file_size(index);
}

QStringList TorrentInfo::trackers() const
{
    QStringList trackers;

    foreach (const libt::announce_entry &tracker, m_nativeInfo->trackers())
        trackers.append(QString::fromUtf8(tracker.url.c_str()));

    return trackers;
}

QStringList TorrentInfo::urlSeeds() const
{
    QStringList urlSeeds;

    foreach (const libt::web_seed_entry &webSeed, m_nativeInfo->web_seeds())
        if (webSeed.type == libt::web_seed_entry::url_seed)
            urlSeeds.append(QString::fromUtf8(webSeed.url.c_str()));

    return urlSeeds;
}

QByteArray TorrentInfo::metadata() const
{
    return QByteArray(m_nativeInfo->metadata().get(), m_nativeInfo->metadata_size());
}

void TorrentInfo::renameFile(uint index, const QString &newPath)
{
    m_nativeInfo->rename_file(index, newPath.toStdString());
}

boost::intrusive_ptr<libtorrent::torrent_info> TorrentInfo::nativeInfo() const
{
    return new libtorrent::torrent_info(*m_nativeInfo.data());
}
