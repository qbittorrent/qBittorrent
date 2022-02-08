/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include <libtorrent/bdecode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <QByteArray>
#include <QDebug>
#include <QRegularExpression>
#include <QThread>

#include "base/algorithm.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/tagset.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "infohash.h"
#include "loadtorrentparams.h"

namespace BitTorrent
{
    class BencodeResumeDataStorage::Worker final : public QObject
    {
        Q_DISABLE_COPY_MOVE(Worker)

    public:
        explicit Worker(const Path &resumeDataDir);

        void store(const TorrentID &id, const LoadTorrentParams &resumeData) const;
        void remove(const TorrentID &id) const;
        void storeQueue(const QVector<TorrentID> &queue) const;

    private:
        const Path m_resumeDataDir;
    };
}

namespace
{
    template <typename LTStr>
    QString fromLTString(const LTStr &str)
    {
        return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    }

    using ListType = lt::entry::list_type;

    ListType setToEntryList(const TagSet &input)
    {
        ListType entryList;
        entryList.reserve(input.size());
        for (const QString &setValue : input)
            entryList.emplace_back(setValue.toStdString());
        return entryList;
    }
}

BitTorrent::BencodeResumeDataStorage::BencodeResumeDataStorage(const Path &path, QObject *parent)
    : ResumeDataStorage {parent}
    , m_resumeDataPath {path}
    , m_ioThread {new QThread {this}}
    , m_asyncWorker {new Worker(m_resumeDataPath)}
{
    Q_ASSERT(path.isAbsolute());

    if (!m_resumeDataPath.exists() && !Utils::Fs::mkpath(m_resumeDataPath))
    {
        throw RuntimeError(tr("Cannot create torrent resume folder: \"%1\"")
                    .arg(m_resumeDataPath.toString()));
    }

    const QRegularExpression filenamePattern {QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$")};
    const QStringList filenames = QDir(m_resumeDataPath.data()).entryList(QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);

    m_registeredTorrents.reserve(filenames.size());
    for (const QString &filename : filenames)
    {
         const QRegularExpressionMatch rxMatch = filenamePattern.match(filename);
         if (rxMatch.hasMatch())
             m_registeredTorrents.append(TorrentID::fromString(rxMatch.captured(1)));
    }

    loadQueue(m_resumeDataPath / Path("queue"));

    qDebug() << "Registered torrents count: " << m_registeredTorrents.size();

    m_asyncWorker->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_asyncWorker, &QObject::deleteLater);
    m_ioThread->start();
}

BitTorrent::BencodeResumeDataStorage::~BencodeResumeDataStorage()
{
    m_ioThread->quit();
    m_ioThread->wait();
}

QVector<BitTorrent::TorrentID> BitTorrent::BencodeResumeDataStorage::registeredTorrents() const
{
    return m_registeredTorrents;
}

std::optional<BitTorrent::LoadTorrentParams> BitTorrent::BencodeResumeDataStorage::load(const TorrentID &id) const
{
    const QString idString = id.toString();
    const Path fastresumePath = m_resumeDataPath / Path(idString + QLatin1String(".fastresume"));
    const Path torrentFilePath = m_resumeDataPath / Path(idString + QLatin1String(".torrent"));

    QFile resumeDataFile {fastresumePath.data()};
    if (!resumeDataFile.open(QIODevice::ReadOnly))
    {
        LogMsg(tr("Cannot read file %1: %2").arg(fastresumePath.toString(), resumeDataFile.errorString()), Log::WARNING);
        return std::nullopt;
    }

    QFile metadataFile {torrentFilePath.data()};
    if (metadataFile.exists() && !metadataFile.open(QIODevice::ReadOnly))
    {
        LogMsg(tr("Cannot read file %1: %2").arg(torrentFilePath.toString(), metadataFile.errorString()), Log::WARNING);
        return std::nullopt;
    }

    const QByteArray data = resumeDataFile.readAll();
    const QByteArray metadata = (metadataFile.isOpen() ? metadataFile.readAll() : "");

    return loadTorrentResumeData(data, metadata);
}

std::optional<BitTorrent::LoadTorrentParams> BitTorrent::BencodeResumeDataStorage::loadTorrentResumeData(
        const QByteArray &data, const QByteArray &metadata) const
{
    const QByteArray allData = ((metadata.isEmpty() || data.isEmpty())
                                ? data : (data.chopped(1) + metadata.mid(1)));

    lt::error_code ec;
    const lt::bdecode_node root = lt::bdecode(allData, ec);
    if (ec || (root.type() != lt::bdecode_node::dict_t))
        return std::nullopt;

    LoadTorrentParams torrentParams;
    torrentParams.restored = true;
    torrentParams.category = fromLTString(root.dict_find_string_value("qBt-category"));
    torrentParams.name = fromLTString(root.dict_find_string_value("qBt-name"));
    torrentParams.hasSeedStatus = root.dict_find_int_value("qBt-seedStatus");
    torrentParams.firstLastPiecePriority = root.dict_find_int_value("qBt-firstLastPiecePriority");
    torrentParams.seedingTimeLimit = root.dict_find_int_value("qBt-seedingTimeLimit", Torrent::USE_GLOBAL_SEEDING_TIME);

    torrentParams.savePath = Profile::instance()->fromPortablePath(
                Path(fromLTString(root.dict_find_string_value("qBt-savePath"))));
    torrentParams.useAutoTMM = torrentParams.savePath.isEmpty();
    if (!torrentParams.useAutoTMM)
    {
        torrentParams.downloadPath = Profile::instance()->fromPortablePath(
                    Path(fromLTString(root.dict_find_string_value("qBt-downloadPath"))));
    }

    // TODO: The following code is deprecated. Replace with the commented one after several releases in 4.4.x.
    // === BEGIN DEPRECATED CODE === //
    const lt::bdecode_node contentLayoutNode = root.dict_find("qBt-contentLayout");
    if (contentLayoutNode.type() == lt::bdecode_node::string_t)
    {
        const QString contentLayoutStr = fromLTString(contentLayoutNode.string_value());
        torrentParams.contentLayout = Utils::String::toEnum(contentLayoutStr, TorrentContentLayout::Original);
    }
    else
    {
        const bool hasRootFolder = root.dict_find_int_value("qBt-hasRootFolder");
        torrentParams.contentLayout = (hasRootFolder ? TorrentContentLayout::Original : TorrentContentLayout::NoSubfolder);
    }
    // === END DEPRECATED CODE === //
    // === BEGIN REPLACEMENT CODE === //
    //    torrentParams.contentLayout = Utils::String::parse(
    //                fromLTString(root.dict_find_string_value("qBt-contentLayout")), TorrentContentLayout::Default);
    // === END REPLACEMENT CODE === //

    const lt::string_view ratioLimitString = root.dict_find_string_value("qBt-ratioLimit");
    if (ratioLimitString.empty())
        torrentParams.ratioLimit = root.dict_find_int_value("qBt-ratioLimit", Torrent::USE_GLOBAL_RATIO * 1000) / 1000.0;
    else
        torrentParams.ratioLimit = fromLTString(ratioLimitString).toDouble();

    const lt::bdecode_node tagsNode = root.dict_find("qBt-tags");
    if (tagsNode.type() == lt::bdecode_node::list_t)
    {
        for (int i = 0; i < tagsNode.list_size(); ++i)
        {
            const QString tag = fromLTString(tagsNode.list_string_value_at(i));
            torrentParams.tags.insert(tag);
        }
    }

    lt::add_torrent_params &p = torrentParams.ltAddTorrentParams;

    p = lt::read_resume_data(root, ec);
    p.save_path = Profile::instance()->fromPortablePath(
                Path(fromLTString(p.save_path))).toString().toStdString();

    if (p.flags & lt::torrent_flags::stop_when_ready)
    {
        // If torrent has "stop_when_ready" flag set then it is actually "stopped"
        torrentParams.stopped = true;
        torrentParams.operatingMode = TorrentOperatingMode::AutoManaged;
        // ...but temporarily "resumed" to perform some service jobs (e.g. checking)
        p.flags &= ~lt::torrent_flags::paused;
        p.flags |= lt::torrent_flags::auto_managed;
    }
    else
    {
        torrentParams.stopped = (p.flags & lt::torrent_flags::paused) && !(p.flags & lt::torrent_flags::auto_managed);
        torrentParams.operatingMode = (p.flags & lt::torrent_flags::paused) || (p.flags & lt::torrent_flags::auto_managed)
                ? TorrentOperatingMode::AutoManaged : TorrentOperatingMode::Forced;
    }

    const bool hasMetadata = (p.ti && p.ti->is_valid());
    if (!hasMetadata && !root.dict_find("info-hash"))
        return std::nullopt;

    return torrentParams;
}

void BitTorrent::BencodeResumeDataStorage::store(const TorrentID &id, const LoadTorrentParams &resumeData) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, id, resumeData]()
    {
        m_asyncWorker->store(id, resumeData);
    });
}

void BitTorrent::BencodeResumeDataStorage::remove(const TorrentID &id) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, id]()
    {
        m_asyncWorker->remove(id);
    });
}

void BitTorrent::BencodeResumeDataStorage::storeQueue(const QVector<TorrentID> &queue) const
{
    QMetaObject::invokeMethod(m_asyncWorker, [this, queue]()
    {
        m_asyncWorker->storeQueue(queue);
    });
}

void BitTorrent::BencodeResumeDataStorage::loadQueue(const Path &queueFilename)
{
    QFile queueFile {queueFilename.data()};
    if (!queueFile.exists())
        return;

    if (queueFile.open(QFile::ReadOnly))
    {
        const QRegularExpression hashPattern {QLatin1String("^([A-Fa-f0-9]{40})$")};
        QByteArray line;
        int start = 0;
        while (!(line = queueFile.readLine().trimmed()).isEmpty())
        {
            const QRegularExpressionMatch rxMatch = hashPattern.match(line);
            if (rxMatch.hasMatch())
            {
                const auto torrentID = TorrentID::fromString(rxMatch.captured(1));
                const int pos = m_registeredTorrents.indexOf(torrentID, start);
                if (pos != -1)
                {
                    std::swap(m_registeredTorrents[start], m_registeredTorrents[pos]);
                    ++start;
                }
            }
        }
    }
    else
    {
        LogMsg(tr("Couldn't load torrents queue from '%1'. Error: %2")
               .arg(queueFile.fileName(), queueFile.errorString()), Log::WARNING);
    }
}

BitTorrent::BencodeResumeDataStorage::Worker::Worker(const Path &resumeDataDir)
    : m_resumeDataDir {resumeDataDir}
{
}

void BitTorrent::BencodeResumeDataStorage::Worker::store(const TorrentID &id, const LoadTorrentParams &resumeData) const
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

        const Path torrentFilepath = m_resumeDataDir / Path(QString::fromLatin1("%1.torrent").arg(id.toString()));
        const nonstd::expected<void, QString> result = Utils::IO::saveToFile(torrentFilepath, metadata);
        if (!result)
        {
            LogMsg(tr("Couldn't save torrent metadata to '%1'. Error: %2.")
                   .arg(torrentFilepath.toString(), result.error()), Log::CRITICAL);
            return;
        }
    }

    data["qBt-ratioLimit"] = static_cast<int>(resumeData.ratioLimit * 1000);
    data["qBt-seedingTimeLimit"] = resumeData.seedingTimeLimit;
    data["qBt-category"] = resumeData.category.toStdString();
    data["qBt-tags"] = setToEntryList(resumeData.tags);
    data["qBt-name"] = resumeData.name.toStdString();
    data["qBt-seedStatus"] = resumeData.hasSeedStatus;
    data["qBt-contentLayout"] = Utils::String::fromEnum(resumeData.contentLayout).toStdString();
    data["qBt-firstLastPiecePriority"] = resumeData.firstLastPiecePriority;

    if (!resumeData.useAutoTMM)
    {
        data["qBt-savePath"] = Profile::instance()->toPortablePath(resumeData.savePath).data().toStdString();
        data["qBt-downloadPath"] = Profile::instance()->toPortablePath(resumeData.downloadPath).data().toStdString();
    }

    const Path resumeFilepath = m_resumeDataDir / Path(QString::fromLatin1("%1.fastresume").arg(id.toString()));
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(resumeFilepath, data);
    if (!result)
    {
        LogMsg(tr("Couldn't save torrent resume data to '%1'. Error: %2.")
               .arg(resumeFilepath.toString(), result.error()), Log::CRITICAL);
    }
}

void BitTorrent::BencodeResumeDataStorage::Worker::remove(const TorrentID &id) const
{
    const Path resumeFilename {QString::fromLatin1("%1.fastresume").arg(id.toString())};
    Utils::Fs::removeFile(m_resumeDataDir / resumeFilename);

    const Path torrentFilename {QString::fromLatin1("%1.torrent").arg(id.toString())};
    Utils::Fs::removeFile(m_resumeDataDir / torrentFilename);
}

void BitTorrent::BencodeResumeDataStorage::Worker::storeQueue(const QVector<TorrentID> &queue) const
{
    QByteArray data;
    data.reserve(((BitTorrent::TorrentID::length() * 2) + 1) * queue.size());
    for (const BitTorrent::TorrentID &torrentID : queue)
        data += (torrentID.toString().toLatin1() + '\n');

    const Path filepath = m_resumeDataDir / Path("queue");
    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(filepath, data);
    if (!result)
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
            .arg(filepath.toString(), result.error()), Log::CRITICAL);
    }
}
