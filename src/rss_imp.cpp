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

#include "rss_imp.h"
#include "rss.h"

#include <QDesktopServices>
#include <QInputDialog>
#include <QMenu>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QTimer>
#include <QString>

    // display a right-click menu
    void RSSImp::displayRSSListMenu(const QPoint& pos){
      QMenu myFinishedListMenu(this);
      QTreeWidgetItem* item = listStreams->itemAt(pos);
      QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
      if(item != 0) {
        myFinishedListMenu.addAction(actionMark_all_as_read);
	myFinishedListMenu.addAction(actionDelete);
        if(selectedItems.size() == 1)
	 myFinishedListMenu.addAction(actionRename);
	myFinishedListMenu.addAction(actionRefresh);
      }else{
        myFinishedListMenu.addAction(actionCreate);
        myFinishedListMenu.addAction(actionRefreshAll);
      }
      myFinishedListMenu.exec(mapToGlobal(pos)+QPoint(10,33));
    }

    // add a stream by a button
    void RSSImp::on_addStream_button_clicked() {
      createStream();
    }

    // delete a stream by a button
    void RSSImp::on_delStream_button_clicked() {
      QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
      QTreeWidgetItem *item;
      if(!selectedItems.size()) return;
      int ret = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to delete this stream from the list?"),
      tr("&Yes"), tr("&No"),
          QString(), 0, 1);
      if(!ret) {
        QStringList urlsToDelete;
        foreach(item, selectedItems){
          QString url = item->data(1, Qt::DisplayRole).toString();
          urlsToDelete << url;
        }
        QString url;
        foreach(url, urlsToDelete){
          if(selectedFeedUrl == url){
            textBrowser->clear();
            listNews->clear();
          }
          rssmanager->removeStream(url);
          delete getTreeItemFromUrl(url);
        }
        if(urlsToDelete.size())
          rssmanager->saveStreamList();
      }
    }

    // refresh all streams by a button
    void RSSImp::on_refreshAll_button_clicked() {
      refreshAllStreams();
    }

    // open the url of the news in a browser
    void RSSImp::openInBrowser(QListWidgetItem *item) {
      RssItem* news =  rssmanager->getFeed(selectedFeedUrl)->getItem(listNews->row(item));
      QString link = news->getLink();
      if(!link.isEmpty())
        QDesktopServices::openUrl(QUrl(link));
    }

    //right-click on stream : give him an alias
    void RSSImp::renameStream() {
      QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
      Q_ASSERT(selectedItems.size() == 1);
      QTreeWidgetItem *item = selectedItems.at(0);
      QString url = item->data(1, Qt::DisplayRole).toString();
      bool ok;
      QString newAlias = QInputDialog::getText(this, tr("Please choose a new name for this stream"), tr("New stream name:"), QLineEdit::Normal, rssmanager->getFeed(url)->getAlias(), &ok);
      if(ok) {
        rssmanager->setAlias(url, newAlias);
      }
    }

    //right-click on stream : refresh it
    void RSSImp::refreshSelectedStreams() {
      QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
      QTreeWidgetItem* item;
      foreach(item, selectedItems){
        QString url = item->text(1);
        textBrowser->clear();
        listNews->clear();
        rssmanager->refresh(url);
        item->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
      }
    }

    void RSSImp::on_actionMark_all_as_read_triggered() {
      textBrowser->clear();
      listNews->clear();
      QList<QTreeWidgetItem*> selectedItems = listStreams->selectedItems();
      QTreeWidgetItem* item;
      foreach(item, selectedItems){
        QString url = item->text(1);
        RssStream *feed = rssmanager->getFeed(url);
        feed->markAllAsRead();
        item->setData(0, Qt::DisplayRole, feed->getAliasOrUrl()+ QString::fromUtf8("  (0)"));
      }
      if(selectedItems.size())
        refreshNewsList(selectedItems.last(), 0);
    }

    //right-click somewhere, refresh all the streams
    void RSSImp::refreshAllStreams() {
      textBrowser->clear();
      listNews->clear();
      unsigned int nbFeeds = listStreams->topLevelItemCount();
      for(unsigned int i=0; i<nbFeeds; ++i)
        listStreams->topLevelItem(i)->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
      rssmanager->refreshAll();
      updateLastRefreshedTimeForStreams();
    }

    void RSSImp::fillFeedsList() {
      QList<RssStream*> feeds = rssmanager->getRssFeeds();
      RssStream* stream;
      foreach(stream, feeds){
        QTreeWidgetItem* item = new QTreeWidgetItem(listStreams);
        item->setData(0, Qt::DisplayRole, stream->getAliasOrUrl()+ QString::fromUtf8("  (0)"));
        item->setData(0,Qt::DecorationRole, QVariant(QIcon(QString::fromUtf8(":/Icons/loading.png"))));
        item->setData(1, Qt::DisplayRole, stream->getUrl());
        item->setToolTip(0, QString::fromUtf8("<b>")+tr("Description:")+QString::fromUtf8("</b> ")+stream->getDescription()+QString::fromUtf8("<br/><b>")+tr("url:")+QString::fromUtf8("</b> ")+stream->getUrl()+QString::fromUtf8("<br/><b>")+tr("Last refresh:")+QString::fromUtf8("</b> ")+stream->getLastRefreshElapsedString());
      }
    }

    //right-click, register a new stream
    void RSSImp::createStream() {
      bool ok;
      QString newUrl = QInputDialog::getText(this, tr("Please type a rss stream url"), tr("Stream URL:"), QLineEdit::Normal, "http://", &ok);
      if(ok) {
        newUrl = newUrl.trimmed();
        if(!newUrl.isEmpty()){
          RssStream *stream = rssmanager->addStream(newUrl);
          if(stream == 0){
            // Already existing
            QMessageBox::warning(this, tr("qBittorrent"),
                         tr("This rss feed is already in the list."),
                         QMessageBox::Ok);
            return;
          }
          QTreeWidgetItem* item = new QTreeWidgetItem(listStreams);
          item->setText(0, stream->getAliasOrUrl() + QString::fromUtf8("  (0)"));
          item->setText(1, stream->getUrl());
          item->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
          item->setToolTip(0, QString::fromUtf8("<b>")+tr("Description:")+QString::fromUtf8("</b> ")+stream->getDescription()+QString::fromUtf8("<br/><b>")+tr("url:")+QString::fromUtf8("</b> ")+stream->getUrl()+QString::fromUtf8("<br/><b>")+tr("Last refresh:")+QString::fromUtf8("</b> ")+stream->getLastRefreshElapsedString());
          if(listStreams->topLevelItemCount() == 1)
            selectFirstFeed();
          rssmanager->refresh(newUrl);
          rssmanager->saveStreamList();
        }
      }
    }

    void RSSImp::updateLastRefreshedTimeForStreams() {
      unsigned int nbFeeds = listStreams->topLevelItemCount();
      for(unsigned int i=0; i<nbFeeds; ++i){
	QTreeWidgetItem* item = listStreams->topLevelItem(i);
        RssStream* stream = rssmanager->getFeed(item->data(1, Qt::DisplayRole).toString());
        item->setToolTip(0, QString::fromUtf8("<b>")+tr("Description:")+QString::fromUtf8("</b> ")+stream->getDescription()+QString::fromUtf8("<br/><b>")+tr("url:")+QString::fromUtf8("</b> ")+stream->getUrl()+QString::fromUtf8("<br/><b>")+tr("Last refresh:")+QString::fromUtf8("</b> ")+stream->getLastRefreshElapsedString());
      }
    }

    // fills the newsList
    void RSSImp::refreshNewsList(QTreeWidgetItem* item, int) {
      selectedFeedUrl = item->text(1);
      RssStream *stream = rssmanager->getFeed(selectedFeedUrl);
      listNews->clear();
      qDebug("Getting the list of news");
      QList<RssItem*> news = stream->getNewsList();
      qDebug("Got the list of news");
      RssItem* article;
      foreach(article, news){
        QListWidgetItem* it = new QListWidgetItem(article->getTitle(), listNews);
        if(article->isRead()){
          it->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
          it->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
        }else{
          it->setData(Qt::ForegroundRole, QVariant(QColor("blue")));
          it->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere2.png")));
        }
      }
      qDebug("Added all news to the GUI");
      selectFirstNews();
      qDebug("First news selected");
    }

    // display a news
    void RSSImp::refreshTextBrowser(QListWidgetItem *item) {
      RssItem* article = rssmanager->getFeed(selectedFeedUrl)->getItem(listNews->row(item));
      QString html;
      html += "<div style='border: 2px solid red; margin-left: 5px; margin-right: 5px; margin-bottom: 5px;'>";
      html += "<div style='background-color: #678db2; font-weight: bold; color: #fff;'>"+article->getTitle() + "</div>";
      if(article->getDate().isValid()) {
        html += "<div style='background-color: #efefef;'><b>"+tr("Date: ")+"</b>"+article->getDate().toString()+"</div>";
      }
      if(!article->getAuthor().isEmpty()) {
        html += "<div style='background-color: #efefef;'><b>"+tr("Author: ")+"</b>"+article->getAuthor()+"</div>";
      }
      html += "</div>";
      html += "<divstyle='margin-left: 5px; margin-right: 5px;'>"+article->getDescription()+"</div>";
      textBrowser->setHtml(html);
      article->setRead();
      item->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
      item->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
      updateFeedNbNews(selectedFeedUrl);
    }
		
		void RSSImp::saveSlidersPosition() {
			// Remember sliders positions
			QSettings settings("qBittorrent", "qBittorrent");
			settings.setValue("rss/splitter_h", splitter_h->saveState());
			settings.setValue("rss/splitter_v", splitter_v->saveState());
			qDebug("Splitters position saved");
		}
		
		void RSSImp::restoreSlidersPosition() {
			QSettings settings("qBittorrent", "qBittorrent");
			QByteArray pos_h = settings.value("rss/splitter_h", QByteArray()).toByteArray();
			if(!pos_h.isNull()) {
				splitter_h->restoreState(pos_h);
			}
			QByteArray pos_v = settings.value("rss/splitter_v", QByteArray()).toByteArray();
			if(!pos_v.isNull()) {
				splitter_v->restoreState(pos_v);
			}
		}

    QTreeWidgetItem* RSSImp::getTreeItemFromUrl(QString url) const{
      unsigned int nbItems = listStreams->topLevelItemCount();
      for(unsigned int i = 0; i<nbItems; ++i){
        QTreeWidgetItem* item = listStreams->topLevelItem(i);
        if(item->text(1) == url)
          return item;
      }
      qDebug("Cannot find url %s in feeds list", (const char*)url.toLocal8Bit());
      Q_ASSERT(false); // Should never go through here
      return (QTreeWidgetItem*)0;
    }

    void RSSImp::updateFeedIcon(QString url, QString icon_path){
      QTreeWidgetItem *item = getTreeItemFromUrl(url);
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(icon_path)));
    }

    void RSSImp::updateFeedNbNews(QString url){
      QTreeWidgetItem *item = getTreeItemFromUrl(url);
      RssStream *stream = rssmanager->getFeed(url);
      item->setText(0, stream->getAliasOrUrl() + QString::fromUtf8("  (") + QString::number(stream->getNbUnRead(), 10)+ QString(")"));
    }

    void RSSImp::updateFeedInfos(QString url, QString aliasOrUrl, unsigned int nbUnread){
      QTreeWidgetItem *item = getTreeItemFromUrl(url);
      RssStream *stream = rssmanager->getFeed(url);
      item->setText(0, aliasOrUrl + QString::fromUtf8("  (") + QString::number(nbUnread, 10)+ QString(")"));
      item->setData(0,Qt::DecorationRole, QVariant(QIcon(stream->getIconPath())));
      item->setToolTip(0, QString::fromUtf8("<b>")+tr("Description:")+QString::fromUtf8("</b> ")+stream->getDescription()+QString::fromUtf8("<br/><b>")+tr("url:")+QString::fromUtf8("</b> ")+stream->getUrl()+QString::fromUtf8("<br/><b>")+tr("Last refresh:")+QString::fromUtf8("</b> ")+stream->getLastRefreshElapsedString());
      // If the feed is selected, update the displayed news
      if(selectedFeedUrl == url){
        refreshNewsList(getTreeItemFromUrl(url), 0);
      }
    }

    RSSImp::RSSImp() : QWidget(){
      setupUi(this);
      // icons of bottom buttons
      addStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/subscribe.png")));
      delStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/unsubscribe.png")));
      refreshAll_button->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));
      actionMark_all_as_read->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/button_ok.png")));
      // icons of right-click menu
      actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/unsubscribe16.png")));
      actionRename->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/log.png")));
      actionRefresh->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));
      actionCreate->setIcon(QIcon(QString::fromUtf8(":/Icons/oxygen/subscribe16.png")));
      actionRefreshAll->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));

      // Hide second column (url)
      listStreams->hideColumn(1);

      rssmanager = new RssManager();
      fillFeedsList();
      connect(rssmanager, SIGNAL(feedInfosChanged(QString, QString, unsigned int)), this, SLOT(updateFeedInfos(QString, QString, unsigned int)));
      connect(rssmanager, SIGNAL(feedIconChanged(QString, QString)), this, SLOT(updateFeedIcon(QString, QString)));

      connect(listStreams, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayRSSListMenu(const QPoint&)));
      connect(actionDelete, SIGNAL(triggered()), this, SLOT(on_delStream_button_clicked()));
      connect(actionRename, SIGNAL(triggered()), this, SLOT(renameStream()));
      connect(actionRefresh, SIGNAL(triggered()), this, SLOT(refreshSelectedStreams()));
      connect(actionCreate, SIGNAL(triggered()), this, SLOT(createStream()));
      connect(actionRefreshAll, SIGNAL(triggered()), this, SLOT(refreshAllStreams()));
      connect(listStreams, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(refreshNewsList(QTreeWidgetItem*,int)));
      connect(listNews, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(refreshTextBrowser(QListWidgetItem *)));
      connect(listNews, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(openInBrowser(QListWidgetItem *)));
      refreshTimeTimer = new QTimer(this);
      connect(refreshTimeTimer, SIGNAL(timeout()), this, SLOT(updateLastRefreshedTimeForStreams()));
      refreshTimeTimer->start(60000); // 1min
      // Select first news of first feed
      selectFirstFeed();
      // Refresh all feeds
      rssmanager->refreshAll();
			// Restore sliders position
			restoreSlidersPosition();
			// Bind saveSliders slots
			connect(splitter_v, SIGNAL(splitterMoved(int, int)), this, SLOT(saveSlidersPosition()));
			connect(splitter_h, SIGNAL(splitterMoved(int, int)), this, SLOT(saveSlidersPosition()));
      qDebug("RSSImp constructed");
    }

    void RSSImp::selectFirstFeed(){
      if(listStreams->topLevelItemCount()){
        QTreeWidgetItem *first = listStreams->topLevelItem(0);
        listStreams->setCurrentItem(first);
        selectedFeedUrl = first->text(1);
      }
    }

    void RSSImp::selectFirstNews(){
      if(listNews->count()){
        listNews->setCurrentRow(0);
        refreshTextBrowser(listNews->currentItem());
      }
    }

    RSSImp::~RSSImp(){
      qDebug("Deleting RSSImp...");
      delete refreshTimeTimer;
      delete rssmanager;
      qDebug("RSSImp deleted");
    }

