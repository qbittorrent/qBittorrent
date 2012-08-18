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

#ifndef RSSPARSER_H
#define RSSPARSER_H

#include "rssarticle.h"
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

struct ParsingJob;

class RssParser : public QThread
{
  Q_OBJECT

public:
  explicit RssParser(QObject *parent = 0);
  virtual ~RssParser();

signals:
  void newArticle(const QString& feedUrl, const QVariantHash& rssArticle);
  void feedTitle(const QString& feedUrl, const QString& title);
  void feedParsingFinished(const QString& feedUrl, const QString& error);

public slots:
  void parseRssFile(const QString& feedUrl, const QString& filePath);
  void clearFeedData(const QString& feedUrl);

protected:
  virtual void run();
  static QDateTime parseDate(const QString& string);
  void parseRssArticle(QXmlStreamReader& xml, const QString& feedUrl);
  void parseRSSChannel(QXmlStreamReader& xml, const QString& feedUrl);
  void parseRSS(const ParsingJob& job);
  void reportFailure(const ParsingJob& job, const QString& error);

private:
  bool m_running;
  QMutex m_mutex;
  QQueue<ParsingJob> m_queue;
  QWaitCondition m_waitCondition;
  QHash<QString/*feedUrl*/, QString/*lastBuildDate*/> m_lastBuildDates; // Optimization
};

#endif // RSSPARSER_H
