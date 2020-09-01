/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Kacper Michajłow <kasper93@gmail.com>
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

#include "torrentloaderthread.h"

#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>

#include <QDir>
#include <QRegularExpression>

#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"
#include "base/bittorrent/session.h"
#include "base/utils/fs.h"
#include "torrenthandleimpl.h"

using namespace BitTorrent;

namespace
{
    inline QString fromLTString(const lt::string_view &str)
    {
        return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    }
}

TorrentLoaderThread::TorrentLoaderThread(const QString &resumeFolderPath,
                                         bool isQueueingEnabled,
                                         bool isTempPathEnabled,
                                         const QString &tempPath,
                                         QObject *parent)
    : QThread(parent)
    , m_isQueueingEnabled(isQueueingEnabled)
    , m_isTempPathEnabled(isTempPathEnabled)
    , m_tempPath(Utils::Fs::toNativePath(tempPath).toStdString())
    , m_resumeDataDir(resumeFolderPath)
{
    qRegisterMetaType<LoadTorrentParams>();
}

void TorrentLoaderThread::run()
{
    QStringList fastresumes = m_resumeDataDir.entryList(
                QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);

    const auto readFile = [](const QString &path, QByteArray &buf) -> bool
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            LogMsg(tr("Cannot read file %1: %2").arg(path, file.errorString()), Log::WARNING);
            return false;
        }

        buf = file.readAll();
        return true;
    };

    const QRegularExpression rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));

    if (m_isQueueingEnabled) {
        QFile queueFile {m_resumeDataDir.absoluteFilePath(QLatin1String {"queue"})};
        QStringList queue;
        if (queueFile.open(QFile::ReadOnly)) {
            QByteArray line;
            while (!(line = queueFile.readLine()).isEmpty())
                queue.append(QString::fromLatin1(line.trimmed()) + QLatin1String {".fastresume"});
        }
        else {
            LogMsg(tr("Couldn't load torrents queue from '%1'. Error: %2")
                .arg(queueFile.fileName(), queueFile.errorString()), Log::WARNING);
        }

        if (!queue.empty())
            fastresumes = queue + List::toSet(fastresumes).subtract(List::toSet(queue)).values();
    }

    for (const QString &fastresumeName : asConst(fastresumes)) {
        const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
        if (!rxMatch.hasMatch()) continue;

        const QString hash = rxMatch.captured(1);
        const QString fastresumePath = m_resumeDataDir.absoluteFilePath(fastresumeName);
        const QString torrentFilePath = m_resumeDataDir.filePath(QString::fromLatin1("%1.torrent").arg(hash));

        QByteArray data;
        LoadTorrentParams torrentParams;
        TorrentInfo metadata = TorrentInfo::loadFromFile(torrentFilePath);

        if (!readFile(fastresumePath, data) || !loadTorrentResumeData(data, metadata, torrentParams)) {
            LogMsg(tr("Unable to resume torrent '%1'.", "e.g: Unable to resume torrent 'hash'.")
                       .arg(hash), Log::CRITICAL);
            continue;
        }

        emit loadTorrent(torrentParams);
    }
}

bool TorrentLoaderThread::loadTorrentResumeData(const QByteArray &data, const TorrentInfo &metadata, LoadTorrentParams &torrentParams) const
{
    torrentParams = {};

    lt::error_code ec;
    const lt::bdecode_node root = lt::bdecode(data, ec);
    if (ec || (root.type() != lt::bdecode_node::dict_t)) return false;

    torrentParams.restored = true;
    torrentParams.category = fromLTString(root.dict_find_string_value("qBt-category"));
    torrentParams.name = fromLTString(root.dict_find_string_value("qBt-name"));
    torrentParams.savePath = Profile::instance()->fromPortablePath(
        Utils::Fs::toUniformPath(fromLTString(root.dict_find_string_value("qBt-savePath"))));
    torrentParams.hasSeedStatus = root.dict_find_int_value("qBt-seedStatus");
    torrentParams.firstLastPiecePriority = root.dict_find_int_value("qBt-firstLastPiecePriority");
    torrentParams.hasRootFolder = root.dict_find_int_value("qBt-hasRootFolder");
    torrentParams.seedingTimeLimit = root.dict_find_int_value("qBt-seedingTimeLimit", TorrentHandle::USE_GLOBAL_SEEDING_TIME);

    const lt::string_view ratioLimitString = root.dict_find_string_value("qBt-ratioLimit");
    if (ratioLimitString.empty())
        torrentParams.ratioLimit = root.dict_find_int_value("qBt-ratioLimit", TorrentHandle::USE_GLOBAL_RATIO * 1000) / 1000.0;
    else
        torrentParams.ratioLimit = fromLTString(ratioLimitString).toDouble();

    const lt::bdecode_node tagsNode = root.dict_find("qBt-tags");
    if (tagsNode.type() == lt::bdecode_node::list_t) {
        for (int i = 0; i < tagsNode.list_size(); ++i) {
            const QString tag = fromLTString(tagsNode.list_string_value_at(i));
            if (Session::isValidTag(tag))
                torrentParams.tags << tag;
        }
    }

    lt::add_torrent_params &p = torrentParams.ltAddTorrentParams;

    p = lt::read_resume_data(root, ec);
    p.save_path = Profile::instance()->fromPortablePath(fromLTString(p.save_path)).toStdString();
    if (metadata.isValid())
        p.ti = metadata.nativeInfo();

    const bool hasMetadata = (p.ti && p.ti->is_valid());
    if (!hasMetadata && !root.dict_find("info-hash")) {
        static_assert(QT_VERSION_CHECK(QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX) <= QT_VERSION_CHECK(4, 3, 0),
                      "TODO: The following code is deprecated. Remove it after 4.3.0.");
        // === BEGIN DEPRECATED CODE === //
        // Try to load from legacy data used in older versions for torrents w/o metadata
        const lt::bdecode_node magnetURINode = root.dict_find("qBt-magnetUri");
        if (magnetURINode.type() == lt::bdecode_node::string_t) {
            lt::parse_magnet_uri(magnetURINode.string_value(), p, ec);

            if (m_isTempPathEnabled) {
                p.save_path = m_tempPath;
            }
            else {
                // If empty then Automatic mode, otherwise Manual mode
                const QString savePath = torrentParams.savePath.isEmpty()
                                            ? BitTorrent::Session::instance()->categorySavePath(torrentParams.category)
                                            : torrentParams.savePath;
                p.save_path = Utils::Fs::toNativePath(savePath).toStdString();
            }

            const lt::bdecode_node addedTimeNode = root.dict_find("qBt-addedTime");
            if (addedTimeNode.type() == lt::bdecode_node::int_t)
                p.added_time = addedTimeNode.int_value();

            const lt::bdecode_node sequentialNode = root.dict_find("qBt-sequential");
            if (sequentialNode.type() == lt::bdecode_node::int_t) {
                if (static_cast<bool>(sequentialNode.int_value()))
                    p.flags |= lt::torrent_flags::sequential_download;
                else
                    p.flags &= ~lt::torrent_flags::sequential_download;
            }

            if (torrentParams.name.isEmpty() && !p.name.empty())
                torrentParams.name = QString::fromStdString(p.name);
        }
        // === END DEPRECATED CODE === //
        else {
            return false;
        }
    }

    return true;
}
