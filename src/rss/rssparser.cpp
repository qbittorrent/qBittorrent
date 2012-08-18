/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#include "rssparser.h"
#include "downloadthread.h"
#include <QDebug>
#include <QFile>
#include <QRegExp>
#include <QStringList>
#include <QVariant>

struct ParsingJob {
  QString feedUrl;
  QString filePath;
};

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

// Ported to Qt4 from KDElibs4
QDateTime RssParser::parseDate(const QString &string) {
  const QString str = string.trimmed();
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

RssParser::RssParser(QObject *parent) :
  QThread(parent), m_running(true)
{
  start();
}

RssParser::~RssParser()
{
  m_running = false;
  m_waitCondition.wakeOne();
  wait();
}

void RssParser::parseRssFile(const QString& feedUrl, const QString& filePath)
{
  qDebug() << Q_FUNC_INFO << feedUrl << filePath;
  m_mutex.lock();
  ParsingJob job = {feedUrl, filePath};
  m_queue.enqueue(job);
  // Wake up thread.
  if (m_queue.count() == 1) {
    qDebug() << Q_FUNC_INFO << "Waking up thread";
    m_waitCondition.wakeOne();
  }
  m_mutex.unlock();
}

void RssParser::clearFeedData(const QString &feedUrl)
{
  m_mutex.lock();
  m_lastBuildDates.remove(feedUrl);
  m_mutex.unlock();
}

void RssParser::run()
{
  while (m_running) {
    m_mutex.lock();
    if (!m_queue.empty()) {
      ParsingJob job = m_queue.dequeue();
      m_mutex.unlock();
      parseRSS(job);
    } else {
      qDebug() << Q_FUNC_INFO << "Thread is waiting.";
      m_waitCondition.wait(&m_mutex);
      qDebug() << Q_FUNC_INFO << "Thread woke up.";
      m_mutex.unlock();
    }
  }
}

void RssParser::parseRssArticle(QXmlStreamReader& xml, const QString& feedUrl)
{
  QVariantHash article;

  while(!xml.atEnd()) {
    xml.readNext();

    if(xml.isEndElement() && xml.name() == "item")
      break;

    if (xml.isStartElement()) {
      if (xml.name() == "title")
        article["title"] = xml.readElementText();
      else if (xml.name() == "enclosure") {
        if (xml.attributes().value("type") == "application/x-bittorrent")
          article["torrent_url"] = xml.attributes().value("url").toString();
      }
      else if (xml.name() == "link")
        article["news_link"] = xml.readElementText();
      else if (xml.name() == "description")
        article["description"] = xml.readElementText();
      else if (xml.name() == "pubDate")
        article["date"] = parseDate(xml.readElementText());
      else if (xml.name() == "author")
        article["author"] = xml.readElementText();
      else if (xml.name() == "guid")
        article["id"] = xml.readElementText();
    }
  }

  if (!article.contains("id")) {
    // Item does not have a guid, fall back to some other identifier
    const QString link = article.value("news_link").toString();
    if (!link.isEmpty())
      article["id"] = link;
    else {
      const QString title = article.value("title").toString();
      if (!title.isEmpty())
        article["id"] = title;
      else {
        qWarning() << "Item has no guid, link or title, ignoring it...";
        return;
      }
    }
  }

  emit newArticle(feedUrl, article);
}

void RssParser::parseRSSChannel(QXmlStreamReader& xml, const QString& feedUrl)
{
  qDebug() << Q_FUNC_INFO << feedUrl;
  Q_ASSERT(xml.isStartElement() && xml.name() == "channel");

  while(!xml.atEnd()) {
    xml.readNext();

    if (xml.isStartElement()) {
      if (xml.name() == "title") {
        QString title = xml.readElementText();
        emit feedTitle(feedUrl, title);
      }
      else if (xml.name() == "lastBuildDate") {
        QString lastBuildDate = xml.readElementText();
        if (!lastBuildDate.isEmpty()) {
          QMutexLocker locker(&m_mutex);
          if (m_lastBuildDates.value(feedUrl, "") == lastBuildDate) {
            qDebug() << "The RSS feed has not changed since last time, aborting parsing.";
            return;
          }
          m_lastBuildDates[feedUrl] = lastBuildDate;
        }
      }
      else if (xml.name() == "item") {
        parseRssArticle(xml, feedUrl);
      }
    }
  }
}

// read and create items from a rss document
void RssParser::parseRSS(const ParsingJob& job)
{
  qDebug() << Q_FUNC_INFO << job.feedUrl << job.filePath;
  QFile fileRss(job.filePath);
  if (!fileRss.open(QIODevice::ReadOnly | QIODevice::Text)) {
    reportFailure(job, tr("Failed to open downloaded RSS file."));
    return;
  }
  QXmlStreamReader xml(&fileRss);

  bool found_channel = false;
  while (xml.readNextStartElement()) {
    if (xml.name() == "rss") {
      // Find channels
      while (xml.readNextStartElement()) {
        if (xml.name() == "channel") {
          parseRSSChannel(xml, job.feedUrl);
          found_channel = true;
          break;
        } else {
          qDebug() << "Skip rss item: " << xml.name();
          xml.skipCurrentElement();
        }
      }
      break;
    } else {
      qDebug() << "Skip root item: " << xml.name();
      xml.skipCurrentElement();
    }
  }

  if (xml.hasError()) {
    reportFailure(job, xml.errorString());
    return;
  }

  if (!found_channel) {
    reportFailure(job, tr("Invalid RSS feed at %1.").arg(job.feedUrl));
    return;
  }

  // Clean up
  QFile::remove(job.filePath);
  emit feedParsingFinished(job.feedUrl, QString());
}

void RssParser::reportFailure(const ParsingJob& job, const QString& error)
{
  QFile::remove(job.filePath);
  emit feedParsingFinished(job.feedUrl, error);
}
