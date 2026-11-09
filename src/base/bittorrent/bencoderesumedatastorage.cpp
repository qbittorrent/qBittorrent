/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "bencoderesumedatastorage.h"

#include <memory>
#include <queue>

#include <libtorrent/bdecode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/version.hpp>
#include <libtorrent/write_resume_data.hpp>
#if LIBTORRENT_VERSION_NUM >= 20100
#include <libtorrent/load_torrent.hpp>
#endif

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QWaitCondition>

#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/tagset.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/sslkey.h"
#include "base/utils/string.h"
#include "infohash.h"
#include "loadtorrentparams.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    using namespace BitTorrent;

    class Job
    {
    public:
        virtual ~Job() = default;
        virtual void perform(const Path &resumeDataDir) = 0;
    };

    class StoreJob final : public Job
    {
    public:
        StoreJob(const TorrentID &torrentID, LoadTorrentParams resumeData);
        void perform(const Path &resumeDataDir) override;

    private:
        const TorrentID m_torrentID;
        const LoadTorrentParams m_resumeData;
    };

    class RemoveJob final : public Job
    {
    public:
        explicit RemoveJob(const TorrentID &torrentID);
        void perform(const Path &resumeDataDir) override;

    private:
        const TorrentID m_torrentID;
    };

    class StoreQueueJob final : public Job
    {
    public:
        explicit StoreQueueJob(const QList<TorrentID> &queue);
        void perform(const Path &resumeDataDir) override;

    private:
        const QList<TorrentID> m_queue;
    };
}

namespace BitTorrent
{
    class BencodeResumeDataStorage::Worker final : public QThread
    {
        Q_DISABLE_COPY_MOVE(Worker)

    public:
        Worker(const Path &resumeDataDir, QObject *parent = nullptr);

        void run() override;
        void requestInterruption();

        void store(const TorrentID &id, LoadTorrentParams resumeData);
        void remove(const TorrentID &id);
        void storeQueue(const QList<TorrentID> &queue);

    private:
        void addJob(std::unique_ptr<Job> job);

        const Path m_resumeDataDir;

        std::queue<std::unique_ptr<Job>> m_jobs;
        QMutex m_jobsMutex;
        QWaitCondition m_waitCondition;
    };
}

namespace
{
    const char KEY_SSL_CERTIFICATE[] = "qBt-sslCertificate";
    const char KEY_SSL_PRIVATE_KEY[] = "qBt-sslPrivateKey";
    const char KEY_SSL_DH_PARAMS[] = "qBt-sslDhParams";

    template <typename LTStr>
    QString fromLTString(const LTStr &str)
    {
        return QString::fromUtf8(str.data(), static_cast<qsizetype>(str.size()));
    }

    template <typename LTStr>
    QByteArray toByteArray(const LTStr &str)
    {
        return {str.data(), static_cast<qsizetype>(str.size())};
    }

    using ListType = lt::entry::list_type;

    ListType setToEntryList(const TagSet &input)
    {
        ListType entryList;
        entryList.reserve(input.size());
        for (const Tag &setValue : input)
            entryList.emplace_back(setValue.toString().toStdString());
        return entryList;
    }
}

BitTorrent::BencodeResumeDataStorage::BencodeResumeDataStorage(const Path &path, QObject *parent)
    : ResumeDataStorage(path, parent)
{
    Q_ASSERT(path.isAbsolute());

    if (!path.exists() && !Utils::Fs::mkpath(path))
    {
        throw RuntimeError(tr("Cannot create torrent resume folder: \"%1\"")
                    .arg(path.toString()));
    }

    const QRegularExpression filenamePattern {u"^([A-Fa-f0-9]{40})\\.fastresume$"_s};
    const QStringList filenames = QDir(path.data()).entryList({u"*.fastresume"_s}, QDir::Files);

    m_registeredTorrents.reserve(filenames.size());
    for (const QString &filename : filenames)
    {
        const QRegularExpressionMatch rxMatch = filenamePattern.match(filename);
        if (rxMatch.hasMatch())
            m_registeredTorrents.append(TorrentID::fromString(rxMatch.captured(1)));
    }

    loadQueue(path / Path(u"queue"_s));

    qDebug() << "Registered torrents count: " << m_registeredTorrents.size();

    m_asyncWorker = new Worker(path, this);
    m_asyncWorker->start();
}

BencodeResumeDataStorage::~BencodeResumeDataStorage()
{
    m_asyncWorker->requestInterruption();
    m_asyncWorker->wait();
}

QList<BitTorrent::TorrentID> BitTorrent::BencodeResumeDataStorage::registeredTorrents() const
{
    return m_registeredTorrents;
}

BitTorrent::LoadResumeDataResult BitTorrent::BencodeResumeDataStorage::load(const TorrentID &id) const
{
    const QString idString = id.toString();
    const Path fastresumePath = path() / Path(idString + u".fastresume");
    const Path torrentFilePath = path() / Path(idString + u".torrent");
    const qint64 torrentSizeLimit = Preferences::instance()->getTorrentFileSizeLimit();

    const auto resumeDataReadResult = Utils::IO::readFile(fastresumePath, -1);
    if (!resumeDataReadResult)
        return nonstd::make_unexpected(resumeDataReadResult.error().message);

    const auto metadataReadResult = Utils::IO::readFile(torrentFilePath, torrentSizeLimit);
    if (!metadataReadResult)
    {
        if (metadataReadResult.error().status != Utils::IO::ReadError::NotExist)
            return nonstd::make_unexpected(metadataReadResult.error().message);
    }

    const QByteArray data = resumeDataReadResult.value();
    const QByteArray metadata = metadataReadResult.value_or(QByteArray());
    return loadTorrentResumeData(data, metadata);
}

void BitTorrent::BencodeResumeDataStorage::doLoadAll() const
{
    qDebug() << "Loading torrents count: " << m_registeredTorrents.size();

    emit const_cast<BencodeResumeDataStorage *>(this)->loadStarted(m_registeredTorrents);

    for (const TorrentID &torrentID : asConst(m_registeredTorrents))
        onResumeDataLoaded(torrentID, load(torrentID));

    emit const_cast<BencodeResumeDataStorage *>(this)->loadFinished();
}

void BitTorrent::BencodeResumeDataStorage::loadQueue(const Path &queueFilename)
{
    const int lineMaxLength = 48;

    QFile queueFile {queueFilename.data()};
    if (!queueFile.exists())
        return;

    if (!queueFile.open(QFile::ReadOnly))
    {
        LogMsg(tr("Couldn't load torrents queue: %1").arg(queueFile.errorString()), Log::WARNING);
        return;
    }

    QHash<TorrentID, qsizetype> registeredTorrentsIndexes;
    registeredTorrentsIndexes.reserve(m_registeredTorrents.length());
    for (qsizetype i = 0; i < m_registeredTorrents.length(); ++i)
        registeredTorrentsIndexes.insert(m_registeredTorrents.at(i), i);

    const QRegularExpression hashPattern {u"^([A-Fa-f0-9]{40})$"_s};
    qsizetype queuePos = 0;
    while (true)
    {
        const auto line = QString::fromLatin1(queueFile.readLine(lineMaxLength).trimmed());
        if (line.isEmpty())
            break;

        const QRegularExpressionMatch rxMatch = hashPattern.match(line);
        if (rxMatch.hasMatch())
        {
            const auto torrentID = BitTorrent::TorrentID::fromString(rxMatch.captured(1));
            const qsizetype pos = registeredTorrentsIndexes.value(torrentID, -1);
            if (pos != -1)
            {
                if (pos != queuePos)
                {
                    m_registeredTorrents.swapItemsAt(pos, queuePos);
                    registeredTorrentsIndexes.insert(m_registeredTorrents.at(pos), pos);
                }
                ++queuePos;
            }
        }
    }
}

BitTorrent::LoadResumeDataResult BitTorrent::BencodeResumeDataStorage::loadTorrentResumeData(const QByteArray &data, const QByteArray &metadata) const
{
    const auto *pref = Preferences::instance();

    lt::error_code ec;
    const lt::bdecode_node resumeDataRoot = lt::bdecode(data, ec
            , nullptr, pref->getBdecodeDepthLimit(), pref->getBdecodeTokenLimit());
    if (ec)
        return nonstd::make_unexpected(tr("Cannot parse resume data: %1").arg(QString::fromStdString(ec.message())));

    if (resumeDataRoot.type() != lt::bdecode_node::dict_t)
        return nonstd::make_unexpected(tr("Cannot parse resume data: invalid format"));

    LoadTorrentParams torrentParams;
    torrentParams.category = fromLTString(resumeDataRoot.dict_find_string_value("qBt-category"));
    torrentParams.name = fromLTString(resumeDataRoot.dict_find_string_value("qBt-name"));
    torrentParams.comment = fromLTString(resumeDataRoot.dict_find_string_value("qBt-comment"));
    torrentParams.hasFinishedStatus = resumeDataRoot.dict_find_int_value("qBt-seedStatus");
    torrentParams.firstLastPiecePriority = resumeDataRoot.dict_find_int_value("qBt-firstLastPiecePriority");

    const lt::string_view ratioLimitString = resumeDataRoot.dict_find_string_value("qBt-ratioLimit");
    torrentParams.shareLimits = {
        .ratioLimit = ratioLimitString.empty()
            ? resumeDataRoot.dict_find_int_value("qBt-ratioLimit", DEFAULT_RATIO_LIMIT * 1000) / 1000.0
            : fromLTString(ratioLimitString).toDouble(),
        .seedingTimeLimit = static_cast<int>(resumeDataRoot.dict_find_int_value("qBt-seedingTimeLimit", DEFAULT_SEEDING_TIME_LIMIT)),
        .inactiveSeedingTimeLimit = static_cast<int>(resumeDataRoot.dict_find_int_value("qBt-inactiveSeedingTimeLimit", DEFAULT_SEEDING_TIME_LIMIT)),
        .mode = Utils::String::toEnum(fromLTString(resumeDataRoot.dict_find_string_value("qBt-shareLimitsMode")), ShareLimitsMode::Default),
        .action = Utils::String::toEnum(fromLTString(resumeDataRoot.dict_find_string_value("qBt-shareLimitAction")), ShareLimitAction::Default)
    };

    torrentParams.savePath = Profile::instance()->fromPortablePath(
            Path(fromLTString(resumeDataRoot.dict_find_string_value("qBt-savePath"))));
    torrentParams.useAutoTMM = torrentParams.savePath.isEmpty();
    if (!torrentParams.useAutoTMM)
    {
        torrentParams.downloadPath = Profile::instance()->fromPortablePath(
                Path(fromLTString(resumeDataRoot.dict_find_string_value("qBt-downloadPath"))));
    }

    // TODO: The following code is deprecated. Replace with the commented one after several releases in 4.4.x.
    // === BEGIN DEPRECATED CODE === //
    const lt::bdecode_node contentLayoutNode = resumeDataRoot.dict_find("qBt-contentLayout");
    if (contentLayoutNode.type() == lt::bdecode_node::string_t)
    {
        const QString contentLayoutStr = fromLTString(contentLayoutNode.string_value());
        torrentParams.contentLayout = Utils::String::toEnum(contentLayoutStr, TorrentContentLayout::Original);
    }
    else
    {
        const bool hasRootFolder = resumeDataRoot.dict_find_int_value("qBt-hasRootFolder");
        torrentParams.contentLayout = (hasRootFolder ? TorrentContentLayout::Original : TorrentContentLayout::NoSubfolder);
    }
    // === END DEPRECATED CODE === //
    // === BEGIN REPLACEMENT CODE === //
    //    torrentParams.contentLayout = Utils::String::parse(
    //                fromLTString(root.dict_find_string_value("qBt-contentLayout")), TorrentContentLayout::Default);
    // === END REPLACEMENT CODE === //

    torrentParams.stopCondition = Utils::String::toEnum(
            fromLTString(resumeDataRoot.dict_find_string_value("qBt-stopCondition")), Torrent::StopCondition::None);
    torrentParams.sslParameters =
    {
        .certificate = QSslCertificate(toByteArray(resumeDataRoot.dict_find_string_value(KEY_SSL_CERTIFICATE))),
        .privateKey = Utils::SSLKey::load(toByteArray(resumeDataRoot.dict_find_string_value(KEY_SSL_PRIVATE_KEY))),
        .dhParams = toByteArray(resumeDataRoot.dict_find_string_value(KEY_SSL_DH_PARAMS))
    };

    const lt::bdecode_node tagsNode = resumeDataRoot.dict_find("qBt-tags");
    if (tagsNode.type() == lt::bdecode_node::list_t)
    {
        for (int i = 0; i < tagsNode.list_size(); ++i)
        {
            const Tag tag {fromLTString(tagsNode.list_string_value_at(i))};
            torrentParams.tags.insert(tag);
        }
    }

    lt::add_torrent_params &p = torrentParams.ltAddTorrentParams;

    p = lt::read_resume_data(resumeDataRoot, ec);
    if (ec)
        return nonstd::make_unexpected(tr("Cannot parse resume data: %1").arg(QString::fromStdString(ec.message())));

    if (!metadata.isEmpty())
    {
        const lt::bdecode_node torrentInfoRoot = lt::bdecode(metadata, ec
                , nullptr, pref->getBdecodeDepthLimit(), pref->getBdecodeTokenLimit());
        if (ec)
            return nonstd::make_unexpected(tr("Cannot parse torrent info: %1").arg(QString::fromStdString(ec.message())));

        if (torrentInfoRoot.type() != lt::bdecode_node::dict_t)
            return nonstd::make_unexpected(tr("Cannot parse torrent info: invalid format"));

#if LIBTORRENT_VERSION_NUM >= 20100
        const lt::load_torrent_limits limits {
            .max_buffer_size = static_cast<int>(pref->getTorrentFileSizeLimit()),
            .max_decode_depth = pref->getBdecodeDepthLimit(),
            .max_decode_tokens = pref->getBdecodeTokenLimit(),
            .max_directory_depth = pref->getLoadTorrentMaxDirectoryDepth()};
        const lt::add_torrent_params atp = lt::load_torrent_parsed(torrentInfoRoot, ec, limits);
        if (ec)
            return nonstd::make_unexpected(tr("Cannot parse torrent info: %1").arg(QString::fromStdString(ec.message())));

        p.ti = atp.ti;
        p.creation_date = atp.creation_date;
        p.created_by = atp.created_by;
        p.comment = atp.comment;
#else
        const auto torrentInfo = std::make_shared<lt::torrent_info>(torrentInfoRoot, ec);
        if (ec)
            return nonstd::make_unexpected(tr("Cannot parse torrent info: %1").arg(QString::fromStdString(ec.message())));

        p.ti = torrentInfo;
#endif

#ifdef QBT_USES_LIBTORRENT2
        if (((p.info_hashes.has_v1() && (p.info_hashes.v1 != p.ti->info_hashes().v1))
                || (p.info_hashes.has_v2() && (p.info_hashes.v2 != p.ti->info_hashes().v2))))
#else
        if (!p.info_hash.is_all_zeros() && (p.info_hash != p.ti->info_hash()))
#endif
        {
            return nonstd::make_unexpected(tr("Mismatching info-hash detected in resume data"));
        }
    }

    p.save_path = Profile::instance()->fromPortablePath(
                Path(fromLTString(p.save_path))).toString().toStdString();
    if (p.save_path.empty())
        return nonstd::make_unexpected(tr("Corrupted resume data: %1").arg(tr("save_path is invalid")));

    torrentParams.stopped = (p.flags & lt::torrent_flags::paused) && !(p.flags & lt::torrent_flags::auto_managed);
    torrentParams.operatingMode = (p.flags & lt::torrent_flags::paused) || (p.flags & lt::torrent_flags::auto_managed)
            ? TorrentOperatingMode::AutoManaged : TorrentOperatingMode::Forced;

    if (p.flags & lt::torrent_flags::stop_when_ready)
    {
        p.flags &= ~lt::torrent_flags::stop_when_ready;
        torrentParams.stopCondition = Torrent::StopCondition::FilesChecked;
    }

    const bool hasMetadata = (p.ti && p.ti->is_valid());
    if (!hasMetadata && !resumeDataRoot.dict_find("info-hash"))
        return nonstd::make_unexpected(tr("Resume data is invalid: neither metadata nor info-hash was found"));

    return torrentParams;
}

void BitTorrent::BencodeResumeDataStorage::store(const TorrentID &id, LoadTorrentParams resumeData) const
{
    m_asyncWorker->store(id, std::move(resumeData));
}

void BitTorrent::BencodeResumeDataStorage::remove(const TorrentID &id) const
{
    m_asyncWorker->remove(id);
}

void BitTorrent::BencodeResumeDataStorage::storeQueue(const QList<TorrentID> &queue) const
{
    m_asyncWorker->storeQueue(queue);
}

BitTorrent::BencodeResumeDataStorage::Worker::Worker(const Path &resumeDataDir, QObject *parent)
    : QThread(parent)
    , m_resumeDataDir {resumeDataDir}
{
}

void BitTorrent::BencodeResumeDataStorage::Worker::run()
{
    while (true)
    {
        QMutexLocker jobsLocker {&m_jobsMutex};

        if (m_jobs.empty())
        {
            if (isInterruptionRequested())
                break;

            m_waitCondition.wait(&m_jobsMutex);
            if (m_jobs.empty())
            {
                Q_ASSERT(isInterruptionRequested());
                break;
            }
        }

        std::unique_ptr<Job> job = std::move(m_jobs.front());
        m_jobs.pop();
        jobsLocker.unlock();

        job->perform(m_resumeDataDir);
    }
}

void BencodeResumeDataStorage::Worker::requestInterruption()
{
    const QMutexLocker jobsLocker {&m_jobsMutex};
    QThread::requestInterruption();
    m_waitCondition.wakeAll();
}

void BitTorrent::BencodeResumeDataStorage::Worker::store(const TorrentID &id, LoadTorrentParams resumeData)
{
    addJob(std::make_unique<StoreJob>(id, std::move(resumeData)));
}

void BitTorrent::BencodeResumeDataStorage::Worker::remove(const TorrentID &id)
{
    addJob(std::make_unique<RemoveJob>(id));
}

void BitTorrent::BencodeResumeDataStorage::Worker::storeQueue(const QList<TorrentID> &queue)
{
    addJob(std::make_unique<StoreQueueJob>(queue));
}

void BitTorrent::BencodeResumeDataStorage::Worker::addJob(std::unique_ptr<Job> job)
{
    const QMutexLocker jobsLocker {&m_jobsMutex};
    m_jobs.push(std::move(job));
    m_waitCondition.wakeAll();
}

namespace
{
    using namespace BitTorrent;

    StoreJob::StoreJob(const TorrentID &torrentID, LoadTorrentParams resumeData)
        : m_torrentID {torrentID}
        , m_resumeData {std::move(resumeData)}
    {
    }

    void StoreJob::perform(const Path &resumeDataDir)
    {
        // We need to adjust native libtorrent resume data
        lt::add_torrent_params p = m_resumeData.ltAddTorrentParams;
        p.save_path = Profile::instance()->toPortablePath(Path(p.save_path)).toString().toStdString();
        if (m_resumeData.stopped)
        {
            p.flags |= lt::torrent_flags::paused;
            p.flags &= ~lt::torrent_flags::auto_managed;
        }
        else
        {
            // Torrent can be actually "running" but temporarily "paused" to perform some
            // service jobs behind the scenes so we need to restore it as "running"
            if (m_resumeData.operatingMode == BitTorrent::TorrentOperatingMode::AutoManaged)
            {
                p.flags |= lt::torrent_flags::auto_managed;
            }
            else
            {
                p.flags &= ~lt::torrent_flags::paused;
                p.flags &= ~lt::torrent_flags::auto_managed;
            }
        }

        lt::entry data = lt::write_resume_data(p);

        // metadata is stored in separate .torrent file
        if (p.ti)
        {
            lt::entry::dictionary_type &dataDict = data.dict();
            lt::entry metadata {lt::entry::dictionary_t};
            lt::entry::dictionary_type &metadataDict = metadata.dict();
            metadataDict.insert(dataDict.extract("info"));
            metadataDict.insert(dataDict.extract("creation date"));
            metadataDict.insert(dataDict.extract("created by"));
            metadataDict.insert(dataDict.extract("comment"));

            const Path torrentFilepath = resumeDataDir / Path(u"%1.torrent"_s.arg(m_torrentID.toString()));
            const nonstd::expected<void, QString> result = Utils::IO::saveToFile(torrentFilepath, metadata);
            if (!result)
            {
                LogMsg(ResumeDataStorage::tr("Couldn't save torrent metadata to '%1'. Error: %2.")
                        .arg(torrentFilepath.toString(), result.error()), Log::CRITICAL);
                return;
            }
        }

        data["qBt-ratioLimit"] = static_cast<int>(m_resumeData.shareLimits.ratioLimit * 1000);
        data["qBt-seedingTimeLimit"] = m_resumeData.shareLimits.seedingTimeLimit;
        data["qBt-inactiveSeedingTimeLimit"] = m_resumeData.shareLimits.inactiveSeedingTimeLimit;
        data["qBt-shareLimitsMode"] = Utils::String::fromEnum(m_resumeData.shareLimits.mode).toStdString();
        data["qBt-shareLimitAction"] = Utils::String::fromEnum(m_resumeData.shareLimits.action).toStdString();

        data["qBt-category"] = m_resumeData.category.toStdString();
        data["qBt-tags"] = setToEntryList(m_resumeData.tags);
        data["qBt-name"] = m_resumeData.name.toStdString();
        data["qBt-comment"] = m_resumeData.comment.toStdString();
        data["qBt-seedStatus"] = m_resumeData.hasFinishedStatus;
        data["qBt-contentLayout"] = Utils::String::fromEnum(m_resumeData.contentLayout).toStdString();
        data["qBt-firstLastPiecePriority"] = m_resumeData.firstLastPiecePriority;
        data["qBt-stopCondition"] = Utils::String::fromEnum(m_resumeData.stopCondition).toStdString();

        if (!m_resumeData.sslParameters.certificate.isNull())
            data[KEY_SSL_CERTIFICATE] = m_resumeData.sslParameters.certificate.toPem().toStdString();
        if (!m_resumeData.sslParameters.privateKey.isNull())
            data[KEY_SSL_PRIVATE_KEY] = m_resumeData.sslParameters.privateKey.toPem().toStdString();
        if (!m_resumeData.sslParameters.dhParams.isEmpty())
            data[KEY_SSL_DH_PARAMS] = m_resumeData.sslParameters.dhParams.toStdString();

        if (!m_resumeData.useAutoTMM)
        {
            data["qBt-savePath"] = Profile::instance()->toPortablePath(m_resumeData.savePath).data().toStdString();
            data["qBt-downloadPath"] = Profile::instance()->toPortablePath(m_resumeData.downloadPath).data().toStdString();
        }

        const Path resumeFilepath = resumeDataDir / Path(u"%1.fastresume"_s.arg(m_torrentID.toString()));
        const nonstd::expected<void, QString> result = Utils::IO::saveToFile(resumeFilepath, data);
        if (!result)
        {
            LogMsg(ResumeDataStorage::tr("Couldn't save torrent resume data to '%1'. Error: %2.")
                    .arg(resumeFilepath.toString(), result.error()), Log::CRITICAL);
        }
    }

    RemoveJob::RemoveJob(const TorrentID &torrentID)
        : m_torrentID {torrentID}
    {
    }

    void RemoveJob::perform(const Path &resumeDataDir)
    {
        const Path resumeFilename {u"%1.fastresume"_s.arg(m_torrentID.toString())};
        std::ignore = Utils::Fs::removeFile(resumeDataDir / resumeFilename);

        const Path torrentFilename {u"%1.torrent"_s.arg(m_torrentID.toString())};
        std::ignore = Utils::Fs::removeFile(resumeDataDir / torrentFilename);
    }

    StoreQueueJob::StoreQueueJob(const QList<TorrentID> &queue)
        : m_queue {queue}
    {
    }

    void StoreQueueJob::perform(const Path &resumeDataDir)
    {
        QByteArray data;
        data.reserve(((BitTorrent::TorrentID::length() * 2) + 1) * m_queue.size());
        for (const BitTorrent::TorrentID &torrentID : m_queue)
            data += (torrentID.toString().toLatin1() + '\n');

        const Path filepath = resumeDataDir / Path(u"queue"_s);
        const nonstd::expected<void, QString> result = Utils::IO::saveToFile(filepath, data);
        if (!result)
        {
            LogMsg(ResumeDataStorage::tr("Couldn't save data to '%1'. Error: %2")
                    .arg(filepath.toString(), result.error()), Log::CRITICAL);
        }
    }
}
