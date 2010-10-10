/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez, Arnaud Demaiziere
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
 * Contact: chris@qbittorrent.org, arnaud@qbittorrent.org
 */

#ifndef RSS_H
#define RSS_H

#include <QFile>
#include <QList>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QTime>
#include <QUrl>
#include <QTimer>
#include <QImage>
#include <QDateTime>
#include <QTimer>
#include <QUrl>

#include "misc.h"
#include "feeddownloader.h"
#include "qbtsession.h"
#include "downloadthread.h"

#if QT_VERSION >= 0x040500
#include <QHash>
#else
#include <QMap>
#define QHash QMap
#define toHash toMap
#endif

class RssManager;
class RssFile; // Folder or Stream
class RssFolder;
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

class RssFile: public QObject {
  Q_OBJECT

public:
  enum FileType {STREAM, FOLDER};

  RssFile(): QObject() {}
  virtual ~RssFile() {}

  virtual unsigned int getNbUnRead() const = 0;
  virtual FileType getType() const = 0;
  virtual QString getName() const = 0;
  virtual QString getID() const = 0;
  virtual void removeAllItems() = 0;
  virtual void rename(QString new_name) = 0;
  virtual void markAllAsRead() = 0;
  virtual RssFolder* getParent() const = 0;
  virtual void setParent(RssFolder*) = 0;
  virtual void refresh() = 0;
  virtual void removeAllSettings() = 0;
  virtual QList<RssItem*> getNewsList() const = 0;
  virtual QList<RssItem*> getUnreadNewsList() const = 0;
  QStringList getPath() const {
    QStringList path;
    if(getParent()) {
      path = ((RssFile*)getParent())->getPath();
      path.append(getID());
    }
    return path;
  }
};

// Item of a rss stream, single information
class RssItem: public QObject {
  Q_OBJECT
private:
  RssStream* parent;
  QString id;
  QString title;
  QString torrent_url;
  QString news_link;
  QString description;
  QDateTime date;
  QString author;


  bool is_valid;
  bool read;

protected:
  // Ported to Qt4 from KDElibs4
  QDateTime parseDate(const QString &string) {
    QString str = string.trimmed();
    if (str.isEmpty())
      return QDateTime::currentDateTime();

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
        return QDateTime::currentDateTime();
    } else {
      // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
      rx = QRegExp("^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$");
      if (str.indexOf(rx))
        return QDateTime::currentDateTime();
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
    const int day    = parts[nday].toInt(&ok[0]);
    int year   = parts[nyear].toInt(&ok[1]);
    const int hour   = parts[nhour].toInt(&ok[2]);
    const int minute = parts[nmin].toInt(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
      return QDateTime::currentDateTime();
    int second = 0;
    if (!parts[nsec].isEmpty()) {
      second = parts[nsec].toInt(&ok[0]);
      if (!ok[0])
        return QDateTime::currentDateTime();
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
      year += (i == 2  &&  year < 50) ? 2000: 1900;
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
      return QDateTime::currentDateTime();
    QDateTime result(qdate, QTime(hour, minute, second));
    if (!result.isValid()
      ||  (dayOfWeek >= 0  &&  result.date().dayOfWeek() != dayOfWeek+1))
      return QDateTime::currentDateTime();    // invalid date/time, or weekday doesn't correspond with date
    if (!offset) {
      result.setTimeSpec(Qt::UTC);
    }
    if (leapSecond) {
      // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
      // Convert the time to UTC and check that it is 00:00:00.
      if ((hour*3600 + minute*60 + 60 - offset + 86400*5) % 86400)   // (max abs(offset) is 100 hours)
        return QDateTime::currentDateTime();    // the time isn't the last second of the day
    }
    return result;
  }

public:
  // public constructor
  RssItem(RssStream* parent, QXmlStreamReader& xml): parent(parent), read(false) {
    is_valid = false;
    torrent_url = QString::null;
    news_link = QString::null;
    title = QString::null;
    while(!xml.atEnd()) {
      xml.readNext();

      if(xml.isEndElement() && xml.name() == "item")
        break;

      if(xml.isStartElement()) {
        if(xml.name() == "title") {
          title = xml.readElementText();
        }
        else if(xml.name() == "enclosure") {
          if(xml.attributes().value("type") == "application/x-bittorrent") {
            torrent_url = xml.attributes().value("url").toString();
          }
        }
        else if(xml.name() == "link") {
          news_link = xml.readElementText();
          if(id.isEmpty())
            id = news_link;
        }
        else if(xml.name() == "description") {
          description = xml.readElementText();
        }
        else if(xml.name() == "pubDate") {
          date = parseDate(xml.readElementText());
        }
        else if(xml.name() == "author") {
          author = xml.readElementText();
        }
        else if(xml.name() == "guid") {
          id = xml.readElementText();
        }
      }
    }
    if(!id.isEmpty())
      is_valid = true;
  }

  RssItem(RssStream* parent, QString _id, QString _title, QString _torrent_url, QString _news_link, QString _description, QDateTime _date, QString _author, bool _read):
      parent(parent), id(_id), title(_title), torrent_url(_torrent_url), news_link(_news_link), description(_description), date(_date), author(_author), read(_read){
    if(id.isEmpty())
      id = news_link;
    if(!id.isEmpty()) {
      is_valid = true;
    } else {
      std::cerr << "ERROR: an invalid RSS item was saved" << std::endl;
      is_valid = false;
    }
  }

  ~RssItem(){
  }

  bool has_attachment() const {
    return !torrent_url.isEmpty();
  }

  QString getId() const { return id; }

  QHash<QString, QVariant> toHash() const {
    QHash<QString, QVariant> item;
    item["title"] = title;
    item["id"] = id;
    item["torrent_url"] = torrent_url;
    item["news_link"] = news_link;
    item["description"] = description;
    item["date"] = date;
    item["author"] = author;
    item["read"] = read;
    return item;
  }

  static RssItem* fromHash(RssStream* parent, const QHash<QString, QVariant> &h) {
    return new RssItem(parent, h.value("id", "").toString(), h["title"].toString(), h["torrent_url"].toString(), h["news_link"].toString(),
                       h["description"].toString(), h["date"].toDateTime(), h["author"].toString(), h["read"].toBool());
  }

  RssStream* getParent() const {
    return parent;
  }

  bool isValid() const {
    return is_valid;
  }

  QString getTitle() const{
    return title;
  }

  QString getAuthor() const {
    return author;
  }

  QString getTorrentUrl() const{
    return torrent_url;
  }

  QString getLink() const {
    return news_link;
  }

  QString getDescription() const{
    if(description.isEmpty())
      return tr("No description available");
    return description;
  }

  QDateTime getDate() const {
    return date;
  }

  bool isRead() const{
    return read;
  }

  void setRead(){
    read = true;
  }
};

// Rss stream, loaded form an xml file
class RssStream: public RssFile, public QHash<QString, RssItem*> {
  Q_OBJECT

private:
  RssFolder *parent;
  RssManager *rssmanager;
  Bittorrent *BTSession;
  QString title;
  QString link;
  QString description;
  QString image;
  QString url;
  QString alias;
  QString filePath;
  QString iconPath;
  bool read;
  bool refreshed;
  bool downloadFailure;
  bool currently_loading;

public slots:
  void processDownloadedFile(QString file_path);
  void setDownloadFailed();

public:
  RssStream(RssFolder* parent, RssManager *rssmanager, Bittorrent *BTSession, QString _url);
  ~RssStream();
  RssFolder* getParent() const { return parent; }
  void setParent(RssFolder* _parent) { parent = _parent; }
  FileType getType() const;
  void refresh();
  QString getID() const { return url; }
  void removeAllItems();
  void removeAllSettings();
  bool itemAlreadyExists(QString hash);
  void setLoading(bool val);
  bool isLoading();
  QString getTitle() const;
  void rename(QString _alias);
  QString getName() const;
  QString getLink() const;
  QString getUrl() const;
  QString getDescription() const;
  QString getImage() const;
  QString getFilePath() const;
  QString getIconPath() const;
  bool hasCustomIcon() const;
  void setIconPath(QString path);
  RssItem* getItem(QString name) const;
  unsigned int getNbNews() const;
  void markAllAsRead();
  unsigned int getNbUnRead() const;
  QList<RssItem*> getNewsList() const;
  QList<RssItem*> getUnreadNewsList() const;
  QString getIconUrl();

private:
  short readDoc(QIODevice* device);
  void resizeList();
  short openRss();
};

class RssFolder: public RssFile, public QHash<QString, RssFile*> {
  Q_OBJECT

private:
  RssFolder *parent;
  RssManager *rssmanager;
  downloadThread *downloader;
  Bittorrent *BTSession;
  QString name;

public:
  RssFolder(RssFolder *parent, RssManager *rssmanager, Bittorrent *BTSession, QString name);
  ~RssFolder();
  RssFolder* getParent() const { return parent; }
  void setParent(RssFolder* _parent) { parent = _parent; }
  unsigned int getNbUnRead() const;
  FileType getType() const;
  RssStream* addStream(QString url);
  RssFolder* addFolder(QString name);
  QList<RssStream*> findFeedsWithIcon(QString icon_url) const;
  unsigned int getNbFeeds() const;
  QList<RssFile*> getContent() const;
  QList<RssStream*> getAllFeeds() const;
  QString getName() const;
  QString getID() const { return name; }
  bool hasChild(QString ID) { return this->contains(ID); }
  QList<RssItem*> getNewsList() const;
  QList<RssItem*> getUnreadNewsList() const;
  void removeAllSettings() {
    foreach(RssFile* child, values()) {
      child->removeAllSettings();
    }
  }

  void removeAllItems() {
    foreach(RssFile* child, values()) {
      child->removeAllItems();
    }
    qDeleteAll(values());
    clear();
  }

public slots:
  void refreshAll();
  void addFile(RssFile * item);
  void removeFile(QString ID);
  void refresh();
  void refreshStream(QString url);
  void processFinishedDownload(QString url, QString path);
  void handleDownloadFailure(QString url, QString reason);
  void rename(QString new_name);
  void markAllAsRead();
};

class RssManager: public RssFolder{
  Q_OBJECT

private:
  QTimer newsRefresher;
  unsigned int refreshInterval;
  Bittorrent *BTSession;

signals:
  void feedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread);
  void feedIconChanged(QString url, QString icon_path);

public slots:
  void loadStreamList();
  void saveStreamList();
  void forwardFeedInfosChanged(QString url, QString aliasOrUrl, unsigned int nbUnread);
  void forwardFeedIconChanged(QString url, QString icon_path);
  void moveFile(RssFile* file, RssFolder* dest_folder);
  void updateRefreshInterval(unsigned int val);

public:
  RssManager(Bittorrent *BTSession);
  ~RssManager();
  static void insertSortElem(QList<RssItem*> &list, RssItem *item) {
    int i = 0;
    while(i < list.size() && item->getDate() < list.at(i)->getDate()) {
      ++i;
    }
    list.insert(i, item);
  }

  static QList<RssItem*> sortNewsList(const QList<RssItem*>& news_list) {
    QList<RssItem*> new_list;
    foreach(RssItem *item, news_list) {
      insertSortElem(new_list, item);
    }
    return new_list;
  }

};

#endif
