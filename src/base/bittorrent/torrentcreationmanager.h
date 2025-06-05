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

#pragma once

#include <memory>

#include <QtContainerFwd>
#include <QObject>
#include <QThreadPool>

#include "base/applicationcomponent.h"
#include "base/settingvalue.h"
#include "torrentcreationtask.h"
#include "torrentcreator.h"

namespace BitTorrent
{
    class TorrentCreationManager final : public ApplicationComponent<QObject>
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentCreationManager)

    public:
        explicit TorrentCreationManager(IApplication *app, QObject *parent = nullptr);
        ~TorrentCreationManager() override;

        std::shared_ptr<TorrentCreationTask> createTask(const TorrentCreatorParams &params, bool startSeeding = true);
        std::shared_ptr<TorrentCreationTask> getTask(const QString &id) const;
        QList<std::shared_ptr<TorrentCreationTask>> tasks() const;
        bool deleteTask(const QString &id);

    private:
        QString generateTaskID() const;

        CachedSettingValue<qint32> m_maxTasks;
        CachedSettingValue<qint32> m_numThreads;

        class TaskSet;
        std::unique_ptr<TaskSet> m_tasks;

        QThreadPool m_threadPool;
    };
}
