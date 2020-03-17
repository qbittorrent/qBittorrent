/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include "loglistwidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidgetItem>
#include <QRegularExpression>

#include "base/global.h"
#include "uithememanager.h"

LogListWidget::LogListWidget(const int maxLines, const Log::MsgTypes &types, QWidget *parent)
    : QListWidget(parent)
    , m_maxLines(maxLines)
    , m_types(types)
{
    // Allow multiple selections
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Context menu
    auto *copyAct = new QAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy"), this);
    auto *clearAct = new QAction(UIThemeManager::instance()->getIcon("edit-clear"), tr("Clear"), this);
    connect(copyAct, &QAction::triggered, this, &LogListWidget::copySelection);
    connect(clearAct, &QAction::triggered, this, &LogListWidget::clear);
    addAction(copyAct);
    addAction(clearAct);
    setContextMenuPolicy(Qt::ActionsContextMenu);
}

void LogListWidget::showMsgTypes(const Log::MsgTypes &types)
{
    m_types = types;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *tempItem = item(i);
        if (!tempItem) continue;

        Log::MsgType itemType = static_cast<Log::MsgType>(tempItem->data(Qt::UserRole).toInt());
        setRowHidden(i, !(m_types & itemType));
    }
}

void LogListWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy))
        copySelection();
    else
        QListWidget::keyPressEvent(event);
}

void LogListWidget::appendLine(const QString &line, const Log::MsgType &type)
{
    // We need to use QLabel here to support rich text
    auto *lbl = new QLabel(line);
    lbl->setTextFormat(Qt::RichText);
    lbl->setContentsMargins(4, 2, 4, 2);

    auto *item = new QListWidgetItem;
    item->setSizeHint(lbl->sizeHint());
    item->setData(Qt::UserRole, type);
    insertItem(0, item);
    setItemWidget(item, lbl);
    setRowHidden(0, !(m_types & type));

    const int nbLines = count();
    // Limit log size
    if (nbLines > m_maxLines)
        delete takeItem(nbLines - 1);
}

void LogListWidget::copySelection()
{
    const QRegularExpression htmlTag("<[^>]+>");

    QStringList strings;
    for (QListWidgetItem *it : asConst(selectedItems()))
        strings << static_cast<QLabel*>(itemWidget(it))->text().remove(htmlTag);

    QApplication::clipboard()->setText(strings.join('\n'));
}
