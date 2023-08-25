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

#include "torrentcreationmanager.h"

#include <QByteArray>
#include <QFile>
#include <QPointer>
#include <QUuid>

#define SETTINGS_KEY(name) u"TorrentCreator/Manager/" name

using namespace BitTorrent;

QPointer<TorrentCreationManager> TorrentCreationManager::m_instance = nullptr;

TorrentCreationTask::TorrentCreationTask(const QString &id, const BitTorrent::TorrentCreatorParams &params, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_params(params)
{
    m_timeAdded = QDateTime::currentDateTime();
}

void TorrentCreationTask::setProgress(int progress)
{
    if (m_timeStarted.isNull())
        m_timeStarted = QDateTime::currentDateTime();
    m_progress = progress;
}

bool TorrentCreationTask::isDone() const
{
    return !m_timeDone.isNull();
}

QDateTime TorrentCreationTask::timeAdded() const
{
    return m_timeAdded;
}

QDateTime TorrentCreationTask::timeStarted() const
{
    return m_timeStarted;
}

QDateTime TorrentCreationTask::timeDone() const
{
    return m_timeDone;
}

void TorrentCreationTask::setDone()
{
    if (m_timeStarted.isNull())
        m_timeStarted = QDateTime::currentDateTime();
    m_timeDone = QDateTime::currentDateTime();
}

bool TorrentCreationTask::isDoneWithSuccess() const
{
    return !m_timeDone.isNull() && (!m_result.content.isEmpty() || !m_result.path.isEmpty());
}

bool TorrentCreationTask::isDoneWithError() const
{
    return !m_timeDone.isNull() && !m_errorMsg.isEmpty();
}

bool TorrentCreationTask::isRunning() const
{
    return !m_timeStarted.isNull() && m_timeDone.isNull();
}

QString TorrentCreationTask::id() const
{
    return m_id;
}

QByteArray TorrentCreationTask::content() const
{
    if (!isDoneWithSuccess())
        return QByteArray {};

    if (!m_result.content.isEmpty())
        return m_result.content;

    QFile file(m_result.path.toString());
    if(!file.open(QIODevice::ReadOnly))
        return QByteArray {};

    return file.readAll();
}

const BitTorrent::TorrentCreatorParams &TorrentCreationTask::params() const
{
    return m_params;
}

int TorrentCreationTask::progress() const
{
    return m_progress;
}

QString TorrentCreationTask::errorMsg() const
{
    return m_errorMsg;
}


TorrentCreationManager::TorrentCreationManager()
    : m_maxTasks(SETTINGS_KEY(u"MaxTasks"_s), 256)
    , m_numThreads(SETTINGS_KEY(u"NumThreads"_s), 0)
    , m_threadPool {this}
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    if (m_numThreads > 0)
        m_threadPool.setMaxThreadCount(m_numThreads);
}

TorrentCreationManager::~TorrentCreationManager()
{
    qDeleteAll(m_tasks);
}

TorrentCreationManager *TorrentCreationManager::instance()
{
    if (!m_instance)
        m_instance = new TorrentCreationManager;
    return m_instance;
}

void TorrentCreationManager::freeInstance()
{
    delete m_instance;
}

QString TorrentCreationManager::createTask(const TorrentCreatorParams &params, bool startSeeding)
{
    if (m_tasks.size() >= m_maxTasks)
    {
        // Try to delete old finished tasks to stay under target
        std::pair<TaskSetByCompletion::iterator, TaskSetByCompletion::iterator> p = m_tasks.get<ByCompletion>().equal_range(std::make_tuple(true));
        while(p.first != p.second && m_tasks.size() >= m_maxTasks)
        {
            TorrentCreationTask *task = *p.first;
            p.first = m_tasks.get<ByCompletion>().erase(p.first);
            delete task;
        }
    }
    if (m_tasks.size() >= m_maxTasks)
        return {};

    QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    while (m_tasks.get<ById>().find(taskId) != m_tasks.get<ById>().end())
        taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto *torrentCreator = new TorrentCreator(params, this);
    auto *creationTask = new TorrentCreationTask(taskId, params, this);

    auto onProgress = [creationTask](int progress)
    {
        creationTask->setProgress(progress);
    };
    auto onSuccess = [this, startSeeding, taskId](const TorrentCreatorResult &result)
    {
        markSuccess(taskId, result);
        if (startSeeding)
            result.startSeeding(false);
    };
    auto onFailure = [this, taskId]([[maybe_unused]] const QString &msg)
    {
        markFailed(taskId, msg);
    };
    connect(torrentCreator, &BitTorrent::TorrentCreator::creationSuccess, this, onSuccess);
    connect(torrentCreator, &BitTorrent::TorrentCreator::creationFailure, this, onFailure);
    connect(torrentCreator, &BitTorrent::TorrentCreator::updateProgress, creationTask, onProgress);
    connect(creationTask, &QObject::destroyed, torrentCreator, &BitTorrent::TorrentCreator::requestInterruption);

    m_tasks.get<ById>().insert(creationTask);
    m_threadPool.start(torrentCreator);
    return taskId;
}

QStringList TorrentCreationManager::taskIds() const
{
    QStringList ids;
    ids.reserve(m_tasks.get<ByCompletion>().size());
    for (TorrentCreationTask * task : m_tasks.get<ByCompletion>())
        ids << task->id();
    return ids;
}

TorrentCreationTask *TorrentCreationManager::getTask(const QString &id) const
{
    const auto iter = m_tasks.get<ById>().find(id);
    if (iter == m_tasks.get<ById>().end())
        return nullptr;

    return *iter;
}

bool TorrentCreationManager::deleteTask(const QString &id)
{
    const auto iter = m_tasks.get<ById>().find(id);
    if (iter == m_tasks.get<ById>().end())
        return false;

    TorrentCreationTask *task = *iter;
    m_tasks.get<ById>().erase(iter);
    delete task;
    return true;
}

void TorrentCreationManager::markFailed(const QString &id, const QString &msg)
{

    const auto iter = m_tasks.get<ById>().find(id);
    if (iter != m_tasks.get<ById>().end()){
        m_tasks.get<ByCompletion>().modify(m_tasks.project<ByCompletion>(iter),
                                           [](TorrentCreationTask *task) { task->setDone(); });
        TorrentCreationTask *task = *iter;
        task->m_errorMsg = msg;
    }
}

void TorrentCreationManager::markSuccess(const QString &id, const BitTorrent::TorrentCreatorResult &result)
{
    const auto iter = m_tasks.get<ById>().find(id);
    if (iter != m_tasks.get<ById>().end()){
        m_tasks.get<ByCompletion>().modify(m_tasks.project<ByCompletion>(iter),
                                           [](TorrentCreationTask *task) { task->setDone(); });
        TorrentCreationTask *task = *iter;
        task->m_result = result;
        task->m_params.pieceSize = result.pieceSize;
    }
}
