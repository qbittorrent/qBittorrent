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

#define STREAM_MAX_ITEM 15
// TODO : not used yet
#define GLOBAL_MAX_ITEM 150

#ifndef RSS_H
#define RSS_H

#include <QFile>
#include <QList>
#include <curl/curl.h>
#include <QTemporaryFile>
#include <QSettings>
#include <QDomDocument>

#include "misc.h"
#include "downloadThread.h"

class RssItem;
class RssStream;
class RssManager;

// Item of a rss stream, single information
class RssItem{
  private:

    QString title;
    QString link;
    QString description;
    QString image;
    bool read;
    RssStream* parent;
    QString downloadLink;

  public:
    // public constructor
    RssItem(const QDomElement& properties, RssStream* _parent){
      read = false;
      parent = _parent;
      downloadLink = "none";
      QDomElement property=properties.firstChild().toElement();
      while(!property.isNull())
      {
	    // setters
	if (property.tagName() == "title")
	  title = property.text();
	else if (property.tagName() == "link")
	  link = property.text();
	else if (property.tagName() == "description")
	  description = property.text();
	else if (property.tagName() == "image")
	  image = property.text();
	    // build items
	property = property.nextSibling().toElement();
      }
      //displayItem();
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
      return this->read;
    }

    void setRead(){
      this->read = true;
    }

    RssStream* getParent() const{
      return parent;
    }

    void displayItem(){
      qDebug("        - "+getTitle().toUtf8()+" - "+getLink().toUtf8());
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

  public slots :

    // read and store the downloaded rss' informations
    void processDownloadedFile(const QString&, const QString& file_path, int return_code, const QString&){
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
	return;
      }
      this->openRss();
    }

  public:

    RssStream(const QString& _url){
      url = _url;
      alias = url;
      downloader = new downloadThread(this);
      connect(downloader, SIGNAL(downloadFinished(const QString&, const QString&, int, const QString&)), this, SLOT(processDownloadedFile(const QString&, const QString&, int, const QString&)));
      downloader->downloadUrl(url);
    }

    ~RssStream(){
      removeAllItem();
      delete downloader;
      if(QFile::exists(filePath))
        QFile::remove(filePath);
    }

    int refresh(){
      connect(downloader, SIGNAL(downloadFinished(const QString&, const QString&, int, const QString&)), this, SLOT(processDownloadedFile(const QString&, const QString&, int, const QString&)));
      downloader->downloadUrl(url);
      return 1;
    }

    // delete all the items saved
    void removeAllItem() {
      int i=0;
      while(i<listItem.size()) {
	delete getItem(i);
	i++;
      }
    }
    
    QString getTitle() const{
      return this->title;
    }

    QString getAlias() const{
      return this->alias;
    }

    //prefer the RssManager::setAlias, do not save the changed ones
    void setAlias(const QString& _alias){
      this->alias = _alias;
    }

    QString getLink() const{
      return this->link;
    }

    QString getUrl() const{
      return this->url;
    }

    QString getDescription() const{
      return this->description;
    }

    QString getImage() const{
      return this->image;
    }

    QString getFilePath() const{
      return this->filePath;
    }

    RssItem* getItem(unsigned short index) const{
      return this->listItem.at(index);
    }

    unsigned short getListSize() const{
      return this->listItem.size();
    }

    QList<RssItem*> getListItem() const{
      return this->listItem;
    }

    void displayStream(){
      qDebug("    # "+getTitle().toUtf8()+" - "+getUrl().toUtf8()+" - "+getAlias().toUtf8());
      for(int i=0; i<listItem.size(); i++){
	getItem(i)->displayItem();
      }
    }

  private:

    short read(const QDomDocument& doc) {
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
      while(!channel.isNull())
      {
      // we are reading the rss'main info
	if (channel.tagName() == "channel")
	{
	  QDomElement property=channel.firstChild().toElement();
	  while(!property.isNull())
	  {
	    // setters
	    if (property.tagName() == "title")
	      this->title = property.text();
	    else if (property.tagName() == "link")
	      this->link = property.text();
	    else if (property.tagName() == "description")
	      this->description = property.text();
	    else if (property.tagName() == "image")
	      this->image = property.text();
	    // build items
	    else if(property.tagName() == "item")
	    {
	      if(getListSize()<STREAM_MAX_ITEM) {
	       RssItem* item = new RssItem(property, this);
	       //add it to a list
	       this->listItem.append(item);
	      }
	    }
	    property = property.nextSibling().toElement();
	  }
	}
	channel = channel.nextSibling().toElement();
      }
      return 0;
    }

    // existing and opening test after download
    short openRss(){
      QDomDocument doc("Rss Seed");
      QFile fileRss(filePath);
      if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text))
      {
	qDebug("error : open failed, no file or locked, "+filePath.toUtf8());
	if(QFile::exists(filePath)) {
	 fileRss.remove();
	}
	return -1;
      }
      if(!doc.setContent(&fileRss))
      {
	qDebug("can't read temp file, might be empty");
	fileRss.close();
	if(QFile::exists(filePath)) {
	 fileRss.remove();
	}
	return -1;
      }
      // start reading the xml
      short return_lecture = read(doc);
      fileRss.close();
      if(QFile::exists(filePath)) {
        fileRss.remove();
      }
      return return_lecture;
    }
};

// global class, manage the whole rss stream
class RssManager{

  private :
    QList<RssStream*> streamList;
    QStringList streamListUrl;
    QStringList streamListAlias;

  public :
    RssManager(){
      loadStreamList();
    }

    ~RssManager(){
      saveStreamList();
      for(unsigned short i=0; i<streamList.size(); i++){
	delete getStream(i);
      }
    }

    // load the list of the rss stream
    void loadStreamList(){
      QSettings settings("qBittorrent", "qBittorrent");
      settings.beginGroup("Rss");
      this->streamListUrl = settings.value("streamList").toStringList();
      this->streamListAlias = settings.value("streamAlias").toStringList();
      settings.endGroup();
      for(unsigned short i=0; i<streamListUrl.size(); i++){
	RssStream *stream = new RssStream(this->streamListUrl.at(i));
	stream->setAlias(this->streamListAlias.at(i));
	this->streamList.append(stream);
      }
    }

    // save the list of the rss stream for the next session
    void saveStreamList(){
      QSettings settings("qBittorrent", "qBittorrent");
      settings.beginGroup("Rss");
      settings.setValue("streamList", this->streamListUrl);
      settings.setValue("streamAlias", this->streamListAlias);
      settings.endGroup();
    }

    // add a stream to the manager
    void addStream(RssStream* stream){
      if(hasStream(stream)<0){
	this->streamList.append(stream);
	this->streamListUrl.append(stream->getUrl());
	this->streamListAlias.append(stream->getUrl());
      }
    }

    // add a stream to the manager
    void addStream(QString url){
      // completion of the address
      if(!url.endsWith(".xml")){
	if(url.endsWith(QDir::separator()))
	  url.append("rss.xml");
	else
	{
	  url.append(QDir::separator());
	  url.append("rss.xml");
	}
      }
      
      if(hasStream(url)<0){
	RssStream* stream = new RssStream(url);
	this->streamList.append(stream);
	this->streamListUrl.append(url);
	this->streamListAlias.append(url);
      }
    }

    // remove a stream from the manager
    void removeStream(RssStream* stream){
      short index = hasStream(stream);
      if(index>=0){
	for(unsigned short i=0; i<streamList.size(); i++){
	  if(getStream(i)->getUrl()==stream->getUrl()){
	    delete streamList.at(i);
	    this->streamList.removeAt(i);
	  }
	}
	this->streamListUrl.removeAt(index);
	this->streamListAlias.removeAt(index);
      }
    }

    // remove all the streams in the manager
    void removeAll(){
      QList<RssStream*> newStreamList;
      QStringList newUrlList, newAliasList;
      for(unsigned short i=0; i<streamList.size(); i++){
	delete getStream(i);
      }
      this->streamList = newStreamList;
      this->streamListUrl = newUrlList;
      this->streamListAlias = newAliasList;
    }

    // reload all the xml files from the web
    void refreshAll(){
      QList<RssStream*> newStreamList;
      for(unsigned short i=0; i<streamList.size(); i++){
	delete getStream(i);
      }
      this->streamList = newStreamList;
      for(unsigned short i=0; i<streamListUrl.size(); i++){
	RssStream *stream = new RssStream(this->streamListUrl.at(i));
	stream->setAlias(this->streamListAlias.at(i));
	this->streamList.append(stream);
      }
    }

    void refresh(int index) {
      if(index>=0 && index<getNbStream()) {
	delete getStream(index);
	RssStream *stream = new RssStream(this->streamListUrl.at(index));
	stream->setAlias(this->streamListAlias.at(index));
	this->streamList.replace(index, stream);
      }
    }
    
    // return the position index of a stream, if the manager owns it
    short hasStream(RssStream* stream) const{
      QString url = stream->getUrl();
      return hasStream(url);
    }

    short hasStream(const QString& url) const{
      return streamListUrl.indexOf(url);
    }

    RssStream* getStream(const int& index) const{
      return streamList.at(index);
    }

    void displayManager(){
      for(unsigned short i=0; i<streamList.size(); i++){
	getStream(i)->displayStream();
      }
    }

    int getNbStream() {
      return streamList.size();
    }

    //set an alias to an stream and save it for later
    void setAlias(int index, QString newAlias) {
      if(newAlias.length()>=2 && !streamListAlias.contains(newAlias, Qt::CaseInsensitive)) {
	getStream(index)->setAlias(newAlias);
	streamListAlias.replace(index, newAlias);
      }
    }
      
};

#endif
