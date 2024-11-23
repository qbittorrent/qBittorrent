/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <optional>

#include <libtorrent/add_torrent_params.hpp>

#include <QtContainerFwd>
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "base/3rdparty/expected.hpp"
#include "base/path.h"
#include "torrentinfo.h"

class QByteArray;
class QUrl;

namespace BitTorrent
{
    class InfoHash;
    struct TrackerEntry;

    class TorrentDescriptor
    {
    public:
        TorrentDescriptor() = default;

        InfoHash infoHash() const;
        QString name() const;
        QDateTime creationDate() const;
        QString creator() const;
        QString comment() const;
        QList<TrackerEntry> trackers() const;
        QList<QUrl> urlSeeds() const;
        const std::optional<TorrentInfo> &info() const;

        void setTorrentInfo(TorrentInfo torrentInfo);

        static nonstd::expected<TorrentDescriptor, QString> load(const QByteArray &data) noexcept;
        static nonstd::expected<TorrentDescriptor, QString> loadFromFile(const Path &path) noexcept;
        static nonstd::expected<TorrentDescriptor, QString> parse(const QString &str) noexcept;
        nonstd::expected<void, QString> saveToFile(const Path &path) const;

        const lt::add_torrent_params &ltAddTorrentParams() const;

    private:
        explicit TorrentDescriptor(lt::add_torrent_params ltAddTorrentParams);

        lt::add_torrent_params m_ltAddTorrentParams;
        std::optional<TorrentInfo> m_info;
        QDateTime m_creationDate;
        QString m_creator;
        QString m_comment;
    };
}

Q_DECLARE_METATYPE(BitTorrent::TorrentDescriptor)
