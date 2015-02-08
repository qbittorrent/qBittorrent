/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef TORRENTFILTER_H
#define TORRENTFILTER_H

#include <QString>
#include <QSet>

typedef QSet<QString> QStringSet;

namespace BitTorrent
{

class TorrentHandle;
class TorrentState;

}

class TorrentFilter
{
public:
    enum Type
    {
        All,
        Downloading,
        Seeding,
        Completed,
        Resumed,
        Paused,
        Active,
        Inactive
    };

    static const QString AnyLabel;
    static const QStringSet AnyHash;

    static const TorrentFilter DownloadingTorrent;
    static const TorrentFilter SeedingTorrent;
    static const TorrentFilter CompletedTorrent;
    static const TorrentFilter PausedTorrent;
    static const TorrentFilter ResumedTorrent;
    static const TorrentFilter ActiveTorrent;
    static const TorrentFilter InactiveTorrent;

    TorrentFilter();
    // label: pass empty string for "no label" or null string (QString()) for "any label"
    TorrentFilter(Type type, QStringSet hashSet = AnyHash, QString label = AnyLabel);
    TorrentFilter(QString filter, QStringSet hashSet = AnyHash, QString label = AnyLabel);

    bool setType(Type type);
    bool setTypeByName(const QString &filter);
    bool setHashSet(const QStringSet &hashSet);
    bool setLabel(const QString &label);

    bool match(BitTorrent::TorrentHandle *const torrent) const;
    bool match(const BitTorrent::TorrentState &state, int upSpeed, const QString &hash = "", const QString &label = "") const;

    bool operator==(const TorrentFilter &other) const;
    bool operator!=(const TorrentFilter &other) const;

private:
    bool matchState(const BitTorrent::TorrentState &state, int upSpeed) const;
    bool matchHash(const QString &hash) const;
    bool matchLabel(const QString &label) const;

    bool isTorrentDownloading(const BitTorrent::TorrentState &state) const;
    bool isTorrentSeeding(const BitTorrent::TorrentState &state) const;
    bool isTorrentCompleted(const BitTorrent::TorrentState &state) const;
    bool isTorrentPaused(const BitTorrent::TorrentState &state) const;
    bool isTorrentResumed(const BitTorrent::TorrentState &state) const;
    bool isTorrentActive(const BitTorrent::TorrentState &state, int upSpeed) const;
    bool isTorrentInactive(const BitTorrent::TorrentState &state, int upSpeed) const;

    Type m_type;
    QString m_label;
    QStringSet m_hashSet;
};

#endif // TORRENTFILTER_H
