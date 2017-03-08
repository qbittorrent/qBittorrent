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

#include <QListWidgetItem>
#include <QLabel>
#include <QDateTime>
#include <QColor>
#include <QPalette>

#include "guiiconprovider.h"
#include "loglistwidget.h"
#include "theme/colortheme.h"

#include "ui_executionlog.h"

ExecutionLog::ExecutionLog(QWidget *parent, const Log::MsgTypes &types)
    : QWidget(parent)
    , ui(new Ui::ExecutionLog)
    , m_peerList(new LogListWidget(MAX_LOG_MESSAGES))
{
    ui->setupUi(this);

    m_msgList = new LogListWidget(MAX_LOG_MESSAGES, Log::MsgTypes(types));

#ifndef Q_OS_MAC
    ui->tabConsole->setTabIcon(0, GuiIconProvider::instance()->getIcon("view-calendar-journal"));
    ui->tabConsole->setTabIcon(1, GuiIconProvider::instance()->getIcon("view-filter"));
#endif
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

void ExecutionLog::showMsgTypes(const Log::MsgTypes &types)
{
    m_msgList->showMsgTypes(types);
}

namespace
{
    QString coloredString(const QString &str, const QColor &color)
    {
        return QString(QLatin1String("<font color='%1'>%2</font>"))
            .arg(color.name(), str);
    }
}

void ExecutionLog::addLogMessage(const Log::Msg &msg)
{
    const QDateTime time = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
    const QColor messageColor = Theme::ColorTheme::current().logMessageColor(msg.type);
    const QColor neutralColor = QPalette().color(QPalette::Inactive, QPalette::WindowText);

    QString text = coloredString(time.toString(Qt::SystemLocaleShortDate), neutralColor)
                        + QLatin1String(" - ")
                        + coloredString(msg.message, messageColor);
    m_msgList->appendLine(text, msg.type);
}

void ExecutionLog::addPeerMessage(const Log::Peer &peer)
{
    const QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);
    const QColor IPColor = Theme::ColorTheme::current().logMessageColor(Log::MsgType::CRITICAL);
    const QColor neutralColor = QPalette().color(QPalette::Inactive, QPalette::WindowText);

    QString text = coloredString(time.toString(Qt::SystemLocaleShortDate), neutralColor)
        + QLatin1String(" - ") + coloredString(peer.ip, IPColor) + QLatin1Char(' ');
    if (peer.blocked)
        text += tr("was blocked %1", "x.y.z.w was blocked").arg(peer.reason);
    else
        text += tr("was banned", "x.y.z.w was banned");

    m_peerList->appendLine(text, Log::NORMAL);
}
