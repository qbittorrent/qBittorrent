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

#include "torrentcreatorcontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include "base/global.h"
#include "base/bittorrent/torrentcreationmanager.h"
#include "base/preferences.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "apierror.h"

const QString KEY_COMMENT = u"comment"_s;
const QString KEY_ERROR_MESSAGE = u"errorMessage"_s;
const QString KEY_FORMAT = u"format"_s;
const QString KEY_OPTIMIZE_ALIGNMENT = u"optimizeAlignment"_s;
const QString KEY_PADDED_FILE_SIZE_LIMIT = u"paddedFileSizeLimit"_s;
const QString KEY_PIECE_SIZE = u"pieceSize"_s;
const QString KEY_PRIVATE = u"private"_s;
const QString KEY_PROGRESS = u"progress"_s;
const QString KEY_SOURCE = u"source"_s;
const QString KEY_SOURCE_PATH = u"sourcePath"_s;
const QString KEY_STATUS = u"status"_s;
const QString KEY_TASK_ID = u"taskID"_s;
const QString KEY_TIME_ADDED = u"timeAdded"_s;
const QString KEY_TIME_FINISHED = u"timeFinished"_s;
const QString KEY_TIME_STARTED = u"timeStarted"_s;
const QString KEY_TORRENT_FILE_PATH = u"torrentFilePath"_s;
const QString KEY_TRACKERS = u"trackers"_s;
const QString KEY_URL_SEEDS = u"urlSeeds"_s;

namespace
{
    using Utils::String::parseBool;
    using Utils::String::parseInt;

#ifdef QBT_USES_LIBTORRENT2
    BitTorrent::TorrentFormat parseTorrentFormat(const QString &str)
    {
        if (str == u"v1")
            return BitTorrent::TorrentFormat::V1;
        if (str == u"v2")
            return BitTorrent::TorrentFormat::V2;
        return BitTorrent::TorrentFormat::Hybrid;
    }

    QString torrentFormatToString(const BitTorrent::TorrentFormat torrentFormat)
    {
        switch (torrentFormat)
        {
        case BitTorrent::TorrentFormat::V1:
            return u"v1"_s;
        case BitTorrent::TorrentFormat::V2:
            return u"v2"_s;
        default:
            return u"hybrid"_s;
        }
    }
#endif

    QString taskStatusString(const std::shared_ptr<BitTorrent::TorrentCreationTask> task)
    {
        if (task->isFailed())
            return u"Failed"_s;

        switch (task->state())
        {
        case BitTorrent::TorrentCreationTask::Queued:
        default:
            return u"Queued"_s;
        case BitTorrent::TorrentCreationTask::Running:
            return u"Running"_s;
        case BitTorrent::TorrentCreationTask::Finished:
            return u"Finished"_s;
        }
    }
}

TorrentCreatorController::TorrentCreatorController(BitTorrent::TorrentCreationManager *torrentCreationManager, IApplication *app, QObject *parent)
    : APIController(app, parent)
    , m_torrentCreationManager {torrentCreationManager}
{
}

void TorrentCreatorController::addTaskAction()
{
    requireParams({KEY_SOURCE_PATH});

    const BitTorrent::TorrentCreatorParams createTorrentParams
    {
        .isPrivate = parseBool(params()[KEY_PRIVATE]).value_or(false),
#ifdef QBT_USES_LIBTORRENT2
        .torrentFormat = parseTorrentFormat(params()[KEY_FORMAT].toLower()),
#else
        .isAlignmentOptimized = parseBool(params()[KEY_OPTIMIZE_ALIGNMENT]).value_or(true),
        .paddedFileSizeLimit = parseInt(params()[KEY_PADDED_FILE_SIZE_LIMIT]).value_or(-1),
#endif
        .pieceSize = parseInt(params()[KEY_PIECE_SIZE]).value_or(0),
        .sourcePath = Path(params()[KEY_SOURCE_PATH]),
        .torrentFilePath = Path(params()[KEY_TORRENT_FILE_PATH]),
        .comment = params()[KEY_COMMENT],
        .source = params()[KEY_SOURCE],
        .trackers = params()[KEY_TRACKERS].split(u'|'),
        .urlSeeds = params()[KEY_URL_SEEDS].split(u'|')
    };

    bool const startSeeding = parseBool(params()[u"startSeeding"_s]).value_or(createTorrentParams.torrentFilePath.isEmpty());

    auto task = m_torrentCreationManager->createTask(createTorrentParams, startSeeding);
    if (!task)
        throw APIError(APIErrorType::Conflict, tr("Too many active tasks"));

    setResult(QJsonObject {{KEY_TASK_ID, task->id()}});
}

void TorrentCreatorController::statusAction()
{
    const QString id = params()[KEY_TASK_ID];

    const auto singleTask = m_torrentCreationManager->getTask(id);
    if (!id.isEmpty() && !singleTask)
        throw APIError(APIErrorType::NotFound);

    const QList<std::shared_ptr<BitTorrent::TorrentCreationTask>> tasks = id.isEmpty()
            ? m_torrentCreationManager->tasks()
            : QList<std::shared_ptr<BitTorrent::TorrentCreationTask>> {singleTask};
    QJsonArray statusArray;
    for (const auto &task : tasks)
    {
        QJsonObject taskJson {
            {KEY_TASK_ID, task->id()},
            {KEY_SOURCE_PATH, task->params().sourcePath.toString()},
            {KEY_PIECE_SIZE, task->params().pieceSize},
            {KEY_PRIVATE, task->params().isPrivate},
            {KEY_TIME_ADDED, task->timeAdded().toString()},
#ifdef QBT_USES_LIBTORRENT2
            {KEY_FORMAT, torrentFormatToString(task->params().torrentFormat)},
#else
            {KEY_OPTIMIZE_ALIGNMENT, task->params().isAlignmentOptimized},
            {KEY_PADDED_FILE_SIZE_LIMIT, task->params().paddedFileSizeLimit},
#endif
            {KEY_STATUS, taskStatusString(task)},
        };

        if (!task->params().comment.isEmpty())
            taskJson[KEY_COMMENT] = task->params().comment;

        if (!task->params().torrentFilePath.isEmpty())
            taskJson[KEY_TORRENT_FILE_PATH] = task->params().torrentFilePath.toString();

        if (!task->params().source.isEmpty())
            taskJson[KEY_SOURCE] = task->params().source;

        if (!task->params().trackers.isEmpty())
            taskJson[KEY_TRACKERS] = QJsonArray::fromStringList(task->params().trackers);

        if (!task->params().urlSeeds.isEmpty())
            taskJson[KEY_URL_SEEDS] = QJsonArray::fromStringList(task->params().urlSeeds);

        if (const QDateTime timeStarted = task->timeStarted(); !timeStarted.isNull())
            taskJson[KEY_TIME_STARTED] = timeStarted.toString();

        if (const QDateTime timeFinished = task->timeFinished(); !task->timeFinished().isNull())
            taskJson[KEY_TIME_FINISHED] = timeFinished.toString();

        if (task->isFinished())
        {
            if (task->isFailed())
            {
                taskJson[KEY_ERROR_MESSAGE] = task->errorMsg();
            }
            else
            {
                taskJson[KEY_PIECE_SIZE] = task->result().pieceSize;
            }
        }
        else if (task->isRunning())
        {
            taskJson[KEY_PROGRESS] = task->progress();
        }

        statusArray.append(taskJson);
    }

    setResult(statusArray);
}

void TorrentCreatorController::torrentFileAction()
{
    requireParams({KEY_TASK_ID});
    const QString id = params()[KEY_TASK_ID];

    const auto task = m_torrentCreationManager->getTask(id);
    if (!task)
        throw APIError(APIErrorType::NotFound);

    if (!task->isFinished())
        throw APIError(APIErrorType::Conflict, tr("Torrent creation is still unfinished."));

    if (task->isFailed())
        throw APIError(APIErrorType::Conflict, tr("Torrent creation failed."));

    const auto readResult = Utils::IO::readFile(task->result().torrentFilePath, Preferences::instance()->getTorrentFileSizeLimit());
    if (!readResult)
        throw APIError(APIErrorType::Conflict, readResult.error().message);

    setResult(readResult.value(), u"application/x-bittorrent"_s, (id + u".torrent"));
}

void TorrentCreatorController::deleteTaskAction()
{
    requireParams({KEY_TASK_ID});
    const QString id = params()[KEY_TASK_ID];

    if (!m_torrentCreationManager->deleteTask(id))
        throw APIError(APIErrorType::NotFound);
}
