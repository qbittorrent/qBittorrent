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
#include "misc.h"

    // display a right-click menu
    void RSSImp::displayRSSListMenu(const QPoint& pos){
      moveCurrentItem();
      QMenu myFinishedListMenu(this);
      QTreeWidgetItem* item = listStreams->itemAt(pos);
      if(item!=NULL) {
	myFinishedListMenu.addAction(actionDelete);
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
      if(getNumStreamSelected()<0 || rssmanager.getNbStreams()==0) {
	qDebug("no stream selected");
	return;
      }else {
	int ok = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to delete this stream from the list?"),
	      tr("&Yes"), tr("&No"),
		 QString(), 0, 1);
	if(ok==0) {
	  textBrowser->clear();
	  listNews->clear();
	  rssmanager.removeStream(rssmanager.getStream(getNumStreamSelected()));
	  refreshStreamList();
	}
      }
    }

    // refresh all streams by a button
    void RSSImp::on_refreshAll_button_clicked() {
      refreshAllStreams();
    }

    // display the news of a stream when click on it
    void RSSImp::on_listStreams_clicked() {
      if(rssmanager.getNbStreams()>0) {
	moveCurrentItem();
	rssmanager.getStream(getNumStreamSelected())->setRead();
	// update the color of the stream, is it old ?
	updateStreamName(getNumStreamSelected(), LATENCY);
        refreshNewsList();
      }
    }

    // display the content of a new when clicked on it
    void RSSImp::on_listNews_clicked() {
      listNews->item(listNews->currentRow())->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
      listNews->item(listNews->currentRow())->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
      refreshTextBrowser();
    }

    // open the url of the news in a browser
    void RSSImp::on_listNews_doubleClicked() {
      if(getNumStreamSelected()>=0 && listNews->currentRow()>=0 && rssmanager.getStream(getNumStreamSelected())->getListSize()>0) {
	RssItem* currentItem =  rssmanager.getStream(getNumStreamSelected())->getItem(listNews->currentRow());
	if(currentItem->getLink()!=NULL && currentItem->getLink().length()>5)
	 QDesktopServices::openUrl(QUrl(currentItem->getLink()));
      }
    }

    // move the current selection if it is not a toplevelitem (id : stream)
    void RSSImp::moveCurrentItem() {
      if(getNumStreamSelected()<0) {
	int index = listStreams->indexOfTopLevelItem(listStreams->currentItem()->parent());
	if(index>=0)
	  listStreams->setCurrentItem(listStreams->topLevelItem(index));
	else
	  listStreams->setCurrentItem(listStreams->topLevelItem(0));
      }
    }

    //right-clik on stream : delete it
    void RSSImp::deleteStream() {
      if(rssmanager.getNbStreams()==0) {
	qDebug("no stream selected");
	return;
      }else {
	int ok = QMessageBox::question(this, tr("Are you sure? -- qBittorrent"), tr("Are you sure you want to delete this stream from the list ?"), tr("&Yes"), tr("&No"), QString(), 0, 1);
	if(ok==0) {
	  moveCurrentItem();
	  textBrowser->clear();
	  listNews->clear();
	  rssmanager.removeStream(rssmanager.getStream(getNumStreamSelected()));
	  refreshStreamList();
	}
      }
    }

    //right-clik on stream : give him an alias
    void RSSImp::renameStream() {
      if(rssmanager.getNbStreams()==0) {
	qDebug("no stream selected");
	return;
      }else {
	moveCurrentItem();
	bool ok;
	short index = getNumStreamSelected();
	QString newAlias = QInputDialog::getText(this, tr("Please choose a new name for this stream"), tr("New stream name:"), QLineEdit::Normal, rssmanager.getStream(index)->getAlias(), &ok);
	if(ok) {
	  rssmanager.setAlias(index, newAlias);
	  updateStreamName(index, NEWS);
	}
      }

    }

    //right-clik on stream : refresh it
    void RSSImp::refreshStream() {
      if(rssmanager.getNbStreams()>0) {
	moveCurrentItem();
	short index = getNumStreamSelected();
	textBrowser->clear();
	listNews->clear();
	listStreams->topLevelItem(index)->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
	rssmanager.refresh(index);
      }
      updateLastRefreshedTimeForStreams();
    }

    //right-click somewhere, refresh all the streams
    void RSSImp::refreshAllStreams() {
      textBrowser->clear();
      listNews->clear();
      unsigned short nbstream = rssmanager.getNbStreams();
      for(unsigned short i=0; i<nbstream; ++i)
	listStreams->topLevelItem(i)->setData(0,Qt::DecorationRole, QVariant(QIcon(":/Icons/loading.png")));
      rssmanager.refreshAll();
      updateLastRefreshedTimeForStreams();
    }

    //right-click, register a new stream
    void RSSImp::createStream() {
      bool ok;
      QString newUrl = QInputDialog::getText(this, tr("Please type a rss stream url"), tr("Stream URL:"), QLineEdit::Normal, "http://", &ok);
      if(ok) {
        newUrl = newUrl.trimmed();
        if(!newUrl.isEmpty() && newUrl != "http://"){
          rssmanager.addStream(newUrl);
          refreshStreamList();
        }
      }
    }

    // fills the streamList
    void RSSImp::refreshStreamList() {
      qDebug("Refreshing stream list");
      unsigned short nbstream = rssmanager.getNbStreams();
      listStreams->clear();
      QList<QTreeWidgetItem *> streams;
      for(unsigned short i=0; i<nbstream; ++i) {
	QTreeWidgetItem* stream = new QTreeWidgetItem(listStreams);
	updateStreamName(i, NEWS);
        stream->setToolTip(0, QString("<b>")+tr("Description:")+QString("</b> ")+rssmanager.getStream(i)->getDescription()+QString("<br/><b>")+tr("url:")+QString("</b> ")+rssmanager.getStream(i)->getUrl()+QString("<br/><b>")+tr("Last refresh:")+QString("</b> ")+rssmanager.getStream(i)->getLastRefreshElapsedString());
      }
      qDebug("Stream list refreshed");
    }

    // fills the newsList
    void RSSImp::refreshNewsList() {
      if(rssmanager.getNbStreams()>0) {
	RssStream* currentstream = rssmanager.getStream(getNumStreamSelected());
	listNews->clear();
        unsigned short currentStreamSize = currentstream->getListSize();
	for(unsigned short i=0; i<currentStreamSize; ++i) {
	  new QListWidgetItem(currentstream->getItem(i)->getTitle(), listNews);
	  if(currentstream->getItem(i)->isRead()){
	     listNews->item(i)->setData(Qt::ForegroundRole, QVariant(QColor("grey")));
	     listNews->item(i)->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere.png")));
	  } else {
	    listNews->item(i)->setData(Qt::DecorationRole, QVariant(QIcon(":/Icons/sphere2.png")));
	    listNews->item(i)->setData(Qt::ForegroundRole, QVariant(QColor("blue")));
	  }
	  if(i%2==0)
	    listNews->item(i)->setData(Qt::BackgroundRole, QVariant(QColor(0, 255, 255, 20)));
	}
      }
    }

    // display a news
    void RSSImp::refreshTextBrowser() {
      if(getNumStreamSelected()>=0 && listNews->currentRow()>=0) {
	RssItem* currentitem = rssmanager.getStream(getNumStreamSelected())->getItem(listNews->currentRow());
	textBrowser->setHtml(currentitem->getTitle()+" : \n"+currentitem->getDescription());
	currentitem->setRead();
      }
    }

    void RSSImp::updateLastRefreshedTimeForStreams() {
      unsigned int nbStreams = rssmanager.getNbStreams();
      for(unsigned int i=0; i<nbStreams; ++i){
        listStreams->topLevelItem(i)->setToolTip(0, QString("<b>")+tr("Description:")+QString("</b> ")+rssmanager.getStream(i)->getDescription()+QString("<br/><b>")+tr("url:")+QString("</b> ")+rssmanager.getStream(i)->getUrl()+QString("<br/><b>")+tr("Last refresh:")+QString("</b> ")+rssmanager.getStream(i)->getLastRefreshElapsedString());
      }
    }

    // show the number of news for a stream, his status and an icon
    void RSSImp::updateStreamName(const unsigned short& i, const unsigned short& type) {
      // icon has just been download
      if(type == ICON) {
	listStreams->topLevelItem(i)->setData(0,Qt::DecorationRole, QVariant(QIcon(rssmanager.getStream(i)->getIconPath())));
      }
      // on click, show the age of the stream
      if(type == LATENCY) {
	unsigned short nbitem = rssmanager.getStream(i)->getNbNonRead();
	listStreams->topLevelItem(i)->setText(0,rssmanager.getStream(i)->getAlias().toUtf8()+"  ("+QString::number(nbitem,10).toUtf8()+")");
	if(nbitem==0)
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("red")));
	else if(rssmanager.getStream(i)->getLastRefreshElapsed()>REFRESH_MAX_LATENCY)
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("orange")));
	else
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("green")));
	listStreams->topLevelItem(getNumStreamSelected())->setData(0,Qt::BackgroundRole, QVariant(QColor("white")));
      }
      // when news are refreshed, update all informations
      if(type == NEWS) {
	unsigned short nbitem = rssmanager.getStream(i)->getListSize();
	listStreams->topLevelItem(i)->setText(0,rssmanager.getStream(i)->getAlias().toUtf8()+"  ("+QString::number(nbitem,10).toUtf8()+")");
	if(nbitem==0)
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("red")));
	else if(rssmanager.getStream(i)->getLastRefreshElapsed()>REFRESH_MAX_LATENCY)
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("orange")));
	else
	  listStreams->topLevelItem(i)->setData(0,Qt::ForegroundRole, QVariant(QColor("green")));

	if(!rssmanager.getStream(i)->isRead())
	  listStreams->topLevelItem(i)->setData(0,Qt::BackgroundRole, QVariant(QColor(0, 255, 0, 20)));
	if(getNumStreamSelected()==i) {
	  listNews->clear();
	  refreshNewsList();
	}
	listStreams->topLevelItem(i)->setData(0,Qt::DecorationRole, QVariant(QIcon(rssmanager.getStream(i)->getIconPath())));
	// update description and display last refresh
        listStreams->topLevelItem(i)->setToolTip(0, QString("<b>")+tr("Description:")+QString("</b> ")+rssmanager.getStream(i)->getDescription()+QString("<br/><b>")+tr("url:")+QString("</b> ")+rssmanager.getStream(i)->getUrl()+QString("<br/><b>")+tr("Last refresh:")+QString("</b> ")+rssmanager.getStream(i)->getLastRefreshElapsedString());
      }
    }

    RSSImp::RSSImp() : QWidget(){
      setupUi(this);
      // icons of bottom buttons
      addStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
      delStream_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
      refreshAll_button->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));
      // icons of right-click menu
      actionDelete->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
      actionRename->setIcon(QIcon(QString::fromUtf8(":/Icons/log.png")));
      actionRefresh->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));
      actionCreate->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
      actionRefreshAll->setIcon(QIcon(QString::fromUtf8(":/Icons/refresh.png")));

      connect(listStreams, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayRSSListMenu(const QPoint&)));
      connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteStream()));
      connect(actionRename, SIGNAL(triggered()), this, SLOT(renameStream()));
      connect(actionRefresh, SIGNAL(triggered()), this, SLOT(refreshStream()));
      connect(actionCreate, SIGNAL(triggered()), this, SLOT(createStream()));
      connect(actionRefreshAll, SIGNAL(triggered()), this, SLOT(refreshAllStreams()));
      connect(&refreshTimeTimer, SIGNAL(timeout()), this, SLOT(updateLastRefreshedTimeForStreams()));
      connect(&rssmanager, SIGNAL(streamNeedRefresh(const unsigned short&, const unsigned short&)), this, SLOT(updateStreamName(const unsigned short&, const unsigned short&)));
      refreshTimeTimer.start(60000); // 1min
      refreshStreamList();
      refreshTextBrowser();
      qDebug("RSSImp constructed");
    }

    RSSImp::~RSSImp(){
    }

    short RSSImp::getNumStreamSelected(){
      return listStreams->indexOfTopLevelItem(listStreams->currentItem());
    }

