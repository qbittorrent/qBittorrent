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

#include "torrentcontentmodelfile.h"
#include "torrentcontentmodelfolder.h"
#include "fs_utils.h"
#include "misc.h"

TorrentContentModelFile::TorrentContentModelFile(const libtorrent::file_entry& f,
                                                 TorrentContentModelFolder* parent,
                                                 int file_index)
  : TorrentContentModelItem(parent)
  , m_fileIndex(file_index)
{
  Q_ASSERT(parent);

#if LIBTORRENT_VERSION_MINOR >= 16
  m_name = fsutils::fileName(misc::toQStringU(f.path.c_str()));
#else
  m_name = misc::toQStringU(f.path.filename());
#endif

  // Do not display incomplete extensions
  if (m_name.endsWith(".!qB"))
    m_name.chop(4);

  m_size = (qulonglong)f.size;
}

int TorrentContentModelFile::fileIndex() const
{
  return m_fileIndex;
}

void TorrentContentModelFile::setPriority(int new_prio, bool update_parent)
{
  Q_ASSERT(new_prio != prio::MIXED);

  if (m_priority == new_prio)
    return;

  m_priority = new_prio;

  // Update parent
  if (update_parent)
    m_parentItem->updatePriority();
}

void TorrentContentModelFile::setProgress(qulonglong done)
{
  m_totalDone = done;
  Q_ASSERT(m_totalDone <= m_size);
}
