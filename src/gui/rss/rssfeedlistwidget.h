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

#ifndef RSSFEEDLISTWIDGET_H
#define RSSFEEDLISTWIDGET_H

#include <QSharedPointer>
#include <QTreeWidget>

#include <functional>

#define RSS_WIDGET_ITEM(item) static_cast<RssFeedListWidgetItem*>(item)

namespace Rss
{
    class Folder;
}
typedef QSharedPointer<Rss::Folder> FolderPtr;

class RssFeedListWidgetItem;

class RssFeedListWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit RssFeedListWidget(QWidget *parent = nullptr);
    ~RssFeedListWidget();

    void setRoot(FolderPtr root);
    FolderPtr getRoot() const;

    bool isRoot(QTreeWidgetItem *item) const;
    bool isFolder(QTreeWidgetItem *item) const;
    bool isFeed(QTreeWidgetItem *item) const;
    bool hasFeed(const QString &url) const;

    QList<RssFeedListWidgetItem*> selectedItemsNoRoot() const;

    void walkFolders(std::function<void(RssFeedListWidgetItem*)> callback);

signals:
    void unreadCountChanged(int) const;

public slots:
    void renameSelected();
    void deleteSelected();
    void refreshSelected();
    void refreshAll();

private slots:
    void refreshUnreadCount() const;

protected:
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    RssFeedListWidgetItem *m_stickyRoot;
};

#endif // RSSFEEDLISTWIDGET_H
