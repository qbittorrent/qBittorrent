/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Michael Ziminsky <mgziminsky@gmail.com>
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

#ifndef UTORRENTRSSDATA_H
#define UTORRENTRSSDATA_H

#include <QHash>
#include <QList>
#include <QSharedPointer>

namespace libtorrent
{
    class lazy_entry;
}

namespace Rss
{
    class Feed;
    class DownloadRuleList;
    class Manager;

    typedef QSharedPointer<Feed> FeedPtr;
    typedef QList<FeedPtr> FeedList;

    class UTorrentRssData
    {
    public:
        UTorrentRssData();
        ~UTorrentRssData();

        const QList<FeedPtr>& getFeeds() const;
        const DownloadRuleList& getRules() const;
        bool load(const QString &path);

    private:
        QHash<int, QString> loadFeeds(const libtorrent::lazy_entry &feeds);
        void loadRules(const libtorrent::lazy_entry &filters, const QHash<int, QString> &feedUrls);

        QList<FeedPtr> m_feeds;
        DownloadRuleList *m_rules;
    };
}
#endif // UTORRENTRSSDATA_H
