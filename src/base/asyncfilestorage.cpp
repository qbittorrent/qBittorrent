/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "asyncfilestorage.h"

#include <QDebug>
#include <QMetaObject>

#include "base/utils/fs.h"
#include "base/utils/io.h"

QHash<Path, std::weak_ptr<QFile>> AsyncFileStorage::m_reservedPaths;
QReadWriteLock AsyncFileStorage::m_reservedPathsLock;

AsyncFileStorage::AsyncFileStorage(const Path &storageFolderPath, QObject *parent)
    : QObject(parent)
    , m_storageDir(storageFolderPath)
{
    Q_ASSERT(m_storageDir.isAbsolute());

    const Path lockFilePath = m_storageDir / Path(u"storage.lock"_s);

    {
        const QReadLocker readLocker {&m_reservedPathsLock};
        m_lockFile = m_reservedPaths.value(lockFilePath).lock();
    }

    if (!m_lockFile)
    {
        const QWriteLocker writeLocker {&m_reservedPathsLock};
        if (std::weak_ptr<QFile> &lockFile = m_reservedPaths[lockFilePath]; lockFile.expired()) [[likely]]
        {
            if (!Utils::Fs::mkpath(m_storageDir))
                throw AsyncFileStorageError(tr("Could not create directory '%1'.").arg(m_storageDir.toString()));

            auto lockFileDeleter = [](QFile *file)
            {
                file->close();
                file->remove();
                delete file;
            };
            m_lockFile = std::shared_ptr<QFile>(new QFile(lockFilePath.data()), std::move(lockFileDeleter));

            // TODO: This folder locking approach does not work for UNIX systems. Implement it.
            if (!m_lockFile->open(QFile::WriteOnly))
                throw AsyncFileStorageError(m_lockFile->errorString());

            lockFile = m_lockFile;
        }
        else
        {
            m_lockFile = lockFile.lock();
        }
    }
}

AsyncFileStorage::~AsyncFileStorage() = default;

void AsyncFileStorage::store(const Path &filePath, const QByteArray &data)
{
    QMetaObject::invokeMethod(this, [this, data, filePath] { store_impl(filePath, data); }, Qt::QueuedConnection);
}

Path AsyncFileStorage::storageDir() const
{
    return m_storageDir;
}

void AsyncFileStorage::store_impl(const Path &fileName, const QByteArray &data)
{
    const Path filePath = m_storageDir / fileName;
    qDebug() << "AsyncFileStorage: Saving data to" << filePath.toString();

    const nonstd::expected<void, QString> result = Utils::IO::saveToFile(filePath, data);
    if (!result)
    {
        qDebug() << "AsyncFileStorage: Failed to save data";
        emit failed(filePath, result.error());
    }
}
