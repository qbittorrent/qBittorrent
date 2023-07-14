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
#include "log/logfiltermodel.h"
#include "log/loglistview.h"
#include "log/logmodel.h"
#include "ui_executionlogwidget.h"
#include "uithememanager.h"

ExecutionLogWidget::ExecutionLogWidget(const Log::MsgTypes types, QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::ExecutionLogWidget)
    , m_messageFilterModel(new LogFilterModel(types, this))
{
    m_ui->setupUi(this);

    LogMessageModel *messageModel = new LogMessageModel(this);
    m_messageFilterModel->setSourceModel(messageModel);
    LogListView *messageView = new LogListView(this);
    messageView->setModel(m_messageFilterModel);
    messageView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(messageView, &LogListView::customContextMenuRequested, this, [this, messageView, messageModel]()
    {
        displayContextMenu(messageView, messageModel);
    });

    LogPeerModel *peerModel = new LogPeerModel(this);
    LogListView *peerView = new LogListView(this);
    peerView->setModel(peerModel);
    peerView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(peerView, &LogListView::customContextMenuRequested, this, [this, peerView, peerModel]()
    {
        displayContextMenu(peerView, peerModel);
    });

    m_ui->tabGeneral->layout()->addWidget(messageView);
    m_ui->tabBan->layout()->addWidget(peerView);

#ifndef Q_OS_MACOS
    m_ui->tabConsole->setTabIcon(0, UIThemeManager::instance()->getIcon(u"help-contents"_s, u"view-calendar-journal"_s));
    m_ui->tabConsole->setTabIcon(1, UIThemeManager::instance()->getIcon(u"ip-blocked"_s, u"view-filter"_s));
#endif
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::setMessageTypes(const Log::MsgTypes types)
{
    m_messageFilterModel->setMessageTypes(types);
}

void ExecutionLogWidget::displayContextMenu(const LogListView *view, const BaseLogModel *model) const
{
    QMenu *menu = new QMenu;
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // only show copy action if any of the row is selected
    if (view->currentIndex().isValid())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy")
            , view, &LogListView::copySelection);
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s), tr("Clear")
        , model, &BaseLogModel::reset);

    menu->popup(QCursor::pos());
}
