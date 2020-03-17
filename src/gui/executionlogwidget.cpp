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
#include <QPalette>

#include "base/global.h"
#include "loglistwidget.h"
#include "ui_executionlogwidget.h"
#include "uithememanager.h"

ExecutionLogWidget::ExecutionLogWidget(QWidget *parent, const Log::MsgTypes &types)
    : QWidget(parent)
    , m_ui(new Ui::ExecutionLogWidget)
    , m_msgList(new LogListWidget(MAX_LOG_MESSAGES, types, this))
    , m_peerList(new LogListWidget(MAX_LOG_MESSAGES, Log::ALL, this))
{
    m_ui->setupUi(this);

#ifndef Q_OS_MACOS
    m_ui->tabConsole->setTabIcon(0, UIThemeManager::instance()->getIcon("view-calendar-journal"));
    m_ui->tabConsole->setTabIcon(1, UIThemeManager::instance()->getIcon("view-filter"));
#endif
    m_ui->tabGeneral->layout()->addWidget(m_msgList);
    m_ui->tabBan->layout()->addWidget(m_peerList);

    const Logger *const logger = Logger::instance();
    for (const Log::Msg &msg : asConst(logger->getMessages()))
        addLogMessage(msg);
    for (const Log::Peer &peer : asConst(logger->getPeers()))
        addPeerMessage(peer);
    connect(logger, &Logger::newLogMessage, this, &ExecutionLogWidget::addLogMessage);
    connect(logger, &Logger::newLogPeer, this, &ExecutionLogWidget::addPeerMessage);
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::showMsgTypes(const Log::MsgTypes &types)
{
    m_msgList->showMsgTypes(types);
}

void ExecutionLogWidget::addLogMessage(const Log::Msg &msg)
{
    QString colorName;
    switch (msg.type) {
    case Log::INFO:
        colorName = QLatin1String("blue");
        break;
    case Log::WARNING:
        colorName = QLatin1String("orange");
        break;
    case Log::CRITICAL:
        colorName = QLatin1String("red");
        break;
    default:
        colorName = QApplication::palette().color(QPalette::WindowText).name();
    }

    const QDateTime time = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
    const QString text = QString::fromLatin1("<font color='grey'>%1</font> - <font color='%2'>%3</font>")
        .arg(time.toString(Qt::SystemLocaleShortDate), colorName, msg.message);
    m_msgList->appendLine(text, msg.type);
}

void ExecutionLogWidget::addPeerMessage(const Log::Peer &peer)
{
    const QDateTime time = QDateTime::fromMSecsSinceEpoch(peer.timestamp);
    const QString msg = QString::fromLatin1("<font color='grey'>%1</font> - <font color='red'>%2</font>")
        .arg(time.toString(Qt::SystemLocaleShortDate), peer.ip);

    const QString text = peer.blocked
        ? tr("%1 was blocked %2", "0.0.0.0 was blocked due to reason").arg(msg, peer.reason)
        : tr("%1 was banned", "0.0.0.0 was banned").arg(msg);
    m_peerList->appendLine(text, Log::NORMAL);
}
