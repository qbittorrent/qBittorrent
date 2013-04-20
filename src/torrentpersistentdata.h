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

#include <QVariant>
#include <QDateTime>
#include <QDebug>
#include <libtorrent/version.hpp>
#include <libtorrent/magnet_uri.hpp>
#include "qtorrenthandle.h"
#include "misc.h"
#include <vector>
#include "qinisettings.h"
#include <QHash>

class TorrentTempData : private QIniSettings {
public:
  TorrentTempData() : QIniSettings("qBittorrent", "qBittorrent-resume") {
  }

  static bool hasTempData(const QString &hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    return all_data.contains(hash);
  }

  static void deleteTempData(QString hash) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    if (all_data.contains(hash)) {
      all_data.remove(hash);
      setTorrentsTemporaryData(all_data);
    }
  }

  static void setFilesPriority(QString hash,  const std::vector<int> &pp) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    QStringList pieces_priority;

    std::vector<int>::const_iterator pp_it = pp.begin();
    std::vector<int>::const_iterator pp_itend = pp.end();
    while(pp_it != pp_itend) {
      pieces_priority << QString::number(*pp_it);
      ++pp_it;
    }
    data["files_priority"] = pieces_priority;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static void setFilesPath(QString hash, const QStringList &path_list) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["files_path"] = path_list;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static void setSavePath(QString hash, QString save_path) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["save_path"] = save_path;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static void setLabel(QString hash, QString label) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    qDebug("Saving label %s to tmp data", label.toLocal8Bit().data());
    data["label"] = label;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static void setSequential(QString hash, bool sequential) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["sequential"] = sequential;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static bool isSequential(QString hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("sequential", false).toBool();
  }

  static void setSeedingMode(QString hash,bool seed) {
    QHash<QString, QVariant> all_data = TorrentsTemporaryData();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["seeding"] = seed;
    all_data[hash] = data;
    setTorrentsTemporaryData(all_data);
  }

  static bool isSeedingMode(QString hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("seeding", false).toBool();
  }

  static QString getSavePath(QString hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("save_path").toString();
  }

  static QStringList getFilesPath(QString hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("files_path").toStringList();
  }

  static QString getLabel(QString hash) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    qDebug("Got label %s from tmp data", data.value("label", "").toString().toLocal8Bit().data());
    return data.value("label", "").toString();
  }

  static void getFilesPriority(QString hash, std::vector<int> &fp) {
    const QHash<QString, QVariant> &all_data = TorrentsTemporaryData();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    const QList<int> list_var = misc::intListfromStringList(data.value("files_priority").toStringList());
    foreach (const int &var, list_var) {
      fp.push_back(var);
    }
  }

private:
  QHash<QString, QVariant> TorrentsTemporaryData() const {
    return value("torrents-tmp").toHash();
  }

  void setTorrentsTemporaryData(const QHash<QString, QVariant> &data) {
    setValue("torrents-tmp", data);
  }
};

class TorrentPersistentData : private QIniSettings {
public:
  enum RatioLimit {
    USE_GLOBAL_RATIO = -2,
    NO_RATIO_LIMIT = -1
  };

public:
  TorrentPersistentData() : QIniSettings("qBittorrent", "qBittorrent-resume") {
  }

  static bool isKnownTorrent(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    return all_data.contains(hash);
  }

  static QStringList knownTorrents() {
    const QHash<QString, QVariant> &all_data = Torrents();
    return all_data.keys();
  }

  static void setRatioLimit(const QString &hash, qreal ratio) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["max_ratio"] = ratio;
    all_data[hash] = data;
    setTorrents(all_data);
  }

  static qreal getRatioLimit(const QString &hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("max_ratio", USE_GLOBAL_RATIO).toReal();
  }

  static bool hasPerTorrentRatioLimit() {
    const QHash<QString, QVariant> &all_data = Torrents();
    QHash<QString, QVariant>::ConstIterator it = all_data.constBegin();
    QHash<QString, QVariant>::ConstIterator itend = all_data.constEnd();
    for ( ; it != itend; ++it) {
      if (it.value().toHash().value("max_ratio", USE_GLOBAL_RATIO).toReal() >= 0) {
        return true;
      }
    }
    return false;
  }

  static void setAddedDate(QString hash) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    if (!data.contains("add_date")) {
      data["add_date"] = QDateTime::currentDateTime();
      all_data[hash] = data;
      setTorrents(all_data);
    }
  }

  static QDateTime getAddedDate(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    QDateTime dt = data.value("add_date").toDateTime();
    if (!dt.isValid()) {
      setAddedDate(hash);
      dt = QDateTime::currentDateTime();
    }
    return dt;
  }

  static void setErrorState(QString hash, bool has_error) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["has_error"] = has_error;
    all_data[hash] = data;
    setTorrents(all_data);
  }

  static bool hasError(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("has_error", false).toBool();
  }

  static void setPreviousSavePath(QString hash, QString previous_path) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["previous_path"] = previous_path;
    all_data[hash] = data;
    setTorrents(all_data);
  }

  static QString getPreviousPath(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("previous_path").toString();
  }
  
  static void saveSeedDate(const QTorrentHandle &h) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data[h.hash()].toHash();
    if (h.is_seed())
      data["seed_date"] = QDateTime::currentDateTime();
    else
      data.remove("seed_date");
    all_data[h.hash()] = data;
    setTorrents(all_data);
  }

  static QDateTime getSeedDate(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("seed_date").toDateTime();
  }

  static void deletePersistentData(QString hash) {
    QHash<QString, QVariant> all_data = Torrents();
    if (all_data.contains(hash)) {
      all_data.remove(hash);
      setTorrents(all_data);
    }
  }

  static void saveTorrentPersistentData(const QTorrentHandle &h, QString save_path = QString::null, bool is_magnet = false) {
    Q_ASSERT(h.is_valid());
    qDebug("Saving persistent data for %s", qPrintable(h.hash()));
    // Save persistent data
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(h.hash()).toHash();
    data["is_magnet"] = is_magnet;
    if (is_magnet) {
      data["magnet_uri"] = misc::toQString(make_magnet_uri(h));
    }
    data["seed"] = h.is_seed();
    data["priority"] = h.queue_position();
    if (save_path.isEmpty()) {
      qDebug("TorrentPersistantData: save path is %s", qPrintable(h.save_path()));
      data["save_path"] = h.save_path();
    } else {
      qDebug("TorrentPersistantData: overriding save path is %s", qPrintable(save_path));
      data["save_path"] = save_path; // Override torrent save path (e.g. because it is a temp dir)
    }
    // Label
    data["label"] = TorrentTempData::getLabel(h.hash());
    // Save data
    all_data[h.hash()] = data;
    setTorrents(all_data);
    qDebug("TorrentPersistentData: Saving save_path %s, hash: %s", qPrintable(h.save_path()), qPrintable(h.hash()));
    // Set Added date
    setAddedDate(h.hash());
    // Finally, remove temp data
    TorrentTempData::deleteTempData(h.hash());
  }

  // Setters

  static void saveSavePath(QString hash, QString save_path) {
    Q_ASSERT(!hash.isEmpty());
    qDebug("TorrentPersistentData::saveSavePath(%s)", qPrintable(save_path));
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["save_path"] = save_path;
    all_data[hash] = data;
    setTorrents(all_data);
    qDebug("TorrentPersistentData: Saving save_path: %s, hash: %s", qPrintable(save_path), qPrintable(hash));
  }

  static void saveLabel(QString hash, QString label) {
    Q_ASSERT(!hash.isEmpty());
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["label"] = label;
    all_data[hash] = data;
    setTorrents(all_data);
  }

  static void saveName(QString hash, QString name) {
    Q_ASSERT(!hash.isEmpty());
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["name"] = name;
    all_data[hash] = data;
    setTorrents(all_data);
  }

  static void savePriority(const QTorrentHandle &h) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data[h.hash()].toHash();
    data["priority"] = h.queue_position();
    all_data[h.hash()] = data;
    setTorrents(all_data);
  }

  static void saveSeedStatus(const QTorrentHandle &h) {
    QHash<QString, QVariant> all_data = Torrents();
    QHash<QString, QVariant> data = all_data[h.hash()].toHash();
    bool was_seed = data.value("seed", false).toBool();
    if (was_seed != h.is_seed()) {
      data["seed"] = !was_seed;
      all_data[h.hash()] = data;
      setTorrents(all_data);
      if (!was_seed) {
        // Save completion date
        saveSeedDate(h);
      }
    }
  }

  // Getters
  static QString getSavePath(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    //qDebug("TorrentPersistentData: getSavePath %s", data["save_path"].toString().toLocal8Bit().data());
    return data.value("save_path").toString();
  }

  static QString getLabel(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("label", "").toString();
  }

  static QString getName(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("name", "").toString();
  }

  static int getPriority(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("priority", -1).toInt();
  }

  static bool isSeed(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("seed", false).toBool();
  }

  static bool isMagnet(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    return data.value("is_magnet", false).toBool();
  }

  static QString getMagnetUri(QString hash) {
    const QHash<QString, QVariant> &all_data = Torrents();
    const QHash<QString, QVariant> &data = all_data.value(hash).toHash();
    Q_ASSERT(data.value("is_magnet", false).toBool());
    return data.value("magnet_uri").toString();
  }

private:
  QHash<QString, QVariant> Torrents() const {
    return value("torrents").toHash();
  }

  void setTorrents(const QHash<QString, QVariant> &data) {
    setValue("torrents", data);
  }
};

#endif // TORRENTPERSISTENTDATA_H
