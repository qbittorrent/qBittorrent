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

#ifndef DOWNLOADFROMURL_H
#define DOWNLOADFROMURL_H

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QRegExp>
#include <QStringList>
#include "ui_downloadFromURL.h"

class downloadFromURL : public QDialog, private Ui::downloadFromURL{
  Q_OBJECT

  public:
    downloadFromURL(QWidget *parent): QDialog(parent){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      icon_lbl->setPixmap(QPixmap(QString::fromUtf8(":/Icons/skin/url.png")));
      show();
    }

    ~downloadFromURL(){}

  signals:
    void urlsReadyToBeDownloaded(const QStringList& torrent_urls);

  public slots:
    void on_downloadButton_clicked(){
      QString urls = textUrls->toPlainText();
      QStringList url_list = urls.split(QString::fromUtf8("\n"));
      QString url;
      QStringList url_list_cleaned;
      foreach(url, url_list){
        url = url.trimmed();
        if(!url.isEmpty()){
          if(url_list_cleaned.indexOf(QRegExp(url, Qt::CaseInsensitive, QRegExp::FixedString)) < 0){
            url_list_cleaned << url;
          }
        }
      }
      if(!url_list_cleaned.size()){
        QMessageBox::critical(0, tr("No URL entered"), tr("Please type at least one URL."));
        return;
      }
      emit urlsReadyToBeDownloaded(url_list_cleaned);
      qDebug("Emitted urlsReadytobedownloaded signal");
      close();
    }

    void on_cancelButton_clicked(){
      close();
    }
};

#endif
