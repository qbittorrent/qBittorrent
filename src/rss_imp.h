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

#include "ui_rss.h"
#include "rss.h"

class bittorrent;
class FeedList;
class QTreeWidgetItem;

class RSSImp : public QWidget, public Ui::RSS{
  Q_OBJECT

private:
  RssManager *rssmanager;
  bittorrent *BTSession;
  FeedList *listStreams;

public slots:
  void deleteSelectedItems();

protected slots:
  void on_newFeedButton_clicked();
  void on_updateAllButton_clicked();
  void on_markReadButton_clicked();
  void displayRSSListMenu(const QPoint&);
  void displayItemsListMenu(const QPoint&);
  void renameFiles();
  void refreshSelectedItems();
  void copySelectedFeedsURL();
  void refreshNewsList(QTreeWidgetItem* item);
  void refreshTextBrowser(QListWidgetItem *);
  void updateFeedIcon(QString url, QString icon_path);
  void updateFeedInfos(QString url, QString aliasOrUrl, unsigned int nbUnread);
  void updateItemsInfos(QList<QTreeWidgetItem*> items);
  void updateItemInfos(QTreeWidgetItem *item);
  void openNewsUrl();
  void downloadTorrent();
  void fillFeedsList(QTreeWidgetItem *parent=0, RssFolder *rss_parent=0);
  void selectFirstFeed();
  void saveSlidersPosition();
  void restoreSlidersPosition();
  void showFeedDownloader();
  void askNewFolder();

public:
  RSSImp(bittorrent *BTSession);
  ~RSSImp();

};

#endif
