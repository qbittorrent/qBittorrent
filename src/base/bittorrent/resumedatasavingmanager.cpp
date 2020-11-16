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

#include "resumedatasavingmanager.h"

#include <libtorrent/bencode.hpp>
#include <libtorrent/entry.hpp>

#include <QByteArray>
#include <QSaveFile>

#include "base/logger.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"

ResumeDataSavingManager::ResumeDataSavingManager(const QString &resumeFolderPath)
    : m_resumeDataDir(resumeFolderPath)
{
}

void ResumeDataSavingManager::save(const QString &filename, const QByteArray &data) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly) || (file.write(data) != data.size()) || !file.commit())
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
            .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

void ResumeDataSavingManager::save(const QString &filename, const std::shared_ptr<lt::entry> &data) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    QSaveFile file {filepath};
    if (!file.open(QIODevice::WriteOnly))
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
            .arg(filepath, file.errorString()), Log::CRITICAL);
        return;
    }

    lt::bencode(Utils::IO::FileDeviceOutputIterator {file}, *data);
    if ((file.error() != QFileDevice::NoError) || !file.commit())
    {
        LogMsg(tr("Couldn't save data to '%1'. Error: %2")
            .arg(filepath, file.errorString()), Log::CRITICAL);
    }
}

void ResumeDataSavingManager::remove(const QString &filename) const
{
    const QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    Utils::Fs::forceRemove(filepath);
}
