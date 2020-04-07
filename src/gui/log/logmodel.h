/*
 * Bittorrent Client using Qt and libtorrent.
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

#pragma once

#include <QAbstractListModel>
#include <deque>

namespace Log
{
    struct Msg;
    struct Peer;
}

class BaseLogModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY(BaseLogModel)

public:
    enum DataRole
    {
        TimeRole = Qt::UserRole + 1,
        MessageRole
    };
    
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void reset();

protected:
    struct Message
    {
        QVariant time;
        QVariant message;
        QVariant foregroundRole;
        QVariant userRole;
    };

    void addNewMessage(Message message);

private:
    std::deque<Message> m_messages; // when logger is full, pop_front may take considerable time on vector like container
};

class LogMessageModel : public BaseLogModel
{
    Q_OBJECT

public:
    explicit LogMessageModel(QObject *parent = nullptr);

private slots:
    void handleNewMessage(const Log::Msg &msg);
};

class LogPeerModel : public BaseLogModel
{
    Q_OBJECT

public:
    explicit LogPeerModel(QObject *parent = nullptr);

private slots:
    void handleNewMessage(const Log::Peer &peer);
};
