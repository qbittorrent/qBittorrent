/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Nicolas Peugnet <n.peugnet@free.fr>
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

#include "apitorrentcreatorthread.h"

APITorrentCreatorThread::APITorrentCreatorThread(int creatorId, QObject *parent)
    : BitTorrent::TorrentCreatorThread(parent), m_id(creatorId)
{
    connect(this, &BitTorrent::TorrentCreatorThread::creationSuccess, this, &APITorrentCreatorThread::forwardCreationSuccess);
    connect(this, &BitTorrent::TorrentCreatorThread::creationFailure, this, &APITorrentCreatorThread::forwardCreationFailure);
    connect(this, &BitTorrent::TorrentCreatorThread::updateProgress, this, &APITorrentCreatorThread::forwardUpdateProgress);
}

void APITorrentCreatorThread::forwardCreationSuccess(const QString &path, const QString &branchPath)
{
    struct TorrentCreatorStatus status
    {
        true
        , false
        , 100
        , path
        , branchPath
        , QString()
        , this
    };
    emit creationUpdate(m_id, status);
}

void APITorrentCreatorThread::forwardCreationFailure(const QString &msg)
{
    struct TorrentCreatorStatus status
    {
        false
        , true
        , 0
        , QString()
        , QString()
        , msg
        , this
    };
    emit creationUpdate(m_id, status);
}

void APITorrentCreatorThread::forwardUpdateProgress(int progress)
{
    struct TorrentCreatorStatus status
    {
        false
        , false
        , progress
        , QString()
        , QString()
        , QString()
        , this
    };
    emit creationUpdate(m_id, status);
}