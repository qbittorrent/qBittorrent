/*
 * Bittorrent Client using Qt and libtorrent.
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
 */

#include "executionlogwidget.h"

#include <QDateTime>
#include <QMenu>
#include <QPalette>

#include "base/global.h"
#include "base/logger.h"
#include "log/logfiltermodel.h"
#include "log/loglistview.h"
#include "log/logmodel.h"
#include "ui_executionlogwidget.h"
#include "uithememanager.h"

ExecutionLogWidget::ExecutionLogWidget(QWidget *parent, const Log::MsgTypes types)
    : QWidget(parent)
    , m_ui(new Ui::ExecutionLogWidget)
    , m_messageModel(new LogMessageModel(this))
    , m_peerModel(new LogPeerModel(this))
    , m_messageFilterModel(new LogFilterModel(types, this))
    , m_messageView(new LogListView(this))
    , m_peerView(new LogListView(this))
{
    m_ui->setupUi(this);

    m_messageFilterModel->setSourceModel(m_messageModel);
    m_messageView->setModel(m_messageFilterModel);
    m_messageView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_messageView, &LogListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        displayContextMenu(pos, m_messageView, m_messageModel);
    });

    m_peerView->setModel(m_peerModel);
    m_peerView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_peerView, &LogListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        displayContextMenu(pos, m_peerView, m_peerModel);
    });

    m_ui->tabGeneral->layout()->addWidget(m_messageView);
    m_ui->tabBan->layout()->addWidget(m_peerView);

#ifndef Q_OS_MACOS
    m_ui->tabConsole->setTabIcon(0, UIThemeManager::instance()->getIcon("view-calendar-journal"));
    m_ui->tabConsole->setTabIcon(1, UIThemeManager::instance()->getIcon("view-filter"));
#endif
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::setMsgTypes(const Log::MsgTypes types)
{
    m_messageFilterModel->setMsgTypes(types);
}

void ExecutionLogWidget::displayContextMenu(const QPoint &pos, const LogListView *view, const BaseLogModel *model) const
{
    QMenu *menu = new QMenu;
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // only show copy action if any of the row is selected
    if (view->currentIndex().isValid()) {
        const QAction *copyAct = menu->addAction(UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy"));
        connect(copyAct, &QAction::triggered, view, &LogListView::copySelection);
    }

    const QAction *clearAct = menu->addAction(UIThemeManager::instance()->getIcon("edit-clear"), tr("Clear"));
    connect(clearAct, &QAction::triggered, model, &BaseLogModel::reset);

    menu->popup(view->mapToGlobal(pos));
}
