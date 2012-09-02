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

#include "misc.h"
#include "fs_utils.h"
#include "torrentcontentmodelitem.h"
#include <QDebug>

TorrentContentModelItem::TorrentContentModelItem(const libtorrent::file_entry &f,
                                                 TorrentContentModelItem *parent,
                                                 int file_index):
  m_parentItem(parent), m_type(TFILE), m_fileIndex(file_index), m_totalDone(0)
{
  Q_ASSERT(parent);

#if LIBTORRENT_VERSION_MINOR >= 16
  QString name = fsutils::fileName(misc::toQStringU(f.path.c_str()));
#else
  QString name = misc::toQStringU(f.path.filename());
#endif
  // Do not display incomplete extensions
  if (name.endsWith(".!qB"))
    name.chop(4);

  m_itemData << name
             << QVariant((qulonglong)f.size)
             << 0.
             << prio::NORMAL;

  /* Update parent */
  m_parentItem->appendChild(this);
  m_parentItem->updateSize();
}

TorrentContentModelItem::TorrentContentModelItem(QString name, TorrentContentModelItem *parent):
  m_parentItem(parent), m_type(FOLDER), m_totalDone(0)
{
  // Do not display incomplete extensions
  if (name.endsWith(".!qB"))
    name.chop(4);
  m_itemData << name
             << 0.
             << 0.
             << prio::NORMAL;

  /* Update parent */
  m_parentItem->appendChild(this);
}

TorrentContentModelItem::TorrentContentModelItem(const QList<QVariant>& data):
  m_parentItem(0), m_type(ROOT), m_itemData(data), m_totalDone(0)
{
  Q_ASSERT(data.size() == 4);
}

TorrentContentModelItem::~TorrentContentModelItem()
{
  qDeleteAll(m_childItems);
}

TorrentContentModelItem::FileType TorrentContentModelItem::getType() const
{
  return m_type;
}

int TorrentContentModelItem::getFileIndex() const
{
  Q_ASSERT(m_type == TFILE);
  return m_fileIndex;
}

void TorrentContentModelItem::deleteAllChildren()
{
  Q_ASSERT(m_type == ROOT);
  qDeleteAll(m_childItems);
  m_childItems.clear();
}

const QList<TorrentContentModelItem*>& TorrentContentModelItem::children() const
{
  return m_childItems;
}

QString TorrentContentModelItem::getName() const
{
  return m_itemData.at(COL_NAME).toString();
}

void TorrentContentModelItem::setName(const QString& name)
{
  Q_ASSERT(m_type != ROOT);
  m_itemData.replace(COL_NAME, name);
}

qulonglong TorrentContentModelItem::getSize() const
{
  return m_itemData.value(COL_SIZE).toULongLong();
}

void TorrentContentModelItem::setSize(qulonglong size)
{
  Q_ASSERT (m_type != ROOT);
  if (getSize() == size)
    return;
  m_itemData.replace(COL_SIZE, (qulonglong)size);
  m_parentItem->updateSize();
}

void TorrentContentModelItem::updateSize()
{
  if (m_type == ROOT)
    return;
  Q_ASSERT(m_type == FOLDER);
  qulonglong size = 0;
  foreach (TorrentContentModelItem* child, m_childItems) {
    if (child->getPriority() != prio::IGNORED)
      size += child->getSize();
  }
  setSize(size);
}

void TorrentContentModelItem::setProgress(qulonglong done)
{
  Q_ASSERT (m_type != ROOT);
  if (getPriority() == 0) return;
  m_totalDone = done;
  qulonglong size = getSize();
  Q_ASSERT(m_totalDone <= size);
  qreal progress;
  if (size > 0)
    progress = m_totalDone/(float)size;
  else
    progress = 1.;
  Q_ASSERT(progress >= 0. && progress <= 1.);
  m_itemData.replace(COL_PROGRESS, progress);
  m_parentItem->updateProgress();
}

qulonglong TorrentContentModelItem::getTotalDone() const
{
  return m_totalDone;
}

float TorrentContentModelItem::getProgress() const
{
  Q_ASSERT (m_type != ROOT);
  if (getPriority() == 0)
    return -1;
  qulonglong size = getSize();
  if (size > 0)
    return m_totalDone / (float) getSize();
  return 1.;
}

void TorrentContentModelItem::updateProgress()
{
  if (m_type == ROOT) return;
  Q_ASSERT(m_type == FOLDER);
  m_totalDone = 0;
  foreach (TorrentContentModelItem* child, m_childItems) {
    if (child->getPriority() > 0)
      m_totalDone += child->getTotalDone();
  }
  //qDebug("Folder: total_done: %llu/%llu", total_done, getSize());
  Q_ASSERT(m_totalDone <= getSize());
  setProgress(m_totalDone);
}

int TorrentContentModelItem::getPriority() const
{
  return m_itemData.value(COL_PRIO).toInt();
}

void TorrentContentModelItem::setPriority(int new_prio, bool update_parent)
{
  Q_ASSERT(new_prio != prio::PARTIAL || m_type == FOLDER); // PARTIAL only applies to folders
  const int old_prio = getPriority();
  if (old_prio == new_prio) return;
  qDebug("setPriority(%s, %d)", qPrintable(getName()), new_prio);
  // Reset progress if priority is 0
  if (new_prio == 0) {
    setProgress(0);
  }

  m_itemData.replace(COL_PRIO, new_prio);

  // Update parent
  if (update_parent && m_parentItem) {
    qDebug("Updating parent item");
    m_parentItem->updateSize();
    m_parentItem->updateProgress();
    m_parentItem->updatePriority();
  }

  // Update children
  if (new_prio != prio::PARTIAL && !m_childItems.empty()) {
    qDebug("Updating children items");
    foreach (TorrentContentModelItem* child, m_childItems) {
      // Do not update the parent since
      // the parent is causing the update
      child->setPriority(new_prio, false);
    }
  }
  if (m_type == FOLDER) {
    updateSize();
    updateProgress();
  }
}

// Only non-root folders use this function
void TorrentContentModelItem::updatePriority()
{
  if (m_type == ROOT) return;
  Q_ASSERT(m_type == FOLDER);
  if (m_childItems.isEmpty()) return;
  // If all children have the same priority
  // then the folder should have the same
  // priority
  const int prio = m_childItems.first()->getPriority();
  for (int i=1; i<m_childItems.size(); ++i) {
    if (m_childItems.at(i)->getPriority() != prio) {
      setPriority(prio::PARTIAL);
      return;
    }
  }
  // All child items have the same priority
  // Update own if necessary
  if (prio != getPriority())
    setPriority(prio);
}

TorrentContentModelItem* TorrentContentModelItem::childWithName(const QString& name) const
{
  foreach (TorrentContentModelItem* child, m_childItems) {
    if (child->getName() == name)
      return child;
  }
  return 0;
}

bool TorrentContentModelItem::isFolder() const
{
  return (m_type==FOLDER);
}

void TorrentContentModelItem::appendChild(TorrentContentModelItem* item)
{
  Q_ASSERT(item);
  Q_ASSERT(m_type != TFILE);
  int i=0;
  for (i=0; i<m_childItems.size(); ++i) {
    QString newchild_name = item->getName();
    if (QString::localeAwareCompare(newchild_name, m_childItems.at(i)->getName()) < 0)
      break;
  }
  m_childItems.insert(i, item);
}

TorrentContentModelItem* TorrentContentModelItem::child(int row) const
{
  //Q_ASSERT(row >= 0 && row < childItems.size());
  return m_childItems.value(row, 0);
}

int TorrentContentModelItem::childCount() const
{
  return m_childItems.count();
}

int TorrentContentModelItem::columnCount() const
{
  return m_itemData.count();
}

QVariant TorrentContentModelItem::data(int column) const
{
  if (column == COL_PROGRESS && m_type != ROOT)
      return getProgress();
  return m_itemData.value(column);
}

int TorrentContentModelItem::row() const
{
  if (m_parentItem)
    return m_parentItem->children().indexOf(const_cast<TorrentContentModelItem*>(this));
  return 0;
}

TorrentContentModelItem* TorrentContentModelItem::parent() const
{
  return m_parentItem;
}
