/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
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
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QListWidgetItem>
#include <QLabel>
#include <QRegExp>
#include <QAction>
#include "loglistwidget.h"
#include "guiiconprovider.h"

LogListWidget::LogListWidget(int max_lines, QWidget *parent) :
  QListWidget(parent),
  m_maxLines(max_lines)
{
  // Allow multiple selections
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  // Context menu
  QAction *copyAct = new QAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy"), this);
  QAction *clearAct = new QAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Clear"), this);
  connect(copyAct, SIGNAL(triggered()), SLOT(copySelection()));
  connect(clearAct, SIGNAL(triggered()), SLOT(clearLog()));
  addAction(copyAct);
  addAction(clearAct);
  setContextMenuPolicy(Qt::ActionsContextMenu);
}

void LogListWidget::keyPressEvent(QKeyEvent *event)
{
  if (event->matches(QKeySequence::Copy)) {
    copySelection();
    return;
  }
  if (event->matches(QKeySequence::SelectAll)) {
    selectAll();
    return;
  }
}

void LogListWidget::appendLine(const QString &line)
{
  QListWidgetItem *item = new QListWidgetItem;
  // We need to use QLabel here to support rich text
  QLabel *lbl = new QLabel(line);
  lbl->setContentsMargins(4, 2, 4, 2);
  item->setSizeHint(lbl->sizeHint());
  insertItem(0, item);
  setItemWidget(item, lbl);
  const int nbLines = count();
  // Limit log size
  if (nbLines > m_maxLines)
    delete takeItem(nbLines - 1);
}

void LogListWidget::copySelection()
{
  static QRegExp html_tag("<[^>]+>");
  QList<QListWidgetItem*> items = selectedItems();
  QStringList strings;
  foreach (QListWidgetItem* it, items)
    strings << static_cast<QLabel*>(itemWidget(it))->text().replace(html_tag, "");

  QApplication::clipboard()->setText(strings.join("\n"));
}

void LogListWidget::clearLog() {
  clear();
}
