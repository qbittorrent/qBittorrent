/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christian Kandeler, Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef SCANFOLDERSMODEL_H
#define SCANFOLDERSMODEL_H

#include <QAbstractTableModel>
#include <QList>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

class FileSystemWatcher;

class ScanFoldersModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(ScanFoldersModel)

public:
    enum PathStatus
    {
        Ok,
        DoesNotExist,
        CannotRead,
        CannotWrite,
        AlreadyInList
    };

    static bool initInstance(QObject *parent = 0);
    static void freeInstance();
    static ScanFoldersModel *instance();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    // TODO: removePaths(); singular version becomes private helper functions;
    // also: remove functions should take modelindexes
    PathStatus addPath(const QString &path, bool downloadAtPath);
    void removePath(int row);
    bool removePath(const QString &path);
    PathStatus setDownloadAtPath(int row, bool downloadAtPath);

    bool downloadInTorrentFolder(const QString &filePath) const;
    void makePersistent();

private slots:
    void configure();
    void addTorrentsToSession(const QStringList &pathList);

private:
    explicit ScanFoldersModel(QObject *parent = 0);
    ~ScanFoldersModel();

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    static ScanFoldersModel *m_instance;
    class PathData;
    int findPathData(const QString &path) const;

    QList<PathData*> m_pathList;
    FileSystemWatcher *m_fsWatcher;
};

#endif // SCANFOLDERSMODEL_H
