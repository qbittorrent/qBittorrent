/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#ifndef TORRENTMODEL_H
#define TORRENTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QDateTime>
#include <QIcon>
#include <QTimer>

#include "qtorrenthandle.h"

struct TorrentStatusReport {
  TorrentStatusReport(): nb_downloading(0), nb_seeding(0), nb_active(0), nb_inactive(0), nb_paused(0) {}
  uint nb_downloading; uint nb_seeding; uint nb_active; uint nb_inactive; uint nb_paused;
};

class TorrentModelItem : public QObject {
Q_OBJECT

public:
  enum State {STATE_DOWNLOADING, STATE_STALLED_DL, STATE_STALLED_UP, STATE_SEEDING, STATE_PAUSED_DL, STATE_PAUSED_UP, STATE_QUEUED_DL, STATE_QUEUED_UP, STATE_CHECKING_UP, STATE_CHECKING_DL, STATE_INVALID};
  enum Column {TR_NAME, TR_PRIORITY, TR_SIZE, TR_PROGRESS, TR_STATUS, TR_SEEDS, TR_PEERS, TR_DLSPEED, TR_UPSPEED, TR_ETA, TR_RATIO, TR_LABEL, TR_ADD_DATE, TR_SEED_DATE, TR_TRACKER, TR_DLLIMIT, TR_UPLIMIT, TR_AMOUNT_DOWNLOADED, TR_AMOUNT_LEFT, TR_TIME_ELAPSED, NB_COLUMNS};

public:
  TorrentModelItem(const QTorrentHandle& h);
  inline int columnCount() const { return NB_COLUMNS; }
  QVariant data(int column, int role = Qt::DisplayRole) const;
  bool setData(int column, const QVariant &value, int role = Qt::DisplayRole);
  inline QString hash() const { return m_hash; }

signals:
  void labelChanged(QString previous, QString current);

private:
  State state() const;

private:
  QTorrentHandle m_torrent;
  QDateTime m_addedTime;
  QDateTime m_seedTime;
  QString m_label;
  QString m_name;
  mutable QIcon m_icon;
  mutable QColor m_fgColor;
  QString m_hash; // Cached for safety reasons
};

class TorrentModel : public QAbstractListModel
{
  Q_OBJECT
  Q_DISABLE_COPY(TorrentModel)

public:
  explicit TorrentModel(QObject *parent = 0);
  ~TorrentModel();
  inline int rowCount(const QModelIndex& index = QModelIndex()) const { Q_UNUSED(index); return m_torrents.size(); }
  int columnCount(const QModelIndex &parent=QModelIndex()) const { Q_UNUSED(parent); return TorrentModelItem::NB_COLUMNS; }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::DisplayRole);
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  int torrentRow(const QString &hash) const;
  QString torrentHash(int row) const;
  void setRefreshInterval(int refreshInterval);
  TorrentStatusReport getTorrentStatusReport() const;
  Qt::ItemFlags flags(const QModelIndex &index) const;
  void populate();

signals:
  void torrentAdded(TorrentModelItem *torrentItem);
  void torrentAboutToBeRemoved(TorrentModelItem *torrentItem);
  void torrentChangedLabel(TorrentModelItem *torrentItem, QString previous, QString current);

private slots:
  void addTorrent(const QTorrentHandle& h);
  void removeTorrent(const QString &hash);
  void handleTorrentUpdate(const QTorrentHandle &h);
  void handleFinishedTorrent(const QTorrentHandle& h);
  void notifyTorrentChanged(int row);
  void forceModelRefresh();
  void handleTorrentLabelChange(QString previous, QString current);
  void handleTorrentAboutToBeRemoved(const QTorrentHandle & h);

private:
  void beginInsertTorrent(int row);
  void endInsertTorrent();
  void beginRemoveTorrent(int row);
  void endRemoveTorrent();

private:
  QList<TorrentModelItem*> m_torrents;
  int m_refreshInterval;
  QTimer m_refreshTimer;
};

#endif // TORRENTMODEL_H
