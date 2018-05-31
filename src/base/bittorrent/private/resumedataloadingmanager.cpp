/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  sledgehammer999 <hammered999@gmail.com>
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

#include "resumedataloadingmanager.h"

#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>

#include "base/logger.h"
#include "base/profile.h"
#include "base/bittorrent/session.h"
#include "base/utils/fs.h"

namespace libt = libtorrent;
using namespace BitTorrent;

namespace
{
    bool readFile(const QString &path, QByteArray &buf);
    bool loadTorrentResumeData(const QByteArray &data, CreateTorrentParams &torrentParams, int &prio, MagnetUri &magnetUri);
}

ResumeDataLoadingManager::ResumeDataLoadingManager(const QString &resumeFolderPath)
    : m_resumeDataReady(false)
    , m_lock(QMutex::Recursive)
    , m_resumeDataDir(resumeFolderPath)
{
}

void ResumeDataLoadingManager::loadTorrents()
{
    QStringList fastresumes = m_resumeDataDir.entryList(
                QStringList(QLatin1String("*.fastresume")), QDir::Files, QDir::Unsorted);

    int loadedCount = 0;
    int nextQueuePosition = 1;
    int numOfRemappedFiles = 0;
    const QRegularExpression rx(QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$"));
    foreach (const QString &fastresumeName, fastresumes) {
        const QRegularExpressionMatch rxMatch = rx.match(fastresumeName);
        if (!rxMatch.hasMatch()) continue;

        QString hash = rxMatch.captured(1);
        QString fastresumePath = m_resumeDataDir.absoluteFilePath(fastresumeName);
        QByteArray data;
        CreateTorrentParams torrentParams;
        MagnetUri magnetUri;
        int queuePosition;
        if (readFile(fastresumePath, data) && loadTorrentResumeData(data, torrentParams, queuePosition, magnetUri)) {
            ++loadedCount;
            QString filePath = m_resumeDataDir.filePath(QString("%1.torrent").arg(hash));

            if (queuePosition <= nextQueuePosition) {
                QMutexLocker locker(&m_lock);
                m_readyResumeData.push_back({hash, magnetUri, torrentParams, TorrentInfo::loadFromFile(filePath), data});

                if (queuePosition == nextQueuePosition) {
                    ++nextQueuePosition;
                    while (m_queuedResumeData.contains(nextQueuePosition)) {
                        m_readyResumeData.push_back(m_queuedResumeData.take(nextQueuePosition));
                        ++nextQueuePosition;
                    }
                }

                if (!m_resumeDataReady) {
                    emit resumeDataReady();
                    m_resumeDataReady = true;
                }
            }
            else {
                int q = queuePosition;
                for (; m_queuedResumeData.contains(q); ++q) {
                }
                if (q != queuePosition) {
                    ++numOfRemappedFiles;
                }
                m_queuedResumeData[q] = {hash, magnetUri, torrentParams, TorrentInfo::loadFromFile(filePath), data};
            }
        }
    }

    if (numOfRemappedFiles > 0)
        LogMsg(tr("Queue positions were corrected in %1 resume files").arg(numOfRemappedFiles), Log::CRITICAL);

    // readyresumeData might not have been emptied by the main thread yet
    // so we need to append to that
    QMutexLocker locker(&m_lock);
    for (auto it = m_queuedResumeData.cbegin(); it != m_queuedResumeData.cend(); ++it)
        m_readyResumeData.push_back(it.value());
    m_queuedResumeData.clear();

    if (!m_resumeDataReady) {
        emit resumeDataReady();
        m_resumeDataReady = true;
    }

    emit loadingFinished(loadedCount);
}

void ResumeDataLoadingManager::getResumeData(QVector<TorrentResumeData> &out)
{
    QMutexLocker locker(&m_lock);
    out.clear();
    m_readyResumeData.swap(out);
    m_resumeDataReady = false;
}

namespace
{
    template <typename Entry>
    QSet<QString> entryListToSetImpl(const Entry &entry)
    {
        Q_ASSERT(entry.type() == Entry::list_t);
        QSet<QString> output;
        for (int i = 0; i < entry.list_size(); ++i) {
            const QString tag = QString::fromStdString(entry.list_string_value_at(i));
            if (Session::isValidTag(tag))
                output.insert(tag);
            else
                qWarning() << QString("Dropping invalid stored tag: %1").arg(tag);
        }
        return output;
    }

#if LIBTORRENT_VERSION_NUM < 10100
    bool isList(const libt::lazy_entry *entry)
    {
        return entry && (entry->type() == libt::lazy_entry::list_t);
    }

    QSet<QString> entryListToSet(const libt::lazy_entry *entry)
    {
        return entry ? entryListToSetImpl(*entry) : QSet<QString>();
    }
#else
    bool isList(const libt::bdecode_node &entry)
    {
        return entry.type() == libt::bdecode_node::list_t;
    }

    QSet<QString> entryListToSet(const libt::bdecode_node &entry)
    {
        return entryListToSetImpl(entry);
    }
#endif

    bool readFile(const QString &path, QByteArray &buf)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug("Cannot read file %s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
            return false;
        }

        buf = file.readAll();
        return true;
    }

    bool loadTorrentResumeData(const QByteArray &data, CreateTorrentParams &torrentParams, int &prio,  MagnetUri &magnetUri)
    {
        torrentParams = CreateTorrentParams();
        torrentParams.restored = true;
        torrentParams.skipChecking = false;

        libt::error_code ec;
#if LIBTORRENT_VERSION_NUM < 10100
        libt::lazy_entry fast;
        libt::lazy_bdecode(data.constData(), data.constData() + data.size(), fast, ec);
        if (ec || (fast.type() != libt::lazy_entry::dict_t)) return false;
#else
        libt::bdecode_node fast;
        libt::bdecode(data.constData(), data.constData() + data.size(), fast, ec);
        if (ec || (fast.type() != libt::bdecode_node::dict_t)) return false;
#endif

        torrentParams.savePath = Profile::instance().fromPortablePath(
                                   Utils::Fs::fromNativePath(QString::fromStdString(fast.dict_find_string_value("qBt-savePath"))));

        std::string ratioLimitString = fast.dict_find_string_value("qBt-ratioLimit");
        if (ratioLimitString.empty())
            torrentParams.ratioLimit = fast.dict_find_int_value("qBt-ratioLimit", TorrentHandle::USE_GLOBAL_RATIO * 1000) / 1000.0;
        else
            torrentParams.ratioLimit = QString::fromStdString(ratioLimitString).toDouble();
        torrentParams.seedingTimeLimit = fast.dict_find_int_value("qBt-seedingTimeLimit", TorrentHandle::USE_GLOBAL_SEEDING_TIME);
        // **************************************************************************************
        // Workaround to convert legacy label to category
        // TODO: Should be removed in future
        torrentParams.category = QString::fromStdString(fast.dict_find_string_value("qBt-label"));
        if (torrentParams.category.isEmpty())
            // **************************************************************************************
            torrentParams.category = QString::fromStdString(fast.dict_find_string_value("qBt-category"));
        // auto because the return type depends on the #if above.
        const auto tagsEntry = fast.dict_find_list("qBt-tags");
        if (isList(tagsEntry))
            torrentParams.tags = entryListToSet(tagsEntry);
        torrentParams.name = QString::fromStdString(fast.dict_find_string_value("qBt-name"));
        torrentParams.hasSeedStatus = fast.dict_find_int_value("qBt-seedStatus");
        torrentParams.disableTempPath = fast.dict_find_int_value("qBt-tempPathDisabled");
        torrentParams.hasRootFolder = fast.dict_find_int_value("qBt-hasRootFolder");

        magnetUri = MagnetUri(QString::fromStdString(fast.dict_find_string_value("qBt-magnetUri")));
        torrentParams.paused = fast.dict_find_int_value("qBt-paused");
        torrentParams.forced = fast.dict_find_int_value("qBt-forced");
        torrentParams.firstLastPiecePriority = fast.dict_find_int_value("qBt-firstLastPiecePriority");
        torrentParams.sequential = fast.dict_find_int_value("qBt-sequential");

        prio = fast.dict_find_int_value("qBt-queuePosition");

        return true;
    }
}
