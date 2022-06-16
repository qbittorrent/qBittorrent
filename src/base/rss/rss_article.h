/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2022  Vladimir Golovnev <glassez@yandex.ru>
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
 */

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantHash>

namespace RSS
{
    class Feed;

    class Article final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Article)

        friend class Feed;

        Article(Feed *feed, const QVariantHash &varHash);

    public:
        static const QString KeyId;
        static const QString KeyDate;
        static const QString KeyTitle;
        static const QString KeyAuthor;
        static const QString KeyDescription;
        static const QString KeyTorrentURL;
        static const QString KeyLink;
        static const QString KeyIsRead;

        Feed *feed() const;
        QString guid() const;
        QDateTime date() const;
        QString title() const;
        QString author() const;
        QString description() const;
        QString torrentUrl() const;
        QString link() const;
        bool isRead() const;
        QVariantHash data() const;

        void markAsRead();

        static bool articleDateRecentThan(const Article *article, const QDateTime &date);

    signals:
        void read(Article *article = nullptr);

    private:
        Feed *m_feed = nullptr;
        QString m_guid;
        QDateTime m_date;
        QString m_title;
        QString m_author;
        QString m_description;
        QString m_torrentURL;
        QString m_link;
        bool m_isRead = false;
        QVariantHash m_data;
    };
}
