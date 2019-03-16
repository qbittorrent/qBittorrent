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

#include "guiiconprovider.h"
#include "log/logpeerwidget.h"
#include "log/logwidget.h"
#include "ui_executionlogwidget.h"

ExecutionLogWidget::ExecutionLogWidget(QWidget *parent, const Log::MsgTypes types)
    : QWidget(parent)
    , m_ui(new Ui::ExecutionLogWidget)
    , m_msgList(new LogWidget(types, this))
    , m_peerList(new LogPeerWidget(this))
{
    m_ui->setupUi(this);

#ifndef Q_OS_MAC
    m_ui->tabConsole->setTabIcon(0, GuiIconProvider::instance()->getIcon("view-calendar-journal"));
    m_ui->tabConsole->setTabIcon(1, GuiIconProvider::instance()->getIcon("view-filter"));
#endif
    m_ui->tabGeneral->layout()->addWidget(m_msgList);
    m_ui->tabBan->layout()->addWidget(m_peerList);
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::showMsgTypes(const Log::MsgTypes types)
{
    m_msgList->setMsgTypes(types);
}
