#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QFileSystemWatcher>
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QStringList>
#include <QHash>

#ifndef Q_OS_WIN
#include <QSet>
#include <iostream>
#include <errno.h>
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD)
#include <sys/param.h>
#include <sys/mount.h>
#include <string.h>
#else
#include <sys/vfs.h>
#endif
#endif

#include "fs_utils.h"
#include "misc.h"

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

/*
 * Subclassing QFileSystemWatcher in order to support Network File
 * System watching (NFS, CIFS) on Linux and Mac OS.
 */
class FileSystemWatcher: public QFileSystemWatcher {
  Q_OBJECT

private:
#ifndef Q_OS_WIN
  QList<QDir> watched_folders;
  QPointer<QTimer> watch_timer;
#endif
  QStringList m_filters;
  // Partial torrents
  QHash<QString, int> m_partialTorrents;
  QPointer<QTimer> m_partialTorrentTimer;

#ifndef Q_OS_WIN
private:
  static bool isNetworkFileSystem(QString path) {
    QString file = path;
    if (!file.endsWith("/"))
      file += "/";
    file += ".";
    struct statfs buf;
    if (!statfs(file.toLocal8Bit().constData(), &buf)) {
#ifdef Q_OS_MAC
      // XXX: should we make sure HAVE_STRUCT_FSSTAT_F_FSTYPENAME is defined?
      return (strcmp(buf.f_fstypename, "nfs") == 0 || strcmp(buf.f_fstypename, "cifs") == 0 || strcmp(buf.f_fstypename, "smbfs") == 0);
#else
      return (buf.f_type == (long)CIFS_MAGIC_NUMBER || buf.f_type == (long)NFS_SUPER_MAGIC || buf.f_type == (long)SMB_SUPER_MAGIC);
#endif
    } else {
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

public:
  FileSystemWatcher(QObject *parent): QFileSystemWatcher(parent) {
    m_filters << "*.torrent" << "*.magnet";
    connect(this, SIGNAL(directoryChanged(QString)), this, SLOT(scanLocalFolder(QString)));
  }

  ~FileSystemWatcher() {
#ifndef Q_OS_WIN
    if (watch_timer)
      delete watch_timer;
#endif
    if (m_partialTorrentTimer)
      delete m_partialTorrentTimer;
  }

  QStringList directories() const {
    QStringList dirs;
#ifndef Q_OS_WIN
    if (watch_timer) {
      foreach (const QDir &dir, watched_folders)
        dirs << dir.canonicalPath();
    }
#endif
    dirs << QFileSystemWatcher::directories();
    return dirs;
  }

  void addPath(const QString & path) {
#ifndef Q_OS_WIN
    QDir dir(path);
    if (!dir.exists())
      return;
    // Check if the path points to a network file system or not
    if (isNetworkFileSystem(path)) {
      // Network mode
      qDebug("Network folder detected: %s", qPrintable(path));
      qDebug("Using file polling mode instead of inotify...");
      watched_folders << dir;
      // Set up the watch timer
      if (!watch_timer) {
        watch_timer = new QTimer(this);
        connect(watch_timer, SIGNAL(timeout()), this, SLOT(scanNetworkFolders()));
        watch_timer->start(WATCH_INTERVAL); // 5 sec
      }
    } else {
#endif
      // Normal mode
      qDebug("FS Watching is watching %s in normal mode", qPrintable(path));
      QFileSystemWatcher::addPath(path);
      scanLocalFolder(path);
#ifndef Q_OS_WIN
    }
#endif
  }

  void removePath(const QString & path) {
#ifndef Q_OS_WIN
    QDir dir(path);
    for (int i = 0; i < watched_folders.count(); ++i) {
      if (QDir(watched_folders.at(i)) == dir) {
        watched_folders.removeAt(i);
        if (watched_folders.isEmpty())
          delete watch_timer;
        return;
      }
    }
#endif
    // Normal mode
    QFileSystemWatcher::removePath(path);
  }

protected slots:
  void scanLocalFolder(QString path) {
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

  void scanNetworkFolders() {
#ifndef Q_OS_WIN
    qDebug("scanNetworkFolders() called");
    QStringList torrents;
    // Network folders scan
    foreach (const QDir &dir, watched_folders) {
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

  void processPartialTorrents() {
    QStringList no_longer_partial;

    // Check which torrents are still partial
    foreach (const QString& torrent_path, m_partialTorrents.keys()) {
      if (!QFile::exists(torrent_path)) {
        m_partialTorrents.remove(torrent_path);
        continue;
      }
      if (fsutils::isValidTorrentFile(torrent_path)) {
        no_longer_partial << torrent_path;
         m_partialTorrents.remove(torrent_path);
      } else {
        if (m_partialTorrents[torrent_path] >= MAX_PARTIAL_RETRIES) {
          m_partialTorrents.remove(torrent_path);
          QFile::rename(torrent_path, torrent_path+".invalid");
        } else {
          m_partialTorrents[torrent_path]++;
        }
      }
    }

    // Stop the partial timer if necessary
    if (m_partialTorrents.empty()) {
      m_partialTorrentTimer->stop();
      m_partialTorrentTimer->deleteLater();
      qDebug("No longer any partial torrent.");
    } else {
      qDebug("Still %d partial torrents after delayed processing.", m_partialTorrents.count());
      m_partialTorrentTimer->start(WATCH_INTERVAL);
    }
    // Notify of new torrents
    if (!no_longer_partial.isEmpty())
      emit torrentsAdded(no_longer_partial);
  }

signals:
  void torrentsAdded(QStringList &pathList);

private:
  void startPartialTorrentTimer() {
    Q_ASSERT(!m_partialTorrents.isEmpty());
    if (!m_partialTorrentTimer) {
      m_partialTorrentTimer = new QTimer();
      connect(m_partialTorrentTimer, SIGNAL(timeout()), SLOT(processPartialTorrents()));
      m_partialTorrentTimer->setSingleShot(true);
      m_partialTorrentTimer->start(WATCH_INTERVAL);
    }
  }

  void addTorrentsFromDir(const QDir &dir, QStringList &torrents) {
    const QStringList files = dir.entryList(m_filters, QDir::Files, QDir::Unsorted);
    foreach (const QString &file, files) {
      const QString file_abspath = dir.absoluteFilePath(file);
      if (file_abspath.endsWith(".magnet")) {
        QFile f(file_abspath);
        if (f.open(QIODevice::ReadOnly)
            && !misc::magnetUriToHash(QString::fromLocal8Bit(f.readAll())).isEmpty()) {
          torrents << file_abspath;
        }
      } else if (fsutils::isValidTorrentFile(file_abspath)) {
        torrents << file_abspath;
      } else {
        if (!m_partialTorrents.contains(file_abspath)) {
          qDebug("Partial torrent detected at: %s", qPrintable(file_abspath));
          qDebug("Delay the file's processing...");
          m_partialTorrents.insert(file_abspath, 0);
        }
      }
    }
    if (!m_partialTorrents.empty())
      startPartialTorrentTimer();
  }

};

#endif // FILESYSTEMWATCHER_H
