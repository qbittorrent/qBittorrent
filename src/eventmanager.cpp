/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Ishan Arora and Christophe Dumez
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


#include "eventmanager.h"
#include "bittorrent.h"
#include "misc.h"
#include <QDebug>

EventManager::EventManager(QObject *parent, Bittorrent *BTSession)
    : QObject(parent), BTSession(BTSession)
{
}

QList<QVariantMap> EventManager::getEventList() const {
  return event_list.values();
}

void EventManager::addedTorrent(QTorrentHandle& h)
{
  modifiedTorrent(h);
}

void EventManager::deletedTorrent(QString hash)
{
  event_list.remove(hash);
}

void EventManager::modifiedTorrent(QTorrentHandle h)
{
  QString hash = h.hash();
  QVariantMap event;
  event["eta"] = QVariant(QString::fromUtf8("∞"));
  if(h.is_paused()) {
    if(h.is_seed())
      event["state"] = QVariant("pausedUP");
    else
      event["state"] = QVariant("pausedDL");
  } else {
    if(BTSession->isQueueingEnabled() && h.is_queued()) {
      if(h.is_seed())
        event["state"] = QVariant("queuedUP");
      else
        event["state"] = QVariant("queuedDL");
    } else {
      switch(h.state())
      {
      case torrent_status::finished:
      case torrent_status::seeding:
        if(h.upload_payload_rate() > 0) {
          event["state"] = QVariant("seeding");
        } else {
          event["state"] = QVariant("stalledUP");
        }
        break;
      case torrent_status::allocating:
      case torrent_status::checking_files:
      case torrent_status::queued_for_checking:
      case torrent_status::checking_resume_data:
        if(h.is_seed()) {
          event["state"] = QVariant("checkingUP");
        } else {
          event["state"] = QVariant("checkingDL");
        }
        break;
      case torrent_status::downloading:
      case torrent_status::downloading_metadata:
        if(h.download_payload_rate() > 0)
          event["state"] = QVariant("downloading");
        else
          event["state"] = QVariant("stalledDL");
        event["eta"] = misc::userFriendlyDuration(BTSession->getETA(hash));
        break;
                        default:
        qDebug("No status, should not happen!!! status is %d", h.state());
        event["state"] = QVariant();
      }
    }
  }
  event["name"] = QVariant(h.name());
  event["size"] = QVariant(misc::friendlyUnit(h.actual_size()));
  event["progress"] = QVariant(h.progress());
  event["dlspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.download_payload_rate())));
  if(BTSession->isQueueingEnabled()) {
    if(h.queue_position() >= 0)
      event["priority"] = QVariant(QString::number(h.queue_position()));
    else
      event["priority"] = "*";
  } else {
    event["priority"] = "*";
  }
  event["upspeed"] = QVariant(tr("%1/s", "e.g. 120 KiB/s").arg(misc::friendlyUnit(h.upload_payload_rate())));
  QString seeds = QString::number(h.num_seeds());
  if(h.num_complete() > 0)
    seeds += " ("+QString::number(h.num_complete())+")";
  event["num_seeds"] = QVariant(seeds);
  QString leechs = QString::number(h.num_peers()-h.num_seeds());
  if(h.num_incomplete() > 0)
    leechs += " ("+QString::number(h.num_incomplete())+")";
  event["num_leechs"] = QVariant(leechs);
  event["seed"] = QVariant(h.is_seed());
  event["ratio"] = QVariant(QString::number(BTSession->getRealRatio(hash), 'f', 1));
  event["hash"] = QVariant(hash);
  event_list[hash] = event;
}
