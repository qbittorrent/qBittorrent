/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#ifndef TORRENTPERSISTENTDATA_H
#define TORRENTPERSISTENTDATA_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QReadWriteLock>
#include <vector>
#include "preferences.h"

QT_BEGIN_NAMESPACE
class QDateTime;
QT_END_NAMESPACE

class QTorrentHandle;

class TorrentTempData
{
    // This class stores strings w/o modifying separators
public:
    static bool hasTempData(const QString &hash);
    static void deleteTempData(const QString &hash);
    static void setFilesPriority(const QString &hash,  const std::vector<int> &pp);
    static void setFilesPath(const QString &hash, const QStringList &path_list);
    static void setSavePath(const QString &hash, const QString &save_path);
    static void setLabel(const QString &hash, const QString &label);
    static void setSequential(const QString &hash, const bool &sequential);
    static bool isSequential(const QString &hash);
    static void setSeedingMode(const QString &hash, const bool &seed);
    static bool isSeedingMode(const QString &hash);
    static QString getSavePath(const QString &hash);
    static QStringList getFilesPath(const QString &hash);
    static QString getLabel(const QString &hash);
    static void getFilesPriority(const QString &hash, std::vector<int> &fp);
    static bool isMoveInProgress(const QString &hash);
    static void enqueueMove(const QString &hash, const QString &queuedPath);
    static void startMove(const QString &hash, const QString &oldPath, const QString& newPath);
    static void finishMove(const QString &hash);
    static QString getOldPath(const QString &hash);
    static QString getNewPath(const QString &hash);
    static QString getQueuedPath(const QString &hash);
    static void setAddPaused(const QString &hash, const bool &paused);
    static bool isAddPaused(const QString &hash);

private:
    struct TorrentData
    {
        TorrentData();
        std::vector<int> files_priority;
        QStringList path_list;
        QString save_path;
        QString label;
        bool sequential;
        bool seed;
        bool add_paused;
    };

    struct TorrentMoveState
    {
        TorrentMoveState(QString oldPath, QString newPath);
        // the moving occurs from oldPath to newPath
        // queuedPath is where files should be moved to, when current moving is completed
        QString oldPath;
        QString newPath;
        QString queuedPath;
    };

    static QHash<QString, TorrentData> data;
    static QHash<QString, TorrentMoveState> torrentMoveStates;
};

class HiddenData
{
public:
    static void addData(const QString &hash);
    static bool hasData(const QString &hash);
    static void deleteData(const QString &hash);
    static int getSize();
    static int getDownloadingSize();
    static void gotMetadata(const QString &hash);

private:
    static QHash<QString, bool> data;
    static unsigned int metadata_counter;
};

class TorrentPersistentData: QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TorrentPersistentData)

public:
    enum RatioLimit
    {
        USE_GLOBAL_RATIO = -2,
        NO_RATIO_LIMIT = -1
    };

    static TorrentPersistentData* instance();
    static void drop();
    ~TorrentPersistentData();

    bool isKnownTorrent(QString hash) const;
    QStringList knownTorrents() const;
    void setRatioLimit(const QString &hash, const qreal &ratio);
    qreal getRatioLimit(const QString &hash) const;
    bool hasPerTorrentRatioLimit() const;
    void setAddedDate(const QString &hash, const QDateTime &time);
    QDateTime getAddedDate(const QString &hash) const;
    void setErrorState(const QString &hash, const bool has_error);
    bool hasError(const QString &hash) const;
    QDateTime getSeedDate(const QString &hash) const;
    void deletePersistentData(const QString &hash);
    void saveTorrentPersistentData(const QTorrentHandle &h, const QString &save_path = QString::null, const bool is_magnet = false);

    // Setters
    void saveSavePath(const QString &hash, const QString &save_path);
    void saveLabel(const QString &hash, const QString &label);
    void saveName(const QString &hash, const QString &name);
    void savePriority(const QTorrentHandle &h);
    void savePriority(const QString &hash, const int &queue_pos);
    void saveSeedStatus(const QTorrentHandle &h);
    void saveSeedStatus(const QString &hash, const bool seedStatus);
    void setHasMissingFiles(const QString &hash, bool missing);

    // Getters
    QString getSavePath(const QString &hash) const;
    QString getLabel(const QString &hash) const;
    QString getName(const QString &hash) const;
    int getPriority(const QString &hash) const;
    bool isSeed(const QString &hash) const;
    bool isMagnet(const QString &hash) const;
    QString getMagnetUri(const QString &hash) const;
    bool getHasMissingFiles(const QString& hash);
public slots:
    void save();

private:
    TorrentPersistentData();
    static TorrentPersistentData* m_instance;
    QHash<QString, QVariant> m_data;
    bool dirty;
    QTimer timer;
    mutable QReadWriteLock lock;
    const QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

};

#endif // TORRENTPERSISTENTDATA_H
