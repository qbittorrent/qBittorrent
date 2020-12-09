/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  sledgehammer999 <hammered999@gmail.com>
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

#include "filelogger.h"

#include <chrono>

#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QVector>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"

namespace
{
    const std::chrono::seconds FLUSH_INTERVAL {2};
}

FileLogger::FileLogger(const QString &path, const bool backup, const int maxSize, const bool deleteOld, const int age, const FileLogAgeType ageType)
    : m_backup(backup)
    , m_maxSize(maxSize)
{
    m_flusher.setInterval(FLUSH_INTERVAL);
    m_flusher.setSingleShot(true);
    connect(&m_flusher, &QTimer::timeout, this, &FileLogger::flushLog);

    changePath(path);
    if (deleteOld)
        this->deleteOld(age, ageType);

    const Logger *const logger = Logger::instance();
    for (const Log::Msg &msg : asConst(logger->getMessages()))
        addLogMessage(msg);

    connect(logger, &Logger::newLogMessage, this, &FileLogger::addLogMessage);
}

FileLogger::~FileLogger()
{
    closeLogFile();
}

void FileLogger::changePath(const QString &newPath)
{
    const QDir dir(newPath);
    dir.mkpath(newPath);
    const QString tmpPath = dir.absoluteFilePath("qbittorrent.log");

    if (tmpPath != m_path)
    {
        m_path = tmpPath;

        closeLogFile();
        m_logFile.setFileName(m_path);
        openLogFile();
    }
}

void FileLogger::deleteOld(const int age, const FileLogAgeType ageType)
{
    const QDateTime date = QDateTime::currentDateTime();
    const QDir dir(Utils::Fs::branchPath(m_path));
    const QFileInfoList fileList = dir.entryInfoList(QStringList("qbittorrent.log.bak*")
        , (QDir::Files | QDir::Writable), (QDir::Time | QDir::Reversed));

    for (const QFileInfo &file : fileList)
    {
        QDateTime modificationDate = file.lastModified();
        switch (ageType)
        {
        case DAYS:
            modificationDate = modificationDate.addDays(age);
            break;
        case MONTHS:
            modificationDate = modificationDate.addMonths(age);
            break;
        default:
            modificationDate = modificationDate.addYears(age);
        }
        if (modificationDate > date)
            break;
        Utils::Fs::forceRemove(file.absoluteFilePath());
    }
}

void FileLogger::setBackup(const bool value)
{
    m_backup = value;
}

void FileLogger::setMaxSize(const int value)
{
    m_maxSize = value;
}

void FileLogger::addLogMessage(const Log::Msg &msg)
{
    if (!m_logFile.isOpen()) return;

    QTextStream stream(&m_logFile);
    stream.setCodec("UTF-8");

    switch (msg.type)
    {
    case Log::INFO:
        stream << "(I) ";
        break;
    case Log::WARNING:
        stream << "(W) ";
        break;
    case Log::CRITICAL:
        stream << "(C) ";
        break;
    default:
        stream << "(N) ";
    }

    stream << QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString(Qt::ISODate) << " - " << msg.message << '\n';

    if (m_backup && (m_logFile.size() >= m_maxSize))
    {
        closeLogFile();
        int counter = 0;
        QString backupLogFilename = m_path + ".bak";

        while (QFile::exists(backupLogFilename))
        {
            ++counter;
            backupLogFilename = m_path + ".bak" + QString::number(counter);
        }

        QFile::rename(m_path, backupLogFilename);
        openLogFile();
    }
    else
    {
        if (!m_flusher.isActive())
            m_flusher.start();
    }
}

void FileLogger::flushLog()
{
    if (m_logFile.isOpen())
        m_logFile.flush();
}

void FileLogger::openLogFile()
{
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)
        || !m_logFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner))
        {
        m_logFile.close();
        LogMsg(tr("An error occurred while trying to open the log file. Logging to file is disabled."), Log::CRITICAL);
    }
}

void FileLogger::closeLogFile()
{
    m_flusher.stop();
    m_logFile.close();
}
