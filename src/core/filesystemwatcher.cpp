#include <QtGlobal>
#ifndef Q_OS_WIN
#include <QSet>
#include <iostream>
#include <errno.h>
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#include <string.h>
#elif !defined Q_OS_HAIKU
#include <sys/vfs.h>
#endif
#endif

#include "core/preferences.h"
#include "core/bittorrent/torrentinfo.h"
#include "core/bittorrent/magneturi.h"
#include "filesystemwatcher.h"

#ifndef CIFS_MAGIC_NUMBER
#define CIFS_MAGIC_NUMBER 0xFF534D42
#endif

#ifndef NFS_SUPER_MAGIC
#define NFS_SUPER_MAGIC 0x6969
#endif

#ifndef SMB_SUPER_MAGIC
#define SMB_SUPER_MAGIC 0x517B
#endif

const int WATCH_INTERVAL = 10000; // 10 sec
const int MAX_PARTIAL_RETRIES = 5;

FileSystemWatcher::FileSystemWatcher(QObject *parent)
    : QFileSystemWatcher(parent)
{
    m_filters << "*.torrent" << "*.magnet";
    connect(this, SIGNAL(directoryChanged(QString)), SLOT(scanLocalFolder(QString)));
}

FileSystemWatcher::~FileSystemWatcher()
{
#ifndef Q_OS_WIN
    if (m_watchTimer)
        delete m_watchTimer;
#endif
    if (m_partialTorrentTimer)
        delete m_partialTorrentTimer;
}

QStringList FileSystemWatcher::directories() const
{
    QStringList dirs;
#ifndef Q_OS_WIN
    if (m_watchTimer) {
        foreach (const QDir &dir, m_watchedFolders)
            dirs << dir.canonicalPath();
    }
#endif
    dirs << QFileSystemWatcher::directories();
    return dirs;
}

void FileSystemWatcher::addPath(const QString &path)
{
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    QDir dir(path);
    if (!dir.exists()) return;

    // Check if the path points to a network file system or not
    if (isNetworkFileSystem(path)) {
        // Network mode
        qDebug("Network folder detected: %s", qPrintable(path));
        qDebug("Using file polling mode instead of inotify...");
        m_watchedFolders << dir;
        // Set up the watch timer
        if (!m_watchTimer) {
            m_watchTimer = new QTimer(this);
            connect(m_watchTimer, SIGNAL(timeout()), SLOT(scanNetworkFolders()));
            m_watchTimer->start(WATCH_INTERVAL); // 5 sec
        }
    }
    else {
#endif
        // Normal mode
        qDebug("FS Watching is watching %s in normal mode", qPrintable(path));
        QFileSystemWatcher::addPath(path);
        scanLocalFolder(path);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    }
#endif
}

void FileSystemWatcher::removePath(const QString &path)
{
#ifndef Q_OS_WIN
    QDir dir(path);
    for (int i = 0; i < m_watchedFolders.count(); ++i) {
        if (QDir(m_watchedFolders.at(i)) == dir) {
            m_watchedFolders.removeAt(i);
            if (m_watchedFolders.isEmpty())
                delete m_watchTimer;
            return;
        }
    }
#endif
    // Normal mode
    QFileSystemWatcher::removePath(path);
}

void FileSystemWatcher::scanLocalFolder(QString path)
{
    qDebug("scanLocalFolder(%s) called", qPrintable(path));
    QStringList torrents;
    // Local folders scan
    addTorrentsFromDir(QDir(path), torrents);
    // Report detected torrent files
    if (!torrents.empty()) {
        qDebug("The following files are being reported: %s", qPrintable(torrents.join("\n")));
        emit torrentsAdded(torrents);
    }
}

void FileSystemWatcher::scanNetworkFolders()
{
#ifndef Q_OS_WIN
    qDebug("scanNetworkFolders() called");
    QStringList torrents;
    // Network folders scan
    foreach (const QDir &dir, m_watchedFolders) {
        //qDebug("FSWatcher: Polling manually folder %s", qPrintable(dir.path()));
        addTorrentsFromDir(dir, torrents);
    }
    // Report detected torrent files
    if (!torrents.empty()) {
        qDebug("The following files are being reported: %s", qPrintable(torrents.join("\n")));
        emit torrentsAdded(torrents);
    }
#endif
}

void FileSystemWatcher::processPartialTorrents()
{
    QStringList noLongerPartial;

    // Check which torrents are still partial
    foreach (const QString &torrentPath, m_partialTorrents.keys()) {
        if (!QFile::exists(torrentPath)) {
            m_partialTorrents.remove(torrentPath);
        }
        else if (BitTorrent::TorrentInfo::loadFromFile(torrentPath).isValid()) {
            noLongerPartial << torrentPath;
            m_partialTorrents.remove(torrentPath);
        }
        else if (m_partialTorrents[torrentPath] >= MAX_PARTIAL_RETRIES) {
            m_partialTorrents.remove(torrentPath);
            QFile::rename(torrentPath, torrentPath + ".invalid");
        }
        else {
            ++m_partialTorrents[torrentPath];
        }
    }

    // Stop the partial timer if necessary
    if (m_partialTorrents.empty()) {
        m_partialTorrentTimer->stop();
        m_partialTorrentTimer->deleteLater();
        qDebug("No longer any partial torrent.");
    }
    else {
        qDebug("Still %d partial torrents after delayed processing.", m_partialTorrents.count());
        m_partialTorrentTimer->start(WATCH_INTERVAL);
    }

    // Notify of new torrents
    if (!noLongerPartial.isEmpty())
        emit torrentsAdded(noLongerPartial);
}

void FileSystemWatcher::startPartialTorrentTimer()
{
    Q_ASSERT(!m_partialTorrents.isEmpty());
    if (!m_partialTorrentTimer) {
        m_partialTorrentTimer = new QTimer();
        connect(m_partialTorrentTimer, SIGNAL(timeout()), SLOT(processPartialTorrents()));
        m_partialTorrentTimer->setSingleShot(true);
        m_partialTorrentTimer->start(WATCH_INTERVAL);
    }
}

void FileSystemWatcher::addTorrentsFromDir(const QDir &dir, QStringList &torrents)
{
    const QStringList files = dir.entryList(m_filters, QDir::Files, QDir::Unsorted);
    foreach (const QString &file, files) {
        const QString fileAbsPath = dir.absoluteFilePath(file);
        if (fileAbsPath.endsWith(".magnet")) {
            QFile f(fileAbsPath);
            if (f.open(QIODevice::ReadOnly)
                && !BitTorrent::MagnetUri(QString::fromLocal8Bit(f.readAll())).isValid()) {
                torrents << fileAbsPath;
            }
        }
        else if (BitTorrent::TorrentInfo::loadFromFile(fileAbsPath).isValid()) {
            torrents << fileAbsPath;
        }
        else if (!m_partialTorrents.contains(fileAbsPath)) {
            qDebug("Partial torrent detected at: %s", qPrintable(fileAbsPath));
            qDebug("Delay the file's processing...");
            m_partialTorrents.insert(fileAbsPath, 0);
        }
    }

    if (!m_partialTorrents.empty())
        startPartialTorrentTimer();
}

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
bool FileSystemWatcher::isNetworkFileSystem(QString path)
{
    QString file = path;
    if (!file.endsWith("/"))
        file += "/";
    file += ".";
    struct statfs buf;
    if (!statfs(file.toLocal8Bit().constData(), &buf)) {
#ifdef Q_OS_MAC
        // XXX: should we make sure HAVE_STRUCT_FSSTAT_F_FSTYPENAME is defined?
        return ((strcmp(buf.f_fstypename, "nfs") == 0) || (strcmp(buf.f_fstypename, "cifs") == 0) || (strcmp(buf.f_fstypename, "smbfs") == 0));
#else
        return ((buf.f_type == (long)CIFS_MAGIC_NUMBER) || (buf.f_type == (long)NFS_SUPER_MAGIC) || (buf.f_type == (long)SMB_SUPER_MAGIC));
#endif
    }
    else {
        std::cerr << "Error: statfs() call failed for " << qPrintable(file) << ". Supposing it is a local folder..." << std::endl;
        switch(errno) {
        case EACCES:
            std::cerr << "Search permission is denied for a component of the path prefix of the path" << std::endl;
            break;
        case EFAULT:
            std::cerr << "Buf or path points to an invalid address" << std::endl;
            break;
        case EINTR:
            std::cerr << "This call was interrupted by a signal" << std::endl;
            break;
        case EIO:
            std::cerr << "I/O Error" << std::endl;
            break;
        case ELOOP:
            std::cerr << "Too many symlinks" << std::endl;
            break;
        case ENAMETOOLONG:
            std::cerr << "path is too long" << std::endl;
            break;
        case ENOENT:
            std::cerr << "The file referred by path does not exist" << std::endl;
            break;
        case ENOMEM:
            std::cerr << "Insufficient kernel memory" << std::endl;
            break;
        case ENOSYS:
            std::cerr << "The file system does not detect this call" << std::endl;
            break;
        case ENOTDIR:
            std::cerr << "A component of the path is not a directory" << std::endl;
            break;
        case EOVERFLOW:
            std::cerr << "Some values were too large to be represented in the struct" << std::endl;
            break;
        default:
            std::cerr << "Unknown error" << std::endl;
        }

        std::cerr << "Errno: " << errno << std::endl;
        return false;
    }
}
#endif
