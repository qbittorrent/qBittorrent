/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018
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

#pragma once

#include <QDir>
#include <QFileSystemWatcher>
#include <QHash>
#include <QtContainerFwd>
#include <QTimer>
#include <QVector>

/*
 * Subclassing QFileSystemWatcher in order to support Network File
 * System watching (NFS, CIFS) on Linux and Mac OS.
 */
class FileSystemWatcher : public QFileSystemWatcher
{
    Q_OBJECT

public:
    explicit FileSystemWatcher(QObject *parent = nullptr);

    QStringList directories() const;
    void addPath(const QString &path);
    void removePath(const QString &path);

signals:
    void torrentsAdded(const QStringList &pathList);

protected slots:
    void scanLocalFolder(const QString &path);
    void processPartialTorrents();
    void scanNetworkFolders();

private:
    void processTorrentsInDir(const QDir &dir);

    // Partial torrents
    QHash<QString, int> m_partialTorrents;
    QTimer m_partialTorrentTimer;

    QVector<QDir> m_watchedFolders;
    QTimer m_watchTimer;
};
