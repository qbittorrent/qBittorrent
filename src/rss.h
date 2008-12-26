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
#include <QDateTime>
#include <QCryptographicHash>

#include "misc.h"
#include "downloadThread.h"

class RssManager;
class RssStream;
class RssItem;

static const char shortDay[][4] = {
    "Mon", "Tue", "Wed",
    "Thu", "Fri", "Sat",
    "Sun"
};
static const char longDay[][10] = {
    "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday",
    "Sunday"
};
static const char shortMonth[][4] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec"
};
static const char longMonth[][10] = {
    "January", "February", "March",
    "April", "May", "June",
    "July", "August", "September",
    "October", "November", "December"
};

// Item of a rss stream, single information
class RssItem : public QObject {
  Q_OBJECT
  private:

    QString title;
    QString link;
    QString description;
    QString image;
    QString author;
    QDateTime date;
    QString hash;
    bool read;
    QString downloadLink;

  protected:
    // Ported to Qt4 from KDElibs4
    QDateTime parseDate(const QString &string) {
      QString str = string.trimmed();
      if (str.isEmpty())
          return QDateTime();

      int nyear  = 6;   // indexes within string to values
      int nmonth = 4;
      int nday   = 2;
      int nwday  = 1;
      int nhour  = 7;
      int nmin   = 8;
      int nsec   = 9;
      // Also accept obsolete form "Weekday, DD-Mon-YY HH:MM:SS ±hhmm"
      QRegExp rx("^(?:([A-Z][a-z]+),\\s*)?(\\d{1,2})(\\s+|-)([^-\\s]+)(\\s+|-)(\\d{2,4})\\s+(\\d\\d):(\\d\\d)(?::(\\d\\d))?\\s+(\\S+)$");
      QStringList parts;
      if (!str.indexOf(rx)) {
        // Check that if date has '-' separators, both separators are '-'.
        parts = rx.capturedTexts();
        bool h1 = (parts[3] == QLatin1String("-"));
        bool h2 = (parts[5] == QLatin1String("-"));
        if (h1 != h2)
          return QDateTime();
      } else {
        // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
        rx = QRegExp("^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$");
        if (str.indexOf(rx))
            return QDateTime();
        nyear  = 7;
        nmonth = 2;
        nday   = 3;
        nwday  = 1;
        nhour  = 4;
        nmin   = 5;
        nsec   = 6;
        parts = rx.capturedTexts();
      }
      bool ok[4];
      int day    = parts[nday].toInt(&ok[0]);
      int year   = parts[nyear].toInt(&ok[1]);
      int hour   = parts[nhour].toInt(&ok[2]);
      int minute = parts[nmin].toInt(&ok[3]);
      if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
        return QDateTime();
      int second = 0;
      if (!parts[nsec].isEmpty()) {
        second = parts[nsec].toInt(&ok[0]);
        if (!ok[0])
          return QDateTime();
      }
      bool leapSecond = (second == 60);
      if (leapSecond)
        second = 59;   // apparently a leap second - validate below, once time zone is known
      int month = 0;
      for ( ;  month < 12  &&  parts[nmonth] != shortMonth[month];  ++month) ;
      int dayOfWeek = -1;
      if (!parts[nwday].isEmpty()) {
        // Look up the weekday name
        while (++dayOfWeek < 7  &&  shortDay[dayOfWeek] != parts[nwday]) ;
        if (dayOfWeek >= 7)
          for (dayOfWeek = 0;  dayOfWeek < 7  &&  longDay[dayOfWeek] != parts[nwday];  ++dayOfWeek) ;
      }
//       if (month >= 12 || dayOfWeek >= 7
//       ||  (dayOfWeek < 0  &&  format == RFCDateDay))
//         return QDateTime;
      int i = parts[nyear].size();
      if (i < 4) {
        // It's an obsolete year specification with less than 4 digits
        year += (i == 2  &&  year < 50) ? 2000 : 1900;
      }

      // Parse the UTC offset part
      int offset = 0;           // set default to '-0000'
      bool negOffset = false;
      if (parts.count() > 10) {
        rx = QRegExp("^([+-])(\\d\\d)(\\d\\d)$");
        if (!parts[10].indexOf(rx)) {
          // It's a UTC offset ±hhmm
          parts = rx.capturedTexts();
          offset = parts[2].toInt(&ok[0]) * 3600;
          int offsetMin = parts[3].toInt(&ok[1]);
          if (!ok[0] || !ok[1] || offsetMin > 59)
            return QDateTime();
          offset += offsetMin * 60;
          negOffset = (parts[1] == QLatin1String("-"));
          if (negOffset)
            offset = -offset;
        } else {
          // Check for an obsolete time zone name
          QByteArray zone = parts[10].toLatin1();
          if (zone.length() == 1  &&  isalpha(zone[0])  &&  toupper(zone[0]) != 'J')
            negOffset = true;    // military zone: RFC 2822 treats as '-0000'
          else if (zone != "UT" && zone != "GMT") {    // treated as '+0000'
            offset = (zone == "EDT")                  ? -4*3600
                    : (zone == "EST" || zone == "CDT") ? -5*3600
                    : (zone == "CST" || zone == "MDT") ? -6*3600
                    : (zone == "MST" || zone == "PDT") ? -7*3600
                    : (zone == "PST")                  ? -8*3600
                    : 0;
            if (!offset) {
              // Check for any other alphabetic time zone
              bool nonalpha = false;
              for (int i = 0, end = zone.size();  i < end && !nonalpha;  ++i)
                nonalpha = !isalpha(zone[i]);
              if (nonalpha)
                return QDateTime();
              // TODO: Attempt to recognize the time zone abbreviation?
              negOffset = true;    // unknown time zone: RFC 2822 treats as '-0000'
            }
          }
        }
      }
      QDate qdate(year, month+1, day);   // convert date, and check for out-of-range
      if (!qdate.isValid())
          return QDateTime();
      QDateTime result(qdate, QTime(hour, minute, second));
      if (!result.isValid()
      ||  (dayOfWeek >= 0  &&  result.date().dayOfWeek() != dayOfWeek+1))
        return QDateTime();    // invalid date/time, or weekday doesn't correspond with date
      if (!offset) {
        result.setTimeSpec(Qt::UTC);
      }
      if (leapSecond) {
        // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
        // Convert the time to UTC and check that it is 00:00:00.
        if ((hour*3600 + minute*60 + 60 - offset + 86400*5) % 86400)   // (max abs(offset) is 100 hours)
          return QDateTime();    // the time isn't the last second of the day
      }
      return result;
    }

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
        else if (property.tagName() == "pubDate")
          date = parseDate(property.text());
        else if (property.tagName() == "author")
          author = property.text();
	property = property.nextSibling().toElement();
      }
      hash = QCryptographicHash::hash(QByteArray(title.toUtf8())+QByteArray(description.toUtf8()), QCryptographicHash::Md5);
    }

    ~RssItem(){
    }

    QString getTitle() const{
      return title;
    }

    QString getAuthor() const {
      return author;
    }

    QString getLink() const{
      return link;
    }

    QString getHash() const {
      return hash;
    }

    QString getDescription() const{
      if(description.isEmpty())
        return tr("No description available");
      return description;
    }

    QString getImage() const{
      return image;
    }

    QDateTime getDate() const {
      return date;
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

    bool itemAlreadyExists(QString hash) {
      RssItem * item;
      foreach(item, listItem) {
        if(item->getHash() == hash) return true;
      }
      return false;
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
      qDebug("getAlias() returned Alias: %s", (const char*)alias.toUtf8());
      return alias;
    }

    void setAlias(QString _alias){
      qDebug("setAlias() to %s", (const char*)_alias.toUtf8());
      alias = _alias;
    }

    // Return the alias if the stream has one, the url if it has no alias
    QString getAliasOrUrl() const{
      if(!alias.isEmpty()) {
        qDebug("getAliasOrUrl() returned alias: %s", (const char*)alias.toUtf8());
        return alias;
      }
      if(!title.isEmpty()) {
        qDebug("getAliasOrUrl() returned title: %s", (const char*)title.toUtf8());
        return title;
      }
      qDebug("getAliasOrUrl() returned url: %s", (const char*)url.toUtf8());
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

    void markAllAsRead() {
      RssItem *item;
      foreach(item, listItem){
        if(!item->isRead())
          item->setRead();
      }
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

    int getLastRefreshElapsed() const{
      if(!refreshed)
        return -1;
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
              RssItem * item = new RssItem(property);
              if(!itemAlreadyExists(item->getHash()))
                listItem.append(item);
              else
                delete item;
	    }
	    property = property.nextSibling().toElement();
	  }
	}
	channel = channel.nextSibling().toElement();
      }
      sortList();
      resizeList();
      return 0;
    }

    static void insertSortElem(QList<RssItem*> &list, RssItem *item) {
      int i = 0;
      while(i < list.size() && item->getDate() < list.at(i)->getDate()) {
        ++i;
      }
      list.insert(i, item);
    }

    void sortList() {
      QList<RssItem*> new_list;
      RssItem *item;
      foreach(item, listItem) {
        insertSortElem(new_list, item);
      }
      listItem = new_list;
    }

    void resizeList() {
      QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
      unsigned int max_articles = settings.value(QString::fromUtf8("Preferences/RSS/RSSMaxArticlesPerFeed"), 50).toInt();
      int excess = listItem.size() - max_articles;
      if(excess <= 0) return;
      for(int i=0; i<excess; ++i){
        delete listItem.takeLast();
      }
    }

    // existing and opening test after download
    short openRss(){
      qDebug("openRss() called");
      QDomDocument doc("Rss Seed");
      QFile fileRss(filePath);
      if(!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug("openRss error : open failed, no file or locked, %s", (const char*)filePath.toUtf8());
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
    unsigned int refreshInterval;

  signals:
    void feedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread);
    void feedIconChanged(QString url, QString icon_path);

  public slots :

    void processFinishedDownload(QString url, QString path) {
      if(url.endsWith("favicon.ico")){
        // Icon downloaded
       QImage fileIcon;
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
        if(stream->getLastRefreshElapsed() != -1 && stream->getLastRefreshElapsed() < (int)refreshInterval) return;
        qDebug("Refreshing old feed: %s...", (const char*)url.toUtf8());
        stream->setLoading(true);
        downloader->downloadUrl(url);
        if(!stream->hasCustomIcon()){
          downloader->downloadUrl(stream->getIconUrl());
        }
      }
      // See if refreshInterval has changed
      QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
      unsigned int new_refreshInterval = settings.value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toInt();
      if(new_refreshInterval != refreshInterval) {
        refreshInterval = new_refreshInterval;
        newsRefresher.start(refreshInterval*60000);
      }
    }

  public :
    RssManager(){
      downloader = new downloadThread(this);
      connect(downloader, SIGNAL(downloadFinished(QString, QString)), this, SLOT(processFinishedDownload(QString, QString)));
      connect(downloader, SIGNAL(downloadFailure(QString, QString)), this, SLOT(handleDownloadFailure(QString, QString)));
      loadStreamList();
      connect(&newsRefresher, SIGNAL(timeout()), this, SLOT(refreshOldFeeds()));
      QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
      refreshInterval = settings.value(QString::fromUtf8("Preferences/RSS/RSSRefresh"), 5).toInt();
      newsRefresher.start(refreshInterval*60000);
    }

    ~RssManager(){
      qDebug("Deleting RSSManager");
      saveStreamList();
      qDebug("Deleting all streams");
      qDeleteAll(streams);
      qDebug("Deleting downloader thread");
      delete downloader;
      qDebug("RSSManager deleted");
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
