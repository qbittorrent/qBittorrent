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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include "filelogger.h"
#include "base/logger.h"
#include "base/utils/fs.h"

FileLogger::FileLogger(const QString &path, const bool backup, const int maxSize, const bool deleteOld, const int age, const FileLogAgeType ageType)
    : m_backup(backup)
    , m_maxSize(maxSize)
    , m_logFile(nullptr)
{
    m_flusher.setInterval(0);
    m_flusher.setSingleShot(true);
    connect(&m_flusher, SIGNAL(timeout()), SLOT(flushLog()));

    changePath(path);
    if (deleteOld)
        this->deleteOld(age, ageType);

    const Logger* const logger = Logger::instance();
    foreach (const Log::Msg& msg, logger->getMessages())
        addLogMessage(msg);

    connect(logger, SIGNAL(newLogMessage(const Log::Msg &)), SLOT(addLogMessage(const Log::Msg &)));
}

FileLogger::~FileLogger()
{
    if (!m_logFile) return;
    closeLogFile();
    delete m_logFile;
}

void FileLogger::changePath(const QString& newPath)
{
    QString tmpPath = Utils::Fs::fromNativePath(newPath);
    if (!tmpPath.endsWith('/'))
        tmpPath += "/";
    QDir dir(tmpPath);
    dir.mkpath(tmpPath);
    tmpPath += "qbittorrent.log";

    if (tmpPath != m_path) {
        m_path = tmpPath;

        if (m_logFile) {
            closeLogFile();
            delete m_logFile;
        }
        m_logFile = new QFile(m_path);
        openLogFile();
    }
}

void FileLogger::deleteOld(const int age, const FileLogAgeType ageType)
{
    QDateTime date = QDateTime::currentDateTime();
    QDir dir(m_path);

    switch (ageType) {
    case DAYS:
        date = date.addDays(age);
        break;
    case MONTHS:
        date = date.addMonths(age);
        break;
    default:
        date = date.addYears(age);
    }

    foreach (const QFileInfo file, dir.entryInfoList(QStringList() << "qbittorrent.log.bak*", QDir::Files | QDir::Writable, QDir::Time | QDir::Reversed)) {
        if (file.lastModified() < date)
            break;
        Utils::Fs::forceRemove(file.absoluteFilePath());
    }
}

void FileLogger::setBackup(bool value)
{
    m_backup = value;
}

void FileLogger::setMaxSize(int value)
{
    m_maxSize = value;
}

void FileLogger::addLogMessage(const Log::Msg &msg)
{
    if (!m_logFile) return;

    QTextStream str(m_logFile);
    QString type;

    switch (msg.type) {
    case Log::INFO:
        type = "(I) ";
        break;
    case Log::WARNING:
        type = "(W) ";
        break;
    case Log::CRITICAL:
        type = "(C) ";
        break;
    default:
        type = "(N) ";
    }

    str << type << QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString(Qt::ISODate) << " - " << msg.message << endl;

    if (m_backup && (m_logFile->size() >= (m_maxSize * 1024 * 1024))) {
        closeLogFile();
        int counter = 0;
        QString backupLogFilename = m_path + ".bak";

        while (QFile::exists(backupLogFilename)) {
            ++counter;
            backupLogFilename = m_path + ".bak" + QString::number(counter);
        }

        QFile::rename(m_path, backupLogFilename);
        openLogFile();
    }
    else {
        m_flusher.start();
    }
}

void FileLogger::flushLog()
{
    if (m_logFile)
        m_logFile->flush();
}

void FileLogger::openLogFile()
{
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)
        || !m_logFile->setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        delete m_logFile;
        m_logFile = nullptr;
        Logger::instance()->addMessage(tr("An error occured while trying to open the log file. Logging to file is disabled."), Log::CRITICAL);
    }
}

void FileLogger::closeLogFile()
{
    m_flusher.stop();
    m_logFile->close();
}
