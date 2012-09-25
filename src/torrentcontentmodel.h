/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006-2012  Christophe Dumez
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

#ifndef TORRENTCONTENTMODEL_H
#define TORRENTCONTENTMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVector>
#include <QVariant>
#include <libtorrent/torrent_info.hpp>
#include "torrentcontentmodelitem.h"

class TorrentContentModelFile;

class TorrentContentModel:  public QAbstractItemModel {
  Q_OBJECT

public:
  TorrentContentModel(QObject *parent = 0);
  ~TorrentContentModel();

  void updateFilesProgress(const std::vector<libtorrent::size_type>& fp);
  void updateFilesPriorities(const std::vector<int> &fprio);
  std::vector<int> getFilesPriorities() const;
  bool allFiltered() const;
  virtual int columnCount(const QModelIndex &parent=QModelIndex()) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
  TorrentContentModelItem::ItemType itemType(const QModelIndex& index) const;
  int getFileIndex(const QModelIndex& index);
  virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
  virtual QModelIndex parent(const QModelIndex& index) const;
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  void clear();
  void setupModelData(const libtorrent::torrent_info& t);

signals:
  void filteredFilesChanged();

public slots:
  void selectAll();
  void selectNone();

private:
  TorrentContentModelFolder* m_rootItem;
  QVector<TorrentContentModelFile*> m_filesIndex;
};

#endif // TORRENTCONTENTMODEL_H
