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

#include "executionlog.h"
#include "ui_executionlog.h"
#include "qbtsession.h"
#include "iconprovider.h"

ExecutionLog::ExecutionLog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ExecutionLog)
{
    ui->setupUi(this);
    ui->tabConsole->setTabIcon(0, IconProvider::instance()->getIcon("view-calendar-journal"));
    ui->tabConsole->setTabIcon(1, IconProvider::instance()->getIcon("view-filter"));
    ui->textConsole->setHtml(QBtSession::instance()->getConsoleMessages().join("<br>"));
    connect(QBtSession::instance(), SIGNAL(newConsoleMessage(QString)), SLOT(addLogMessage(QString)));
    ui->textBannedPeers->setHtml(QBtSession::instance()->getPeerBanMessages().join("<br>"));
    connect(QBtSession::instance(), SIGNAL(newBanMessage(QString)), SLOT(addBanMessage(QString)));
}

ExecutionLog::~ExecutionLog()
{
  delete ui;
}

void ExecutionLog::addLogMessage(const QString &msg)
{
   ui->textConsole->setHtml(msg+ui->textConsole->toHtml());
}

void ExecutionLog::addBanMessage(const QString &msg)
{
   ui->textBannedPeers->setHtml(msg+ui->textBannedPeers->toHtml());
}
