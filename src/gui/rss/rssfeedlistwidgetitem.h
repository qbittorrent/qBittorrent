/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Michael Ziminsky
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

#ifndef RSSFEEDLISTWIDGETITEM_H
#define RSSFEEDLISTWIDGETITEM_H

#include <QTreeWidgetItem>
#include <QSharedPointer>

namespace Rss
{
    class File;
    class Folder;
}
typedef QSharedPointer<Rss::File> FilePtr;
typedef QSharedPointer<Rss::Folder> FolderPtr;

class RssFeedListWidget;

class RssFeedListWidgetItem : public QObject, public QTreeWidgetItem
{
    Q_OBJECT

protected:
    enum ItemType
    {
        Sticky = UserType,
        Folder,
        Feed
    };

public:
    explicit RssFeedListWidgetItem(FilePtr file, RssFeedListWidget *view = nullptr);
    virtual ~RssFeedListWidgetItem();

    FilePtr rssFile() const;

    virtual bool operator <(const QTreeWidgetItem &other) const override;

protected:
    RssFeedListWidgetItem(FilePtr file, RssFeedListWidget *view, ItemType type);
    Qt::SortOrder sortOrder() const;

private slots:
    void setName(const QString &name);
    void setUnread(int unread);
    void setTotal(int total);
    void refreshIcon();

private:
    FilePtr m_file;
};

#endif // RSSFEEDLISTWIDGETITEM_H
