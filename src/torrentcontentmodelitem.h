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
enum FilePriority {IGNORED=0, NORMAL=1, HIGH=2, MAXIMUM=7, MIXED=-1};
}

class TorrentContentModelFolder;

class TorrentContentModelItem {
public:
  enum TreeItemColumns {COL_NAME, COL_SIZE, COL_PROGRESS, COL_PRIO, NB_COL};
  enum ItemType { FileType, FolderType };

  TorrentContentModelItem(TorrentContentModelFolder* parent);
  virtual ~TorrentContentModelItem();

  inline bool isRootItem() const { return !m_parentItem; }
  TorrentContentModelFolder* parent() const;
  virtual ItemType itemType() const = 0;

  QString name() const;
  void setName(const QString& name);

  qulonglong size() const;
  qulonglong totalDone() const;

  float progress() const;

  int priority() const;
  virtual void setPriority(int new_prio, bool update_parent = true) = 0;

  int columnCount() const;
  QVariant data(int column) const;
  int row() const;

protected:
  TorrentContentModelFolder* m_parentItem;
  // Root item members
  QList<QVariant> m_itemData;
  // Non-root item members
  QString m_name;
  qulonglong m_size;
  int m_priority;
  qulonglong m_totalDone;
};

#endif // TORRENTCONTENTMODELITEM_H
