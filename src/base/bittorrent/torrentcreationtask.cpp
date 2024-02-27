/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2024  Radu Carpa <radu.carpa@cern.ch>
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

#include "torrentcreationtask.h"

#include "base/addtorrentmanager.h"
#include "base/interfaces/iapplication.h"
#include "base/bittorrent/addtorrentparams.h"

BitTorrent::TorrentCreationTask::TorrentCreationTask(IApplication *app, const QString &id
        , TorrentCreator *torrentCreator, bool startSeeding, QObject *parent)
    : ApplicationComponent(app, parent)
    , m_id {id}
    , m_params {torrentCreator->params()}
    , m_timeAdded {QDateTime::currentDateTime()}
{
    Q_ASSERT(torrentCreator);

    connect(torrentCreator, &BitTorrent::TorrentCreator::started, this, [this]
    {
        m_timeStarted = QDateTime::currentDateTime();
    });

    connect(torrentCreator, &BitTorrent::TorrentCreator::progressUpdated, this
            , [this](const int progress)
    {
        m_progress = progress;
    });

    connect(torrentCreator, &BitTorrent::TorrentCreator::creationSuccess, this
            , [this, app, startSeeding](const TorrentCreatorResult &result)
    {
        m_timeFinished = QDateTime::currentDateTime();
        m_result = result;

        if (!startSeeding)
            return;

        BitTorrent::AddTorrentParams params;
        params.savePath = result.savePath;
        params.skipChecking = true;
        params.useAutoTMM = false;  // otherwise if it is on by default, it will overwrite `savePath` to the default save path

        if (!app->addTorrentManager()->addTorrent(result.torrentFilePath.data(), params))
            m_errorMsg = tr("Failed to start seeding.");
    });

    connect(torrentCreator, &BitTorrent::TorrentCreator::creationFailure, this
            , [this](const QString &errorMsg)
    {
        m_timeFinished = QDateTime::currentDateTime();
        m_errorMsg = errorMsg;
    });
}

QString BitTorrent::TorrentCreationTask::id() const
{
    return m_id;
}

const BitTorrent::TorrentCreatorParams &BitTorrent::TorrentCreationTask::params() const
{
    return m_params;
}

BitTorrent::TorrentCreationTask::State BitTorrent::TorrentCreationTask::state() const
{
    if (m_timeStarted.isNull())
        return Queued;
    if (m_timeFinished.isNull())
        return Running;
    return Finished;
}

bool BitTorrent::TorrentCreationTask::isQueued() const
{
    return (state() == Queued);
}

bool BitTorrent::TorrentCreationTask::isRunning() const
{
    return (state() == Running);
}

bool BitTorrent::TorrentCreationTask::isFinished() const
{
    return (state() == Finished);
}

bool BitTorrent::TorrentCreationTask::isFailed() const
{
    return !m_errorMsg.isEmpty();
}

int BitTorrent::TorrentCreationTask::progress() const
{
    return m_progress;
}

const BitTorrent::TorrentCreatorResult &BitTorrent::TorrentCreationTask::result() const
{
    return m_result;
}

QString BitTorrent::TorrentCreationTask::errorMsg() const
{
    return m_errorMsg;
}

QDateTime BitTorrent::TorrentCreationTask::timeAdded() const
{
    return m_timeAdded;
}

QDateTime BitTorrent::TorrentCreationTask::timeStarted() const
{
    return m_timeStarted;
}

QDateTime BitTorrent::TorrentCreationTask::timeFinished() const
{
    return m_timeFinished;
}
