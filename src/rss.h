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
#define STREAM_MAX_ITEM 50
// 10min
#define STREAM_REFRESH_INTERVAL 600000

#include <QFile>
#include <QList>
#include <QTemporaryFile>
#include <QSettings>
#include <QDomDocument>
#include <QTime>
#include <QUrl>
#include <QTimer>
#include <QImage>
#include <QHash>

#include "misc.h"
#include "downloadThread.h"

#ifdef HAVE_MAGICK
  #include <Magick++.h>
  using namespace Magick;
#endif

class RssManager;
class RssStream;
class RssItem;

// Item of a rss stream, single information
class RssItem : public QObject {
  Q_OBJECT
  private:

    QString title;
    QString link;
    QString description;
    QString image;
    bool read;
    QString downloadLink;

  public:
    // public constructor
    RssItem(const QDomElement& properties) : read(false), downloadLink("none") {
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
      if(description.isEmpty())
        return tr("No description available");
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
    QString link;
    QString description;
    QString image;
    QString url;
    QString alias;
    QString filePath;
    QString iconPath;
    QList<RssItem*> listItem;
    QTime lastRefresh;
    bool read;
    bool refreshed;
    bool downloadFailure;
    bool currently_loading;

  public slots :
    // read and store the downloaded rss' informations
    void processDownloadedFile(QString file_path) {
      // delete the old file
      if(QFile::exists(filePath)) {
        QFile::remove(filePath);
      }
      filePath = file_path;
      downloadFailure = false;
      openRss();
      lastRefresh.start();
      refreshed = true;
    }

    void setDownloadFailed(){
      downloadFailure = true;
      lastRefresh.start();
    }

  public:
    RssStream(QString _url): url(_url), alias(""), iconPath(":/Icons/rss16.png"), refreshed(false), downloadFailure(false), currently_loading(false) {
      qDebug("RSSStream constructed");
    }

    ~RssStream(){
      removeAllItems();
      if(QFile::exists(filePath))
        QFile::remove(filePath);
      if(QFile::exists(iconPath) && !iconPath.startsWith(":/"))
        QFile::remove(iconPath);
    }

    // delete all the items saved
    void removeAllItems() {
      qDeleteAll(listItem);
      listItem.clear();
    }

    void setLoading(bool val) {
      currently_loading = val;
    }

    bool isLoading() {
      return currently_loading;
    }

    QString getTitle() const{
      return title;
    }

    QString getAlias() const{
      return alias;
    }

    void setAlias(QString _alias){
      alias = _alias;
    }

    // Return the alias if the stream has one, the url if it has no alias
    QString getAliasOrUrl() const{
      if(!alias.isEmpty())
        return alias;
      if(!title.isEmpty())
        return title;
      return url;
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

    QString getIconPath() const{
      if(downloadFailure)
        return ":/Icons/unavailable.png";
      return iconPath;
    }

    bool hasCustomIcon() const{
      return !iconPath.startsWith(":/");
    }
    
    void setIconPath(QString path) {
      iconPath = path;
    }

    RssItem* getItem(unsigned int index) const{
      return listItem.at(index);
    }

    unsigned int getNbNews() const{
      return listItem.size();
    }

    unsigned int getNbUnRead() const{
      unsigned int nbUnread=0;
      RssItem *item;
      foreach(item, listItem){
        if(!item->isRead())
          ++nbUnread;
      }
      return nbUnread;
    }

    QList<RssItem*> getNewsList() const{
      return listItem;
    }

    QString getLastRefreshElapsedString() const{
      if(!refreshed)
        return tr("Never");
      return tr("%1 ago", "10min ago").arg(misc::userFriendlyDuration((long)(lastRefresh.elapsed()/1000.)).replace("<", "&lt;"));
    }

    unsigned int getLastRefreshElapsed() const{
      if(!refreshed)
        return STREAM_REFRESH_INTERVAL+1;
      return lastRefresh.elapsed();
    }

    // download the icon from the adress
    QString getIconUrl() {
      QUrl siteUrl(url);
      return QString::fromUtf8("http://")+siteUrl.host()+QString::fromUtf8("/favicon.ico");
    }

  private:
    // read and create items from a rss document
    short readDoc(const QDomDocument& doc) {
      // is it a rss file ?
      QDomElement root = doc.documentElement();
      if(root.tagName() == QString::fromUtf8("html")){
	qDebug("the file is empty, maybe the url is invalid or the server is too busy");
	return -1;
      }
      else if(root.tagName() != QString::fromUtf8("rss")){
	qDebug("the file is not a rss stream, <rss> omitted: %s", root.tagName().toUtf8().data());
	return -1;
      }
      QDomNode rss = root.firstChild();
      QDomElement channel = root.firstChild().toElement();
      unsigned short listsize = getNbNews();
      for(unsigned short i=0; i<listsize; ++i) {
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
	      if(getNbNews() < STREAM_MAX_ITEM) {
	        listItem.append(new RssItem(property));
	      }
	    }
	    property = property.nextSibling().toElement();
	  }
	}
	channel = channel.nextSibling().toElement();
      }
      return 0;
    }

    // not actually used, it is used to resize the list of item AFTER the update, instead of delete it BEFORE, some troubles
    void resizeList() {
      unsigned short lastindex = 0;
      QString firstTitle = getItem(0)->getTitle();
      unsigned short listsize = getNbNews();
      for(unsigned short i=0; i<listsize; ++i) {
        if(getItem(i)->getTitle() == firstTitle)
	  lastindex = i;
      }
      for(unsigned short i=0; i<lastindex; ++i) {
	listItem.removeFirst();
      }
      while(getNbNews()>STREAM_MAX_ITEM) {
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
    QHash<QString, RssStream*> streams;
    downloadThread *downloader;
    QTimer newsRefresher;

  signals:
    void feedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread);
    void feedIconChanged(QString url, QString icon_path);

  public slots :

    void processFinishedDownload(QString url, QString path) {
      if(url.endsWith("favicon.ico")){
        // Icon downloaded
       QImage fileIcon;
#ifdef HAVE_MAGICK
        try{
          QFile::copy(path, path+".ico");
          Image image(QDir::cleanPath(path+".ico").toUtf8().data());
          // Convert to PNG since we can't read ICO format
          image.magick("PNG");
          // Resize to 16x16px
          image.sample(Geometry(16, 16));
          image.write(path.toUtf8().data());
          QFile::remove(path+".ico");
        }catch(Magick::Exception &error_){
          qDebug("favicon conversion to PNG failure: %s", error_.what());
        }
#endif
        if(fileIcon.load(path)) {
          QList<RssStream*> res = findFeedsWithIcon(url);
          RssStream* stream;
          foreach(stream, res){
            stream->setIconPath(path);
            if(!stream->isLoading())
              emit feedIconChanged(stream->getUrl(), stream->getIconPath());
          }
        }else{
          qDebug("Unsupported icon format at %s", (const char*)url.toUtf8());
        }
        return;
      }
      RssStream *stream = streams.value(url, 0);
      if(!stream){
        qDebug("This rss stream was deleted in the meantime, nothing to update");
        return;
      }
      stream->processDownloadedFile(path);
      stream->setLoading(false);
      // If the feed has no alias, then we use the title as Alias
      // this is more user friendly
      if(stream->getAlias().isEmpty()){
        if(!stream->getTitle().isEmpty())
          stream->setAlias(stream->getTitle());
      }
      emit feedInfosChanged(url, stream->getAliasOrUrl(), stream->getNbUnRead());
    }

    void handleDownloadFailure(QString url, QString reason) {
      if(url.endsWith("favicon.ico")){
        // Icon download failure
        qDebug("Could not download icon at %s, reason: %s", (const char*)url.toUtf8(), (const char*)reason.toUtf8());
        return;
      }
      RssStream *stream = streams.value(url, 0);
      if(!stream){
        qDebug("This rss stream was deleted in the meantime, nothing to update");
        return;
      }
      stream->setLoading(false);
      qDebug("Could not download Rss at %s, reason: %s", (const char*)url.toUtf8(), (const char*)reason.toUtf8());
      stream->setDownloadFailed();
      emit feedInfosChanged(url, stream->getAliasOrUrl(), stream->getNbUnRead());
    }

    void refreshOldFeeds(){
      RssStream *stream;
      foreach(stream, streams){
        QString url = stream->getUrl();
        if(stream->isLoading()) return;
        if(stream->getLastRefreshElapsed() < STREAM_REFRESH_INTERVAL) return;
        qDebug("Refreshing old feed: %s...", (const char*)url.toUtf8());
        stream->setLoading(true);
        downloader->downloadUrl(url);
        if(!stream->hasCustomIcon()){
          downloader->downloadUrl(stream->getIconUrl());
        }
      }
    }

  public :
    RssManager(){
      downloader = new downloadThread(this);
      connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processFinishedDownload(QString, QString)));
      connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
      loadStreamList();
      connect(&newsRefresher, SIGNAL(timeout()), this, SLOT(refreshOldFeeds()));
      newsRefresher.start(60000); // 1min
    }

    ~RssManager(){
      qDeleteAll(streams);
      delete downloader;
    }

    // load the list of the rss stream
    void loadStreamList(){
      QSettings settings("qBittorrent", "qBittorrent");
      QStringList streamsUrl = settings.value("Rss/streamList").toStringList();
      QStringList aliases =  settings.value("Rss/streamAlias").toStringList();
      if(streamsUrl.size() != aliases.size()){
        std::cerr << "Corrupted Rss list, not loading it\n";
        return;
      }
      QString url;
      unsigned int i = 0;
      foreach(url, streamsUrl){
        RssStream *stream = new RssStream(url);
        QString alias = aliases.at(i);
        if(!alias.isEmpty()) {
          stream->setAlias(alias);
        }
        streams[url] = stream;
        ++i;
      }
      qDebug("NB RSS streams loaded: %d", streamsUrl.size());
    }

    // save the list of the rss stream for the next session
    void saveStreamList(){
      QList<QPair<QString, QString> > streamsList;
      QStringList streamsUrl;
      QStringList aliases;
      RssStream *stream;
      foreach(stream, streams){
        streamsUrl << stream->getUrl();
        aliases << stream->getAlias();
      }
      QSettings settings("qBittorrent", "qBittorrent");
      settings.beginGroup("Rss");
        settings.setValue("streamList", streamsUrl);
        settings.setValue("streamAlias", aliases);
      settings.endGroup();
    }

    // add a stream to the manager
    void addStream(RssStream* stream){
      QString url = stream->getUrl();
      if(streams.contains(url)){
        qDebug("Not adding the Rss stream because it is already in the list");
        return;
      }
      streams[url] = stream;
      emit feedIconChanged(url, stream->getIconPath());
    }

    // add a stream to the manager
    RssStream* addStream(QString url){
      if(streams.contains(url)){
        qDebug("Not adding the Rss stream because it is already in the list");
        return 0;
      }
      RssStream* stream = new RssStream(url);
      streams[url] = stream;
      refresh(url);
      return stream;
    }

    // remove a stream from the manager
    void removeStream(RssStream* stream){
      QString url = stream->getUrl();
      Q_ASSERT(streams.contains(url));
      delete streams.take(url);
    }

    QList<RssStream*> findFeedsWithIcon(QString icon_url){
      QList<RssStream*> res;
      RssStream* stream;
      foreach(stream, streams){
        if(stream->getIconUrl() == icon_url)
          res << stream;
      }
      return res;
    }

    void removeStream(QString url){
      Q_ASSERT(streams.contains(url));
      delete streams.take(url);
    }

    // remove all the streams in the manager
    void removeAll(){
      qDeleteAll(streams);
      streams.clear();
    }

    // reload all the xml files from the web
    void refreshAll(){
      qDebug("Refreshing all rss feeds");
      RssStream *stream;
      foreach(stream, streams){
        QString url = stream->getUrl();
        if(stream->isLoading()) return;
        qDebug("Refreshing feed: %s...", (const char*)url.toUtf8());
        stream->setLoading(true);
        downloader->downloadUrl(url);
        if(!stream->hasCustomIcon()){
          downloader->downloadUrl(stream->getIconUrl());
        }
      }
    }

    void refresh(QString url) {
      Q_ASSERT(streams.contains(url));
      RssStream *stream = streams[url];
      if(stream->isLoading()) return;
      stream->setLoading(true);
      downloader->downloadUrl(url);
      if(!stream->hasCustomIcon()){
        downloader->downloadUrl(stream->getIconUrl());
      }else{
        qDebug("No need to download this feed's icon, it was already downloaded");
      }
    }

    // XXX: Used?
    unsigned int getNbFeeds() {
      return streams.size();
    }

    RssStream* getFeed(QString url){
      Q_ASSERT(streams.contains(url));
      return streams[url];
    }

    // Set an alias for a stream and save it for later
    void setAlias(QString url, QString newAlias) {
      Q_ASSERT(!newAlias.isEmpty());
      RssStream * stream = streams.value(url, 0);
      Q_ASSERT(stream != 0);
      stream->setAlias(newAlias);
      emit feedInfosChanged(url, stream->getAliasOrUrl(), stream->getNbUnRead());
    }

    // Return all the rss feeds we have
    QList<RssStream*> getRssFeeds() const {
      return streams.values();
    }

};

#endif
