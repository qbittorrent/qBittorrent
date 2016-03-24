/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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
 *
 * Contact: chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#ifndef RSSARTICLE_H
#define RSSARTICLE_H

#include <QDateTime>
#include <QVariantHash>
#include <QSharedPointer>

namespace Rss
{
    class Feed;
    class Article;

    typedef QSharedPointer<Article> ArticlePtr;

    // Item of a rss stream, single information
    class Article: public QObject
    {
        Q_OBJECT

    public:
        Article(Feed *parent);

        // Accessors
        bool hasAttachment() const;
        const QString guid();
        Feed *parent() const;
        const QString title();
        const QString author();
        const QString torrentUrl();
        const QString link();
        QString description() const;
        const QDateTime date();
        bool isRead() const;
        // Setters
        void markAsRead();

        // Serialization
        QVariantHash toHash();
        static ArticlePtr fromHash(Feed *parent, const QVariantHash &hash);

        const QString getValue(const QString &name) const;

    signals:
        void articleWasRead();

    public slots:
        void handleTorrentDownloadSuccess(const QString &url);

    private:
        Feed *m_parent;
        QVariantHash m_hash;
    };
}

#endif // RSSARTICLE_H
