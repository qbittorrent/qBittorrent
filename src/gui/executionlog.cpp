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

#include <QListWidgetItem>
#include <QLabel>
#include <QDateTime>
#include <QColor>
#include <QPalette>
#include "executionlog.h"
#include "ui_executionlog.h"
#include "core/logger.h"
#include "guiiconprovider.h"
#include "loglistwidget.h"

ExecutionLog::ExecutionLog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ExecutionLog)
    , m_msgList(new LogListWidget(MAX_LOG_MESSAGES))
    , m_peerList(new LogListWidget(MAX_LOG_MESSAGES))
{
    ui->setupUi(this);

    ui->tabConsole->setTabIcon(0, GuiIconProvider::instance()->getIcon("view-calendar-journal"));
    ui->tabConsole->setTabIcon(1, GuiIconProvider::instance()->getIcon("view-filter"));
    ui->tabGeneral->layout()->addWidget(m_msgList);
    ui->tabBan->layout()->addWidget(m_peerList);

    const Logger* const logger = Logger::instance();
    foreach (const Log::Msg& msg, logger->getMessages())
        addLogMessage(msg);
    foreach (const Log::Peer& peer, logger->getPeers())
        addPeerMessage(peer);
    connect(logger, SIGNAL(newLogMessage(const Log::Msg &)), SLOT(addLogMessage(const Log::Msg &)));
    connect(logger, SIGNAL(newLogPeer(const Log::Peer &)), SLOT(addPeerMessage(const Log::Peer &)));
}

ExecutionLog::~ExecutionLog()
{
    delete m_msgList;
    delete m_peerList;
    delete ui;
}

void ExecutionLog::addLogMessage(const Log::Msg &msg)
{
    QString text;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
    QColor color;

    switch (msg.type) {
    case Log::INFO:
        color.setNamedColor("blue");
        break;
    case Log::WARNING:
        color.setNamedColor("orange");
        break;
    case Log::CRITICAL:
        color.setNamedColor("red");
        break;
    default:
        color = QApplication::palette().color(QPalette::WindowText);
    }

    text = "<font color='grey'>" + time.toString(Qt::SystemLocaleShortDate) + "</font> - <font color='" + color.name() + "'>" + msg.message + "</font>";
    m_msgList->appendLine(text);
}

void ExecutionLog::addPeerMessage(const Log::Peer& peer)
{
    QString text;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);

    if (peer.blocked)
#if LIBTORRENT_VERSION_NUM < 10000
        text = "<font color='grey'>" + time.toString(Qt::SystemLocaleShortDate) + "</font> - " + tr("<font color='red'>%1</font> was blocked", "x.y.z.w was blocked").arg(peer.ip);
#else
        text = "<font color='grey'>" + time.toString(Qt::SystemLocaleShortDate) + "</font> - " + tr("<font color='red'>%1</font> was blocked %2", "x.y.z.w was blocked").arg(peer.ip).arg(peer.reason);
#endif
    else
        text = "<font color='grey'>" + time.toString(Qt::SystemLocaleShortDate) + "</font> - " + tr("<font color='red'>%1</font> was banned", "x.y.z.w was banned").arg(peer.ip);

    m_peerList->appendLine(text);
}
