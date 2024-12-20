/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include <QWidget>

#include "gui/guiapplicationcomponent.h"

class QListWidgetItem;
class QTreeWidgetItem;

namespace RSS
{
    class Article;
}

namespace Ui
{
    class RSSWidget;
}

class RSSWidget final : public GUIApplicationComponent<QWidget>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RSSWidget)

public:
    explicit RSSWidget(IGUIApplication *app, QWidget *parent = nullptr);
    ~RSSWidget() override;

public slots:
    void deleteSelectedItems();
    void updateRefreshInterval(int val) const;

signals:
    void unreadCountUpdated(int count);

private slots:
    void on_newFeedButton_clicked();
    void refreshAllFeeds();
    void on_markReadButton_clicked();
    void displayRSSListMenu(const QPoint &pos);
    void displayItemsListMenu();
    void renameSelectedRSSItem();
    void editSelectedRSSFeedURL();
    void refreshSelectedItems();
    void copySelectedFeedsURL();
    void handleCurrentFeedItemChanged(QTreeWidgetItem *currentItem);
    void handleCurrentArticleItemChanged(QListWidgetItem *currentItem, QListWidgetItem *previousItem);
    void openSelectedArticlesUrls();
    void downloadSelectedTorrents();
    void saveSlidersPosition();
    void restoreSlidersPosition();
    void askNewFolder();
    void saveFoldersOpenState();
    void loadFoldersOpenState();
    void on_rssDownloaderBtn_clicked();
    void handleSessionProcessingStateChanged(bool enabled);
    void handleUnreadCountChanged();

private:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void renderArticle(const RSS::Article *article) const;

    Ui::RSSWidget *m_ui = nullptr;
};
