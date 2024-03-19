/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "logmodel.h"

#include <QApplication>
#include <QDateTime>
#include <QColor>

#include "base/global.h"
#include "gui/uithememanager.h"

const int MAX_VISIBLE_MESSAGES = 20000;

BaseLogModel::Message::Message(const QString &time, const QString &message, const Log::MsgType type)
    : m_time {time}
    , m_message {message}
    , m_type {type}
{
}

QString BaseLogModel::Message::time() const
{
    return m_time;
}

QString BaseLogModel::Message::message() const
{
    return m_message;
}

Log::MsgType BaseLogModel::Message::type() const
{
    return m_type;
}

BaseLogModel::BaseLogModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_messages(MAX_VISIBLE_MESSAGES)
{
    loadColors();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &BaseLogModel::onUIThemeChanged);
}

int BaseLogModel::rowCount(const QModelIndex &) const
{
    return static_cast<int>(m_messages.size());
}

int BaseLogModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant BaseLogModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    const int messageIndex = index.row();
    if ((messageIndex < 0) || (messageIndex >= static_cast<int>(m_messages.size())))
        return {};

    const Message &message = m_messages[messageIndex];
    switch (role)
    {
    case TimeRole:
        return message.time();
    case MessageRole:
        return message.message();
    case TimeForegroundRole:
        return m_timeForeground;
    case MessageForegroundRole:
        return messageForeground(message);
    case TypeRole:
        return message.type();
    default:
        return {};
    }
}

void BaseLogModel::addNewMessage(const BaseLogModel::Message &message)
{
    // if row is inserted on filled up buffer, the size will not change
    // but because of calling of beginInsertRows function we'll have ghost rows.
    if (m_messages.size() == MAX_VISIBLE_MESSAGES)
    {
        const int lastMessage = static_cast<int>(m_messages.size()) - 1;
        beginRemoveRows(QModelIndex(), lastMessage, lastMessage);
        m_messages.pop_back();
        endRemoveRows();
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_messages.push_front(message);
    endInsertRows();
}

void BaseLogModel::onUIThemeChanged()
{
    loadColors();
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)), {TimeForegroundRole, MessageForegroundRole});
}

void BaseLogModel::loadColors()
{
    m_timeForeground = UIThemeManager::instance()->getColor(u"Log.TimeStamp"_s);
}

void BaseLogModel::reset()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

LogMessageModel::LogMessageModel(QObject *parent)
    : BaseLogModel(parent)
{
    loadColors();

    for (const Log::Msg &msg : asConst(Logger::instance()->getMessages()))
        handleNewMessage(msg);
    connect(Logger::instance(), &Logger::newLogMessage, this, &LogMessageModel::handleNewMessage);
}

void LogMessageModel::handleNewMessage(const Log::Msg &message)
{
    const QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(message.timestamp), QLocale::ShortFormat);
    addNewMessage({time, message.message, message.type});
}

QColor LogMessageModel::messageForeground(const Message &message) const
{
    return m_foregroundForMessageTypes.value(message.type());
}

void LogMessageModel::onUIThemeChanged()
{
    loadColors();
    BaseLogModel::onUIThemeChanged();
}

void LogMessageModel::loadColors()
{
    const auto *themeManager = UIThemeManager::instance();
    const QColor normalColor = themeManager->getColor(u"Log.Normal"_s);
    m_foregroundForMessageTypes =
    {
        {Log::NORMAL, normalColor.isValid() ? normalColor : QApplication::palette().color(QPalette::Active, QPalette::WindowText)},
        {Log::INFO, themeManager->getColor(u"Log.Info"_s)},
        {Log::WARNING, themeManager->getColor(u"Log.Warning"_s)},
        {Log::CRITICAL, themeManager->getColor(u"Log.Critical"_s)}
    };
}

LogPeerModel::LogPeerModel(QObject *parent)
    : BaseLogModel(parent)
{
    loadColors();

    for (const Log::Peer &peer : asConst(Logger::instance()->getPeers()))
        handleNewMessage(peer);
    connect(Logger::instance(), &Logger::newLogPeer, this, &LogPeerModel::handleNewMessage);
}

void LogPeerModel::handleNewMessage(const Log::Peer &peer)
{
    const QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(peer.timestamp), QLocale::ShortFormat);
    const QString message = peer.blocked
            ? tr("%1 was blocked. Reason: %2.", "0.0.0.0 was blocked. Reason: reason for blocking.").arg(peer.ip, peer.reason)
            : tr("%1 was banned", "0.0.0.0 was banned").arg(peer.ip);

    addNewMessage({time, message, Log::NORMAL});
}

QColor LogPeerModel::messageForeground([[maybe_unused]] const Message &message) const
{
    return m_bannedPeerForeground;
}

void LogPeerModel::onUIThemeChanged()
{
    loadColors();
    BaseLogModel::onUIThemeChanged();
}

void LogPeerModel::loadColors()
{
    m_bannedPeerForeground = UIThemeManager::instance()->getColor(u"Log.BannedPeer"_s);
}
