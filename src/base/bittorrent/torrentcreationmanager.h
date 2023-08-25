/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QThreadPool>

#include "base/settingvalue.h"
#include "torrentcreator.h"


namespace BitTorrent
{

    class TorrentCreationTask : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentCreationTask)

        friend class TorrentCreationManager;
    public:
        TorrentCreationTask(const QString &id, const TorrentCreatorParams &params, QObject *parent = nullptr);
        QString id() const;
        QByteArray content() const;
        QString errorMsg() const;
        const TorrentCreatorParams &params() const;
        void setProgress(int progress);
        int progress() const;
        void setDone();
        bool isDone() const;
        QDateTime timeAdded() const;
        QDateTime timeStarted() const;
        QDateTime timeDone() const;

        bool isDoneWithSuccess() const;
        bool isDoneWithError() const;
        bool isRunning() const;

    private:
        QString m_id;
        TorrentCreatorParams m_params;
        int m_progress = 0;
        QDateTime m_timeAdded;
        QDateTime m_timeStarted;
        QDateTime m_timeDone;

        TorrentCreatorResult m_result;
        QString m_errorMsg;
    };

    class TorrentCreationManager final : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TorrentCreationManager)

    public:
        TorrentCreationManager();
        ~TorrentCreationManager() override;
        static TorrentCreationManager *instance();
        static void freeInstance();
        QString createTask(const TorrentCreatorParams &params, bool startSeeding = true);
        TorrentCreationTask* getTask(const QString &id) const;
        bool deleteTask(const QString &id);
        QStringList taskIds() const;

    private:
        static QPointer<TorrentCreationManager> m_instance;

        struct ById{};
        struct ByCompletion{};
        using TasksSet = boost::multi_index_container<
            TorrentCreationTask *,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_unique<
                    boost::multi_index::tag<ById>,
                    boost::multi_index::const_mem_fun<TorrentCreationTask, QString, &TorrentCreationTask::id>>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<ByCompletion>,
                    boost::multi_index::composite_key<
                        TorrentCreationTask *,
                        boost::multi_index::const_mem_fun<TorrentCreationTask, bool, &TorrentCreationTask::isDone>,
                        boost::multi_index::const_mem_fun<TorrentCreationTask, QDateTime, &TorrentCreationTask::timeAdded>
                    >
                >
            >
        >;
        using TaskSetById = TasksSet::index<ById>::type;
        using TaskSetByCompletion = TasksSet::index<ByCompletion>::type;

        CachedSettingValue<quint32> m_maxTasks;
        CachedSettingValue<quint32> m_numThreads;

        TasksSet m_tasks;
        QThreadPool m_threadPool;

        void markFailed(const QString &id, const QString &msg);
        void markSuccess(const QString &id, const TorrentCreatorResult &result);
    };
}
