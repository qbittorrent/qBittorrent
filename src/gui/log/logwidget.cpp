/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  sledgehammer999 <hammered999@gmail.com>
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
 */

#include "logwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>

#include "guiiconprovider.h"
#include "logfiltermodel.h"
#include "logmodel.h"

LogWidget::LogWidget(const Log::MsgTypes types, QWidget *parent)
    : QTreeView(parent)
{
    setUniformRowHeights(true);

    m_listModel = new LogModel(this);
    m_filterModel = new LogFilterModel(types, this);
    m_filterModel->setSourceModel(m_listModel);
    setModel(m_filterModel);

    // Visual settings
    setRootIsDecorated(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setHeaderHidden(true);

    // Shows horizontal scrollbar when needed
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);

#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &LogWidget::displayListMenu);
}

void LogWidget::setMsgTypes(const Log::MsgTypes types)
{
    m_filterModel->setMsgTypes(types);
}

void LogWidget::displayListMenu(const QPoint &pos)
{
    QMenu listMenu(this);
    QAction *copyAct = new QAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy"), &listMenu);
    QAction *clearAct = new QAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Clear"), &listMenu);

    listMenu.addAction(copyAct);
    listMenu.addAction(clearAct);
    const QAction * const res = listMenu.exec(mapToGlobal(pos));

    if (res == copyAct)
        copySelection();
    else if (res == clearAct)
        m_listModel->reset();
}

void LogWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy))
        copySelection();
    else if (event->matches(QKeySequence::SelectAll))
        selectAll();

}

void LogWidget::copySelection() const
{
    QStringList list;
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    for (const QModelIndex index : selectedIndexes)
        list.append(index.data().toString());

    QApplication::clipboard()->setText(list.join('\n'));
}
