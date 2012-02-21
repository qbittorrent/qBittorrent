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

#ifndef TORRENTCONTENTMODELITEM_H
#define TORRENTCONTENTMODELITEM_H

#include <QList>
#include <QVariant>
#include <libtorrent/torrent_info.hpp>

namespace prio {
enum FilePriority {IGNORED=0, NORMAL=1, HIGH=2, MAXIMUM=7, PARTIAL=-1};
}

class TorrentContentModelItem {
public:
  enum TreeItemColumns {COL_NAME, COL_SIZE, COL_PROGRESS, COL_PRIO, NB_COL};
  enum FileType {TFILE, FOLDER, ROOT};

  // File Construction
  TorrentContentModelItem(const libtorrent::torrent_info &t,
                          const libtorrent::file_entry &f,
                          TorrentContentModelItem *parent,
                          int file_index);
  // Folder constructor
  TorrentContentModelItem(QString name, TorrentContentModelItem *parent = 0);
  // Invisible root item constructor
  TorrentContentModelItem(const QList<QVariant>& data);

  ~TorrentContentModelItem();

  FileType getType() const;

  int getFileIndex() const;

  QString getName() const;
  void setName(const QString& name);

  qulonglong getSize() const;
  void setSize(qulonglong size);
  void updateSize();
  qulonglong getTotalDone() const;

  void setProgress(qulonglong done);
  float getProgress() const;
  void updateProgress();

  int getPriority() const;
  void setPriority(int new_prio, bool update_parent=true);
  void updatePriority();

  TorrentContentModelItem* childWithName(const QString& name) const;
  bool isFolder() const;

  void appendChild(TorrentContentModelItem *item);
  TorrentContentModelItem *child(int row);
  int childCount() const;
  int columnCount() const;
  QVariant data(int column) const;
  int row() const;

  TorrentContentModelItem *parent();
  void deleteAllChildren();
  const QList<TorrentContentModelItem*>& children() const;

private:
  TorrentContentModelItem *m_parentItem;
  FileType m_type;
  QList<TorrentContentModelItem*> m_childItems;
  QList<QVariant> m_itemData;
  int m_fileIndex;
  qulonglong m_totalDone;
};

#endif // TORRENTCONTENTMODELITEM_H
