#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QFileSystemWatcher>

#ifndef Q_WS_WIN
#include <QTimer>
#include <QDir>
#include <QPointer>
#include <QStringList>
#include <QSet>
#include <iostream>
#include <errno.h>
#ifdef Q_WS_MAC
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#endif

#ifndef CIFS_MAGIC_NUMBER
#define CIFS_MAGIC_NUMBER 0xFF534D42
#endif

#ifndef NFS_SUPER_MAGIC
#define NFS_SUPER_MAGIC 0x6969
#endif

/*
 * Subclassing QFileSystemWatcher in order to support Network File
 * System watching (NFS, CIFS) on Linux and Mac OS.
 */
class FileSystemWatcher: public QFileSystemWatcher {
  Q_OBJECT

#ifndef Q_WS_WIN
private:
  QDir watched_folder;
  QPointer<QTimer> watch_timer;
  QStringList filters;

protected:
  bool isNetworkFileSystem(QString path) {
    QString file = path;
    if(!file.endsWith(QDir::separator()))
      file += QDir::separator();
    file += ".";
    struct statfs buf;
    if(!statfs(file.toLocal8Bit().data(), &buf)) {
      return (buf.f_type == (long)CIFS_MAGIC_NUMBER || buf.f_type == (long)NFS_SUPER_MAGIC);
    } else {
      std::cerr << "Error: statfs() call failed for " << file.toLocal8Bit().data() << ". Supposing it is a local folder..." << std::endl;
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
    filters << "*.torrent";
    connect(this, SIGNAL(directoryChanged(QString)), this, SLOT(scanFolder()));
  }

  FileSystemWatcher(QString path, QObject *parent): QFileSystemWatcher(parent) {
    filters << "*.torrent";
    connect(this, SIGNAL(directoryChanged(QString)), this, SLOT(scanFolder()));
    addPath(path);
  }

  ~FileSystemWatcher() {
#ifndef Q_WS_WIN
    if(watch_timer)
      delete watch_timer;
#endif
  }

  QStringList directories() const {
#ifndef Q_WS_WIN
    if(watch_timer)
      return QStringList(watched_folder.path());
#endif
    return QFileSystemWatcher::directories();
  }

  void addPath(const QString & path) {
#ifndef Q_WS_WIN
    watched_folder = QDir(path);
    if(!watched_folder.exists()) return;
    // Check if the path points to a network file system or not
    if(isNetworkFileSystem(path)) {
      // Network mode
      Q_ASSERT(!watch_timer);
      qDebug("Network folder detected: %s", path.toLocal8Bit().data());
      qDebug("Using file polling mode instead of inotify...");
      // Set up the watch timer
      watch_timer = new QTimer(this);
      connect(watch_timer, SIGNAL(timeout()), this, SLOT(scanFolder()));
      watch_timer->start(5000); // 5 sec
    } else {
#endif
      // Normal mode
      QFileSystemWatcher::addPath(path);
      scanFolder();
#ifndef Q_WS_WIN
    }
#endif
  }

  void removePath(const QString & path) {
#ifndef Q_WS_WIN
    if(watch_timer) {
      // Network mode
      if(QDir(path) == watched_folder) {
        delete watch_timer;
      }
    } else {
#endif
      // Normal mode
      QFileSystemWatcher::removePath(path);
#ifndef Q_WS_WIN
    }
#endif
  }

protected slots:
  // XXX: Does not detect file size changes to improve performance.
  void scanFolder() {
    QStringList torrents;
    if(watch_timer)
      torrents = watched_folder.entryList(filters, QDir::Files, QDir::Unsorted);
    else
      torrents = QDir(QFileSystemWatcher::directories().first()).entryList(filters, QDir::Files, QDir::Unsorted);
    if(!torrents.empty())
      emit torrentsAdded(torrents);
  }

signals:
  void torrentsAdded(QStringList &pathList);

};

#endif // FILESYSTEMWATCHER_H
