/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include "dbresumedatastorage.h"

#include <utility>

#include <libtorrent/bdecode.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <QByteArray>
#include <QFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QVector>

#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "infohash.h"
#include "loadtorrentparams.h"

namespace
{
    const QString DB_CONNECTION_NAME = u"ResumeDataStorage"_qs;

    const int DB_VERSION = 2;

    const QString DB_TABLE_META = u"meta"_qs;
    const QString DB_TABLE_TORRENTS = u"torrents"_qs;

    const QString META_VERSION = u"version"_qs;

    struct Column
    {
        QString name;
        QString placeholder;
    };

    Column makeColumn(const char *columnName)
    {
        return {QString::fromLatin1(columnName), (u':' + QString::fromLatin1(columnName))};
    }

    const Column DB_COLUMN_ID = makeColumn("id");
    const Column DB_COLUMN_TORRENT_ID = makeColumn("torrent_id");
    const Column DB_COLUMN_QUEUE_POSITION = makeColumn("queue_position");
    const Column DB_COLUMN_NAME = makeColumn("name");
    const Column DB_COLUMN_CATEGORY = makeColumn("category");
    const Column DB_COLUMN_TAGS = makeColumn("tags");
    const Column DB_COLUMN_TARGET_SAVE_PATH = makeColumn("target_save_path");
    const Column DB_COLUMN_DOWNLOAD_PATH = makeColumn("download_path");
    const Column DB_COLUMN_CONTENT_LAYOUT = makeColumn("content_layout");
    const Column DB_COLUMN_RATIO_LIMIT = makeColumn("ratio_limit");
    const Column DB_COLUMN_SEEDING_TIME_LIMIT = makeColumn("seeding_time_limit");
    const Column DB_COLUMN_HAS_OUTER_PIECES_PRIORITY = makeColumn("has_outer_pieces_priority");
    const Column DB_COLUMN_HAS_SEED_STATUS = makeColumn("has_seed_status");
    const Column DB_COLUMN_OPERATING_MODE = makeColumn("operating_mode");
    const Column DB_COLUMN_STOPPED = makeColumn("stopped");
    const Column DB_COLUMN_RESUMEDATA = makeColumn("libtorrent_resume_data");
    const Column DB_COLUMN_METADATA = makeColumn("metadata");
    const Column DB_COLUMN_VALUE = makeColumn("value");

    template <typename LTStr>
    QString fromLTString(const LTStr &str)
    {
        return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    }

    QString quoted(const QString &name)
    {
        const QChar quote = u'`';

        return (quote + name + quote);
    }

    QString makeCreateTableStatement(const QString &tableName, const QStringList &items)
    {
        return u"CREATE TABLE %1 (%2)"_qs.arg(quoted(tableName), items.join(u','));
    }

    std::pair<QString, QString> joinColumns(const QVector<Column> &columns)
    {
        int namesSize = columns.size();
        int valuesSize = columns.size();
        for (const Column &column : columns)
        {
            namesSize += column.name.size() + 2;
            valuesSize += column.placeholder.size();
        }

        QString names;
        names.reserve(namesSize);
        QString values;
        values.reserve(valuesSize);
        for (const Column &column : columns)
        {
            names.append(quoted(column.name) + u',');
            values.append(column.placeholder + u',');
        }
        names.chop(1);
        values.chop(1);

        return std::make_pair(names, values);
    }

    QString makeInsertStatement(const QString &tableName, const QVector<Column> &columns)
    {
        const auto [names, values] = joinColumns(columns);
        return u"INSERT INTO %1 (%2) VALUES (%3)"_qs
                .arg(quoted(tableName), names, values);
    }

    QString makeUpdateStatement(const QString &tableName, const QVector<Column> &columns)
    {
        const auto [names, values] = joinColumns(columns);
        return u"UPDATE %1 SET (%2) = (%3)"_qs
                .arg(quoted(tableName), names, values);
    }

    QString makeOnConflictUpdateStatement(const Column &constraint, const QVector<Column> &columns)
    {
        const auto [names, values] = joinColumns(columns);
        return u" ON CONFLICT (%1) DO UPDATE SET (%2) = (%3)"_qs
                .arg(quoted(constraint.name), names, values);
    }

    QString makeColumnDefinition(const Column &column, const char *definition)
    {
        return u"%1 %2"_qs.arg(quoted(column.name), QString::fromLatin1(definition));
    }
}

namespace BitTorrent
{
    class DBResumeDataStorage::Worker final : public QObject
    {
        Q_DISABLE_COPY_MOVE(Worker)

    public:
        Worker(const Path &dbPath, const QString &dbConnectionName, QReadWriteLock &dbLock);

        void openDatabase() const;
        void closeDatabase() const;

        void store(const TorrentID &id, const LoadTorrentParams &resumeData) const;
        void remove(const TorrentID &id) const;
        void storeQueue(const QVector<TorrentID> &queue) const;

    private:
        const Path m_path;
        const QString m_connectionName;
        QReadWriteLock &m_dbLock;
    };

    namespace
    {
        LoadTorrentParams parseQueryResultRow(const QSqlQuery &query)
        {
            LoadTorrentParams resumeData;
            resumeData.restored = true;
            resumeData.name = query.value(DB_COLUMN_NAME.name).toString();
            resumeData.category = query.value(DB_COLUMN_CATEGORY.name).toString();
            const QString tagsData = query.value(DB_COLUMN_TAGS.name).toString();
            if (!tagsData.isEmpty())
            {
                const QStringList tagList = tagsData.split(u',');
                resumeData.tags.insert(tagList.cbegin(), tagList.cend());
            }
            resumeData.hasSeedStatus = query.value(DB_COLUMN_HAS_SEED_STATUS.name).toBool();
            resumeData.firstLastPiecePriority = query.value(DB_COLUMN_HAS_OUTER_PIECES_PRIORITY.name).toBool();
            resumeData.ratioLimit = query.value(DB_COLUMN_RATIO_LIMIT.name).toInt() / 1000.0;
            resumeData.seedingTimeLimit = query.value(DB_COLUMN_SEEDING_TIME_LIMIT.name).toInt();
            resumeData.contentLayout = Utils::String::toEnum<TorrentContentLayout>(
                        query.value(DB_COLUMN_CONTENT_LAYOUT.name).toString(), TorrentContentLayout::Original);
            resumeData.operatingMode = Utils::String::toEnum<TorrentOperatingMode>(
                        query.value(DB_COLUMN_OPERATING_MODE.name).toString(), TorrentOperatingMode::AutoManaged);
            resumeData.stopped = query.value(DB_COLUMN_STOPPED.name).toBool();

            resumeData.savePath = Profile::instance()->fromPortablePath(
                        Path(query.value(DB_COLUMN_TARGET_SAVE_PATH.name).toString()));
            resumeData.useAutoTMM = resumeData.savePath.isEmpty();
            if (!resumeData.useAutoTMM)
            {
                resumeData.downloadPath = Profile::instance()->fromPortablePath(
                            Path(query.value(DB_COLUMN_DOWNLOAD_PATH.name).toString()));
            }

            const QByteArray bencodedResumeData = query.value(DB_COLUMN_RESUMEDATA.name).toByteArray();

            lt::error_code ec;
            const lt::bdecode_node resumeDataRoot = lt::bdecode(bencodedResumeData, ec);

            lt::add_torrent_params &p = resumeData.ltAddTorrentParams;

            p = lt::read_resume_data(resumeDataRoot, ec);

            if (const QByteArray bencodedMetadata = query.value(DB_COLUMN_METADATA.name).toByteArray(); !bencodedMetadata.isEmpty())
            {
                const lt::bdecode_node torentInfoRoot = lt::bdecode(bencodedMetadata, ec);
                p.ti = std::make_shared<lt::torrent_info>(torentInfoRoot, ec);
            }

            p.save_path = Profile::instance()->fromPortablePath(Path(fromLTString(p.save_path)))
                    .toString().toStdString();

            return resumeData;
        }
    }
}

BitTorrent::DBResumeDataStorage::DBResumeDataStorage(const Path &dbPath, QObject *parent)
    : ResumeDataStorage(dbPath, parent)
    , m_ioThread {new QThread(this)}
{
    const bool needCreateDB = !dbPath.exists();

    auto db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, DB_CONNECTION_NAME);
    db.setDatabaseName(dbPath.data());
    if (!db.open())
        throw RuntimeError(db.lastError().text());

    if (needCreateDB)
    {
        createDB();
    }
    else
    {
        const int dbVersion = currentDBVersion();
        if ((dbVersion == 1) || !db.record(DB_TABLE_TORRENTS).contains(DB_COLUMN_DOWNLOAD_PATH.name))
            updateDBFromVersion1();
    }

    m_asyncWorker = new Worker(dbPath, u"ResumeDataStorageWorker"_qs, m_dbLock);
    m_asyncWorker->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_asyncWorker, &QObject::deleteLater);
    m_ioThread->start();

    RuntimeError *errPtr = nullptr;
    QMetaObject::invokeMethod(m_asyncWorker, [this, &errPtr]()
    {
        try
        {
            m_asyncWorker->openDatabase();
        }
        catch (const RuntimeError &err)
        {
            errPtr = new RuntimeError(err);
        }
    }, Qt::BlockingQueuedConnection);

    if (errPtr)
        throw *errPtr;
}

BitTorrent::DBResumeDataStorage::~DBResumeDataStorage()
{
    QMetaObject::invokeMethod(m_asyncWorker, &Worker::closeDatabase);
    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);

    m_ioThread->quit();
    m_ioThread->wait();
}

QVector<BitTorrent::TorrentID> BitTorrent::DBResumeDataStorage::registeredTorrents() const
{
    const auto selectTorrentIDStatement = u"SELECT %1 FROM %2 ORDER BY %3;"_qs
            .arg(quoted(DB_COLUMN_TORRENT_ID.name), quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_QUEUE_POSITION.name));

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    QSqlQuery query {db};

    if (!query.exec(selectTorrentIDStatement))
        throw RuntimeError(query.lastError().text());

    QVector<TorrentID> registeredTorrents;
    registeredTorrents.reserve(query.size());
    while (query.next())
        registeredTorrents.append(BitTorrent::TorrentID::fromString(query.value(0).toString()));

    return registeredTorrents;
}

BitTorrent::LoadResumeDataResult BitTorrent::DBResumeDataStorage::load(const TorrentID &id) const
{
    const QString selectTorrentStatement = u"SELECT * FROM %1 WHERE %2 = %3;"_qs
        .arg(quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_TORRENT_ID.name), DB_COLUMN_TORRENT_ID.placeholder);

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    QSqlQuery query {db};
    try
    {
        if (!query.prepare(selectTorrentStatement))
            throw RuntimeError(query.lastError().text());

        query.bindValue(DB_COLUMN_TORRENT_ID.placeholder, id.toString());
        if (!query.exec())
            throw RuntimeError(query.lastError().text());

        if (!query.next())
            throw RuntimeError(tr("Not found."));
    }
    catch (const RuntimeError &err)
    {
        return nonstd::make_unexpected(tr("Couldn't load resume data of torrent '%1'. Error: %2")
            .arg(id.toString(), err.message()));
    }

    return parseQueryResultRow(query);
}

void BitTorrent::DBResumeDataStorage::store(const TorrentID &id, const LoadTorrentParams &resumeData) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, id, resumeData]()
    {
        m_asyncWorker->store(id, resumeData);
    });
}

void BitTorrent::DBResumeDataStorage::remove(const BitTorrent::TorrentID &id) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, id]()
    {
        m_asyncWorker->remove(id);
    });
}

void BitTorrent::DBResumeDataStorage::storeQueue(const QVector<TorrentID> &queue) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, queue]()
    {
        m_asyncWorker->storeQueue(queue);
    });
}

void BitTorrent::DBResumeDataStorage::doLoadAll() const
{
    const QString connectionName = u"ResumeDataStorageLoadAll"_qs;

    {
        auto db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, connectionName);
        db.setDatabaseName(path().data());
        if (!db.open())
            throw RuntimeError(db.lastError().text());

        QSqlQuery query {db};

        const auto selectTorrentIDStatement = u"SELECT %1 FROM %2 ORDER BY %3;"_qs
                .arg(quoted(DB_COLUMN_TORRENT_ID.name), quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_QUEUE_POSITION.name));

        const QReadLocker locker {&m_dbLock};

        if (!query.exec(selectTorrentIDStatement))
            throw RuntimeError(query.lastError().text());

        QVector<TorrentID> registeredTorrents;
        registeredTorrents.reserve(query.size());
        while (query.next())
            registeredTorrents.append(TorrentID::fromString(query.value(0).toString()));

        emit const_cast<DBResumeDataStorage *>(this)->loadStarted(registeredTorrents);

        const auto selectStatement = u"SELECT * FROM %1 ORDER BY %2;"_qs.arg(quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_QUEUE_POSITION.name));
        if (!query.exec(selectStatement))
            throw RuntimeError(query.lastError().text());

        while (query.next())
        {
            const auto torrentID = TorrentID::fromString(query.value(DB_COLUMN_TORRENT_ID.name).toString());
            onResumeDataLoaded(torrentID, parseQueryResultRow(query));
        }
    }

    emit const_cast<DBResumeDataStorage *>(this)->loadFinished();

    QSqlDatabase::removeDatabase(connectionName);
}

int BitTorrent::DBResumeDataStorage::currentDBVersion() const
{
    const auto selectDBVersionStatement = u"SELECT %1 FROM %2 WHERE %3 = %4;"_qs
            .arg(quoted(DB_COLUMN_VALUE.name), quoted(DB_TABLE_META), quoted(DB_COLUMN_NAME.name), DB_COLUMN_NAME.placeholder);

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    QSqlQuery query {db};

    if (!query.prepare(selectDBVersionStatement))
        throw RuntimeError(query.lastError().text());

    query.bindValue(DB_COLUMN_NAME.placeholder, META_VERSION);

    const QReadLocker locker {&m_dbLock};

    if (!query.exec())
        throw RuntimeError(query.lastError().text());

    if (!query.next())
        throw RuntimeError(tr("Database is corrupted."));

    bool ok;
    const int dbVersion = query.value(0).toInt(&ok);
    if (!ok)
        throw RuntimeError(tr("Database is corrupted."));

    return dbVersion;
}

void BitTorrent::DBResumeDataStorage::createDB() const
{
    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);

    const QWriteLocker locker {&m_dbLock};

    if (!db.transaction())
        throw RuntimeError(db.lastError().text());

    QSqlQuery query {db};

    try
    {
        const QStringList tableMetaItems = {
            makeColumnDefinition(DB_COLUMN_ID, "INTEGER PRIMARY KEY"),
            makeColumnDefinition(DB_COLUMN_NAME, "TEXT NOT NULL UNIQUE"),
            makeColumnDefinition(DB_COLUMN_VALUE, "BLOB")
        };
        const QString createTableMetaQuery = makeCreateTableStatement(DB_TABLE_META, tableMetaItems);
        if (!query.exec(createTableMetaQuery))
            throw RuntimeError(query.lastError().text());

        const QString insertMetaVersionQuery = makeInsertStatement(DB_TABLE_META, {DB_COLUMN_NAME, DB_COLUMN_VALUE});
        if (!query.prepare(insertMetaVersionQuery))
            throw RuntimeError(query.lastError().text());

        query.bindValue(DB_COLUMN_NAME.placeholder, META_VERSION);
        query.bindValue(DB_COLUMN_VALUE.placeholder, DB_VERSION);

        if (!query.exec())
            throw RuntimeError(query.lastError().text());

        const QStringList tableTorrentsItems = {
            makeColumnDefinition(DB_COLUMN_ID, "INTEGER PRIMARY KEY"),
            makeColumnDefinition(DB_COLUMN_TORRENT_ID, "BLOB NOT NULL UNIQUE"),
            makeColumnDefinition(DB_COLUMN_QUEUE_POSITION, "INTEGER NOT NULL DEFAULT -1"),
            makeColumnDefinition(DB_COLUMN_NAME, "TEXT"),
            makeColumnDefinition(DB_COLUMN_CATEGORY, "TEXT"),
            makeColumnDefinition(DB_COLUMN_TAGS, "TEXT"),
            makeColumnDefinition(DB_COLUMN_TARGET_SAVE_PATH, "TEXT"),
            makeColumnDefinition(DB_COLUMN_DOWNLOAD_PATH, "TEXT"),
            makeColumnDefinition(DB_COLUMN_CONTENT_LAYOUT, "TEXT NOT NULL"),
            makeColumnDefinition(DB_COLUMN_RATIO_LIMIT, "INTEGER NOT NULL"),
            makeColumnDefinition(DB_COLUMN_SEEDING_TIME_LIMIT, "INTEGER NOT NULL"),
            makeColumnDefinition(DB_COLUMN_HAS_OUTER_PIECES_PRIORITY, "INTEGER NOT NULL"),
            makeColumnDefinition(DB_COLUMN_HAS_SEED_STATUS, "INTEGER NOT NULL"),
            makeColumnDefinition(DB_COLUMN_OPERATING_MODE, "TEXT NOT NULL"),
            makeColumnDefinition(DB_COLUMN_STOPPED, "INTEGER NOT NULL"),
            makeColumnDefinition(DB_COLUMN_RESUMEDATA, "BLOB NOT NULL"),
            makeColumnDefinition(DB_COLUMN_METADATA, "BLOB")
        };
        const QString createTableTorrentsQuery = makeCreateTableStatement(DB_TABLE_TORRENTS, tableTorrentsItems);
        if (!query.exec(createTableTorrentsQuery))
            throw RuntimeError(query.lastError().text());

        if (!db.commit())
            throw RuntimeError(db.lastError().text());
    }
    catch (const RuntimeError &)
    {
        db.rollback();
        throw;
    }
}

void BitTorrent::DBResumeDataStorage::updateDBFromVersion1() const
{
    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);

    const QWriteLocker locker {&m_dbLock};

    if (!db.transaction())
        throw RuntimeError(db.lastError().text());

    QSqlQuery query {db};

    try
    {
        const auto alterTableTorrentsQuery = u"ALTER TABLE %1 ADD %2"_qs
                .arg(quoted(DB_TABLE_TORRENTS), makeColumnDefinition(DB_COLUMN_DOWNLOAD_PATH, "TEXT"));
        if (!query.exec(alterTableTorrentsQuery))
            throw RuntimeError(query.lastError().text());

        const QString updateMetaVersionQuery = makeUpdateStatement(DB_TABLE_META, {DB_COLUMN_NAME, DB_COLUMN_VALUE});
        if (!query.prepare(updateMetaVersionQuery))
            throw RuntimeError(query.lastError().text());

        query.bindValue(DB_COLUMN_NAME.placeholder, META_VERSION);
        query.bindValue(DB_COLUMN_VALUE.placeholder, DB_VERSION);

        if (!query.exec())
            throw RuntimeError(query.lastError().text());

        if (!db.commit())
            throw RuntimeError(db.lastError().text());
    }
    catch (const RuntimeError &)
    {
        db.rollback();
        throw;
    }
}

BitTorrent::DBResumeDataStorage::Worker::Worker(const Path &dbPath, const QString &dbConnectionName, QReadWriteLock &dbLock)
    : m_path {dbPath}
    , m_connectionName {dbConnectionName}
    , m_dbLock {dbLock}
{
}

void BitTorrent::DBResumeDataStorage::Worker::openDatabase() const
{
    auto db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, m_connectionName);
    db.setDatabaseName(m_path.data());
    if (!db.open())
        throw RuntimeError(db.lastError().text());
}

void BitTorrent::DBResumeDataStorage::Worker::closeDatabase() const
{
    QSqlDatabase::removeDatabase(m_connectionName);
}

void BitTorrent::DBResumeDataStorage::Worker::store(const TorrentID &id, const LoadTorrentParams &resumeData) const
{
    // We need to adjust native libtorrent resume data
    lt::add_torrent_params p = resumeData.ltAddTorrentParams;
    p.save_path = Profile::instance()->toPortablePath(Path(p.save_path))
            .toString().toStdString();
    if (resumeData.stopped)
    {
        p.flags |= lt::torrent_flags::paused;
        p.flags &= ~lt::torrent_flags::auto_managed;
    }
    else
    {
        // Torrent can be actually "running" but temporarily "paused" to perform some
        // service jobs behind the scenes so we need to restore it as "running"
        if (resumeData.operatingMode == BitTorrent::TorrentOperatingMode::AutoManaged)
        {
            p.flags |= lt::torrent_flags::auto_managed;
        }
        else
        {
            p.flags &= ~lt::torrent_flags::paused;
            p.flags &= ~lt::torrent_flags::auto_managed;
        }
    }

    QVector<Column> columns {
        DB_COLUMN_TORRENT_ID,
        DB_COLUMN_NAME,
        DB_COLUMN_CATEGORY,
        DB_COLUMN_TAGS,
        DB_COLUMN_TARGET_SAVE_PATH,
        DB_COLUMN_CONTENT_LAYOUT,
        DB_COLUMN_RATIO_LIMIT,
        DB_COLUMN_SEEDING_TIME_LIMIT,
        DB_COLUMN_HAS_OUTER_PIECES_PRIORITY,
        DB_COLUMN_HAS_SEED_STATUS,
        DB_COLUMN_OPERATING_MODE,
        DB_COLUMN_STOPPED,
        DB_COLUMN_RESUMEDATA
    };

    lt::entry data = lt::write_resume_data(p);

    // metadata is stored in separate column
    QByteArray bencodedMetadata;
    if (p.ti)
    {
        lt::entry::dictionary_type &dataDict = data.dict();
        lt::entry metadata {lt::entry::dictionary_t};
        lt::entry::dictionary_type &metadataDict = metadata.dict();
        metadataDict.insert(dataDict.extract("info"));
        metadataDict.insert(dataDict.extract("creation date"));
        metadataDict.insert(dataDict.extract("created by"));
        metadataDict.insert(dataDict.extract("comment"));

        try
        {
            bencodedMetadata.reserve(512 * 1024);
            lt::bencode(std::back_inserter(bencodedMetadata), metadata);
        }
        catch (const std::exception &err)
        {
            LogMsg(tr("Couldn't save torrent metadata. Error: %1.")
                   .arg(QString::fromLocal8Bit(err.what())), Log::CRITICAL);
            return;
        }

        columns.append(DB_COLUMN_METADATA);
    }

    QByteArray bencodedResumeData;
    bencodedResumeData.reserve(256 * 1024);
    lt::bencode(std::back_inserter(bencodedResumeData), data);

    const QString insertTorrentStatement = makeInsertStatement(DB_TABLE_TORRENTS, columns)
            + makeOnConflictUpdateStatement(DB_COLUMN_TORRENT_ID, columns);
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query {db};

    try
    {
        if (!query.prepare(insertTorrentStatement))
            throw RuntimeError(query.lastError().text());

        query.bindValue(DB_COLUMN_TORRENT_ID.placeholder, id.toString());
        query.bindValue(DB_COLUMN_NAME.placeholder, resumeData.name);
        query.bindValue(DB_COLUMN_CATEGORY.placeholder, resumeData.category);
        query.bindValue(DB_COLUMN_TAGS.placeholder, (resumeData.tags.isEmpty()
            ? QVariant(QVariant::String) : resumeData.tags.join(u","_qs)));
        query.bindValue(DB_COLUMN_CONTENT_LAYOUT.placeholder, Utils::String::fromEnum(resumeData.contentLayout));
        query.bindValue(DB_COLUMN_RATIO_LIMIT.placeholder, static_cast<int>(resumeData.ratioLimit * 1000));
        query.bindValue(DB_COLUMN_SEEDING_TIME_LIMIT.placeholder, resumeData.seedingTimeLimit);
        query.bindValue(DB_COLUMN_HAS_OUTER_PIECES_PRIORITY.placeholder, resumeData.firstLastPiecePriority);
        query.bindValue(DB_COLUMN_HAS_SEED_STATUS.placeholder, resumeData.hasSeedStatus);
        query.bindValue(DB_COLUMN_OPERATING_MODE.placeholder, Utils::String::fromEnum(resumeData.operatingMode));
        query.bindValue(DB_COLUMN_STOPPED.placeholder, resumeData.stopped);

        if (!resumeData.useAutoTMM)
        {
            query.bindValue(DB_COLUMN_TARGET_SAVE_PATH.placeholder, Profile::instance()->toPortablePath(resumeData.savePath).data());
            query.bindValue(DB_COLUMN_DOWNLOAD_PATH.placeholder, Profile::instance()->toPortablePath(resumeData.downloadPath).data());
        }

        query.bindValue(DB_COLUMN_RESUMEDATA.placeholder, bencodedResumeData);
        if (!bencodedMetadata.isEmpty())
            query.bindValue(DB_COLUMN_METADATA.placeholder, bencodedMetadata);

        const QWriteLocker locker {&m_dbLock};
        if (!query.exec())
            throw RuntimeError(query.lastError().text());
    }
    catch (const RuntimeError &err)
    {
        LogMsg(tr("Couldn't store resume data for torrent '%1'. Error: %2")
            .arg(id.toString(), err.message()), Log::CRITICAL);
    }
}

void BitTorrent::DBResumeDataStorage::Worker::remove(const TorrentID &id) const
{
    const auto deleteTorrentStatement = u"DELETE FROM %1 WHERE %2 = %3;"_qs
            .arg(quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_TORRENT_ID.name), DB_COLUMN_TORRENT_ID.placeholder);

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query {db};

    try
    {
        if (!query.prepare(deleteTorrentStatement))
            throw RuntimeError(query.lastError().text());

        query.bindValue(DB_COLUMN_TORRENT_ID.placeholder, id.toString());

        const QWriteLocker locker {&m_dbLock};
        if (!query.exec())
            throw RuntimeError(query.lastError().text());
    }
    catch (const RuntimeError &err)
    {
        LogMsg(tr("Couldn't delete resume data of torrent '%1'. Error: %2")
            .arg(id.toString(), err.message()), Log::CRITICAL);
    }
}

void BitTorrent::DBResumeDataStorage::Worker::storeQueue(const QVector<TorrentID> &queue) const
{
    const auto updateQueuePosStatement = u"UPDATE %1 SET %2 = %3 WHERE %4 = %5;"_qs
            .arg(quoted(DB_TABLE_TORRENTS), quoted(DB_COLUMN_QUEUE_POSITION.name), DB_COLUMN_QUEUE_POSITION.placeholder
                 , quoted(DB_COLUMN_TORRENT_ID.name), DB_COLUMN_TORRENT_ID.placeholder);

    auto db = QSqlDatabase::database(m_connectionName);

    try
    {
        const QWriteLocker locker {&m_dbLock};

        if (!db.transaction())
            throw RuntimeError(db.lastError().text());

        QSqlQuery query {db};

        try
        {
            if (!query.prepare(updateQueuePosStatement))
                throw RuntimeError(query.lastError().text());

            int pos = 0;
            for (const TorrentID &torrentID : queue)
            {
                query.bindValue(DB_COLUMN_TORRENT_ID.placeholder, torrentID.toString());
                query.bindValue(DB_COLUMN_QUEUE_POSITION.placeholder, pos++);
                if (!query.exec())
                    throw RuntimeError(query.lastError().text());
            }

            if (!db.commit())
                throw RuntimeError(db.lastError().text());
        }
        catch (const RuntimeError &)
        {
            db.rollback();
            throw;
        }
    }
    catch (const RuntimeError &err)
    {
        LogMsg(tr("Couldn't store torrents queue positions. Error: %1")
            .arg(err.message()), Log::CRITICAL);
    }
}
