/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#ifndef DOWNLOADTHREAD_H
#define DOWNLOADTHREAD_H

#include <QThread>
#include <QFile>
#include <QTemporaryFile>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QStringList>
#include <curl/curl.h>

class subDownloadThread : public QThread {
  Q_OBJECT
  private:
    QString url;
    bool abort;

  public:
    subDownloadThread(QObject *parent, QString url);
    ~subDownloadThread();
    QString errorCodeToString(CURLcode status);

  signals:
    // For subthreads
    void downloadFinishedST(subDownloadThread* st, QString url, QString file_path);
    void downloadFailureST(subDownloadThread* st, QString url, QString reason);

  protected:
    void run();
};

class downloadThread : public QThread {
  Q_OBJECT

  private:
    QStringList url_list;
    QStringList downloading_list;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    QList<subDownloadThread*> subThreads;

  signals:
    void downloadFinished(QString url, QString file_path);
    void downloadFailure(QString url, QString reason);

  public:
    downloadThread(QObject* parent);

    ~downloadThread();

    void downloadUrl(QString url);
    void setProxy(QString IP, int port, QString username, QString password);

  protected:
    void run();

  protected slots:
    void propagateDownloadedFile(subDownloadThread* st, QString url, QString path);
    void propagateDownloadFailure(subDownloadThread* st, QString url, QString reason);
};

#endif
