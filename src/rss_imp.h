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
 * Contact : chris@qbittorrent.org arnaud@qbittorrent.org
 */
#ifndef __RSS_IMP_H__
#define __RSS_IMP_H__

#define REFRESH_MAX_LATENCY 600000

#include "ui_rss.h"

class QTimer;
class RssManager;

class RSSImp : public QWidget, public Ui::RSS{
  Q_OBJECT

  private:
    RssManager *rssmanager;
    QTimer *refreshTimeTimer;
    QString selectedFeedUrl;

  protected slots:
    void on_addStream_button_clicked();
    void on_delStream_button_clicked();
    void on_refreshAll_button_clicked();
    void displayRSSListMenu(const QPoint&);
    void renameStream();
    void refreshSelectedStreams();
    void createStream();
    void refreshAllStreams();
    void refreshNewsList(QTreeWidgetItem* item, int);
    void refreshTextBrowser(QListWidgetItem *);
    void updateLastRefreshedTimeForStreams();
    void updateFeedIcon(QString url, QString icon_path);
    void updateFeedInfos(QString url, QString aliasOrUrl, unsigned int nbUnread);
    void openInBrowser(QListWidgetItem *);
    void fillFeedsList();
    void selectFirstFeed();
    void selectFirstNews();
    void updateFeedNbNews(QString url);

  public:
    RSSImp();
    ~RSSImp();
    QTreeWidgetItem* getTreeItemFromUrl(QString url) const;
};

#endif
