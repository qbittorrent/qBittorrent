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

#include "resumedatamanager.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>

#include <QByteArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"

ResumeDataManager::ResumeDataManager(const QString &resumeFolderPath)
    : m_resumeDataDir(resumeFolderPath)
{
}

void ResumeDataManager::load(bool queued)
{
    QStringList fastresumes = m_resumeDataDir.entryList(
                QStringList {QLatin1String("*.fastresume")}, QDir::Files, QDir::Unsorted);

    const auto readFile = [](const QString &path, QByteArray &buf) -> bool
    {
        QFile file {path};
        if (!file.open(QIODevice::ReadOnly)) {
            LogMsg(tr("Cannot read file %1: %2").arg(path, file.errorString()), Log::WARNING);
            return false;
        }

        buf = file.readAll();
        return true;
    };

    qDebug("Loading torrents...");
    qDebug("Queue size: %d", fastresumes.size());

    const QRegularExpression rx {QLatin1String("^([A-Fa-f0-9]{40})\\.fastresume$")};

    if (queued) {
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

        QByteArray rawResumeData;
        QByteArray rawMetadata;
        if (readFile(fastresumePath, rawResumeData)
                && (!QFile::exists(torrentFilePath) || readFile(torrentFilePath, rawMetadata))) {
            emit loaded(hash, rawResumeData, rawMetadata);
        }
        else {
            LogMsg(tr("Unable to load torrent '%1'.", "e.g: Unable to load torrent 'hash'.")
                   .arg(hash), Log::CRITICAL);
        }
    }

    emit loadingDone();
}

void ResumeDataManager::save(const QString &filename, const QByteArray &data) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly) || (file.write(data) != data.size()) || !file.commit()) {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
               .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

void ResumeDataManager::save(const QString &filename, const std::shared_ptr<lt::entry> &data) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly)) {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
               .arg(filepath, file.errorString()), Log::CRITICAL);
        return;
    }

    lt::bencode(Utils::IO::FileDeviceOutputIterator {file}, *data);
    if ((file.error() != QFileDevice::NoError) || !file.commit()) {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
               .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

void ResumeDataManager::remove(const QString &filename) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    Utils::Fs::forceRemove(filepath);
}
