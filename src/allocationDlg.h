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

#ifndef BANDWIDTH_ALLOCATION_H
#define BANDWIDTH_ALLOCATION_H

#include <QDialog>
#include <QList>
#include <QSettings>
#include "ui_bandwidth_limit.h"
#include "misc.h"
#include "bittorrent.h"

using namespace libtorrent;

class BandwidthAllocationDialog : public QDialog, private Ui_bandwidth_dlg {
  Q_OBJECT

  public:
    BandwidthAllocationDialog(QWidget *parent, bool uploadMode, bittorrent *BTSession, QStringList hashes): QDialog(parent), uploadMode(uploadMode), hashes(hashes){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      qDebug("Bandwidth allocation dialog creation");
      this->BTSession = BTSession;
      if(hashes.size() == 0)
        global = true;
      else
        global = false;
      if(uploadMode)
        lblTitle->setText(tr("Upload limit:"));
      else
        lblTitle->setText(tr("Download limit:"));
      connect(bandwidthSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBandwidthLabel(int)));
      if(!global){
        unsigned int nbTorrents = hashes.size();
        if(!nbTorrents) close();
        int val = 0;
        int max = -1;
        if(nbTorrents == 1){
          QTorrentHandle h = BTSession->getTorrentHandle(hashes.at(0));
          if(uploadMode){
            if(h.upload_limit() > 0)
              val = (int)(h.upload_limit() / 1024.);
            if(BTSession->getSession()->upload_rate_limit() > 0)
              max = (int)(BTSession->getSession()->upload_rate_limit() / 1024.);
          }else{
            if(h.download_limit() > 0)
              val = (int)(h.download_limit() / 1024.);
            if(BTSession->getSession()->download_rate_limit() > 0){
              qDebug("there is a global download rate limit at: %d kb/s", (int)(BTSession->getSession()->download_rate_limit() / 1024.));
              max = (int)(BTSession->getSession()->download_rate_limit() / 1024.);
            }
          }
          if(max != -1)
            bandwidthSlider->setMaximum(max);
          qDebug("Bandwidth limit: %d", val);
          if(val > bandwidthSlider->maximum())
            val = bandwidthSlider->maximum();
          else if(val < bandwidthSlider->minimum())
              val = 0;
          bandwidthSlider->setValue(val);
          if(val == 0) {
            limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
            kb_lbl->setText(QString::fromUtf8(""));
          } else {
            limit_lbl->setText(misc::toQString(val));
          }
        }else{
          qDebug("More than one torrent selected, no initilization");
          bandwidthSlider->setValue(0);
          limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
          kb_lbl->setText(QString::fromUtf8(""));
        }
      }else{
        // Global limit
        int val = 0;
        session *s = BTSession->getSession();
        if(uploadMode){
          if(s->upload_rate_limit() > 0)
            val = (int)(s->upload_rate_limit()/1024.);
        }else{
          if(s->download_rate_limit() > 0)
            val = (int)(s->download_rate_limit()/1024.);
        }
        if(val == 0){
          bandwidthSlider->setValue(0);
          limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
          kb_lbl->setText(QString::fromUtf8(""));
        }else{
          bandwidthSlider->setValue(val);
        }
      }
      connect(buttonBox, SIGNAL(accepted()), this, SLOT(setBandwidth()));
      show();
    }

    ~BandwidthAllocationDialog(){
      qDebug("Deleting bandwidth allocation dialog");
    }

  protected slots:
    void updateBandwidthLabel(int val){
      if(val == 0){
        limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
        kb_lbl->setText(QString::fromUtf8(""));
      }else{
        limit_lbl->setText(misc::toQString(val));
        kb_lbl->setText(tr("KiB/s"));
      }
    }

    void setBandwidth(){
      qDebug("setBandwidth called");
      int val = bandwidthSlider->value();
      if(!global){
        QString hash;
        if(uploadMode) {
          foreach(hash, hashes) {
            if(!val)
              BTSession->setUploadLimit(hash, -1);
            else
              BTSession->setUploadLimit(hash, val*1024);
            qDebug("Setting upload limit");
          }
        } else {
          foreach(hash, hashes) {
            if(!val)
              BTSession->setDownloadLimit(hash, -1);
            else
              BTSession->setDownloadLimit(hash, val*1024);
            qDebug("Setting download limit");
          }
        }
      }else{
        QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
        session *s = BTSession->getSession();
        if(uploadMode){
          if(!val)
            s->set_upload_rate_limit(-1);
          else
            s->set_upload_rate_limit(val*1024);
          settings.setValue(QString::fromUtf8("Options/Main/UPLimit"), val);
        }else{
          if(!val)
            s->set_download_rate_limit(-1);
          else
            s->set_download_rate_limit(val*1024);
          settings.setValue(QString::fromUtf8("Options/Main/DLLimit"), val);
        }
      }
      close();
    }

  private:
    bool uploadMode;
    bool global;
    bittorrent *BTSession;
    QStringList hashes;
};

#endif
