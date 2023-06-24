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

#include <QtGlobal>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QThread>

#include "base/global.h"
#include "base/logger.h"
#include "base/utils/fs.h"
#include "base/utils/gzip.h"

namespace
{
    const std::chrono::seconds FLUSH_INTERVAL {2};

    bool isObsolete(const QFileInfo &info, FileLogger::FileLogAgeType ageType, int age)
    {
        QDateTime modificationDate = info.lastModified();
        switch (ageType)
        {
        case FileLogger::DAYS:
            modificationDate = modificationDate.addDays(age);
            break;
        case FileLogger::MONTHS:
            modificationDate = modificationDate.addMonths(age);
            break;
        default:
            modificationDate = modificationDate.addYears(age);
        }
        return modificationDate <= QDateTime::currentDateTime();
    }

    class WorkerThread final : public QThread
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(WorkerThread)

    public:
        WorkerThread(const Path &p)
            : m_path {p}
        {}

        void run() override
        {
            Path destPath {m_path + u"."_s + QString::number(QDateTime::currentSecsSinceEpoch(), 36) + u".gz"_s};
            QString err;
            if (compressBackupFile(m_path, destPath, 6, err))
            {
                Utils::Fs::removeFile(m_path);
                emit finished(destPath, err, true);
            }
        }

    signals:
        void finished(const Path &p, const QString &e, const bool c);

    private:
        bool compressBackupFile(const Path &sourcePath, const Path &destPath, int level, QString &msg)
        {
            QFile source {sourcePath.data()};
            const QDateTime atime = source.fileTime(QFileDevice::FileAccessTime);
            const QDateTime mtime = source.fileTime(QFileDevice::FileModificationTime);
            // It seems the created time can't be modified on UNIX.
            const QDateTime ctime = source.fileTime(QFileDevice::FileBirthTime);
            const QDateTime mctime = source.fileTime(QFileDevice::FileMetadataChangeTime);

            if (!source.open(QIODevice::ReadOnly))
            {
                msg = FileLogger::tr("Can't open %1!").arg(sourcePath.data());
                return false;
            }

            bool ok = false;
            const QByteArray data = Utils::Gzip::compress(source.readAll(), level, &ok);
            source.close();

            if (ok)
            {
                QFile dest {destPath.data()};
                if (!dest.open(QFile::WriteOnly) || (dest.write(data) == -1))
                {
                    msg = FileLogger::tr("Couldn't save to %1.").arg(destPath.data());
                    ok = false;
                }
                dest.close();

                if (ok)
                {
                    // Change the file's timestamp.
                    dest.open(QIODevice::Append);
                    dest.setFileTime(atime, QFileDevice::FileAccessTime);
                    dest.setFileTime(mtime, QFileDevice::FileModificationTime);
                    dest.setFileTime(ctime, QFileDevice::FileBirthTime);
                    dest.setFileTime(mctime, QFileDevice::FileMetadataChangeTime);
                    dest.close();
                }
                else
                {
                    Utils::Fs::removeFile(destPath);
                }
            }
            return ok;
        }

        Path m_path;
    };
}

FileLogger::FileLogger(const Path &path, const bool backup
                       , const int maxSize, const bool deleteOld, const int age
                       , const FileLogAgeType ageType, bool compressBackups)
    : m_age(age)
    , m_ageType(ageType)
    , m_backup(backup)
    , m_compressBackups(compressBackups)
    , m_deleteOld(deleteOld)
    , m_maxSize(maxSize)
{
    m_flusher.setInterval(FLUSH_INTERVAL);
    m_flusher.setSingleShot(true);
    connect(&m_flusher, &QTimer::timeout, this, &FileLogger::flushLog);

    changePath(path);

    const Logger *const logger = Logger::instance();
    for (const Log::Msg &msg : asConst(logger->getMessages()))
        addLogMessage(msg);

    connect(logger, &Logger::newLogMessage, this, &FileLogger::addLogMessage);
}

FileLogger::~FileLogger()
{
    closeLogFile();
}

void FileLogger::changePath(const Path &newPath)
{
    // compare paths as strings to perform case sensitive comparison on all the platforms
    if (newPath.data() == m_path.parentPath().data())
        return;

    closeLogFile();

    m_path = newPath / Path(u"qbittorrent.log"_s);
    m_logFile.setFileName(m_path.data());

    Utils::Fs::mkpath(newPath);

    if (m_deleteOld)
    {
        deleteOld();
    }

    if (isObsolete(QFileInfo(m_path.data()), m_ageType, m_age))
    {
        Utils::Fs::removeFile(m_path);
    }
    else
    {
        if (m_backup && (m_logFile.size() >= m_maxSize))
        {
            makeBackup();
        }
    }

    openLogFile();
}

Path FileLogger::handleResults(const Path &renameFrom, const QString &msg, const bool compressed) const
{
    if (!(msg.isEmpty()))
    {
        qDebug() << u"Error: "_s << msg;
    }

    Path renameTo = m_path + u".bak"_s + (compressed ? u".gz"_s :  u""_s);
    int counter = 0;

    while(renameTo.exists())
    {
        renameTo = m_path + u".bak"_s + QString::number(++counter) + (compressed ? u".gz"_s :  u""_s);
    }

    Utils::Fs::renameFile(renameFrom, renameTo);
    return renameTo;
}

void FileLogger::makeBackup()
{
    Path renameTo = handleResults(m_path, {}, false);

    if (m_compressBackups)
    {
        WorkerThread *thread = new WorkerThread(renameTo);
        connect(thread, &WorkerThread::finished, this, &FileLogger::handleResults);
        connect(thread, &WorkerThread::finished, thread, &QObject::deleteLater);
        thread->start();
    }
}

void FileLogger::deleteOld()
{
    const QStringList nameFilter { u"qbittorrent.log.bak*"_s + (m_compressBackups ? u".gz"_s : u""_s) };
    const QFileInfoList fileList = QDir(m_path.parentPath().data()).entryInfoList(
            nameFilter, (QDir::Files | QDir::Writable), (QDir::Time | QDir::Reversed));

    for (const QFileInfo &file : fileList)
    {
        if (isObsolete(file, m_ageType, m_age))
        {
            Utils::Fs::removeFile(Path(file.absoluteFilePath()));
        }
        else
        {
            break;
        }
    }
}

void FileLogger::setAge(const int value)
{
    m_age = value;
}

void FileLogger::setAgeType(const FileLogAgeType value)
{
    m_ageType = value;
}


void FileLogger::setBackup(const bool value)
{
    m_backup = value;
}

void FileLogger::setCompressBackups(const bool value)
{
    m_compressBackups = value;
}

void FileLogger::setDeleteOld(const bool value)
{
    m_deleteOld = value;
}

void FileLogger::setMaxSize(const int value)
{
    m_maxSize = value;
}

void FileLogger::addLogMessage(const Log::Msg &msg)
{
    if (!m_logFile.isOpen()) return;

    QTextStream stream(&m_logFile);

    switch (msg.type)
    {
    case Log::INFO:
        stream << QStringView(u"(I) ");
        break;
    case Log::WARNING:
        stream << QStringView(u"(W) ");
        break;
    case Log::CRITICAL:
        stream << QStringView(u"(C) ");
        break;
    default:
        stream << QStringView(u"(N) ");
    }

    stream << QDateTime::fromSecsSinceEpoch(msg.timestamp).toString(Qt::ISODate) << QStringView(u" - ") << msg.message << QChar(u'\n');

    if (m_deleteOld)
    {
        deleteOld();
    }

    if (m_backup && (m_logFile.size() >= m_maxSize))
    {
        closeLogFile();
        makeBackup();
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

#include "filelogger.moc"
