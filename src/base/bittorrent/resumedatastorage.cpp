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

#include "resumedatastorage.h"

#include <libtorrent/bdecode.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <QByteArray>
#include <QRegularExpression>
#include <QSaveFile>

#include "base/algorithm.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"
#include "torrentinfo.h"

namespace
{
    template <typename LTStr>
    QString fromLTString(const LTStr &str)
    {
        return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    }

    using ListType = lt::entry::list_type;

    ListType setToEntryList(const QSet<QString> &input)
    {
        ListType entryList;
        entryList.reserve(input.size());
        for (const QString &setValue : input)
            entryList.emplace_back(setValue.toStdString());
        return entryList;
    }
}

BitTorrent::BencodeResumeDataStorage::BencodeResumeDataStorage(const QString &resumeFolderPath)
    : m_resumeDataDir {resumeFolderPath}
{
    const QRegularExpression filenamePattern {QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$")};
    const QStringList filenames = m_resumeDataDir.entryList(QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);

    m_registeredTorrents.reserve(filenames.size());
    for (const QString &filename : filenames)
    {
         const QRegularExpressionMatch rxMatch = filenamePattern.match(filename);
         if (rxMatch.hasMatch())
             m_registeredTorrents.append(TorrentID::fromString(rxMatch.captured(1)));
    }

    QFile queueFile {m_resumeDataDir.absoluteFilePath(QLatin1String("queue"))};
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

    qDebug("Registered torrents count: %d", m_registeredTorrents.size());
}

QVector<BitTorrent::TorrentID> BitTorrent::BencodeResumeDataStorage::registeredTorrents() const
{
    return m_registeredTorrents;
}

std::optional<BitTorrent::LoadTorrentParams> BitTorrent::BencodeResumeDataStorage::load(const TorrentID &id) const
{
    const QString idString = id.toString();
    const QString fastresumePath = m_resumeDataDir.absoluteFilePath(QString::fromLatin1("%1.fastresume").arg(idString));
    const QString torrentFilePath = m_resumeDataDir.absoluteFilePath(QString::fromLatin1("%1.torrent").arg(idString));

    const auto readFile = [](const QString &path, QByteArray &buf) -> bool
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            LogMsg(tr("Cannot read file %1: %2").arg(path, file.errorString()), Log::WARNING);
            return false;
        }

        buf = file.readAll();
        return true;
    };

    QByteArray data;
    if (!readFile(fastresumePath, data))
        return std::nullopt;

    const TorrentInfo metadata = TorrentInfo::loadFromFile(torrentFilePath);

    return loadTorrentResumeData(data, metadata);
}

void BitTorrent::BencodeResumeDataStorage::storeQueue(const QVector<TorrentID> &queue) const
{
    QByteArray data;
    data.reserve(((TorrentID::length() * 2) + 1) * queue.size());
    for (const TorrentID &torrentID : queue)
        data += (torrentID.toString().toLatin1() + '\n');

    const QString filepath = m_resumeDataDir.absoluteFilePath(QLatin1String("queue"));
    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly) || (file.write(data) != data.size()) || !file.commit())
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
            .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

std::optional<BitTorrent::LoadTorrentParams> BitTorrent::BencodeResumeDataStorage::loadTorrentResumeData(
        const QByteArray &data, const TorrentInfo &metadata) const
{
    lt::error_code ec;
    const lt::bdecode_node root = lt::bdecode(data, ec);
    if (ec || (root.type() != lt::bdecode_node::dict_t)) return std::nullopt;

    LoadTorrentParams torrentParams;
    torrentParams.restored = true;
    torrentParams.category = fromLTString(root.dict_find_string_value("qBt-category"));
    torrentParams.name = fromLTString(root.dict_find_string_value("qBt-name"));
    torrentParams.savePath = Profile::instance()->fromPortablePath(
                Utils::Fs::toUniformPath(fromLTString(root.dict_find_string_value("qBt-savePath"))));
    torrentParams.hasSeedStatus = root.dict_find_int_value("qBt-seedStatus");
    torrentParams.firstLastPiecePriority = root.dict_find_int_value("qBt-firstLastPiecePriority");
    torrentParams.seedingTimeLimit = root.dict_find_int_value("qBt-seedingTimeLimit", Torrent::USE_GLOBAL_SEEDING_TIME);

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
    p.save_path = Profile::instance()->fromPortablePath(fromLTString(p.save_path)).toStdString();
    if (metadata.isValid())
        p.ti = metadata.nativeInfo();

    if (p.flags & lt::torrent_flags::stop_when_ready)
    {
        // If torrent has "stop_when_ready" flag set then it is actually "stopped"
        torrentParams.paused = true;
        torrentParams.forced = false;
        // ...but temporarily "resumed" to perform some service jobs (e.g. checking)
        p.flags &= ~lt::torrent_flags::paused;
        p.flags |= lt::torrent_flags::auto_managed;
    }
    else
    {
        torrentParams.paused = (p.flags & lt::torrent_flags::paused) && !(p.flags & lt::torrent_flags::auto_managed);
        torrentParams.forced = !(p.flags & lt::torrent_flags::paused) && !(p.flags & lt::torrent_flags::auto_managed);
    }

    const bool hasMetadata = (p.ti && p.ti->is_valid());
    if (!hasMetadata && !root.dict_find("info-hash"))
        return std::nullopt;

    return torrentParams;
}

void BitTorrent::BencodeResumeDataStorage::store(const TorrentID &id, const LoadTorrentParams &resumeData) const
{
    // We need to adjust native libtorrent resume data
    lt::add_torrent_params p = resumeData.ltAddTorrentParams;
    p.save_path = Profile::instance()->toPortablePath(QString::fromStdString(p.save_path)).toStdString();
    if (resumeData.paused)
    {
        p.flags |= lt::torrent_flags::paused;
        p.flags &= ~lt::torrent_flags::auto_managed;
    }
    else
    {
        // Torrent can be actually "running" but temporarily "paused" to perform some
        // service jobs behind the scenes so we need to restore it as "running"
        if (!resumeData.forced)
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

    data["qBt-savePath"] = Profile::instance()->toPortablePath(resumeData.savePath).toStdString();
    data["qBt-ratioLimit"] = static_cast<int>(resumeData.ratioLimit * 1000);
    data["qBt-seedingTimeLimit"] = resumeData.seedingTimeLimit;
    data["qBt-category"] = resumeData.category.toStdString();
    data["qBt-tags"] = setToEntryList(resumeData.tags);
    data["qBt-name"] = resumeData.name.toStdString();
    data["qBt-seedStatus"] = resumeData.hasSeedStatus;
    data["qBt-contentLayout"] = Utils::String::fromEnum(resumeData.contentLayout).toStdString();
    data["qBt-firstLastPiecePriority"] = resumeData.firstLastPiecePriority;

    const QString filepath = m_resumeDataDir.absoluteFilePath(QString::fromLatin1("%1.fastresume").arg(id.toString()));

    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly))
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
               .arg(filepath, file.errorString()), Log::CRITICAL);
        return;
    }

    lt::bencode(Utils::IO::FileDeviceOutputIterator {file}, data);
    if ((file.error() != QFileDevice::NoError) || !file.commit())
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
               .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

void BitTorrent::BencodeResumeDataStorage::remove(const TorrentID &id) const
{
    const QString resumeFilename = QString::fromLatin1("%1.fastresume").arg(id.toString());
    Utils::Fs::forceRemove(m_resumeDataDir.absoluteFilePath(resumeFilename));

    const QString torrentFilename = QString::fromLatin1("%1.torrent").arg(id.toString());
    Utils::Fs::forceRemove(m_resumeDataDir.absoluteFilePath(torrentFilename));
}
