#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QFileSystemWatcher>
#include <QDir>
#include <QTimer>
#include <QPointer>
#include <QStringList>
#include <QHash>

/*
 * Subclassing QFileSystemWatcher in order to support Network File
 * System watching (NFS, CIFS) on Linux and Mac OS.
 */
class FileSystemWatcher : public QFileSystemWatcher
{
    Q_OBJECT

public:
    explicit FileSystemWatcher(QObject *parent = 0);
    ~FileSystemWatcher();

    QStringList directories() const;
    void addPath(const QString &path);
    void removePath(const QString &path);

signals:
    void torrentsAdded(const QStringList &pathList);

protected slots:
    void scanLocalFolder(QString path);
    void scanNetworkFolders();
    void processPartialTorrents();

private:
    void startPartialTorrentTimer();
    void addTorrentsFromDir(const QDir &dir, QStringList &torrents);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    static bool isNetworkFileSystem(QString path);
#endif

#ifndef Q_OS_WIN
    QList<QDir> m_watchedFolders;
    QPointer<QTimer> m_watchTimer;
#endif
    QStringList m_filters;
    // Partial torrents
    QHash<QString, int> m_partialTorrents;
    QPointer<QTimer> m_partialTorrentTimer;
};

#endif // FILESYSTEMWATCHER_H
