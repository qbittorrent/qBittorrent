/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Luke Memet (lukemmtt)
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

#include "macosshiftclickhandler.h"

#include <QMouseEvent>
#include <QTreeView>

MacOSShiftClickHandler::MacOSShiftClickHandler(QTreeView *treeView)
    : QObject(treeView)
    , m_treeView {treeView}
{
    treeView->installEventFilter(this);
}

bool MacOSShiftClickHandler::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_treeView) && (event->type() == QEvent::MouseButtonPress))
    {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton)
            return false;

        const QModelIndex clickedIndex = m_treeView->indexAt(mouseEvent->position().toPoint());
        if (!clickedIndex.isValid())
            return false;

        const Qt::KeyboardModifiers modifiers = mouseEvent->modifiers();
        const bool shiftPressed = modifiers.testFlag(Qt::ShiftModifier);

        if (shiftPressed && m_lastClickedIndex.isValid())
        {
            const QItemSelection selection(m_lastClickedIndex, clickedIndex);
            const bool commandPressed = modifiers.testFlag(Qt::ControlModifier);
            if (commandPressed)
                m_treeView->selectionModel()->select(selection, (QItemSelectionModel::Select | QItemSelectionModel::Rows));
            else
                m_treeView->selectionModel()->select(selection, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
            m_treeView->selectionModel()->setCurrentIndex(clickedIndex, QItemSelectionModel::NoUpdate);
            return true;
        }

        if (!modifiers.testFlags(Qt::AltModifier | Qt::MetaModifier))
            m_lastClickedIndex = clickedIndex;
    }

    return QObject::eventFilter(watched, event);
}
