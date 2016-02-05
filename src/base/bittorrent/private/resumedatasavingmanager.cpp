/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QDebug>
#ifdef QBT_USES_QT5
#include <QSaveFile>
#else
#include <QFile>
#endif

#include "base/logger.h"
#include "base/utils/fs.h"
#include "resumedatasavingmanager.h"

ResumeDataSavingManager::ResumeDataSavingManager(const QString &resumeFolderPath)
    : m_resumeDataDir(resumeFolderPath)
{
}

void ResumeDataSavingManager::saveResumeData(QString infoHash, QByteArray data) const
{
    QString filename = QString("%1.fastresume").arg(infoHash);
    QString filepath = m_resumeDataDir.absoluteFilePath(filename);

    qDebug() << "Saving resume data in" << filepath;
#ifdef QBT_USES_QT5
    QSaveFile resumeFile(filepath);
#else
    QFile resumeFile(filepath);
#endif
    if (resumeFile.open(QIODevice::WriteOnly)) {
        resumeFile.write(data);
#ifdef QBT_USES_QT5
        if (!resumeFile.commit()) {
            Logger::instance()->addMessage(QString("Couldn't save resume data in %1. Error: %2")
                                           .arg(filepath).arg(resumeFile.errorString()), Log::WARNING);
        }
#endif
    }
}
