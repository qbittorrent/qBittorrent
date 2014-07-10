/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2013  Nick Tiskov
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
 * Contact : daymansmail@gmail.com
 */

#include "statsdialog.h"
#include "ui_statsdialog.h"

#include "misc.h"

StatsDialog::StatsDialog(QWidget *parent) :   QDialog(parent), ui(new Ui::StatsDialog) {
  ui->setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  connect(ui->buttonOK, SIGNAL(clicked()), SLOT(close()));
  session = QBtSession::instance();
  updateUI();
  t = new QTimer(this);
  t->setInterval(1500);
  connect(t, SIGNAL(timeout()), SLOT(updateUI()));
  t->start();
  show();
}

StatsDialog::~StatsDialog() {
  t->stop();
  delete t;
  delete ui;
}

void StatsDialog::updateUI() {
  libtorrent::session* s = session->getSession();
  libtorrent::cache_status cache = s->get_cache_status();
  libtorrent::session_status ss = s->status();

  // Alltime DL/UL
  quint64 atd = session->getAlltimeDL();
  quint64 atu = session->getAlltimeUL();
  ui->labelAlltimeDL->setText(misc::friendlyUnit(atd));
  ui->labelAlltimeUL->setText(misc::friendlyUnit(atu));
  // Total waste (this session)
  ui->labelWaste->setText(misc::friendlyUnit(ss.total_redundant_bytes + ss.total_failed_bytes));
  // Global ratio
  ui->labelGlobalRatio->setText(
        ( atd > 0 && atu > 0 ) ?
          misc::accurateDoubleToString((qreal)atu / (qreal)atd, 2) :
          "-"
          );
  // Cache hits
  ui->labelCacheHits->setText(
        ( cache.blocks_read > 0 && cache.blocks_read_hit > 0 ) ?
          misc::accurateDoubleToString(100. * (qreal)cache.blocks_read_hit / (qreal)cache.blocks_read, 2) :
          "-"
          );
  // Buffers size
  ui->labelTotalBuf->setText(misc::friendlyUnit(cache.total_used_buffers * 16 * 1024));
  // Disk overload (100%) equivalent
  // From lt manual: disk_write_queue and disk_read_queue are the number of peers currently waiting on a disk write or disk read
  // to complete before it receives or sends any more data on the socket. It'a a metric of how disk bound you are.

  // num_peers is not reliable (adds up peers, which didn't even overcome tcp handshake)
  const std::vector<libtorrent::torrent_handle> torrents = session->getTorrents();
  std::vector<libtorrent::torrent_handle>::const_iterator iBegin = torrents.begin();
  std::vector<libtorrent::torrent_handle>::const_iterator iEnd = torrents.end();
  quint32 peers = 0;
  for ( ; iBegin < iEnd ; ++iBegin)
    peers += (*iBegin).status().num_peers;
  ui->labelWriteStarve->setText(
        ( ss.disk_write_queue > 0 && peers > 0 ) ?
          misc::accurateDoubleToString(100. * (qreal)ss.disk_write_queue / (qreal)peers, 2) + "%" :
          QString("0\%")
        );
  ui->labelReadStarve->setText(
        ( ss.disk_read_queue > 0 && peers > 0 ) ?
          misc::accurateDoubleToString(100. * (qreal)ss.disk_read_queue / (qreal)peers, 2) + "%" :
          QString("0\%")
      );
  // Disk queues
  ui->labelQueuedJobs->setText(QString::number(cache.job_queue_length));
  ui->labelJobsTime->setText(QString::number(cache.average_job_time));
  ui->labelQueuedBytes->setText(misc::friendlyUnit(cache.queued_bytes));

  // Total connected peers
  ui->labelPeers->setText(QString::number(ss.num_peers));
}
