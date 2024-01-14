/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Jonathan Ketchker
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QList>
#include <QObject>

namespace RSS
{
    class Article;
    class Folder;
    class Session;

    class Item : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Item)

        friend class Folder;
        friend class Session;

    public:
        virtual QList<Article *> articles() const = 0;
        virtual int unreadCount() const = 0;
        virtual void markAsRead() = 0;
        virtual void refresh() = 0;
        virtual void updateFetchDelay() = 0;

        QString path() const;
        QString name() const;

        virtual QJsonValue toJsonValue(bool withData = false) const = 0;

        static const QChar PathSeparator;

        static bool isValidPath(const QString &path);
        static QString joinPath(const QString &path1, const QString &path2);
        static QStringList expandPath(const QString &path);
        static QString parentPath(const QString &path);
        static QString relativeName(const QString &path);

    signals:
        void pathChanged(Item *item = nullptr);
        void unreadCountChanged(Item *item = nullptr);
        void aboutToBeDestroyed(Item *item = nullptr);
        void newArticle(Article *article);
        void articleRead(Article *article);
        void articleAboutToBeRemoved(Article *article);

    protected:
        explicit Item(const QString &path);
        ~Item() override = default;

        virtual void cleanup() = 0;

    private:
        void setPath(const QString &path);

        QString m_path;
    };
}
