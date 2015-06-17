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

#include <QString>
#include <QList>
#include <QUrl>
#include <QDateTime>

#include <libtorrent/error_code.hpp>

#include "core/utils/misc.h"
#include "core/utils/fs.h"
#include "core/utils/string.h"
#include "infohash.h"
#include "trackerentry.h"
#include "torrentinfo.h"

namespace libt = libtorrent;
using namespace BitTorrent;

TorrentInfo::TorrentInfo(boost::intrusive_ptr<const libt::torrent_info> nativeInfo)
    : m_nativeInfo(const_cast<libt::torrent_info *>(nativeInfo.get()))
{
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
    TorrentInfo info(new libt::torrent_info(Utils::String::toStdString(Utils::Fs::toNativePath(path)), ec));
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
    return Utils::String::fromStdString(m_nativeInfo->name());
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
    return Utils::String::fromStdString(m_nativeInfo->creator());
}

QString TorrentInfo::comment() const
{
    if (!isValid()) return QString();
    return Utils::String::fromStdString(m_nativeInfo->comment());
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

int TorrentInfo::piecesCount() const
{
    if (!isValid()) return -1;
    return m_nativeInfo->num_pieces();
}

QString TorrentInfo::filePath(int index) const
{
    if (!isValid()) return QString();
    return Utils::Fs::fromNativePath(Utils::String::fromStdString(m_nativeInfo->files().file_path(index)));
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
    return Utils::Fs::fromNativePath(Utils::String::fromStdString(m_nativeInfo->orig_files().file_path(index)));
}

qlonglong TorrentInfo::fileSize(int index) const
{
    if (!isValid()) return -1;
    return m_nativeInfo->files().file_size(index);
}

qlonglong TorrentInfo::fileOffset(int index) const
{
    if (!isValid()) return  -1;
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

void TorrentInfo::renameFile(uint index, const QString &newPath)
{
    if (!isValid()) return;
    m_nativeInfo->rename_file(index, Utils::String::toStdString(newPath));
}

boost::intrusive_ptr<libtorrent::torrent_info> TorrentInfo::nativeInfo() const
{
    return m_nativeInfo;
}
