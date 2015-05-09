/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez, Arnaud Demaiziere
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
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */
#ifndef __RSS_IMP_H__
#define __RSS_IMP_H__

#define REFRESH_MAX_LATENCY 600000

#include <QPointer>
#include <QShortcut>

#include "ui_rss.h"
#include "rssfolder.h"
#include "rssmanager.h"

class FeedListWidget;

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
QT_END_NAMESPACE

class RSSImp: public QWidget, public Ui::RSS
{
    Q_OBJECT

public:
    RSSImp(QWidget *parent);
    ~RSSImp();

public slots:
    void deleteSelectedItems();
    void updateRefreshInterval(uint val);

private slots:
    void on_newFeedButton_clicked();
    void refreshAllFeeds();
    void on_markReadButton_clicked();
    void displayRSSListMenu(const QPoint&);
    void displayItemsListMenu(const QPoint&);
    void renameSelectedRssFile();
    void refreshSelectedItems();
    void copySelectedFeedsURL();
    void populateArticleList(QTreeWidgetItem* item);
    void refreshTextBrowser();
    void updateFeedIcon(const QString &url, const QString &icon_path);
    void updateFeedInfos(const QString &url, const QString &display_name, uint nbUnread);
    void onFeedContentChanged(const QString& url);
    void updateItemsInfos(const QList<QTreeWidgetItem*> &items);
    void updateItemInfos(QTreeWidgetItem *item);
    void openSelectedArticlesUrls();
    void downloadSelectedTorrents();
    void fillFeedsList(QTreeWidgetItem *parent = 0, const RssFolderPtr& rss_parent = RssFolderPtr());
    void saveSlidersPosition();
    void restoreSlidersPosition();
    void askNewFolder();
    void saveFoldersOpenState();
    void loadFoldersOpenState();
    void on_actionManage_cookies_triggered();
    void on_settingsButton_clicked();
    void on_rssDownloaderBtn_clicked();

private:
    static QListWidgetItem* createArticleListItem(const RssArticlePtr& article);
    static QTreeWidgetItem* createFolderListItem(const RssFilePtr& rssFile);

private:
    RssManagerPtr m_rssManager;
    FeedListWidget *m_feedList;
    QListWidgetItem* m_currentArticle;
    QShortcut *editHotkey;
    QShortcut *deleteHotkey;

};

#endif
