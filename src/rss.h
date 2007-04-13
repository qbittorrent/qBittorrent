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
 * Contact : chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#ifndef RSS_H
#define RSS_H

// MAX ITEM A STREAM
#define STREAM_MAX_ITEM 18
// FIXME: not used yet
#define GLOBAL_MAX_ITEM 150
// avoid crash if too many refresh
#define REFRESH_FREQ_MAX 5000

#include <QFile>
#include <QList>
#include <curl/curl.h>
#include <QTemporaryFile>
#include <QSettings>
#include <QDomDocument>
#include <QTime>

#include "misc.h"
#include "downloadThread.h"

class RssManager;
class RssStream;
class RssItem;

// Item of a rss stream, single information
class RssItem{
  private:

    QString title;
    QString link;
    QString description;
    QString image;
    bool read;
    QString downloadLink;

  public:
    // public constructor
    RssItem(const QDomElement& properties){
      read = false;
      downloadLink = "none";
      QDomElement property = properties.firstChild().toElement();
      while(!property.isNull()) {
	if (property.tagName() == "title")
	  title = property.text();
	else if (property.tagName() == "link")
	  link = property.text();
	else if (property.tagName() == "description")
	  description = property.text();
	else if (property.tagName() == "image")
	  image = property.text();
	property = property.nextSibling().toElement();
      }
    }

    ~RssItem(){
    }

    QString getTitle() const{
      return title;
    }

    QString getLink() const{
      return link;
    }

    QString getDescription() const{
      return description;
    }

    QString getImage() const{
      return image;
    }

    QString getDownloadLink() const{
      return downloadLink;
    }

    bool isRead() const{
      return read;
    }

    void setRead(){
      read = true;
    }
};

// Rss stream, loaded form an xml file
class RssStream : public QObject{
  Q_OBJECT

  private:
    QString title;
    QString alias;
    QString link;
    QString description;
    QString image;
    QString url;
    QString filePath;
    QList<RssItem*> listItem;
    downloadThread* downloader;
    QTime lastRefresh;
    bool read;

  signals:
    void refreshFinished(const QString& msg);
    
  public slots :
    // read and store the downloaded rss' informations
    void processDownloadedFile(const QString&, const QString& file_path, int return_code, const QString&) {
      // delete the former file
      if(QFile::exists(filePath)) {
        QFile::remove(filePath);
      }
      filePath = file_path;
      if(return_code){
        // Download failed
	qDebug("(download failure) "+file_path.toUtf8());
	if(QFile::exists(filePath)) {
	  QFile::remove(file_path);
	}
	emit refreshFinished(url);
	return;
      }
      openRss();
      emit refreshFinished(url);
    }

  public:
    RssStream(const QString& _url) {
      url = _url;
      alias = url;
      read = true;
      downloader = new downloadThread(this);
      connect(downloader, SIGNAL(downloadFinished(const QString&, const QString&, int, const QString&)), this, SLOT(processDownloadedFile(const QString&, const QString&, int, const QString&)));
      downloader->downloadUrl(url);
      lastRefresh.start();
    }

    ~RssStream(){
      removeAllItem();
      delete downloader;
      if(QFile::exists(filePath))
        QFile::remove(filePath);
    }

    // delete all the items saved
    void removeAllItem() {
      unsigned int listSize = listItem.size();
      for(unsigned int i=0; i<listSize; ++i){
	delete getItem(i);
      }
    }

    void refresh() {
      connect(downloader, SIGNAL(downloadFinished(const QString&, const QString&, int, const QString&)), this, SLOT(processDownloadedFile(const QString&, const QString&, int, const QString&)));
      downloader->downloadUrl(url);
      lastRefresh.start();
    }

    QString getTitle() const{
      return title;
    }

    QString getAlias() const{
      return alias;
    }

    //prefer the RssManager::setAlias, do not save the changed ones
    void setAlias(const QString& _alias){
      alias = _alias;
    }

    QString getLink() const{
      return link;
    }

    QString getUrl() const{
      return url;
    }

    QString getDescription() const{
      return description;
    }

    QString getImage() const{
      return image;
    }

    QString getFilePath() const{
      return filePath;
    }

    RssItem* getItem(unsigned int index) const{
      return listItem.at(index);
    }

    unsigned short getListSize() const{
      return listItem.size();
    }

    QList<RssItem*> getListItem() const{
      return listItem;
    }

    unsigned int getLastRefreshElapsed() const{
      return lastRefresh.elapsed();
    }

    bool isRead() const {
      return read;
    }

    void setRead() {
      read = true;
    }

  private:
    // read and create items from a rss document
    short readDoc(const QDomDocument& doc) {
      // is it a rss file ?
      QDomElement root = doc.documentElement();
      if(root.tagName() == "html"){
	qDebug("the file is empty, maybe the url is wrong or the server is too busy");
	return -1;
      }
      else if(root.tagName() != "rss"){
	qDebug("the file is not a rss stream, <rss> omitted"+root.tagName().toUtf8());
	return -1;
      }
      QDomNode rss = root.firstChild();
      QDomElement channel = root.firstChild().toElement();
      unsigned short listsize = getListSize();
      for(unsigned short i=0; i<listsize; i++) {
	listItem.removeLast();
      }
      
      while(!channel.isNull()) {
      // we are reading the rss'main info
	if (channel.tagName() == "channel") {
	  QDomElement property = channel.firstChild().toElement();
	  while(!property.isNull()) {
	    if (property.tagName() == "title") {
	      title = property.text();
              if(alias==getUrl())
		setAlias(title);
	    }
	    else if (property.tagName() == "link")
	      link = property.text();
	    else if (property.tagName() == "description")
	      description = property.text();
	    else if (property.tagName() == "image")
	      image = property.text();
	    else if(property.tagName() == "item") {
	      if(getListSize() < STREAM_MAX_ITEM) {
	        listItem.append(new RssItem(property));
	      }
	    }
	    property = property.nextSibling().toElement();
	  }
	  read = false;
	}
	channel = channel.nextSibling().toElement();
      }
      return 0;
    }

    // not actually used, it is used to resize the list of item AFTER the update, instead of delete it BEFORE, some troubles
    void resizeList() {
      unsigned short lastindex = 0;
      QString firstTitle = getItem(0)->getTitle();
      unsigned short listsize = getListSize();
      for(unsigned short i=0; i<listsize; i++) {
        if(getItem(i)->getTitle() == firstTitle)
	  lastindex = i;
      }
      for(unsigned short i=0; i<lastindex; i++) {
	listItem.removeFirst();
      }
      while(getListSize()>STREAM_MAX_ITEM) {
	listItem.removeAt(STREAM_MAX_ITEM);
      }
	
    }

    // existing and opening test after download
    short openRss(){
      QDomDocument doc("Rss Seed");
      QFile fileRss(filePath);
      if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
	qDebug("error : open failed, no file or locked, "+filePath.toUtf8());
	if(QFile::exists(filePath)) {
	 fileRss.remove();
	}
	return -1;
      }
      if(!doc.setContent(&fileRss)) {
	qDebug("can't read temp file, might be empty");
	fileRss.close();
	if(QFile::exists(filePath)) {
	 fileRss.remove();
	}
	return -1;
      }
      // start reading the xml
      short return_lecture = readDoc(doc);
      fileRss.close();
      if(QFile::exists(filePath)) {
        fileRss.remove();
      }
      return return_lecture;
    }
};

// global class, manage the whole rss stream
class RssManager : public QObject{
  Q_OBJECT
  
  private :
    QList<RssStream*> streamList;
    QStringList streamListUrl;

  signals:
    void streamNeedRefresh(const unsigned short&);

  public slots :
    void streamNeedRefresh(const QString& _url) {
      emit(streamNeedRefresh(hasStream(_url)));
    }
    
  public :
    RssManager(){
      loadStreamList();
    }

    ~RssManager(){
      saveStreamList();
      unsigned int streamListSize = streamList.size();
      for(unsigned int i=0; i<streamListSize; ++i){
	delete getStream(i);
      }
    }

    // load the list of the rss stream
    void loadStreamList(){
      QSettings settings("qBittorrent", "qBittorrent");
      settings.beginGroup("Rss");
      streamListUrl = settings.value("streamList").toStringList();
      QStringList streamListAlias = settings.value("streamAlias").toStringList();
      //check that both list have same size
      while(streamListUrl.size()>streamListAlias.size())
	streamListUrl.removeLast();
      while(streamListAlias.size()>streamListUrl.size())
	streamListAlias.removeLast();
      qDebug("NB RSS streams loaded: %d", streamListUrl.size());
      settings.endGroup();
      unsigned int streamListUrlSize = streamListUrl.size();
      for(unsigned int i=0; i<streamListUrlSize; ++i){
	RssStream *stream = new RssStream(streamListUrl.at(i));
	stream->setAlias(streamListAlias.at(i));
	streamList.append(stream);
	connect(stream, SIGNAL(refreshFinished(const QString&)), this, SLOT(streamNeedRefresh(const QString&)));
      }
    }

    // save the list of the rss stream for the next session
    void saveStreamList(){
      streamListUrl.clear();
      QStringList streamListAlias;
      for(unsigned short i=0; i<getNbStream(); i++) {
        streamListUrl.append(getStream(i)->getUrl());
	streamListAlias.append(getStream(i)->getAlias());
      }
      QSettings settings("qBittorrent", "qBittorrent");
      settings.beginGroup("Rss");
      settings.setValue("streamList", streamListUrl);
      settings.setValue("streamAlias", streamListAlias);
      settings.endGroup();
    }

    // add a stream to the manager
    void addStream(RssStream* stream){
      if(hasStream(stream) < 0){
	streamList.append(stream);
	streamListUrl.append(stream->getUrl());
	connect(stream, SIGNAL(refreshFinished(const QString&)), this, SLOT(streamNeedRefresh(const QString&)));
      }else{
        qDebug("Not adding the Rss stream because it is already in the list");
      }
    }

    // add a stream to the manager
    void addStream(QString url){
      // XXX : is it useful ?
      // completion of the address
      /*if(!url.endsWith(".xml")) {
	if(url.endsWith("/")) {
	  url.append("rss.xml");
        } else {
	  url.append("/rss.xml");
	}
      }*/

      if(hasStream(url) < 0) {
	RssStream* stream = new RssStream(url);
	streamList.append(stream);
	streamListUrl.append(url);
	connect(stream, SIGNAL(refreshFinished(const QString&)), this, SLOT(streamNeedRefresh(const QString&)));
      }else {
        qDebug("Not adding the Rss stream because it is already in the list");
      }
    }

    // remove a stream from the manager
    void removeStream(RssStream* stream){
      short index = hasStream(stream);
      if(index != -1){
        delete streamList.takeAt(index);
	streamListUrl.removeAt(index);
      }
    }

    // remove all the streams in the manager
    void removeAll(){
      QList<RssStream*> newStreamList;
      QStringList newUrlList;
      unsigned int streamListSize = streamList.size();
      for(unsigned int i=0; i<streamListSize; ++i){
	delete getStream(i);
      }
      streamList = newStreamList;
      streamListUrl = newUrlList;
    }

    // reload all the xml files from the web
    void refreshAll(){
      unsigned int streamListUrlSize = streamListUrl.size();
      for(unsigned int i=0; i<streamListUrlSize; ++i){
	getStream(i)->refresh();
	connect(getStream(i), SIGNAL(refreshFinished(const QString&)), this, SLOT(streamNeedRefresh(const QString&)));
      }
    }

    void refresh(int index) {
      if(index>=0 && index<getNbStream()) {
	if(getStream(index)->getLastRefreshElapsed()>REFRESH_FREQ_MAX) {
	  getStream(index)->refresh();
	  connect(getStream(index), SIGNAL(refreshFinished(const QString&)), this, SLOT(streamNeedRefresh(const QString&)));
	}
      }
    }

    // return the position index of a stream, if the manager owns it
    short hasStream(RssStream* stream) const{
      return hasStream(stream->getUrl());
    }

    short hasStream(const QString& url) const{
      return streamListUrl.indexOf(url);
    }

    RssStream* getStream(const unsigned short& index) const{
      return streamList.at(index);
    }

    unsigned short getNbStream() {
      return streamList.size();
    }

    //set an alias to an stream and save it for later
    void setAlias(unsigned short index, QString newAlias) {
      if(newAlias.length()>=2 && !getListAlias().contains(newAlias, Qt::CaseInsensitive)) {
	getStream(index)->setAlias(newAlias);
      }
    }

    QStringList getListAlias() {
      QStringList listAlias;
      for(unsigned short i=0; i<getNbStream(); i++) {
        listAlias.append(getStream(i)->getAlias());
      }
      return listAlias;
    }

};

#endif
