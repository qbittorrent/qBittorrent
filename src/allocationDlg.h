/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2007  Christophe Dumez
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
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
    BandwidthAllocationDialog(QWidget *parent, bool uploadMode, bittorrent *BTSession, QStringList hashes): QDialog(parent), uploadMode(uploadMode){
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
        QString hash;
        foreach(hash, hashes){
          torrent_handle h = BTSession->getTorrentHandle(hash);
          if(!h.is_valid()){
            qDebug("Error: Invalid Handle!");
            continue;
          }else{
            handles << h;
          }
        }
        unsigned int nbTorrents = handles.size();
        if(!nbTorrents) close();
        int val;
        if(nbTorrents == 1){
          torrent_handle h = handles.at(0);
          if(uploadMode)
            val = h.upload_limit();
          else
            val = h.download_limit();
          qDebug("Bandwidth limit: %d", val);
          if(val > bandwidthSlider->maximum() || val < bandwidthSlider->minimum())
              val = -1;
          bandwidthSlider->setValue(val);
          if(val == -1) {
            limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
            kb_lbl->setText("");
          } else {
            limit_lbl->setText(QString(misc::toString(val).c_str()));
          }
        }else{
          qDebug("More than one torrent selected, no initilization");
          bandwidthSlider->setValue(-1);
          limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
          kb_lbl->setText("");
        }
      }else{
        // Global limit
        int val;
        session *s = BTSession->getSession();
        if(uploadMode)
          val = (int)(s->upload_rate_limit()/1024.);
        else
          val = (int)(s->download_rate_limit()/1024.);
        if(val == -1){
          bandwidthSlider->setValue(-1);
          limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
          kb_lbl->setText("");
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
      if(val == -1){
        limit_lbl->setText(tr("Unlimited", "Unlimited (bandwidth)"));
        kb_lbl->setText("");
      }else{
        limit_lbl->setText(QString(misc::toString(val).c_str()));
        kb_lbl->setText(tr("KiB/s"));
      }
    }

    void setBandwidth(){
      int val = bandwidthSlider->value();
      if(!global){
        torrent_handle h;
        if(uploadMode) {
          foreach(h, handles) {
            h.set_upload_limit(val*1024);
            qDebug("Setting upload limit");
          }
        } else {
          foreach(h, handles) {
            h.set_download_limit(val*1024);
            qDebug("Setting download limit");
          }
        }
      }else{
        QSettings settings("qBittorrent", "qBittorrent");
        session *s = BTSession->getSession();
        if(uploadMode){
          s->set_upload_rate_limit(val*1024);
          settings.setValue("Options/Main/UPLimit", val);
        }else{
          s->set_download_rate_limit(val*1024);
          settings.setValue("Options/Main/DLLimit", val);
        }
      }
      close();
    }

  private:
    bool uploadMode;
    bool global;
    bittorrent *BTSession;
    QList<torrent_handle> handles;
};

#endif
