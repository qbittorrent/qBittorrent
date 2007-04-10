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
#include "ui_bandwidth_limit.h"
#include "misc.h"
#include "bittorrent.h"

using namespace libtorrent;

class BandwidthAllocationDialog : public QDialog, private Ui_bandwidth_dlg {
  public:
    BandwidthAllocationDialog(QWidget *parent, bool uploadMode, bittorrent *BTSession, QString hash): QDialog(parent), uploadMode(uploadMode){
      setupUi(this);
      setAttribute(Qt::WA_DeleteOnClose);
      this->BTSession = BTSession;
      if(uploadMode)
        lblTitle->setText(tr("Upload limit:"));
      else
        lblTitle->setText(tr("Download limit:"));
      connect(bandwidthSlider, SIGNAL(valueChanged(int)), this, SLOT(updateBandwidthLabel(int)));
      h = BTSession->getTorrentHandle(hash);
      if(!h.is_valid()){
        qDebug("Error: Invalid Handle!");
        close();
      }
      // TODO: Uncomment the following lines as soon as we upgrade
      // to libtorrent svn to correctly initialize the bandwidth slider.
//       if(uploadMode) {
//         int val = h.upload_limit();
//         if(val > bandwidthSlider->maximum() || val < bandwidthSlider->minimum())
//           val = -1;
//         bandwidthSlider->setValue(val);
//       } else {
//         int val = h.download_limit();
//         if(val > bandwidthSlider->maximum() || val < bandwidthSlider->minimum())
//           val = -1;
//         bandwidthSlider->setValue(val);
//       }
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

  private:
    bool uploadMode;
    bittorrent *BTSession;
    torrent_handle h;
};

#endif
