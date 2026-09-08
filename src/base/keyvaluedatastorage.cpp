/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "keyvaluedatastorage.h"

#include <concepts>
#include <memory>
#include <queue>

#include <QHash>
#include <QMutex>
#include <QPromise>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QWaitCondition>

#include "exceptions.h"
#include "logger.h"
#include "path.h"
#include "profile.h"

using namespace Qt::Literals::StringLiterals;

const QString DB_STORAGE_PREFIX = u"QBT_KeyValueDataStorage_"_s;
const QString DB_COLUMN_ID = u"id"_s;
const QString DB_COLUMN_KEY = u"key"_s;
const QString DB_COLUMN_VALUE = u"value"_s;

namespace
{
    Path makeDBPath(const QString &storageName)
    {
        return specialFolderLocation(SpecialFolder::Data) / Path(storageName + u".db");
    }

    class Job
    {
    public:
        explicit Job(const QString &storageName, const QString &tableName)
            : m_storageName {storageName}
            , m_tableName {tableName}
        {
        }

        virtual ~Job() = default;
        virtual void perform(QSqlDatabase db) = 0;

    protected:
        QString m_storageName;
        QString m_tableName;
    };

    class StoreValueJob final : public Job
    {
    public:
        StoreValueJob(const QString &storageName, const QString &tableName, const QString &key, QVariant value)
            : Job(storageName, tableName)
            , m_key {key}
            , m_value {std::move(value)}
        {
        }

        void perform(QSqlDatabase db) override
        {
            const QString insertStatement = u"INSERT INTO `%1` (%2, %3) VALUES (:%2, :%3) ON CONFLICT (%2) DO UPDATE SET (%2, %3) = (:%2, :%3)"_s
                    .arg(m_tableName, DB_COLUMN_KEY, DB_COLUMN_VALUE);

            QSqlQuery query {db};
            try
            {
                if (!query.prepare(insertStatement))
                    throw RuntimeError(query.lastError().text());

                query.bindValue((u":" + DB_COLUMN_KEY), m_key);
                query.bindValue((u":" + DB_COLUMN_VALUE), m_value);

                if (!query.exec())
                    throw RuntimeError(query.lastError().text());
            }
            catch (const RuntimeError &err)
            {
                LogMsg(KeyValueDataStorage::tr("Inserting (updating) database record failed. Database: %1. Error: %2")
                        .arg(m_storageName, err.message()), Log::WARNING);
            }
        }

    private:
        QString m_key;
        QVariant m_value;
    };

    class FetchValueJob final : public Job
    {
    public:
        explicit FetchValueJob(const QString &storageName, const QString &tableName, const QString &key, QPromise<QVariant> &&promise)
            : Job(storageName, tableName)
            , m_key {key}
            , m_promise {std::move(promise)}
        {
        }

        void perform(QSqlDatabase db) override
        {
            const QString selectStatement = u"SELECT `%1` FROM `%2` WHERE `%3` = :%3;"_s
                    .arg(DB_COLUMN_VALUE, m_tableName, DB_COLUMN_KEY);

            QSqlQuery query {db};
            try
            {
                if (!query.prepare(selectStatement))
                    throw RuntimeError(query.lastError().text());

                query.bindValue((u":" + DB_COLUMN_KEY), m_key);
                if (!query.exec())
                    throw RuntimeError(query.lastError().text());

                if (query.next())
                    m_promise.addResult(query.value(DB_COLUMN_VALUE));
                else
                    m_promise.addResult({});

                m_promise.finish();
            }
            catch (const RuntimeError &err)
            {
                m_promise.addResult({});
                LogMsg(KeyValueDataStorage::tr("Fetching database record failed. Database: %1. Error: %2")
                        .arg(m_storageName, err.message()), Log::WARNING);
            }
        }

    private:
        QString m_key;
        QPromise<QVariant> m_promise;
    };

    class RemoveValueJob final : public Job
    {
    public:
        explicit RemoveValueJob(const QString &storageName, const QString &tableName, const QString &key)
            : Job(storageName, tableName)
            , m_key {key}
        {
        }

        void perform(QSqlDatabase db) override
        {
            const auto deleteStatement = u"DELETE FROM `%1` WHERE `%2` = :%2;"_s
                    .arg(m_tableName, DB_COLUMN_KEY);

            QSqlQuery query {db};
            try
            {
                if (!query.prepare(deleteStatement))
                    throw RuntimeError(query.lastError().text());

                query.bindValue((u":" + DB_COLUMN_KEY), m_key);

                if (!query.exec())
                    throw RuntimeError(query.lastError().text());
            }
            catch (const RuntimeError &err)
            {
                LogMsg(KeyValueDataStorage::tr("Deleting database record failed. Database: %1. Error: %2")
                        .arg(m_storageName, err.message()), Log::WARNING);
            }
        }

    private:
        QString m_key;
    };

    template <std::integral T>
    QByteArray toHex(const T value)
    {
        return QByteArray(reinterpret_cast<const char *>(&value), sizeof(value)).toHex();
    }
}

class KeyValueDataStorage::Worker final : public QThread
{
    Q_DISABLE_COPY_MOVE(Worker)

public:
    explicit Worker(const Path &dbPath, const QString &storageName, QObject *parent = nullptr);

    void run() override;
    void requestInterruption();

    void storeValue(const QString &key, QVariant &&value);
    void fetchValue(const QString &key, QPromise<QVariant> &&promise);
    void removeValue(const QString &key);

private:
    bool initDB(QSqlDatabase &db);
    void addJob(std::unique_ptr<Job> job);

    Path m_dbPath;
    QString m_storageName;
    QString m_tableName;

    QReadWriteLock m_dbLock;

    std::queue<std::unique_ptr<Job>> m_jobs;
    QMutex m_jobsMutex;
    QWaitCondition m_waitCondition;
};

KeyValueDataStorage::KeyValueDataStorage(const QString &storageName, QObject *parent)
    : QObject(parent)
    , m_storageName {storageName}
{
}

KeyValueDataStorage::~KeyValueDataStorage()
{
    if (m_asyncWorker)
    {
        m_asyncWorker->requestInterruption();
        m_asyncWorker->wait();
    }
}

QFuture<QVariant> KeyValueDataStorage::fetchValue(const QString &key) const
{
    if (!m_asyncWorker)
    {
        const Path dbPath = makeDBPath(m_storageName);
        if (dbPath.exists())
        {
            m_asyncWorker = new Worker(dbPath, m_storageName, const_cast<KeyValueDataStorage *>(this));
            m_asyncWorker->start();
        }
    }

    if (!m_asyncWorker)
        return QtFuture::makeReadyValueFuture<QVariant>({});

    QPromise<QVariant> promise;
    const auto future = promise.future();
    promise.start();
    m_asyncWorker->fetchValue(key, std::move(promise));
    return future;
}

void KeyValueDataStorage::storeValue(const QString &key, const QVariant &value)
{
    storeValue(key, QVariant(value));
}

void KeyValueDataStorage::storeValue(const QString &key, QVariant &&value)
{
    if (!m_asyncWorker)
    {
        m_asyncWorker = new Worker(makeDBPath(m_storageName), m_storageName, this);
        m_asyncWorker->start();
    }

    m_asyncWorker->storeValue(key, std::move(value));
}

void KeyValueDataStorage::removeValue(const QString &key)
{
    if (!m_asyncWorker)
    {
        const Path dbPath = makeDBPath(m_storageName);
        if (dbPath.exists())
        {
            m_asyncWorker = new Worker(dbPath, m_storageName, this);
            m_asyncWorker->start();
        }
    }

    if (m_asyncWorker)
        m_asyncWorker->removeValue(key);
}

KeyValueDataStorage::Worker::Worker(const Path &dbPath, const QString &storageName, QObject *parent)
    : QThread(parent)
    , m_dbPath {dbPath}
    , m_storageName {storageName}
    , m_tableName {DB_STORAGE_PREFIX + storageName}
{
}

void KeyValueDataStorage::Worker::run()
{
    const QString connectionName = DB_STORAGE_PREFIX + QString::fromLatin1(toHex(qHash(this)));
    Q_ASSERT(!QSqlDatabase::database(connectionName, false).isValid());

    {
        auto db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connectionName);
        db.setDatabaseName(m_dbPath.data());
    }

    {
        auto db = QSqlDatabase::database(connectionName, false);

        {
            const QMutexLocker jobsMutexLocker {&m_jobsMutex};
            if (!m_jobs.empty())
            {
                m_dbLock.lockForWrite();
                if (!initDB(db))
                {
                    m_dbLock.unlock();
                    return;
                }
            }
        }

        int64_t transactedJobsCount = 0;
        while (true)
        {
            QMutexLocker jobsMutexLocker {&m_jobsMutex};
            if (m_jobs.empty())
            {
                if (transactedJobsCount > 0)
                {
                    db.commit();
                    db.close();
                    m_dbLock.unlock();

                    transactedJobsCount = 0;
                }

                if (isInterruptionRequested())
                    break;

                m_waitCondition.wait(&m_jobsMutex);
                if (isInterruptionRequested())
                    break;

                m_dbLock.lockForWrite();
                if (!initDB(db))
                {
                    m_dbLock.unlock();
                    break;
                }
            }
            std::unique_ptr<Job> job = std::move(m_jobs.front());
            m_jobs.pop();
            jobsMutexLocker.unlock();

            job->perform(db);
            ++transactedJobsCount;
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void KeyValueDataStorage::Worker::requestInterruption()
{
    QThread::requestInterruption();
    m_waitCondition.wakeAll();
}

void KeyValueDataStorage::Worker::storeValue(const QString &key, QVariant &&value)
{
    addJob(std::make_unique<StoreValueJob>(m_storageName, m_tableName, key, std::move(value)));
}

void KeyValueDataStorage::Worker::fetchValue(const QString &key, QPromise<QVariant> &&promise)
{
    addJob(std::make_unique<FetchValueJob>(m_storageName, m_tableName, key, std::move(promise)));
}

void KeyValueDataStorage::Worker::removeValue(const QString &key)
{
    addJob(std::make_unique<RemoveValueJob>(m_storageName, m_tableName, key));
}

bool KeyValueDataStorage::Worker::initDB(QSqlDatabase &db)
{
    if (!db.open())
    {
        LogMsg(tr("Opening database failed. Database: %1. Error: %2")
                .arg(m_storageName, db.lastError().text()), Log::WARNING);
        return false;
    }

    if (!db.transaction())
    {
        LogMsg(tr("Starting database transaction failed. Database: %1. Error: %2")
                .arg(m_storageName, db.lastError().text()), Log::WARNING);
        return false;
    }

    QSqlQuery query {db};

    const QString createTableQuery = u"CREATE TABLE IF NOT EXISTS `%1` (%2 INTEGER PRIMARY KEY, %3 TEXT NOT NULL UNIQUE, %4 BLOB)"_s
            .arg(m_tableName, DB_COLUMN_ID, DB_COLUMN_KEY, DB_COLUMN_VALUE);
    if (!query.exec(createTableQuery))
    {
        LogMsg(tr("Creating database table failed. Database: %1. Error: %2")
                .arg(m_storageName, db.lastError().text()), Log::WARNING);
        return false;
    }

    return true;
}

void KeyValueDataStorage::Worker::addJob(std::unique_ptr<Job> job)
{
    m_jobsMutex.lock();
    m_jobs.push(std::move(job));
    m_jobsMutex.unlock();

    m_waitCondition.wakeAll();
}
