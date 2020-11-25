/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
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
#include <QSaveFile>

AsyncFileStorage::AsyncFileStorage(const QString &storageFolderPath, QObject *parent)
    : QObject(parent)
    , m_storageDir(storageFolderPath)
    , m_lockFile(m_storageDir.absoluteFilePath(QStringLiteral("storage.lock")))
{
    if (!m_storageDir.mkpath(m_storageDir.absolutePath()))
        throw AsyncFileStorageError
        {tr("Could not create directory '%1'.")
                .arg(m_storageDir.absolutePath())};

    // TODO: This folder locking approach does not work for UNIX systems. Implement it.
    if (!m_lockFile.open(QFile::WriteOnly))
        throw AsyncFileStorageError {m_lockFile.errorString()};
}

AsyncFileStorage::~AsyncFileStorage()
{
    m_lockFile.close();
    m_lockFile.remove();
}

void AsyncFileStorage::store(const QString &fileName, const QByteArray &data)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QMetaObject::invokeMethod(this, [this, data, fileName]() { store_impl(fileName, data); }
                              , Qt::QueuedConnection);
#else
    QMetaObject::invokeMethod(this, "store_impl", Qt::QueuedConnection
                              , Q_ARG(QString, fileName), Q_ARG(QByteArray, data));
#endif
}

QDir AsyncFileStorage::storageDir() const
{
    return m_storageDir;
}

void AsyncFileStorage::store_impl(const QString &fileName, const QByteArray &data)
{
    const QString filePath = m_storageDir.absoluteFilePath(fileName);
    QSaveFile file(filePath);
    qDebug() << "AsyncFileStorage: Saving data to" << filePath;
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(data);
        if (!file.commit())
        {
            qDebug() << "AsyncFileStorage: Failed to save data";
            emit failed(filePath, file.errorString());
        }
    }
}
