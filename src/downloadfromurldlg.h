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

#ifndef DOWNLOADFROMURL_H
#define DOWNLOADFROMURL_H

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QRegExp>
#include <QStringList>
#include <QClipboard>
#include "ui_downloadfromurldlg.h"

class downloadFromURL : public QDialog, private Ui::downloadFromURL{
  Q_OBJECT

  public:
    downloadFromURL(QWidget *parent): QDialog(parent) {
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      setModal(true);
      show();
      // Paste clipboard if there is an URL in it
      QString clip_txt = qApp->clipboard()->text();
      if (clip_txt.startsWith("http://", Qt::CaseInsensitive) || clip_txt.startsWith("https://", Qt::CaseInsensitive) || clip_txt.startsWith("ftp://", Qt::CaseInsensitive) || clip_txt.startsWith("magnet:", Qt::CaseInsensitive) || clip_txt.startsWith("bc://bt/", Qt::CaseInsensitive)) {
        textUrls->setText(clip_txt);
      }
    }

    ~downloadFromURL() {}

  signals:
    void urlsReadyToBeDownloaded(const QStringList& torrent_urls);

  public slots:
    void on_downloadButton_clicked() {
      close();
      QString urls = textUrls->toPlainText();
      QStringList url_list = urls.split(QString::fromUtf8("\n"));
      QString url;
      QStringList url_list_cleaned;
      foreach (url, url_list) {
        url = url.trimmed();
        if (!url.isEmpty()) {
          if (url_list_cleaned.indexOf(QRegExp(url, Qt::CaseInsensitive, QRegExp::FixedString)) < 0) {
            url_list_cleaned << url;
          }
        }
      }
      if (!url_list_cleaned.size()) {
        QMessageBox::critical(0, tr("No URL entered"), tr("Please type at least one URL."));
        return;
      }
      emit urlsReadyToBeDownloaded(url_list_cleaned);
      qDebug("Emitted urlsReadytobedownloaded signal");
    }

    void on_cancelButton_clicked() {
      close();
    }
};

#endif
