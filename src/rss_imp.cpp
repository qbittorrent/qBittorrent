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

#include "rss_imp.h"
#include <QDesktopServices>
#include <QInputDialog>
#include <QMenu>
#include <QStandardItemModel>

    // display a right-click menu
    void RSSImp::displayFinishedListMenu(const QPoint& pos){
      QMenu myFinishedListMenu(this);
      QListWidgetItem* item = listStreams->itemAt(pos);
      if(item!=NULL) {
	myFinishedListMenu.addAction(actionDelete);
	myFinishedListMenu.addAction(actionRename);
	myFinishedListMenu.addAction(actionRefresh);
      }
      myFinishedListMenu.addAction(actionCreate);
      myFinishedListMenu.addAction(actionRefreshAll);
      myFinishedListMenu.exec(mapToGlobal(pos)+QPoint(10,33));
    }

    // add a stream by a button
    void RSSImp::on_addStream_button_clicked() {
      createStream();
    }

    // delete a stream by a button
    void RSSImp::on_delStream_button_clicked() {
      if(listStreams->currentRow()<0 || rssmanager.getNbStream()==0) {
	qDebug("no stream selected");
	return;
      }else {
	textBrowser->clear();
	listNews->clear();
	rssmanager.removeStream(rssmanager.getStream(listStreams->currentRow()));
	refreshStreamList();
      }
    }

    // refresh all streams by a button
    void RSSImp::on_refreshAll_button_clicked() {
      refreshAllStream();
    }

    // display the news of a stream when click on it
    void RSSImp::on_listStreams_clicked() {
      refreshNewsList();
    }

    // display the content of a new when clicked on it
    void RSSImp::on_listNews_clicked() {
      refreshTextBrowser();
    }

    // open the url of the news in a browser
    void RSSImp::on_listNews_doubleClicked() {
      if(listStreams->currentRow()>=0 && listNews->currentRow()>=0 && rssmanager.getStream(listStreams->currentRow())->getListSize()>0) {
	RssItem* currentItem =  rssmanager.getStream(listStreams->currentRow())->getItem(listNews->currentRow());
	if(currentItem->getLink()!=NULL && currentItem->getLink().length()>5)
	 QDesktopServices::openUrl(QUrl(currentItem->getLink()));
      }
    }

    //right-clik on stream : delete it
    void RSSImp::deleteStream() {
      if(rssmanager.getNbStream()==0) {
	qDebug("no stream selected");
	return;
      }else {
	int index = listStreams->currentRow();
	textBrowser->clear();
	listNews->clear();
	rssmanager.removeStream(rssmanager.getStream(index));
	refreshStreamList();
      }
    }

    //right-clik on stream : give him an alias
    void RSSImp::renameStream() {
      if(rssmanager.getNbStream()==0) {
	qDebug("no stream selected");
	return;
      }else {
	bool ok;
	int index = listStreams->currentRow();
	QString newAlias = QInputDialog::getText(this, tr("Please choose a new name for this stream"), tr("New stream name:"), QLineEdit::Normal, rssmanager.getStream(index)->getAlias(), &ok);
	if(ok) {
	  rssmanager.setAlias(index, newAlias);
	  refreshStreamList();
	}
      }

    }

    //right-clik on stream : refresh it
    void RSSImp::refreshStream() {
      if(rssmanager.getNbStream()>0 && lastRefresh.elapsed()>REFRESH_FREQ_MAX) {
	int index = listStreams->currentRow();
	textBrowser->clear();
	listNews->clear();
	rssmanager.refresh(index);
	refreshStreamList();
	lastRefresh.start();
      }
    }

    //right-click somewhere, refresh all the streams
    void RSSImp::refreshAllStream() {
      if(lastRefresh.elapsed()>REFRESH_FREQ_MAX) {
	textBrowser->clear();
	listNews->clear();
	rssmanager.refreshAll();
	refreshStreamList();
	lastRefresh.start();
      }
    }

    //right-click, register a new stream
    void RSSImp::createStream() {
      bool ok;
      QString newUrl = QInputDialog::getText(this, tr("Please type a rss stream url"), tr("Stream URL:"), QLineEdit::Normal, "http://", &ok);
      if(ok) {
	rssmanager.addStream(newUrl);
	refreshStreamList();
      }
    }

    // fills the streamList
    void RSSImp::refreshStreamList() {
      int currentStream = listStreams->currentRow();
      listStreams->clear();
      for(int i=0; i<rssmanager.getNbStream(); i++) {
	new QListWidgetItem(rssmanager.getStream(i)->getAlias()+" ("+QString::number(rssmanager.getStream(i)->getListSize(),10).toUtf8()+")", listStreams);
      }
      listStreams->setCurrentRow(currentStream);
    }

    // fills the newsList
    void RSSImp::refreshNewsList() {
      if(rssmanager.getNbStream()>0) {
	RssStream* currentstream = rssmanager.getStream(listStreams->currentRow());
	listNews->clear();
        unsigned int currentStreamSize = currentstream->getListSize();
	for(unsigned int i=0; i<currentStreamSize; ++i) {
	  new QListWidgetItem(currentstream->getItem(i)->getTitle(), listNews);
	}
      }
    }

    // display a news
    void RSSImp::refreshTextBrowser() {
      if(listStreams->currentRow()>=0 && listNews->currentRow()>=0) {
	RssItem* currentitem = rssmanager.getStream(listStreams->currentRow())->getItem(listNews->currentRow());
	textBrowser->setHtml(currentitem->getTitle()+" : \n"+currentitem->getDescription());
	currentitem->setRead();
      }
    }

    // show the number of news for each stream
    void RSSImp::updateStreamNbNews() {
      for(int i=0; i<rssmanager.getNbStream(); i++) {
        listStreams->item(i)->setText(rssmanager.getStream(i)->getAlias()+" ("+QString::number(rssmanager.getStream(i)->getListSize(),10).toUtf8()+")");
      }
    }

    RSSImp::RSSImp() : QWidget(){
      setupUi(this);
      addStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
      delStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
      refreshAll_button->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));
      connect(listStreams, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayFinishedListMenu(const QPoint&)));
      connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteStream()));
      connect(actionRename, SIGNAL(triggered()), this, SLOT(renameStream()));
      connect(actionRefresh, SIGNAL(triggered()), this, SLOT(refreshStream()));
      connect(actionCreate, SIGNAL(triggered()), this, SLOT(createStream()));
      connect(actionRefreshAll, SIGNAL(triggered()), this, SLOT(refreshAllStream()));
      refreshStreamList();
      refreshTextBrowser();
      timer = new QTimer(this);
      connect(timer, SIGNAL(timeout()), this, SLOT(updateStreamNbNews()));
      timer->start(5000);
      lastRefresh.start();
    }

    RSSImp::~RSSImp(){
      delete timer;
    }



